/*
 * pmlogextract - extract desired metrics from PCP archive logs
 *
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
 */

#include <math.h>
#include <ctype.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"
#include "logger.h"
#include <assert.h>

global_t	global;
indomspec_t	*indom_root = NULL;
metricspec_t	*metric_root = NULL;

#ifdef PCP_DEBUG
long totalmalloc = 0;
#endif

/*
 *  Usage
 */
static void
usage(void)
{
    fprintf(stderr,
"Usage: %s [options] input-archive output-archive\n\
\n\
Options:\n\
  -c configfile  file to load config from\n\
  -C             parse config file(s) and quit\n\
  -v             verbose\n\
  -w             emit warnings [default is silence]\n",
	pmProgname);
}

/*
 *  Global variables
 */
static int	first_datarec = 1;		/* first record flag */

off_t		new_log_offset;			/* new log offset */
off_t		new_meta_offset;		/* new meta offset */
off_t		old_log_offset;			/* old log offset */
off_t		old_meta_offset;		/* old meta offset */


/* archive control stuff */
char			*outarchname = NULL;	/* name of output archive */
static __pmLogCtl	logctl;		/* output archive control */
inarch_t		inarch;		/* input archive control */

/* command line args */
char	*configfile = NULL;		/* -c name of config file */
int	Cflag = 0;			/* -C parse config and quit */
int	wflag = 0;			/* -w emit warnings */
int	vflag = 0;			/* -v verbosity */

/*
 *  report that archive is corrupted
 */
static void
_report(FILE *fp)
{
    off_t	here;
    struct stat	sbuf;

    here = lseek(fileno(fp), 0L, SEEK_CUR);
    fprintf(stderr, "%s: Error occurred at byte offset %ld into a file of",
	    pmProgname, (long)here);
    if (fstat(fileno(fp), &sbuf) < 0)
	fprintf(stderr, ": stat: %s\n", osstrerror());
    else
	fprintf(stderr, " %ld bytes.\n", (long)sbuf.st_size);
    fprintf(stderr, "The last record, and the remainder of this file will not be extracted.\n");
    exit(1);
}

/*
 *  switch output volumes
 */
static void
newvolume(int vol)
{
    FILE		*newfp;

    if ((newfp = __pmLogNewFile(outarchname, vol)) != NULL) {
	fclose(logctl.l_mfp);
	logctl.l_mfp = newfp;
	logctl.l_label.ill_vol = logctl.l_curvol = vol;
	__pmLogWriteLabel(logctl.l_mfp, &logctl.l_label);
	fflush(logctl.l_mfp);
    }
    else {
	fprintf(stderr, "%s: __pmLogNewFile(%s,%d) Error: %s\n",
		pmProgname, outarchname, vol, pmErrStr(-oserror()));
	exit(1);
    }
}

/* construct new archive label */
static void
newlabel(void)
{
    __pmLogLabel	*lp = &logctl.l_label;

    /* copy magic number, pid, host and timezone */
    lp->ill_magic = inarch.label.ll_magic;
    lp->ill_pid = inarch.label.ll_pid;
    if (global.flags & GLOBAL_CHANGE_HOSTNAME)
	strncpy(lp->ill_hostname, global.hostname, PM_LOG_MAXHOSTLEN);
    else
	strncpy(lp->ill_hostname, inarch.label.ll_hostname, PM_LOG_MAXHOSTLEN);
    if (global.flags & GLOBAL_CHANGE_TZ)
	strncpy(lp->ill_tz, global.tz, PM_TZ_MAXLEN);
    else
	strncpy(lp->ill_tz, inarch.label.ll_tz, PM_TZ_MAXLEN);
}

/*
 * write label records at the start of each physical file
 */
