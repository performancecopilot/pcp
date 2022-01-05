/*
 * Produce metadata stats summary for delta-indom proposal and V3 archives
 *
 * Copyright (c) 2021 Ken McDonell.  All Rights Reserved.
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
 * Debug flags:
 * -Dappl0	report added/dropped instances
 */
#include "./pmapi.h"
#include "./libpcp.h"
#include "./internal.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef struct {
    int		inst;
    char	*name;
} sortrec_t;

/* one per indom */
typedef struct {
    pmInDom	indom;
    int		first_numinst;
    int		added;
    int		dropped;
    int		numinst;
    int		all_numinst;
    size_t	v2_size;
    size_t	v3_size;
    int		*tbuf;
    sortrec_t	*ctl;
} stats_t;

/* indom table */
static stats_t	*stats = NULL;
static int	numstats = 0;

/* strings for external instance names */
static size_t	v2_str;
static size_t	v3_str;
static int	v2_count;
static int	v3_count;

/* lazy loading */
static size_t	v2_maps;
static size_t	v3_maps;

static int
compar(const void *a, const void *b)
{
    return ((sortrec_t *)a)->inst - ((sortrec_t *)b)->inst;
}

static int
is_sorted(int numinst, sortrec_t *ctl)
{
    int		i;

    for (i = 1; i < numinst; i++) {
	if (ctl[i].inst < ctl[i-1].inst) {
	    fprintf(stderr, "[%d] %d < [%d] %d\n", i, ctl[i].inst, i-1, ctl[i-1].inst);
	    return 0;
	}
    }
    return 1;
}

static void
report(void)
{
    stats_t	*sp;
    int		unchanged = 0;

    for (sp = stats; sp < &stats[numstats]; sp++) {
	if (sp->added + sp->dropped == 0) {
	    unchanged++;
	    continue;
	}
	printf("InDom: %s", pmInDomStr(sp->indom));
	printf(" logged instances:");
	printf(" (V2) %d (V3) %d [%d + %d - %d = %d",
	    sp->all_numinst,
	    sp->first_numinst + sp->added + sp->dropped,
	    sp->first_numinst, sp->added, sp->dropped,
	    sp->numinst);
	if (sp->first_numinst + sp->added - sp->dropped != sp->numinst)
	    printf(" botch: != %d",
		sp->first_numinst + sp->added - sp->dropped);
	printf("]\n");
    }
    if (unchanged) {
	if (unchanged != numstats)
	    printf("Plus ");
	printf("%d static InDoms (no change)\n", unchanged);
    }

    if (unchanged == numstats)
	exit(0);

}

static stats_t
*find(pmInDom indom)
{
    int		i;
    stats_t	*tmp;

    for (i = 0; i < numstats; i++) {
	if (stats[i].indom == indom)
	    return &stats[i];
    }

    /* first time for this indom alloc and initialize */
    numstats++;

    tmp = (stats_t *)realloc(stats, numstats * sizeof(stats_t));
    if (tmp == NULL) {
    }
    stats = tmp;

    memset(&stats[numstats-1], 0, sizeof(stats_t));
    stats[numstats-1].indom = indom;

    return &stats[numstats-1];
}

static char
*pr_size(size_t n)
{
    static char	buf[10];

    if (n < 10*1024)
	sprintf(buf, "%7zd  ", n);
    else if (n < 10*1024*1024)
	sprintf(buf, "%6.2fKb", n / (double)1024);
    else if (n < 10*1024*1024*1024L)
	sprintf(buf, "%6.2fMb", n / (double)(1024*1024));
    else
	sprintf(buf, "%6.2fGb", n / (double)(1024*1024*1024L));

    return buf;
}

/*
 * snarfed from e_loglabel.c
 */
