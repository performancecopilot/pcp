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
    int		id;		/* internal instance identifier */
    char	*name;		/* external instance identifier */
} inst_t;


typedef struct {
    pmInDom	indom;
    long	bytes;			/* total bytes per indom */
    long	replicated_bytes;	/* inst and iname the same */
    int		ninst;			/* number of unique instances */
    inst_t	*inst_tab;		/* unique instances */
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

		    bufp = buf;
		    bufp += sizeof(pmTimeval);
		    indom = __ntohpmInDom(*((__int32_t *)bufp));
		    if (vflag)
			printf("INDOM: %s", pmInDomStr(indom));
		    for (i = 0; i < nindom; i++) {
			if (indom_tab[i].indom == indom)
			    break;
		    }
		    if (i == nindom) {
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
			indom_tab[i].replicated_bytes = 0;
			indom_tab[i].ninst = 0;
			indom_tab[i].inst_tab = NULL;
		    }
		    indom_tab[i].bytes += (int)sizeof(header.type) + need;

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

    printf("  metrics: %ld bytes [%.0f%%, %d records]\n",
	bytes[TYPE_DESC], 100*(float)bytes[TYPE_DESC]/sbuf.st_size, nrec[TYPE_DESC]);
    printf("  indoms: %ld bytes [%.0f%%, %d records]\n",
	bytes[TYPE_INDOM], 100*(float)bytes[TYPE_INDOM]/sbuf.st_size, nrec[TYPE_INDOM]);
    if (dflag) {
	for (i = 0; i < nindom; i++) {
	    printf("    %s: %ld bytes\n", pmInDomStr(indom_tab[i].indom), indom_tab[i].bytes);
	}
    }
    printf("  labels: %ld bytes [%.0f%%, %d records]\n",
	bytes[TYPE_LABEL], 100*(float)bytes[TYPE_LABEL]/sbuf.st_size, nrec[TYPE_LABEL]);
    printf("  texts: %ld bytes [%.0f%%, %d records]\n",
	bytes[TYPE_TEXT], 100*(float)bytes[TYPE_TEXT]/sbuf.st_size, nrec[TYPE_TEXT]);
    printf("  overhead: %ld bytes [%.0f%%]\n",
	oheadbytes, 100*(float)oheadbytes/sbuf.st_size);
    sbuf.st_size -= (bytes[TYPE_DESC] + bytes[TYPE_INDOM] + bytes[TYPE_LABEL] + bytes[TYPE_TEXT] + oheadbytes);
    if (sbuf.st_size != 0)
	printf("  unaccounted for: %ld bytes\n", (long)sbuf.st_size);

    return;
}