void
writelabel(int do_rewind)
{
    off_t	old_offset;

    if (do_rewind) {
	old_offset = ftell(logctl.l_tifp);
	rewind(logctl.l_tifp);
    }
    logctl.l_label.ill_vol = PM_LOG_VOL_TI;
    __pmLogWriteLabel(logctl.l_tifp, &logctl.l_label);
    if (do_rewind)
	fseek(logctl.l_tifp, (long)old_offset, SEEK_SET);

    if (do_rewind) {
	old_offset = ftell(logctl.l_mdfp);
	rewind(logctl.l_mdfp);
    }
    logctl.l_label.ill_vol = PM_LOG_VOL_META;
    __pmLogWriteLabel(logctl.l_mdfp, &logctl.l_label);
    if (do_rewind)
	fseek(logctl.l_mdfp, (long)old_offset, SEEK_SET);

    if (do_rewind) {
	old_offset = ftell(logctl.l_mfp);
	rewind(logctl.l_mfp);
    }
    logctl.l_label.ill_vol = 0;
    __pmLogWriteLabel(logctl.l_mfp, &logctl.l_label);
    if (do_rewind)
	fseek(logctl.l_mfp, (long)old_offset, SEEK_SET);
}

/*
 *  create a mark record
 */
__pmPDU *
_createmark(void)
{
    mark_t	*markp;

    markp = (mark_t *)malloc(sizeof(mark_t));
    if (markp == NULL) {
	fprintf(stderr, "%s: Error: mark_t malloc: %s\n",
		pmProgname, osstrerror());
	exit(1);
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
        totalmalloc += sizeof(mark_t);
        printf ("_createmark : allocated %d\n", (int)sizeof(mark_t));
    }
#endif

    markp->len = (int)sizeof(mark_t);
    markp->type = markp->from = 0;
    markp->timestamp.tv_sec = 0;		// TODO, was current
    markp->timestamp.tv_usec = 0;		// TODO, was current
    markp->timestamp.tv_usec += 1000;	/* + 1msec */
    if (markp->timestamp.tv_usec > 1000000) {
	markp->timestamp.tv_usec -= 1000000;
	markp->timestamp.tv_sec++;
    }
    markp->numpmid = 0;
    return((__pmPDU *)markp);
}

/*
 * read next metadata record 
 */
static int
nextmeta()
{
    int			sts;
    __pmLogCtl		*lcp;

    lcp = inarch.ctxp->c_archctl->ac_log;
    if ((sts = _pmLogGet(lcp, PM_LOG_VOL_META, &inarch.metarec)) < 0) {
	if (sts != PM_ERR_EOL) {
	    fprintf(stderr, "%s: Error: _pmLogGet[meta %s]: %s\n",
		    pmProgname, inarch.name, pmErrStr(sts));
	    _report(lcp->l_mdfp);
	}
	return -1;
    }

    return ntohl(inarch.metarec[1]);
}


/*
 * read next log record
 *
 * return status is
 * 0		ok
 * 1		ok, but volume switched
 * PM_ERR_EOL	end of file
 * -1		fatal error
 */
static int
nextlog(void)
{
    int			sts;
    __pmLogCtl		*lcp;
    int			old_vol;


    lcp = inarch.ctxp->c_archctl->ac_log;
    old_vol = inarch.ctxp->c_archctl->ac_log->l_curvol;

    if ((sts = __pmLogRead(lcp, PM_MODE_FORW, NULL, &inarch.rp)) < 0) {
	if (sts != PM_ERR_EOL) {
	    fprintf(stderr, "%s: Error: __pmLogRead[log %s]: %s\n",
		    pmProgname, inarch.name, pmErrStr(sts));
	    _report(lcp->l_mfp);
	}
	return -1;
    }

    return old_vol == inarch.ctxp->c_archctl->ac_log->l_curvol ? 0 : 1;
}

/*
 * parse command line arguments
 */
