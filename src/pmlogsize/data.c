/*
 * Copyright (c) 2018 Ken McDonell, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * Note:
 * 	Decoding of data records follows the logic of __pmLogRead_ctx()
 * 	from libpcp ... if that changes, need to make the same changes here.
 */

#include <ctype.h>
#include "pmapi.h"
#include "libpcp.h"
#include "logsize.h"

typedef struct {
    pmID		pmid;
    int			numnames;	/* may not be available */
    char		**names;	/* may not be available */
    long		bytes;
    int			nrec;		/* number of pmResults containing the metric */
    int			nval;		/* total number of values */
    int			ndup_val;	/* number of replicated values */
    long		dup_bytes;	
    int			valfmt;
    __pmHashCtl		values;
} metric_t;

static int		nmetric;	/* number of unique metrics seen */
static metric_t		*metric_tab;	/* metric entries */

/* sort largest first */
static int
metric_compar(const void *a, const void *b)
{
    return ((metric_t *)a)->bytes < ((metric_t *)b)->bytes;
}

static int
value_eq(pmValueBlock *ap, pmValueBlock *bp)
{
    char	*avp;
    char	*bvp;
    int		i;
    int		vlen = ap->vlen - sizeof(int);

    if (ap->vlen != bp->vlen)
	return 0;
    if (ap->vtype != bp->vtype)
	return 0;

    avp = ap->vbuf;
    bvp = bp->vbuf;
    for (i = 0; i < vlen; i++) {
	if (*avp != *bvp)
	    break;
	avp++;
	bvp++;
    }

    return i == vlen ? 1 : 0;
}

/*
 * Callback to free space for value cached with an instance hash node
 */
static __pmHashWalkState
hash_cb(const __pmHashNode *hptr, void *info)
{
    metric_t	*metricp = (metric_t *)info;
    pmValue	*vp = (pmValue *)hptr->data;

    if (metricp->valfmt != PM_VAL_INSITU)
	free(vp->value.pval);
    free(vp);

    return PM_HASH_WALK_DELETE_NEXT;
}

/*
 * if all == 0, cleanup cached values, e.g. after <mark>
 * if all == 1, cleanup cached values and metric_tab
 */
static void
cleanup(int all)
{
    metric_t	*metricp;

    for (metricp = metric_tab; metricp < &metric_tab[nmetric]; metricp++) {
	if (all && metricp->numnames > 0)
	    free(metricp->names);
	if (rflag) {
	    /* free hash table for instance values */
	    __pmHashWalkCB(hash_cb, metricp, &metricp->values);
	    /* free hash table */
	    if (metricp->values.hash != NULL)
		free(metricp->values.hash);
	    /* reset hash table */
	    __pmHashInit(&metricp->values);
	}

    }
    if (all)
	free(metric_tab);

}