typedef struct {
    __uint32_t	magic;		/* PM_LOG_MAGIC|PM_LOG_VERS02 */
    __int32_t	pid;		/* PID of logger */
    __int32_t	start_sec;	/* start of this log (pmTimeval) */
    __int32_t	start_usec;
    __int32_t	vol;		/* current log volume no. */
    char	hostname[PM_LOG_MAXHOSTLEN]; /* name of collection host */
    char	timezone[PM_TZ_MAXLEN];	/* $TZ at collection host */
} __pmLabel_v2;

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		n;
    int		rlen;
    int		check;
    int		i;
    int		j;
    size_t	v2_size;
    size_t	v3_size;
    __pmContext	*ctxp;
    __pmFILE	*f;
    __pmLogHdr	h;
    int		count[5] = { 0,0,0,0,0 };
    char	*types[5] = { "?", "DESC", "INDOM", "LABEL", "TEXT" };
    size_t	bytes[5] = { 0,0,0,0,0 };
    stats_t	*sp;
    struct stat	sbuf;

    while ((c = getopt(argc, argv, "D:")) != EOF) {
	switch (c) {
	    case 'D':
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "Bad debug options (%s)\n", optarg);
		exit(1);
	    }
	}
    }

    if (optind != argc-1) {
	fprintf(stderr, "Usage: delta-indom-stats [-D flags] archive\n");
	return(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, argv[optind])) < 0) {
	fprintf(stderr, "pmNewContext(%s): %s\n", argv[optind], pmErrStr(sts));
	exit(1);
    }


    if ((ctxp = __pmHandleToPtr(sts)) == NULL) {
	fprintf(stderr, " __pmHandleToPtr(%d) returns NULL!\n", sts);
	exit(1);
    }

    /* single threaded, release context lock */
    PM_UNLOCK(ctxp->c_lock);

    printf("%s:\n", ctxp->c_archctl->ac_log->name);

    f = ctxp->c_archctl->ac_log->mdfp;

    /*
     * snarfed from __pmLogLoadMeta() in logmeta.c
     */
    __pmFseek(f, (long)(sizeof(__pmLabel_v2) + 2*sizeof(int)), SEEK_SET);
    for ( ; ; ) {
	n = (int)__pmFread(&h, 1, sizeof(__pmLogHdr), f);

	/* swab hdr */
	h.len = ntohl(h.len);
	h.type = ntohl(h.type);

	if (n != sizeof(__pmLogHdr) || h.len <= 0) {
            if (__pmFeof(f)) {
		__pmClearerr(f);
                sts = 0;
		break;
            }
	    fprintf(stderr, "header read -> %d: expected: %d or h.len (%d) <= 0\n",
		    n, (int)sizeof(__pmLogHdr), h.len);
	    if (__pmFerror(f)) {
		__pmClearerr(f);
		sts = -oserror();
	    }
	    else
		sts = PM_ERR_LOGREC;
	    break;
	}
	rlen = h.len - (int)sizeof(__pmLogHdr) - (int)sizeof(int);
	count[h.type]++;
	bytes[h.type] += h.len;
	if (h.type == TYPE_INDOM_V2) {
	    pmTimeval		*tv;
	    //pmTimespec		when;
	    pmInResult		in;
	    char		*namebase;
	    int			*tbuf, *stridx;
	    sortrec_t		*ctl;
	    int			k, allinbuf = 0;

	    if ((tbuf = (int *)malloc(rlen)) == NULL) {
		fprintf(stderr, "tbuf: malloc failed: %d\n", rlen);
		exit(1);
	    }
	    if ((n = (int)__pmFread(tbuf, 1, rlen, f)) != rlen) {
		fprintf(stderr, "__pmFread: indom read -> %d: expected: %d\n",
			n, rlen);
		if (__pmFerror(f)) {
		    __pmClearerr(f);
		    sts = -oserror();
		}
		else
		    sts = PM_ERR_LOGREC;
		free(tbuf);
		goto end;
	    }

	    k = 0;
	    tv = (pmTimeval *)&tbuf[k];
	    //when.tv_sec = ntohl(tv->tv_sec);
	    //when.tv_nsec = ntohl(tv->tv_usec) * 1000;
	    k += sizeof(*tv)/sizeof(int);
	    in.indom = __ntohpmInDom((unsigned int)tbuf[k++]);
	    in.numinst = ntohl(tbuf[k++]);
	    sp = find((pmInDom)in.indom);
	    if (sp->ctl == NULL) {
		/* first time */
		sp->all_numinst = sp->first_numinst = in.numinst;
	    }
	    else {
		sp->all_numinst += in.numinst;
	    }
	    sp->v2_size += h.len;
	    sp->v3_size += sizeof(__pmLogHdr)	/* log record header */
	    		   + sizeof(in.indom)
	    		   + sizeof(in.numinst)
	                   + sizeof(int);	/* log record trailer */
	    if (in.numinst > 0) {
		in.instlist = &tbuf[k];
		k += in.numinst;
		stridx = &tbuf[k];
#if defined(HAVE_32BIT_PTR)
		in.namelist = (char **)stridx;
		allinbuf = 1; /* allocation is all in tbuf */
#else
		allinbuf = 0; /* allocation for namelist + tbuf */
		/* need to allocate to hold the pointers */
		in.namelist = (char **)malloc(in.numinst * sizeof(char*));
		if (in.namelist == NULL) {
		    fprintf(stderr, "in.namelist: malloc failed: %zd\n", in.numinst * sizeof(char*));
		    exit(1);
		}
#endif
		k += in.numinst;
		namebase = (char *)&tbuf[k];
	        for (i = 0; i < in.numinst; i++) {
		    in.instlist[i] = ntohl(in.instlist[i]);
	            in.namelist[i] = &namebase[ntohl(stridx[i])];
		}

		/* build a control structure for sorting */
		ctl = (sortrec_t *)malloc(in.numinst * sizeof(sortrec_t));
		for (i = 0; i < in.numinst; i++) {
		    ctl[i].inst = in.instlist[i];
		    ctl[i].name = in.namelist[i];
		}

		if (in.numinst > 1) {
		    qsort((void *)ctl, in.numinst, sizeof(sortrec_t), compar);
		}
		/* just being sure that our qsort use produces a sorted indom */
		if (!is_sorted(in.numinst, ctl)) {
		    printf("not sorted\n");
		    break;
		}
		if (in.namelist != NULL && !allinbuf)
		    free(in.namelist);


		if (sp->ctl != NULL) {
		    /*
		     * 2nd or later record for the indom ...
		     * find what's added, what's dropped and what's the same
		     */
		    for (i = 0, j = 0; i < sp->numinst || j < in.numinst; ) {
			if ((i < sp->numinst && j == in.numinst) ||
			    (i < sp->numinst && j < in.numinst && sp->ctl[i].inst < ctl[j].inst)) {
			    if (pmDebugOptions.appl0)
				printf("[%d] dropped %d -> %-29.29s\n", count[h.type]-1, sp->ctl[i].inst, sp->ctl[i].name);
			    sp->dropped++;
			    sp->v3_size += 2*sizeof(int);
			    v3_maps += sizeof(int) + sizeof(char *);
			    v3_count++;
			    i++;
			}
			else if ((i == sp->numinst && j < in.numinst) ||
			         (i < sp->numinst && j < in.numinst && sp->ctl[i].inst > ctl[j].inst)) {
			    if (pmDebugOptions.appl0)
				printf("[%d] added %d -> %-29.29s\n", count[h.type]-1, ctl[j].inst, ctl[j].name);
			    sp->added++;
			    sp->v3_size += 2*sizeof(int) + strlen(ctl[j].name) + 1;
			    v3_str += strlen(ctl[j].name) + 1;
			    v3_maps += sizeof(int) + sizeof(char *);
			    v3_count++;
			    j++;
			}
			else if (sp->ctl[i].inst == ctl[j].inst) {
			    // printf("same %d -> %-29.29s ... %-29.29s\n", sp->ctl[i].inst, sp->ctl[i].name, ctl[j].name);
			    i++;
			    j++;
			}
			else {
			    printf("botch: i=%d sp->numist=%d j=%d in.numinst=%d\n", i, sp->numinst, j, in.numinst);
			    exit(1);
			}
		    }
		    free(sp->tbuf);
		    free(sp->ctl);
		}
		else {
		    /* first time, all instance names needed */
		    for (i = 0; i < in.numinst; i++)
			v3_str += strlen(ctl[i].name) + 1;
		    v3_maps += in.numinst * (sizeof(int) + sizeof(char *));
		    v3_count += in.numinst;
		}

		sp->numinst = in.numinst;
		sp->tbuf = tbuf;
		sp->ctl = ctl;
	    }
	    else {
		/* no instances, or an error */
		free(tbuf);
	    }
	}
	else {
	    /* skip this record, not TYPE_INDOM_V2 */
	    __pmFseek(f, (long)rlen, SEEK_CUR);
	}
	n = (int)__pmFread(&check, 1, sizeof(check), f);
	check = ntohl(check);
	if (n != sizeof(check) || h.len != check) {
	    if (__pmFerror(f)) {
		__pmClearerr(f);
		sts = -oserror();
	    }
	    else
		sts = PM_ERR_LOGREC;
	    break;
	}
    }