int
parseargs(int argc, char *argv[])
{
    int			c;
    int			sts;
    int			errflag = 0;
    struct stat		sbuf;

    while ((c = getopt(argc, argv, "c:CD:vw?")) != EOF) {
	switch (c) {

	case 'c':	/* config file */
	    configfile = optarg;
	    if (stat(configfile, &sbuf) < 0) {
		fprintf(stderr, "%s: %s - invalid file\n",
			pmProgname, configfile);
		errflag++;
	    }
	    break;


	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'C':	/* parse configs and quit */
	    Cflag = 1;
	    vflag = 1;
	    break;

	case 'w':	/* print warnings */
	    wflag = 1;
	    break;

	case 'v':	/* verbosity */
	    vflag++;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag == 0 && optind != argc-2)
	errflag++;

    return -errflag;
}

int
parseconfig(void)
{
    int		errflag = 0;
    extern FILE * yyin;

    if ((yyin = fopen(configfile, "r")) == NULL) {
	fprintf(stderr, "%s: Cannot open config file \"%s\": %s\n",
		pmProgname, configfile, osstrerror());
	exit(1);
    }

    if (yyparse() != 0)
	exit(1);

    fclose(yyin);
    yyin = NULL;

    return(-errflag);
}

#if 0
void
writerlist(rlist_t **rlready, double mintime)
{
    int		needti;		/* need to flush/update */
    double	titime;		/* time of last temporal index write */
    double	restime;	/* time of result */
    rlist_t	*elm;		/* element of rlready to be written out */
    __pmPDU	*pb;		/* pdu buffer */
    __pmTimeval	this;		/* timeval of this record */
    unsigned long	peek_offset;

    needti = 0;
    titime = 0.0;

    while (*rlready != NULL) {


	/*
	 * Even without a -v option, we may need to switch volumes
	 * if the data file exceeds 2^31-1 bytes
	 */
	peek_offset = ftell(logctl.l_mfp);
	peek_offset += ((__pmPDUHdr *)pb)->len - sizeof(__pmPDUHdr) + 2*sizeof(int);
	if (peek_offset > 0x7fffffff) {
	    newvolume(outarchname, (__pmTimeval *)&pb[3]);
	}

	/* make sure that we do not write out the temporal index more
	 * than once for the same timestamp
	 */
	if (needti && titime >= restime)
	    needti = 0;

	/* flush/update */
	if (needti) {
	    titime = restime;

	    fflush(logctl.l_mfp);
	    fflush(logctl.l_mdfp);

	    if (old_log_offset == 0)
		old_log_offset = sizeof(__pmLogLabel)+2*sizeof(int);

            new_log_offset = ftell(logctl.l_mfp);
            new_meta_offset = ftell(logctl.l_mdfp);

            fseek(logctl.l_mfp, (long)old_log_offset, SEEK_SET);
            fseek(logctl.l_mdfp, (long)old_meta_offset, SEEK_SET);

            __pmLogPutIndex(&logctl, &this);

            fseek(logctl.l_mfp, (long)new_log_offset, SEEK_SET);
            fseek(logctl.l_mdfp, (long)new_meta_offset, SEEK_SET);

            old_log_offset = ftell(logctl.l_mfp);
            old_meta_offset = ftell(logctl.l_mdfp);

        }

	/* LOG: free PDU buffer */
	__pmUnpinPDUBuf(pb);
	pb = NULL;

	elm->res = NULL;
	elm->next = NULL;
	free(elm);

    } /*while(*rlready)*/
}


/*
 *  mark record has been created and assigned to inarch.logrec
 *  write it out
 */
void
writemark(void)
{
    int		sts;
    mark_t      *p = (mark_t *)inarch.logrec;

    if (!inarch.mark) {
	fprintf(stderr, "%s: Fatal Error!\n", pmProgname);
	fprintf(stderr, "    writemark called, but mark not set\n");
	exit(1);
    }

    if (p == NULL) {
	fprintf(stderr, "%s: Fatal Error!\n", pmProgname);
	fprintf(stderr, "    writemark called, but no pdu\n");
	exit(1);
    }

    p->timestamp.tv_sec = htonl(p->timestamp.tv_sec);
    p->timestamp.tv_usec = htonl(p->timestamp.tv_usec);

    if ((sts = __pmLogPutResult(&logctl, inarch.logrec)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogPutResult: log data: %s\n",
		pmProgname, pmErrStr(sts));
	exit(1);
    }
    free(inarch.logrec);
    inarch.logrec = NULL;
}
#endif

char *
SemStr(int sem)
{
    static char	buf[20];

    if (sem == PM_SEM_COUNTER) snprintf(buf, sizeof(buf), "counter");
    else if (sem == PM_SEM_INSTANT) snprintf(buf, sizeof(buf), "instant");
    else if (sem == PM_SEM_DISCRETE) snprintf(buf, sizeof(buf), "discrete");
    else snprintf(buf, sizeof(buf), "bad sem? %d", sem);

    return buf;
}

static void
reportconfig(void)
{
    indomspec_t		*ip;
    metricspec_t	*mp;
    int			i;
    int			change = 0;

    printf("PCP Archive Log Rewrite Specifications Summary\n");
    change |= (global.flags != 0);
    if (global.flags & GLOBAL_CHANGE_HOSTNAME)
	printf("Hostname:\t%s -> %s\n", inarch.label.ll_hostname, global.hostname);
    if (global.flags & GLOBAL_CHANGE_TZ)
	printf("Timezone:\t%s -> %s\n", inarch.label.ll_tz, global.tz);
    if (global.flags & GLOBAL_CHANGE_TIME) {
	static struct tm	*tmp;
	char			*sign = "";
	time_t			time;
	if (global.time.tv_sec < 0) {
	    time = (time_t)(-global.time.tv_sec);
	    sign = "-";
	}
	else
	    time = (time_t)global.time.tv_sec;
	tmp = gmtime(&time);
	tmp->tm_hour += 24 * tmp->tm_yday;
	if (tmp->tm_hour < 10)
	    printf("Delta:\t\t-> %s%02d:%02d:%02d.%06d\n", sign, tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int)global.time.tv_usec);
	else
	    printf("Delta:\t\t-> %s%d:%02d:%02d.%06d\n", sign, tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int)global.time.tv_usec);
    }
    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	int		hdr_done = 0;
	if (ip->new_indom != PM_INDOM_NULL) {
	    printf("\nInstance Domain: %s\n", pmInDomStr(ip->old_indom));
	    hdr_done = 1;
	    printf("pmInDom:\t-> %s\n", pmInDomStr(ip->new_indom));
	    change |= 1;
	}
	for (i = 0; i < ip->numinst; i++) {
	    change |= (ip->flags[i] != 0);
	    if (ip->flags[i]) {
		if (hdr_done == 0) {
		    printf("\nInstance Domain: %s\n", pmInDomStr(ip->old_indom));
		    hdr_done = 1;
		}
		printf("Instance:\t\[%d] \"%s\" -> ", ip->old_inst[i], ip->old_iname[i]);
		if (ip->flags[i] & INST_DELETE)
		    printf("DELETE\n");
		else {
		    if (ip->flags[i] & INST_CHANGE_INST)
			printf("[%d] ", ip->new_inst[i]);
		    else
			printf("[%d] ", ip->old_inst[i]);
		    if (ip->flags[i] & INST_CHANGE_INAME)
			printf("\"%s\"\n", ip->new_iname[i]);
		    else
			printf("\"%s\"\n", ip->old_iname[i]);
		}
	    }
	}
    }
    for (mp = metric_root; mp != NULL; mp = mp->m_next) {
	if (mp->flags != 0) {
	    change |= 1;
	    printf("\nMetric: %s (%s)\n", mp->old_name, pmIDStr(mp->old_desc.pmid));
	}
	if (mp->flags & METRIC_CHANGE_PMID) {
	    printf("pmID:\t\t%s ->", pmIDStr(mp->old_desc.pmid));
	    printf(" %s\n", pmIDStr(mp->new_desc.pmid));
	}
	if (mp->flags & METRIC_CHANGE_NAME)
	    printf("Name:\t\t%s -> %s\n", mp->old_name, mp->new_name);
	if (mp->flags & METRIC_CHANGE_TYPE) {
	    printf("Type:\t\t%s ->", pmTypeStr(mp->old_desc.type));
	    printf(" %s\n", pmTypeStr(mp->new_desc.type));
	}
	if (mp->flags & METRIC_CHANGE_INDOM) {
	    printf("InDom:\t\t%s ->", pmInDomStr(mp->old_desc.indom));
	    printf(" %s\n", pmInDomStr(mp->new_desc.indom));
	}
	if (mp->flags & METRIC_CHANGE_SEM) {
	    printf("Semantics:\t%s ->", SemStr(mp->old_desc.sem));
	    printf(" %s\n", SemStr(mp->new_desc.sem));
	}
	if (mp->flags & METRIC_CHANGE_UNITS) {
	    printf("Units:\t\t%s ->", pmUnitsStr(&mp->old_desc.units));
	    printf(" %s\n", pmUnitsStr(&mp->new_desc.units));
	}
	if (mp->flags & METRIC_DELETE)
	    printf("DELETE\n");
    }
    if (change == 0)
	printf("No changes\n");
}

