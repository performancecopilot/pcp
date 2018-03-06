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
 * 	Decoding of metadata records follows the logic of __pmLogLoadMeta()
 * 	from libpcp ... if that changes, need to make the same changes here.
 */

#include <ctype.h>
#include "pmapi.h"
#include "libpcp.h"
#include "logsize.h"

typedef struct {
    int		inst;		/* internal instance identifier */
    char	*name;		/* external instance identifier */
} inst_t;


typedef struct {
    pmInDom	indom;
    int		nrec;
    long	bytes;			/* total bytes per indom */
    int		nuniq_inst;		/* number of unique instances */
    inst_t	*inst_tab;		/* unique instances */
    int		ndup_inst;		/* inst and name the same */
    long	dup_bytes;		/* bytes for inst and name the same */
} indom_t;

/* sort largest first */
static int
indom_compar(const void *a, const void *b)
{
    return ((indom_t *)a)->bytes < ((indom_t *)b)->bytes;
}

void
do_meta(__pmFILE *f)
{
    long	oheadbytes = __pmFtell(f);
    long	bytes[5] = { 0, 0, 0, 0, 0 };
    long	sum_bytes;
    int		nrec[5] = { 0, 0, 0, 0, 0 };
    __pmLogHdr	header;
    __pmPDU	trailer;
    int		need;
    int		sts;
    int		i;
    int		j;
    int		k;
    int		buflen = 0;
    char	*buf = NULL;
    char	*bufp;
    struct stat	sbuf;
    int		nindom = 0;		/* number of unique indoms seen */
    indom_t	*indom_tab = NULL;	/* nindom entries */
    indom_t	*indomp;

    __pmFstat(f, &sbuf);

    while ((sts = __pmFread(&header, 1, sizeof(header), f)) == sizeof(header)) {
	oheadbytes += sizeof(header.len);
	header.len = ntohl(header.len);
	header.type = ntohl(header.type);
	need = header.len - (int)sizeof(__pmLogHdr) - (int)sizeof(trailer);
	if (need > buflen) {
	    if (buf != NULL)
		free(buf);
	    buf = (char *)malloc(need);
	    if (buf == NULL) {
		fprintf(stderr, "Error: metadata buffer malloc(%d) failed\n", need);
		exit(1);
	    }
	    buflen = need;
	}
	if ((sts = __pmFread(buf, 1, need, f)) != need) {
	    fprintf(stderr, "Error: metadata read failed: len %d not %d\n", sts, need);
	    exit(1);
	}
	if (header.type < TYPE_DESC || header.type > TYPE_TEXT) {
	    fprintf(stderr, "Error: bad metadata type: %d\n", header.type);
	    exit(1);
	}
	nrec[header.type]++;
	bytes[header.type] += (int)sizeof(header.type) + need;
	
	switch (header.type) {
	    case TYPE_DESC:
		if (vflag) {
		    pmDesc	*dp = (pmDesc *)buf;
		    __pmPDU	*ip;
		    int		numnames;

		    dp->pmid = __ntohpmID(dp->pmid);
		    printf("PMID: %s", pmIDStr(dp->pmid));
		    dp++;
		    ip = (__pmPDU *)dp;
		    numnames = ntohl(*ip);
		    ip++;
		    bufp = (char *)ip;
		    for (i = 0; i < numnames; i++) {
			__pmPDU	len;
			memmove((void *)&len, (void*)bufp, sizeof(len));
			len = ntohl(len);
			bufp += sizeof(len);
			printf(" %*.*s", len, len, bufp);
			bufp += len;
		    }
		    putchar('\n');
		}
		break;
	    case TYPE_INDOM:
		if (vflag || dflag || rflag) {
		    pmInDom	indom;
		    int		ninst;
		    int		inst;
		    __pmPDU	*stridx;
		    char	*str;

		    bufp = buf;
		    bufp += sizeof(pmTimeval);
		    indom = __ntohpmInDom(*((__pmPDU *)bufp));
		    bufp += sizeof(pmInDom);
		    if (vflag)
			printf("INDOM: %s", pmInDomStr(indom));
		    for (i = 0, indomp = indom_tab; i < nindom; i++, indomp++) {
			if (indomp->indom == indom)
			    break;
		    }
		    if (i == nindom) {
			/* first time seen for this indom */
			indom_t		*indom_tab_tmp;
			nindom++;
			indom_tab_tmp = (indom_t *)realloc(indom_tab, nindom*sizeof(indom_t));
			if (indom_tab_tmp == NULL) {
			    fprintf(stderr, "Error: metadata indom_tab realloc(%d) failed\n", (int)(nindom*sizeof(indom_t)));
			    exit(1);
			}
			indom_tab = indom_tab_tmp;
			indomp = &indom_tab[i];
			indomp->indom = indom;
			indomp->nrec = 0;
			indomp->bytes = 0;
			indomp->nuniq_inst = 0;
			indomp->inst_tab = NULL;
			indomp->ndup_inst = 0;
			indomp->dup_bytes = 0;
		    }

		    indomp->nrec++;
		    ninst = ntohl(*((__pmPDU *)bufp));
		    bufp += sizeof(__pmPDU);
		    /* record type, timestamp, indom, numinst */
		    indomp->bytes += sizeof(__pmPDU) + sizeof(pmTimeval) + sizeof(pmInDom) + sizeof(__pmPDU);
		    if (vflag) {
			printf(" %d instance", ninst);
			if (ninst > 1)
			    putchar('s');
		    }
		    stridx = (__pmPDU *)&bufp[ninst*sizeof(__pmPDU)];
		    str = (char *)&bufp[2*ninst*sizeof(__pmPDU)];
		    for (j = 0; j < ninst; j++) {
			inst = ntohl(*((__pmPDU *)bufp));
			bufp += sizeof(__pmPDU);
			stridx[j] = ntohl(stridx[j]);
			if (vflag) {
			    if (j == 0)
				printf(" %d \"%s\"", inst, &str[stridx[j]]);
			    else if (j == ninst-1)
				printf(" ... %d \"%s\"", inst, &str[stridx[j]]);
			}
			for (k = 0; k < indomp->nuniq_inst; k++) {
			    if (indomp->inst_tab[k].inst != inst)
				continue;
			    if (strcmp(indomp->inst_tab[k].name, &str[stridx[j]]) == 0)
				break;
			}
			if (k == indomp->nuniq_inst) {
			    /* first time for this instance in this indom */
			    inst_t	*inst_tab_tmp;
			    indomp->nuniq_inst++;
			    inst_tab_tmp = (inst_t *)realloc(indomp->inst_tab, indomp->nuniq_inst*sizeof(inst_t));
			    if (inst_tab_tmp == NULL) {
				fprintf(stderr, "Error: metadata inst_tab realloc(%d) failed\n", (int)(indomp->nuniq_inst*sizeof(inst_t)));
				exit(1);
			    }
			    indomp->inst_tab = inst_tab_tmp;
			    indomp->inst_tab[k].inst = inst;
			    if ((indomp->inst_tab[k].name = strdup(&str[stridx[j]])) == NULL) {
				fprintf(stderr, "Error: metadata inst name strdup(\"%s\") failed\n", &str[stridx[j]]);
				exit(1);
			    }

			}
			else {
			    /* duplicate instance in this indom */
			    indomp->ndup_inst++;
			    indomp->dup_bytes += 2*sizeof(__pmPDU) + strlen(indomp->inst_tab[k].name) + 1;
			}
			indomp->bytes += 2*sizeof(__pmPDU) + strlen(indomp->inst_tab[k].name) + 1;
		    }
		    if (vflag)
			putchar('\n');
		}
		break;
	    case TYPE_LABEL:
		if (dflag) {
		    /* TODO */
		    if (nrec[TYPE_LABEL] == 1)
			printf("LABEL: TODO ... nothing reported as yet\n");
		}
		break;
	    case TYPE_TEXT:
		if (dflag) {
		    /* TODO */
		    if (nrec[TYPE_TEXT] == 1)
			printf("TEXT: TODO ... nothing reported as yet\n");
		}
		break;
	}

	__pmFread(&trailer, 1, sizeof(trailer), f);
	oheadbytes += sizeof(trailer);
    }

    if (nrec[TYPE_DESC] > 0) {
	printf("  metrics: %ld bytes [%.0f%%, %d records]\n",
	    bytes[TYPE_DESC], 100*(float)bytes[TYPE_DESC]/sbuf.st_size, nrec[TYPE_DESC]);
    }

    if (nrec[TYPE_INDOM] > 0) {
	printf("  indoms: %ld bytes [%.0f%%, %d records",
	    bytes[TYPE_INDOM], 100*(float)bytes[TYPE_INDOM]/sbuf.st_size, nrec[TYPE_INDOM]);
	if (dflag) {
	    j = 0;
	    if (indom_tab != NULL) {
		for (indomp = indom_tab; indomp < &indom_tab[nindom]; indomp++) {
		    j += indomp->nuniq_inst + indomp->ndup_inst;
		}
	    }
	    printf(", %d instances", j);
	    sum_bytes = 0;
	}
	printf("]\n");

	if (dflag) {
	    qsort(indom_tab, nindom, sizeof(indom_tab[0]), indom_compar);
	    for (indomp = indom_tab; indomp < &indom_tab[nindom]; indomp++) {
		if (thres != -1 && 100*(float)sum_bytes/bytes[TYPE_INDOM] > thres) {
		    /* -x cutoff reached */
		    printf("    ...\n");
		    break;
		}
		printf("    %s: %ld bytes [%.0f%%, %d record",
		    pmInDomStr(indomp->indom), indomp->bytes,
		    100*(float)indomp->bytes/sbuf.st_size,
		    indomp->nrec);
		if (indomp->nrec > 1)
		    putchar('s');
		printf(", %d instance", indomp->nuniq_inst + indomp->ndup_inst);
		if (indomp->nuniq_inst + indomp->ndup_inst > 1)
		    putchar('s');
		if (rflag && indomp->ndup_inst > 0) {
		    printf(" (%ld bytes for", indomp->dup_bytes);
		    printf(" %d dup", indomp->ndup_inst);
		    if (indomp->ndup_inst > 1)
			putchar('s');
		    putchar(')');
		}
		putchar(']');
		putchar('\n');
		sum_bytes += indomp->bytes;
	    }
	}
    }

    if (nrec[TYPE_LABEL] > 0) {
	printf("  labels: %ld bytes [%.0f%%, %d records]\n",
	    bytes[TYPE_LABEL], 100*(float)bytes[TYPE_LABEL]/sbuf.st_size, nrec[TYPE_LABEL]);
    }

    if (nrec[TYPE_TEXT] > 0) {
	printf("  texts: %ld bytes [%.0f%%, %d records]\n",
	    bytes[TYPE_TEXT], 100*(float)bytes[TYPE_TEXT]/sbuf.st_size, nrec[TYPE_TEXT]);
    }

    printf("  overhead: %ld bytes [%.0f%%]\n",
	oheadbytes, 100*(float)oheadbytes/sbuf.st_size);
    sbuf.st_size -= (bytes[TYPE_DESC] + bytes[TYPE_INDOM] + bytes[TYPE_LABEL] + bytes[TYPE_TEXT] + oheadbytes);

    if (sbuf.st_size != 0)
	printf("  unaccounted for: %ld bytes\n", (long)sbuf.st_size);

    free(buf);
    if (indom_tab != NULL) {
	for (indomp = indom_tab; indomp < &indom_tab[nindom]; indomp++) {
	    for (k = 0; k < indomp->nuniq_inst; k++) {
		free(indomp->inst_tab[k].name);
	    }
	    free(indomp->inst_tab);
	}
	free(indom_tab);
    }

    return;
}

