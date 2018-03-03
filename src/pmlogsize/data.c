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
    __pmHashNode	*values;
} metric_t;

/* sort largest first */
static int
metric_compar(const void *a, const void *b)
{
    return ((metric_t *)a)->bytes < ((metric_t *)b)->bytes;
}

void
do_data(__pmFILE *f, char *fname)
{
    long	oheadbytes = __pmFtell(f);
    long	bytes = 0;
    long	sum_bytes;
    int		nrec = 0;
    __pmPDU	header;
    __pmPDU	trailer;
    int		need;
    int		rlen;
    int		sts;
    int		i;
    int		j;
    int		k;
    int		ctx;
    int		buflen = 0;
    char	*buf = NULL;
    struct stat	sbuf;
    int		nmetric = 0;		/* number of unique metrics seen */
    metric_t	*metric_tab = NULL;	/* metric entries */
    metric_t	*metricp;
    __pmPDUHdr *php;
    pmResult	*rp;

    ctx = pmNewContext(PM_CONTEXT_ARCHIVE, fname);	/* OK if this fails */

    __pmFstat(f, &sbuf);

    while ((sts = __pmFread(&header, 1, sizeof(header), f)) == sizeof(header)) {
	oheadbytes += sizeof(header);
	header = ntohl(header);
	rlen = header - (int)sizeof(header) - (int)sizeof(trailer);
	bytes += rlen - sizeof(pmTimeval) - sizeof(__pmPDU);
	oheadbytes += sizeof(pmTimeval) + sizeof(__pmPDU);
	need = rlen + sizeof(__pmPDUHdr);
	if (need > buflen) {
	    if (buf != NULL)
		free(buf);
	    buf = (char *)malloc(need);
	    if (buf == NULL) {
		fprintf(stderr, "Error: data buffer malloc(%d) failed\n", need);
		exit(1);
	    }
	    buflen = need;
	}
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

	if (dflag) {
	    for (i = 0; i < rp->numpmid; i++) {
		pmValueSet	*vsp = rp->vset[i];
		pmID		pmid = vsp->pmid;
		int		len;

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
		    metricp->values = NULL;
		    if (ctx >= 0)
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
		metricp->nrec++;

		len = sizeof(vsp->pmid) + sizeof(vsp->numval);
		if (vsp->numval > 0)
		    len += sizeof(vsp->valfmt) + vsp->numval * sizeof(__pmValue_PDU);
		for (j = 0; j < vsp->numval; j++) {
		    if (vsp->valfmt != PM_VAL_INSITU) {
			len += PM_PDU_SIZE_BYTES(vsp->vlist[j].value.pval->vlen);
		    }
		}
		if (vflag)
		    printf(" bytes=%d\n", len);

		metricp->bytes += len;
	    }

	}

	pmFreeResult(rp);

	__pmFread(&trailer, 1, sizeof(trailer), f);
	oheadbytes += sizeof(trailer);
	nrec++;
    }

    printf("  data: %ld bytes [%.0f%%, %d records]\n",
	bytes, 100*(float)bytes/sbuf.st_size, nrec);

    if (dflag) {
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
	    if (metricp->nrec > 1)
		putchar('s');
#if 0
	    printf(", %d values", metricp->nuniq_inst + metricp->ndup_inst);
	    if (metricp->nuniq_inst + metricp->ndup_inst > 1)
		putchar('s');
	    if (rflag && metricp->ndup_inst > 0) {
		printf(" (%ld bytes for", metricp->dup_bytes);
		printf(" %d dup", metricp->ndup_inst);
		if (metricp->ndup_inst > 1)
		    putchar('s');
		putchar(')');
	    }
#endif
	    putchar(']');
	    putchar('\n');
	    sum_bytes += metricp->bytes;
	}
    }

    printf("  overhead: %ld bytes [%.0f%%]\n", oheadbytes, 100*(float)oheadbytes/sbuf.st_size);
    sbuf.st_size -= (bytes + oheadbytes);
    if (sbuf.st_size != 0)
	printf("  unaccounted for: %ld bytes\n", (long)sbuf.st_size);

    return;
}