static int
fixstamp(struct timeval *tvp)
{
    if (global.flags & GLOBAL_CHANGE_TIME) {
	if (global.time.tv_sec > 0) {
	    tvp->tv_sec += global.time.tv_sec;
	    tvp->tv_usec += global.time.tv_usec;
	    if (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	    }
	    return 1;
	}
	else if (global.time.tv_sec < 0) {
	    /* parser makes tv_sec < 0 and tv_usec >= 0 */
	    tvp->tv_sec += global.time.tv_sec;
	    tvp->tv_usec -= global.time.tv_usec;
	    if (tvp->tv_usec < 0) {
		tvp->tv_sec--;
		tvp->tv_usec += 1000000;
	    }
	    return 1;
	}
    }
    return 0;
}

int
main(int argc, char **argv)
{
    int		sts;
    int		stslog;			/* sts from nextlog() */
    int		stsmeta = 0;		/* sts from nextmeta() */
    int		ti_idx;			/* next slot for input temporal index */
    int		needti = 0;

    __pmSetProgname(argv[0]);

    /* process cmd line args */
    if (parseargs(argc, argv) < 0) {
	usage();
	exit(1);
    }

    /* input archive */
    inarch.name = argv[argc-2];
    inarch.logrec = inarch.metarec = NULL;
    inarch.mark = 0;
    inarch.rp = NULL;

    if ((inarch.ctx = pmNewContext(PM_CONTEXT_ARCHIVE, inarch.name)) < 0) {
	fprintf(stderr, "%s: Error: cannot open archive \"%s\": %s\n",
		pmProgname, inarch.name, pmErrStr(inarch.ctx));
	exit(1);
    }
    inarch.ctxp = __pmHandleToPtr(inarch.ctx);
    assert(inarch.ctxp != NULL);

    if ((sts = pmGetArchiveLabel(&inarch.label)) < 0) {
	fprintf(stderr, "%s: Error: cannot get archive label record (%s): %s\n", pmProgname, inarch.name, pmErrStr(sts));
	exit(1);
    }

    if ((inarch.label.ll_magic & 0xff) != PM_LOG_VERS02) {
	fprintf(stderr,"%s: Error: illegal version number %d in archive (%s)\n",
		pmProgname, inarch.label.ll_magic & 0xff, inarch.name);
	exit(1);
    }

    /* output archive */
    outarchname = argv[argc-1];

    /*
     * process config file
     * TODO - more than one configfile via multiple -c options
     * TODO - configfile is dir => readdir and process all files
     */
    if (configfile != NULL) {
	if (parseconfig() < 0) {
	    usage();
	    exit(1);
	}
    }

    /*
     * TODO - semantic checks
     * metric indom modified _and_ indom in archive => error
     * metric indom modified _and_ domain(indom) != domain(pmid) => warning
     * re-written indom - no duplicate instances or instance names to first
     * space
     */

    if (vflag)
	reportconfig();

    if (Cflag)
	exit(0);

    /* create output log - must be done before writing label */
    if ((sts = __pmLogCreate("", outarchname, PM_LOG_VERS02, &logctl)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogCreate(%s): %s\n",
		pmProgname, outarchname, pmErrStr(sts));
	exit(1);
    }

    /* initialize and write label records */
    newlabel();
    logctl.l_state = PM_LOG_STATE_INIT;
    writelabel(0);

    first_datarec = 1;
    ti_idx = 0;

    /*
     * loop
     *	- get next log record
     *	- write out new/changed meta data required by this log record
     *	- write out log
     *	- do ti update if necessary
     */
    while (1) {
	static long	in_offset;		/* for -Dappl0 */
	static long	out_offset;		/* for -Dappl0 */

	fflush(logctl.l_mdfp);
	old_meta_offset = ftell(logctl.l_mdfp);

	in_offset = ftell(inarch.ctxp->c_archctl->ac_log->l_mfp);
	stslog = nextlog();
	if (stslog < 0) {
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "Log: read EOF @ offset=%ld\n", in_offset);
#endif
	    break;
	}
	if (stslog == 1) {
	    /* volume change */
	    newvolume(inarch.ctxp->c_archctl->ac_log->l_curvol);
	}
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    struct timeval	stamp;
	    fprintf(stderr, "Log: read ");
	    stamp.tv_sec = inarch.rp->timestamp.tv_sec;
	    stamp.tv_usec = inarch.rp->timestamp.tv_usec;
	    __pmPrintStamp(stderr, &stamp);
	    fprintf(stderr, " numpmid=%d @ offset=%ld\n", inarch.rp->numpmid, in_offset);
	}