end:
    report();

    putchar('\n');
    printf("On disk analysis ...\n");
    printf("%8s %8s %9s %9s %9s\n", "Type", "Count", "V2 Size", "V3 Size", "Saving");
    printf("%8s %8s %9s %9s %9s\n", "", "", "    (uncompressed)", "", "");
    /* add in label record */
    v2_size = v3_size = sizeof(__pmLabel_v2) + 2*sizeof(int);
    for (sp = stats; sp < &stats[numstats]; sp++) {
	v2_size += sp->v2_size;
	v3_size += sp->v3_size;
    }
    bytes[0] = count[0] = 0;
    for (i = 1; i < sizeof(types)/sizeof(types[0]); i++) {
	printf("%8s %8d %9s", types[i], count[i], pr_size(bytes[i]));
	count[0] += count[i];
	if (i == TYPE_INDOM_V2) {
	    printf(" %9s", pr_size(v3_size));
	    printf(" %9s (%.1f%%)\n", pr_size(v2_size - v3_size),
		100.0*((long)v2_size - (long)v3_size) / v2_size);
	}
	else {
	    printf(" %9s\n", pr_size(bytes[i]));
	    bytes[0] += bytes[i];
	}
    }
    v2_size += bytes[0];
    v3_size += bytes[0];
    printf("%8s %8d %9s", "All meta", count[0], pr_size(v2_size));
    printf(" %9s", pr_size(v3_size));
    printf(" %9s (%.1f%%)\n", pr_size(v2_size - v3_size),
	100.0*((long)v2_size - (long)v3_size) / v2_size);

    /* now the temporal index (optional) */
    if (ctxp->c_archctl->ac_log->tifp) {
	if ((sts = __pmFstat(ctxp->c_archctl->ac_log->tifp, &sbuf)) < 0) {
	    fprintf(stderr, "__pmFstat: %s\n", pmErrStr(sts));
	    exit(1);
	}
	printf("%8s %8zd %9s", "Index",
		(sbuf.st_size-sizeof(__pmLabel_v2))/sizeof(__pmLogTI),
		pr_size(sbuf.st_size));
	printf(" %9s\n", pr_size(sbuf.st_size));
	v2_size += sbuf.st_size;
	v3_size += sbuf.st_size;
    }

    /* and finally each data volume */
    for (i = ctxp->c_archctl->ac_log->minvol; i <= ctxp->c_archctl->ac_log->maxvol; i++) {
	char	fname[MAXPATHLEN+1];
	pmsprintf(fname, sizeof(fname), "%s.%d", ctxp->c_archctl->ac_log->name, i);
	if ((f = __pmFopen(fname, "r")) == NULL) {
	    fprintf(stderr, "__pmFopen(%s): %s\n", fname, pmErrStr(-oserror()));
	    continue;
	}
	if ((sts = __pmFstat(f, &sbuf)) < 0) {
	    fprintf(stderr, "__pmFstat(%s): %s\n", fname, pmErrStr(sts));
	    exit(1);
	}
	printf("%5s.%2d %8s %9s", "Data", i, "", pr_size(sbuf.st_size));
	printf(" %9s\n", pr_size(sbuf.st_size));
	v2_size += sbuf.st_size;
	v3_size += sbuf.st_size;
    }

    printf("%8s %8s %9s", "Total", "", pr_size(v2_size));
    printf(" %9s", pr_size(v3_size));
    printf(" %9s (%.1f%%) %s\n", pr_size(v2_size - v3_size),
	100.0*((long)v2_size - (long)v3_size) / v2_size,
	ctxp->c_archctl->ac_log->name);

    putchar('\n');
    v2_size = v3_size = 0;
    printf("Memory footprint analysis ...\n");
    printf("%12s %9s %9s %9s %9s %9s\n", "Type", "V2 Count", "V2 Size", "V3 Count", "V3 Size", "Saving");

    /*
     * need to get v2_maps and v2_str from loaded instance domains
     * ... "dup" indom handling in __pmLogLoadMeta() (see
     * __pmLogAddInDom() and PMLOGPUTINDOM_DUP) means there may be
     * fewer loaded than appear in the .meta file
     */
    for (i = 0; i < ctxp->c_archctl->ac_log->hashindom.hsize; i++) {
	__pmHashNode	*hp;
	__pmLogInDom	*idp;
	for (hp = ctxp->c_archctl->ac_log->hashindom.hash[i]; hp != NULL; hp = hp->next) {
	    for (idp = (__pmLogInDom *)hp->data; idp != NULL; idp = idp->next) {
		v2_maps += idp->numinst * (sizeof(int) + sizeof(char *));
		v2_count += idp->numinst;
		for (j = 0; j < idp->numinst; j++) {
		    v2_str += strlen(idp->namelist[j])+1;
		}
	    }
	}
    }

    printf("%12s %9d %9s", "Inst names", v2_count, pr_size(v2_str));
    v2_size += v2_str;
    printf(" %9d %9s", v3_count, pr_size(v3_str));
    v3_size += v3_str;
    printf(" %9s (%.1f%%)\n", pr_size(v2_str - v3_str),
	100.0*(long)(v2_str - v3_str)/(long)(v2_str));

    printf("%12s %9s %9s", "Lazy load", "", pr_size(v2_maps));
    v2_size += v2_maps;
    printf(" %9s %9s", "", pr_size(v3_maps));
    v3_size += v3_maps;
    printf(" %9s (%.1f%%)\n", pr_size(v2_maps - v3_maps),
	100.0*(long)(v2_maps - v3_maps)/(long)(v2_maps));

    printf("%12s %9s %9s", "Total", "", pr_size(v2_size));
    printf(" %9s %9s", "", pr_size(v3_size));
    printf(" %9s (%.1f%%) %s\n", pr_size(v2_size - v3_size),
	100.0*(long)(v2_size - v3_size)/(long)(v2_size),
	ctxp->c_archctl->ac_log->name);


    return(sts);
}
