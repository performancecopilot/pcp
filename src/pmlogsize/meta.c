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
    long	bytes;			/* total bytes per indom */
    int		ninst;			/* number of unique instances */
    inst_t	*inst_tab;		/* unique instances */
    int		ndup_inst;		/* inst and name the same */
    long	dup_bytes;		/* bytes for inst and name the same */
} indom_t;

void
do_meta(__pmFILE *f)
{
    long	oheadbytes = __pmFtell(f);
    long	bytes[5] = { 0, 0, 0, 0, 0 };
    int		nrec[5] = { 0, 0, 0, 0, 0 };
    __pmLogHdr	header;
    int		trailer;
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
		    __int32_t	*ip;
		    int		numnames;

		    dp->pmid = __ntohpmID(dp->pmid);
		    printf("PMID: %s", pmIDStr(dp->pmid));
		    dp++;
		    ip = (__int32_t *)dp;
		    numnames = ntohl(*ip);
		    ip++;
		    bufp = (char *)ip;
		    for (i = 0; i < numnames; i++) {
			__int32_t	len;
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
		    __int32_t	*stridx;
		    char	*str;

		    bufp = buf;
		    bufp += sizeof(pmTimeval);
		    indom = __ntohpmInDom(*((__int32_t *)bufp));
		    bufp += sizeof(pmInDom);
		    if (vflag)
			printf("INDOM: %s", pmInDomStr(indom));
		    for (i = 0; i < nindom; i++) {
			if (indom_tab[i].indom == indom)
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
			indom_tab[i].indom = indom;
			indom_tab[i].bytes = 0;
			indom_tab[i].ninst = 0;
			indom_tab[i].inst_tab = NULL;
			indom_tab[i].ndup_inst = 0;
			indom_tab[i].dup_bytes = 0;
		    }
		    indom_tab[i].bytes += (int)sizeof(header.type) + need;
		    ninst = ntohl(*((__int32_t *)bufp));
		    bufp += sizeof(__int32_t);
		    if (vflag) {
			printf(" %d instance", ninst);
			if (ninst > 1)
			    putchar('s');
		    }
		    stridx = (__int32_t *)&bufp[ninst*sizeof(__int32_t)];
		    str = (char *)&bufp[2*ninst*sizeof(__int32_t)];
		    for (j = 0; j < ninst; j++) {
			inst = ntohl(*((__int32_t *)bufp));
			bufp += sizeof(__int32_t);
			stridx[j] = ntohl(stridx[j]);
			if (vflag) {
			    if (j == 0)
				printf(" %d \"%s\"", inst, &str[stridx[j]]);
			    else if (j == ninst-1)
				printf(" ... %d \"%s\"", inst, &str[stridx[j]]);
			}
			for (k = 0; k < indom_tab[i].ninst; k++) {
			    if (indom_tab[i].inst_tab[k].inst != inst)
				continue;
			    if (strcmp(indom_tab[i].inst_tab[k].name, &str[stridx[j]]) == 0)
				break;
			}
			if (k == indom_tab[i].ninst) {
			    /* first time for this instance in this indom */
			    inst_t	*inst_tab_tmp;
			    indom_tab[i].ninst++;
			    inst_tab_tmp = (inst_t *)realloc(indom_tab[i].inst_tab, indom_tab[i].ninst*sizeof(inst_t));
			    if (inst_tab_tmp == NULL) {
				fprintf(stderr, "Error: metadata inst_tab realloc(%d) failed\n", (int)(indom_tab[i].ninst*sizeof(inst_t)));
				exit(1);
			    }
			    indom_tab[i].inst_tab = inst_tab_tmp;
			    indom_tab[i].inst_tab[k].inst = inst;
			    if ((indom_tab[i].inst_tab[k].name = strdup(&str[stridx[j]])) == NULL) {
				fprintf(stderr, "Error: metadata inst name strdup(\"%s\") failed\n", &str[stridx[j]]);
				exit(1);
			    }

			}
			else {
			    /* duplicate instance in this indom */
			    indom_tab[i].ndup_inst++;
			    indom_tab[i].dup_bytes += 2*sizeof(__int32_t) + strlen(indom_tab[i].inst_tab[k].name) + 1;
			}
			indom_tab[i].bytes += 2*sizeof(__int32_t) + strlen(indom_tab[i].inst_tab[k].name) + 1;
		    }
		    if (vflag)
			putchar('\n');
		}
		break;
	    case TYPE_LABEL:
		break;
	    case TYPE_TEXT:
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
	printf("  indoms: %ld bytes [%.0f%%, %d records]\n",
	    bytes[TYPE_INDOM], 100*(float)bytes[TYPE_INDOM]/sbuf.st_size, nrec[TYPE_INDOM]);
	if (dflag) {
	    for (i = 0; i < nindom; i++) {
		printf("    %s: %ld bytes %d instance",
		    pmInDomStr(indom_tab[i].indom), indom_tab[i].bytes,
		    indom_tab[i].ninst + indom_tab[i].ndup_inst);
		if (indom_tab[i].ninst + indom_tab[i].ndup_inst > 1)
		    putchar('s');
		if (indom_tab[i].ndup_inst > 0) {
		    printf(" (%d duplicate instance", indom_tab[i].ndup_inst);
		    if (indom_tab[i].ndup_inst > 1)
			putchar('s');
		    printf(" %ld bytes)", indom_tab[i].dup_bytes);
		}
		putchar('\n');
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

    return;
}