#endif

	if (ti_idx < inarch.ctxp->c_archctl->ac_log->l_numti) {
	    __pmLogTI	*tip = &inarch.ctxp->c_archctl->ac_log->l_ti[ti_idx];
	    if (tip->ti_stamp.tv_sec == inarch.rp->timestamp.tv_sec &&
	        tip->ti_stamp.tv_usec == inarch.rp->timestamp.tv_usec) {
		/*
		 * timestamp on input pmResult matches next temporal index
		 * entry for input archive ... make sure matching temporal
		 * index entry added to output archive
		 */
		needti = 1;
		ti_idx++;
	    }
	}

	/*
	 * optionally rewrite timestamp in pmResult for global time
	 * adjustment ... flows to output pmResult, indom entries in
	 * metadata, temporal index entries and label records
	 * */
	fixstamp(&inarch.rp->timestamp);

	/*
	 * process metadata until find an indom record with timestamp
	 * after the current log record, or a metric record for a pmid
	 * that is not in the current log record
	 */
	for ( ; ; ) {
	    pmID	pmid;			/* pmid for TYPE_DESC */
	    pmInDom	indom;			/* indom for TYPE_INDOM */

	    if (stsmeta == 0) {
		in_offset = ftell(inarch.ctxp->c_archctl->ac_log->l_mdfp);
		stsmeta = nextmeta();
#if PCP_DEBUG
		if (stsmeta < 0 && pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "Metadata: read EOF @ offset=%ld\n", in_offset);
#endif
	    }
	    if (stsmeta < 0) {
		break;
	    }
	    if (stsmeta == TYPE_DESC) {
		int	i;
		pmid = __ntohpmID(inarch.metarec[2]);
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "Metadata: read PMID %s @ offset=%ld\n", pmIDStr(pmid), in_offset);
#endif
		/*
		 * if pmid not in next pmResult, we're done ...
		 */
		for (i = 0; i < inarch.rp->numpmid; i++) {
		    if (pmid == inarch.rp->vset[i]->pmid)
			break;
		}
		if (i == inarch.rp->numpmid)
		    break;
		// TODO rewrite pmDesc meta data? yes unpack, rewrite, pack
	    }
	    else if (stsmeta == TYPE_INDOM) {
		struct timeval	stamp;
		__pmTimeval	*tvp = (__pmTimeval *)&inarch.metarec[2];
		indom = __ntohpmInDom((unsigned int)inarch.metarec[4]);
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "Metadata: read InDom %s @ offset=%ld\n", pmInDomStr(indom), in_offset);
#endif
		// TODO rewrite? yes unpack, pack
		/* if time of indom > next pmResult stop processing metadata */
		stamp.tv_sec = ntohl(tvp->tv_sec);
		stamp.tv_usec = ntohl(tvp->tv_usec);
		if (fixstamp(&stamp)) {
		    /* global time adjustment specified */
		    tvp->tv_sec = htonl(stamp.tv_sec);
		    tvp->tv_usec = htonl(stamp.tv_usec);
		}
		if (stamp.tv_sec > inarch.rp->timestamp.tv_sec)
		    break;
		if (stamp.tv_sec == inarch.rp->timestamp.tv_sec &&
		    stamp.tv_usec > inarch.rp->timestamp.tv_usec)
		    break;
		needti = 1;
	    }
	    else {
		fprintf(stderr, "%s: Error: unrecognised meta data type: %d\n",
		    pmProgname, stsmeta);
		exit(1);
	    }
	    out_offset = ftell(logctl.l_mdfp);
	    if ((sts = _pmLogPut(logctl.l_mdfp, inarch.metarec)) < 0) {
		fprintf(stderr, "%s: Error: _pmLogPut: meta data : %s\n",
			pmProgname, pmErrStr(sts));
		exit(1);
	    }
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		if (stsmeta == TYPE_DESC)
		    fprintf(stderr, "Metadata: write PMID %s @ offset=%ld\n", pmIDStr(pmid), out_offset);
		else
		    fprintf(stderr, "Metadata: write InDom %s @ offset=%ld\n", pmInDomStr(indom), out_offset);
	    }