void
do_data(__pmFILE *f, char *fname)
{
    long	oheadbytes = __pmFtell(f);
    long	bytes = 0;
    long	sum_bytes;
    int		nrec = 0;
    int		nmark = 0;
    __pmPDU	header;
    __pmPDU	trailer;
    int		need;
    int		rlen;
    int		sts;
    int		i;
    int		j;
    int		k;
    int		ctx;
    char	*buf;
    struct stat	sbuf;
    metric_t	*metricp;
    __pmPDUHdr *php;
    pmResult	*rp;

    nmetric = 0;
    metric_tab = NULL;

    ctx = pmNewContext(PM_CONTEXT_ARCHIVE, fname);	/* OK if this fails */

    __pmFstat(f, &sbuf);

    while ((sts = __pmFread(&header, 1, sizeof(header), f)) == sizeof(header)) {
	oheadbytes += sizeof(header);
	header = ntohl(header);
	rlen = header - (int)sizeof(header) - (int)sizeof(trailer);
	bytes += rlen - sizeof(pmTimeval) - sizeof(__pmPDU);
	oheadbytes += sizeof(pmTimeval) + sizeof(__pmPDU);
	need = rlen + sizeof(__pmPDUHdr);
	buf = (char *)__pmFindPDUBuf(need);
	if (buf == NULL) {
	    fprintf(stderr, "Error: data buffer __pmFindPDUBuf(%d) failed\n", need);
	    exit(1);
	}

	/*
	 * read record, but leave prefix space for __pmPDUHdr so we can
	 * use __pmDecodeResult() below
	 */
	if ((sts = __pmFread(&buf[sizeof(__pmPDUHdr)], 1, rlen, f)) != rlen) {
	    fprintf(stderr, "Error: data read failed: len %d not %d\n", sts, need);
	    exit(1);
	}

	php = (__pmPDUHdr *)buf;
	php->len = header + sizeof(trailer);
	php->type = PDU_RESULT;
	php->from = FROM_ANON;

	sts = __pmDecodeResult((__pmPDU *)buf, &rp);
	if (sts < 0) {
	    fprintf(stderr, "Error: __pmDecodeResult failed: %s\n", pmErrStr(sts));
	    exit(1);
	}

	__pmFread(&trailer, 1, sizeof(trailer), f);
	oheadbytes += sizeof(trailer);

	if (rp->numpmid == 0) {
	    /* <mark> record */
	    if (rflag)
		cleanup(0);
	    nmark++;
	    continue;
	}

	if (dflag) {
	    /* per metric details */
	    for (i = 0; i < rp->numpmid; i++) {
		pmValueSet	*vsp = rp->vset[i];
		pmID		pmid = vsp->pmid;
		int		len;
		__pmHashNode	*hptr;
		pmValue		*vp;

		if (vflag)
		    printf("PMID: %s", pmIDStr(pmid));
		for (j = 0, metricp = metric_tab; j < nmetric; j++, metricp++) {
		    if (metricp->pmid == pmid)
			break;
		}
		if (j == nmetric) {
		    /* first time seen for this metric */
		    metric_t		*metric_tab_tmp;
		    nmetric++;
		    metric_tab_tmp = (metric_t *)realloc(metric_tab, nmetric*sizeof(metric_t));
		    if (metric_tab_tmp == NULL) {
			fprintf(stderr, "Error: data metric_tab realloc(%d) failed\n", (int)(nmetric*sizeof(metric_t)));
			exit(1);
		    }
		    metric_tab = metric_tab_tmp;
		    metricp = &metric_tab[j];
		    metricp->pmid = pmid;
		    metricp->bytes = 0;
		    metricp->nrec = 0;
		    metricp->nval = 0;
		    metricp->ndup_val = 0;
		    metricp->dup_bytes = 0;
		    metricp->valfmt = vsp->valfmt;
		    if (rflag)
			__pmHashInit(&metricp->values);
		    if (ctx >= 0) {
			/* we have a PMAPI context, so get the metric name(s) */
			metricp->numnames = pmNameAll(pmid, &metricp->names);
			if (vflag) {
			    if (metricp->numnames > 0) {
				printf(" (");
				for (k = 0; k < metricp->numnames; k++) {
				    if (k > 0)
					printf(", ");
				    printf("%s", metricp->names[k]);
				}
				printf(")");
			    }
			}
		    }
		}
		metricp->nrec++;

		len = sizeof(vsp->pmid) + sizeof(vsp->numval);
		if (vsp->numval > 0)
		    len += sizeof(vsp->valfmt) + vsp->numval * sizeof(__pmValue_PDU);
		for (j = 0; j < vsp->numval; j++) {
		    if (vsp->valfmt != PM_VAL_INSITU)
			len += PM_PDU_SIZE_BYTES(vsp->vlist[j].value.pval->vlen);
		    metricp->nval++;

		    if (!rflag)
			continue;

		    /*
		     * replicated value?
		     * check this value against previous value (if any)
		     * for this metric-instance
		     */
		    if ((hptr = __pmHashSearch(vsp->vlist[j].inst, &metricp->values)) == NULL) {
			if (__pmHashAdd(vsp->vlist[j].inst, NULL, &metricp->values) < 0) {
			    fprintf(stderr, "Error: __pmHashAdd failed for pmid %s and inst %d\n", pmIDStr(vsp->pmid), vsp->vlist[j].inst);
			    exit(1);
			}
			if ((hptr = __pmHashSearch(vsp->vlist[j].inst, &metricp->values)) == NULL) {
			    fprintf(stderr, "Error: __pmHashSearch after __pmHashAdd failed for pmid %s and inst %d\n", pmIDStr(vsp->pmid), vsp->vlist[j].inst);
			    exit(1);
			}
		    }
		    if (hptr->data != NULL) {
			/* have previous value */
			vp = (pmValue *)hptr->data;
			if (vsp->valfmt == PM_VAL_INSITU) {
			    if (vsp->vlist[j].value.lval == vp->value.lval) {
				/* replicated value */
				metricp->ndup_val++;
				metricp->dup_bytes += sizeof(__pmValue_PDU);
			    }
			    else {
				/* different, save this value for next time */
				vp->value.lval = vsp->vlist[j].value.lval;
			    }
			}
			else {
			    vp = hptr->data;
			    if (value_eq(vsp->vlist[j].value.pval, vp->value.pval)) {
				/* replicated value */
				metricp->ndup_val++;
				metricp->dup_bytes += sizeof(__pmValue_PDU) + vp->value.pval->vlen;
			    }
			    else {
				/*
				 * different, need to free old pmValueBlock
				 * and initialize a new one for next time
				 */
				int	vlen = vsp->vlist[j].value.pval->vlen;
				free(vp->value.pval);
				vp->value.pval = (pmValueBlock *)malloc(vlen);
				if (vp->value.pval == NULL) {
				    fprintf(stderr, "Error: pmid %s inst %d pmValueBlock(%d) re-malloc failed\n", pmIDStr(vsp->pmid), vsp->vlist[j].inst, vlen);
				    exit(1);
				}
				memcpy(vp->value.pval, vsp->vlist[j].value.pval, vlen);
			    }
			}
		    }
		    else {
			/* no previous value, save this one for next time */
			vp = (pmValue *)malloc(sizeof(pmValue));
			if (vp == NULL) {
			    fprintf(stderr, "Error: pmid %s inst %d pmValue malloc failed\n", pmIDStr(vsp->pmid), vsp->vlist[j].inst);
			    exit(1);
			}
			hptr->data = vp;
			vp->inst = vsp->vlist[j].inst;
			if (vsp->valfmt == PM_VAL_INSITU) {
			    vp->value.lval = vsp->vlist[j].value.lval;
			}
			else {
			    int		vlen = vsp->vlist[j].value.pval->vlen;
			    vp->value.pval = (pmValueBlock *)malloc(vlen);
			    if (vp->value.pval == NULL) {
				fprintf(stderr, "Error: pmid %s inst %d pmValueBlock(%d) malloc failed\n", pmIDStr(vsp->pmid), vsp->vlist[j].inst, vlen);
				exit(1);
			    }
			    memcpy(vp->value.pval, vsp->vlist[j].value.pval, vlen);
			}
		    }
		    
		}
		if (vflag)
		    printf(" bytes=%d\n", len);

		metricp->bytes += len;
	    }

	}

	pmFreeResult(rp);
	if ((sts = __pmUnpinPDUBuf(buf)) < 0) {
	    fprintf(stderr, "Error: __pmUnpinPDUBuf failed: %s\n", pmErrStr(sts));
	    exit(1);
	}

	nrec++;
    }

    printf("  data: %ld bytes [%.0f%%, %d records",
	bytes, 100*(float)bytes/sbuf.st_size, nrec);
    if (nmark > 0)
	printf(" (+ %d <mark> records)", nmark);
    printf("]\n");

    if (dflag && nmetric != 0) {
	sum_bytes = 0;
	qsort(metric_tab, nmetric, sizeof(metric_tab[0]), metric_compar);
	for (metricp = metric_tab; metricp < &metric_tab[nmetric]; metricp++) {
	    if (thres != -1 && 100*(float)sum_bytes/bytes > thres) {
		/* -x cutoff reached */
		printf("    ...\n");
		break;
	    }
	    printf("    %s: %ld bytes [%.0f%%, %d record",
		pmIDStr(metricp->pmid), metricp->bytes,
		100*(float)metricp->bytes/sbuf.st_size,
		metricp->nrec);
	    if (metricp->nrec != 1)
		putchar('s');
	    printf(", %d value", metricp->nval);
	    if (metricp->nval != 1)
		putchar('s');
	    if (rflag && metricp->ndup_val > 0) {
		printf(" (%ld bytes for", metricp->dup_bytes);
		printf(" %d dup", metricp->ndup_val);
		if (metricp->ndup_val != 1)
		    putchar('s');
		putchar(')');
	    }
	    putchar(']');
	    if (metricp->numnames > 0) {
		printf(" (");
		for (k = 0; k < metricp->numnames; k++) {
		    if (k > 0)
			printf(", ");
		    printf("%s", metricp->names[k]);
		}
		printf(")");
	    }
	    putchar('\n');
	    sum_bytes += metricp->bytes;
	}
    }

    printf("  overhead: %ld bytes [%.0f%%]\n", oheadbytes, 100*(float)oheadbytes/sbuf.st_size);
    sbuf.st_size -= (bytes + oheadbytes);
    if (sbuf.st_size != 0)
	printf("  unaccounted for: %ld bytes\n", (long)sbuf.st_size);

    cleanup(1);

    return;
}