#endif
	    stsmeta = 0;
	}

	if (first_datarec) {
	    first_datarec = 0;
	    /* any global time adjustment done after nextlog() above */
	    logctl.l_label.ill_start.tv_sec = inarch.rp->timestamp.tv_sec;
	    logctl.l_label.ill_start.tv_usec = inarch.rp->timestamp.tv_usec;
	    /* need to fix start-time in label records */
	    writelabel(1);
	    needti = 1;
	}

	if (needti) {
	    __pmTimeval	tstamp;

	    fflush(logctl.l_mdfp);
	    fflush(logctl.l_mfp);
	    new_meta_offset = ftell(logctl.l_mdfp);
            fseek(logctl.l_mdfp, (long)old_meta_offset, SEEK_SET);
	    tstamp.tv_sec = inarch.rp->timestamp.tv_sec;
	    tstamp.tv_usec = inarch.rp->timestamp.tv_usec;
            __pmLogPutIndex(&logctl, &tstamp);
            fseek(logctl.l_mdfp, (long)new_meta_offset, SEEK_SET);
	    needti = 0;
        }

	sts = __pmEncodeResult(PDU_OVERRIDE2, inarch.rp, &inarch.logrec);
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: __pmEncodeResult: %s\n",
		    pmProgname, pmErrStr(sts));
	    exit(1);
	}

	out_offset = ftell(logctl.l_mfp);
	if ((sts = __pmLogPutResult(&logctl, inarch.logrec)) < 0) {
	    fprintf(stderr, "%s: Error: __pmLogPutResult: log data: %s\n",
		    pmProgname, pmErrStr(sts));
	    exit(1);
	}
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    struct timeval	stamp;
	    fprintf(stderr, "Log: write ");
	    stamp.tv_sec = inarch.rp->timestamp.tv_sec;
	    stamp.tv_usec = inarch.rp->timestamp.tv_usec;
	    __pmPrintStamp(stderr, &stamp);
	    fprintf(stderr, " numpmid=%d @ offset=%ld\n", inarch.rp->numpmid, out_offset);
	}
#endif

	if (inarch.rp->numpmid == 0)
	    /* mark record */
	    needti = 1;

	pmFreeResult(inarch.rp);

    }

#if 0

	/* prepare to write out log record
	 */
	if (ilog != 0) {
	    fprintf(stderr, "%s: Fatal Error!\n", pmProgname);
	    fprintf(stderr, "    log file index = %d\n", ilog);
	    exit(1);
	}

	if (inarch.mark)
	    writemark();
	else {
	    /* result is to be written out, but there is no _Nresult
	     */
	    if (inarch._Nresult == NULL) {
		fprintf(stderr, "%s: Fatal Error!\n", pmProgname);
		fprintf(stderr, "    pick == LOG and _Nresult = NULL\n");
		exit(1);
	    }
	    insertresult(&rlready, inarch._Nresult);
	    writerlist(&rlready, now);

	    /* writerlist frees elm (elements of rlready) but does not
	     * free rp & _Nresult
	     */

	    /* free rp & _Nresult
	     *	_Nresult may contain space that was allocated
	     *	in __pmStuffValue this space has PM_VAL_SPTR format,
	     *	and has to be freed first
	     *	(in order to avoid memory leaks)
	     */
	    if (inarch.rp != inarch._Nresult && inarch._Nresult != NULL) {
		pmValueSet	*vsetp;
		for (i=0; i<inarch._Nresult->numpmid; i++) {
		    vsetp = inarch._Nresult->vset[i];
		    if (vsetp->valfmt == PM_VAL_SPTR) {
			for (j=0; j<vsetp->numval; j++) {
			    free(vsetp->vlist[j].value.pval);
			}
		    }
		}
		free(inarch._Nresult);
	    }
	    if (inarch.rp != NULL)
		pmFreeResult(inarch.rp);
	    inarch.rp = NULL;
	    inarch._Nresult = NULL;
	}
    } /*while()*/

	fflush(logctl.l_mfp);
	fflush(logctl.l_mdfp);

	if (old_log_offset == 0)
	    old_log_offset = sizeof(__pmLogLabel)+2*sizeof(int);

	new_log_offset = ftell(logctl.l_mfp);
	new_meta_offset = ftell(logctl.l_mdfp);

	__pmLogPutIndex(&logctl, &current);


    }

#endif

    exit(0);
}
