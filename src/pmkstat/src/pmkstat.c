/*
 * Kernel statistics -- this program does fairly high level kernel perf stats.
 * Target systems are large DBMS machines (not graphics!).
 *
 * It is intended as a first point of call to analyse performance problems,
 * after which other tools can be used for more details in each subsystem.
 *
 * Mark Goodwin, Thu Dec  2 14:21:52 EST 1993
 * markgw@sgi.com
 *
 * Copyright (c) 1995-2003 Silicon Graphics, Inc.  All Rights Reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: pmkstat.c,v 1.6 2004/06/07 10:17:19 nathans Exp $"

#define HAVE_NETWORK 0

#include "pmapi.h"
#include "impl.h"
#include "pmnsmap.h"

#define _KMEMUSER
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_IMMU_H
#include <sys/immu.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

extern char *optarg;
extern int optind;

/* arg flags */
static double interval=5.0;
static int iter=0;
static double duration, sum_time;
static time_t now_t;

/* private functions */
#define MAX_MID	100

#define firstval(p, id, sel) (p->vset[id]->numval > 0 ? p->vset[id]->vlist[0].value.sel : 0)

static double ulong_diff(__uint32_t this, __uint32_t prev);

/* helper function for Extended Time Base */
static void
getDoubleAsXTB(double *realtime, int *ival, int *mode)
{
#define SECS_IN_24_DAYS 2073600.0

    if (*realtime > SECS_IN_24_DAYS) {
        *ival = (int)*realtime;
	*mode = (*mode & 0x0000ffff) | PM_XTB_SET(PM_TIME_SEC);
    }
    else {
	*ival = (int)(*realtime * 1000.0);
	*mode = (*mode & 0x0000ffff) | PM_XTB_SET(PM_TIME_MSEC);
    }
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    char	*p;
    int		errflag = 0;
    int		type = 0;
    int		mode = PM_MODE_INTERP;
    char	*host;
    char	*pmnsfile = PM_NS_DEFAULT;
    int		samples = 0;
    char	*endnum;
    int		i;
    int		pauseFlag=0;
    int		numpmid;
    pmResult	*resp;
    pmResult	*prev;
    pmID	pmidlist[MAX_MID];
    pmDesc	desclist[MAX_MID];
    pmAtomValue	la;
    unsigned long long	dkread, lastdkread;
    unsigned long long	dkwrite, lastdkwrite;
#if HAVE_NETWORK
    unsigned long long	pktin, pktout;
    unsigned long long	lastpktin, lastpktout;
#endif

    char	local[MAXHOSTNAMELEN];
    pmLogLabel	label;
    int		zflag = 0;			/* for -z */
    char 	*tz = NULL;			/* for -Z timezone */
    int		tzh;				/* initial timezone handle */
    char	timebuf[26];
    int		runocc;
    int		swapocc;
    int		pcp_context;
    int		reconnecting = 0;
    double	runq;
    double	swapq;
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;
    char	    *Sflag = NULL;		/* argument of -S flag */
    char	    *Tflag = NULL;		/* argument of -T flag */
    char	    *Aflag = NULL;		/* argument of -A flag */
    char	    *Oflag = NULL;		/* argument of -O flag */
    struct timeval  first;			/* initial sample time */
    struct timeval  last;			/* final sample time */
    struct timeval  tv;
    char	    *msg;
    double	    runtime;
    double	    tmp;

#ifdef __sgi
    __pmSetAuthClient();
#endif

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    setlinebuf(stdout);

    while ((c = getopt(argc, argv, "A:a:D:h:Ln:O:ps:S:t:T:U:zZ:?")) != EOF) {
	switch (c) {

	case 'A':	/* sample time alignment */
	    Aflag = optarg;
	    break;

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

	case 'd':	/* pause between updates when replaying an archive */
	    pauseFlag++;
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

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'L':	/* standalone, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    type = PM_CONTEXT_LOCAL;
	    (void)gethostname(local, MAXHOSTNAMELEN);
	    local[MAXHOSTNAMELEN-1] = '\0';
	    host = local;
	    break;

	case 'n':	/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 'O':	/* time window offset */
	    Oflag = optarg;
	    break;

	case 's':	/* sample count */
	    if (Tflag) {
		fprintf(stderr, "%s: at most one of -T and -s allowed\n", pmProgname);
		errflag++;
	    }
	    samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples < 0.0) {
		fprintf(stderr, "%s: -s requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case 'S':	/* time window start */
	    Sflag = optarg;
	    break;

	case 't':	/* update interval */
	    if (pmParseInterval(optarg, &tv, &endnum) < 0) {
		fprintf(stderr, "%s: -t argument is not in pmParseInterval(3) format:\n", pmProgname);
		fprintf(stderr, "%s\n", endnum);
		free(endnum);
		errflag++;
	    }
	    else {
		/* note: this value of tv is _not_ used later on */
		interval = tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
	    }
	    break;

	case 'T':	/* time window emd */
	    if (samples) {
		fprintf(stderr, "%s: at most one of -T and -s allowed\n", pmProgname);
		errflag++;
	    }
	    Tflag = optarg;
	    break;

	case 'U':	/* raw archive (undocumented) */
	    type = PM_CONTEXT_ARCHIVE;
	    mode = PM_MODE_FORW;
	    host = optarg;
	    break;

	case 'z':	/* timezone from host */
	    if (tz != NULL) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    zflag++;
	    break;

	case 'Z':	/* $TZ timezone */
	    if (zflag) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    tz = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (pauseFlag && (type == 0 || type == PM_CONTEXT_HOST)) {
	fprintf(stderr, "%s: -p can only be used with -a\n", pmProgname);
	errflag++;
    }

    if (zflag && type == 0) {
	fprintf(stderr, "%s: -z requires an explicit -a or -h option\n", pmProgname);
	errflag++;
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options]\n\
\n\
Options:\n\
  -A align      align sample times on natural boundaries\n\
  -a archive	metrics source is a PCP log archive\n\
  -d            delay, pause between updates for archive replay\n\
  -h host	metrics source is PMCD on host\n\
  -L            use standalone connection to localhost\n\
  -n pmnsfile   use an alternative PMNS\n\
  -O offset     initial offset into the time window\n\
  -S starttime  start of the time window\n\
  -s samples	terminate after this many iterations\n\
  -t interval	sample interval [default 5 seconds]\n\
  -T endtime    end of the time window\n\
  -Z timezone   set reporting timezone\n\
  -z            set reporting timezone to local time of metrics source\n",
		pmProgname);
	exit(1);
    }

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((sts = pmLoadNameSpace(pmnsfile)) < 0) {
	    printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, pmnsfile, pmErrStr(sts));
	    exit(1);
	}
    }

#ifdef MALLOC_AUDIT
    _malloc_reset_();
    atexit(_malloc_audit_);
#endif

    if ( type == PM_CONTEXT_LOCAL && geteuid() != (uid_t)0) {
        fprintf(stderr, "%s Error: you need to be root to use -L\n", pmProgname);
        exit(1);
    }

    if (type == 0) {
	type = PM_CONTEXT_HOST;
	(void)gethostname(local, MAXHOSTNAMELEN);
	local[MAXHOSTNAMELEN-1] = '\0';
	host = local;
    }

    if ((sts = pmNewContext(type, host)) < 0) {
	if (type == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	else if (type == PM_CONTEXT_LOCAL)
	    fprintf(stderr, "%s: Cannot establish local standalone connection: %s\n",
		pmProgname, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	exit(1);
    }
    pcp_context = sts;

    if (type == PM_CONTEXT_ARCHIVE) {
	if ((sts = pmGetArchiveLabel(&label)) < 0) {
	    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmProgname, pmErrStr(sts));
	    exit(1);
	}
	first = label.ll_start;
	host = label.ll_hostname;
	if ((sts = pmGetArchiveEnd(&last)) < 0) {
	    fprintf(stderr, "%s: Cannot determine end of archive: %s",
		pmProgname, pmErrStr(sts));
	    exit(1);
	}
    }
    else {
	gettimeofday(&first, NULL);
	last.tv_sec = INT_MAX;
    }

    if (zflag) {
	if ((tzh = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmProgname, pmErrStr(tzh));
	    exit(1);
	}
	printf("Note: timezone set to local timezone of host \"%s\"\n\n", host);
    }
    else if (tz != NULL) {
	if ((tzh = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		pmProgname, tz, pmErrStr(tzh));
	    exit(1);
	}
	printf("Note: timezone set to \"TZ=%s\"\n\n", tz);
    }
    else
	/* save this one */
	tzh = pmNewContextZone();

    if (pmParseTimeWindow(Sflag, Tflag, Aflag, Oflag,
			   &first, &last,
                           &tv, &last, &first, &msg) < 0) {
	fprintf(stderr, "%s: %s", pmProgname, msg);
	exit(1);
    }

    if (Tflag) {
	runtime = (double)(last.tv_sec - first.tv_sec + 
			   (last.tv_usec - first.tv_usec) / 1000000);
	samples = runtime / interval;
    }

    if (type == PM_CONTEXT_ARCHIVE) {
	int tmp_ival;
	int tmp_mode = mode;
	getDoubleAsXTB(&interval, &tmp_ival, &tmp_mode);

	if ((sts = pmSetMode(tmp_mode, &first, tmp_ival)) < 0) {
	    fprintf(stderr, "%s: pmSetMode failed: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}
    }

    if (optind < argc) {
	for (; optind < argc; optind++)
	    (void)fprintf(stderr,
		"%s: Error: unexpected argument \"%s\"\n", pmProgname, argv[optind]);
	exit(1);
    }

    setbuf(stdout, NULL);

    /*
     * build vector of all of the desired metrics at each sample
     */
    numpmid = sizeof(_sample) / sizeof(char *);
    if ((sts = pmLookupName(numpmid, _sample, pmidlist)) < 0) {
	printf("%s: pmLookupName: %s\n", pmProgname, pmErrStr(sts));
	if (sts == PM_ERR_NAME || sts == PM_ERR_NONLEAF) {
	    for (i = 0; i < numpmid; i++) {
		if (pmidlist[i] == PM_ID_NULL)
		    fprintf(stderr, "%s: metric \"%s\" not in name space\n", 
		            pmProgname, _sample[i]);
	    }
        }
    }
    for (i = 0; i < numpmid; i++) {
	if (pmidlist[i] == PM_ID_NULL) {
	    desclist[i].indom = PM_INDOM_NULL;
	    desclist[i].pmid = PM_ID_NULL;
	} else {
	    if ((sts = pmLookupDesc(pmidlist[i], desclist+i)) < 0) {
		fprintf(stderr, 
			"%s: Warning: cannot retrieve description for "
			"metric \"%s\" (PMID: %s)\nReason: %s\n",
			pmProgname, _sample[i], pmIDStr(pmidlist[i]),
			pmErrStr(sts));
		desclist[i].indom = PM_INDOM_NULL;
		desclist[i].pmid = PM_ID_NULL;
	    }
	}
    }
    if (desclist[LOADAV].indom != PM_INDOM_NULL) {
	int	inst;
	if (type == PM_CONTEXT_ARCHIVE)
	    inst = pmLookupInDomArchive(desclist[LOADAV].indom, "1 minute");
	else
	    inst = pmLookupInDom(desclist[LOADAV].indom, "1 minute");

	if (inst >= 0) {
	    pmDelProfile(desclist[LOADAV].indom, 0, NULL);	/* all off */
	    pmAddProfile(desclist[LOADAV].indom, 1, &inst);	/* enable [0] */
	}
    }

    for (iter=(-1); samples==0 || iter < samples; iter++) {

        if (reconnecting > 0) {
            if ((sts = pmReconnectContext(pcp_context)) < 0) {
                /* metrics source is still unavailable */
                goto NEXT;
            }
            else {
                /*
                 * Reconnected successfully, but need headings
                 * and another fetch before reporting values
                 */
                reconnecting--;
            }
        }

	if ((sts = pmFetch(numpmid, pmidlist, &resp)) < 0) {
	    if (sts == PM_ERR_IPC) {
		fprintf(stderr, "Fetch failed. Reconnecting ...\n");
		reconnecting = 2;
		goto NEXT;
	    }
	    else {
		fprintf(stderr, "pmFetch: %s\n", pmErrStr(sts));
		exit(1);
	    }
	}
#ifdef DESPERATE
	__pmDumpResult(stdout, resp);
#endif
	if (desclist[DKREAD].pmid == PM_ID_NULL || resp->vset[DKREAD]->numval != 1)
	    la.ull = -1;
	else
	    pmExtractValue(resp->vset[DKREAD]->valfmt,
		&resp->vset[DKREAD]->vlist[0], desclist[DKREAD].type,
		&la, PM_TYPE_U64);
	dkread = la.ull;

	if (desclist[DKWRITE].pmid == PM_ID_NULL || resp->vset[DKWRITE]->numval != 1)
	    la.ull = -1;
	else
	    pmExtractValue(resp->vset[DKWRITE]->valfmt,
		&resp->vset[DKWRITE]->vlist[0], desclist[DKWRITE].type,
		&la, PM_TYPE_U64);
	dkwrite = la.ull;

#if HAVE_NETWORK
	pktin = -1;
	if (desclist[PACK_IN].pmid != PM_ID_NULL) {
	    for (i = 0; i < resp->vset[PACK_IN]->numval; i++) {
		pmExtractValue(resp->vset[PACK_IN]->valfmt,
			&resp->vset[PACK_IN]->vlist[i], desclist[PACK_IN].type,
			&la, PM_TYPE_U64);
		if (pktin == -1)
		    pktin = la.ull;
		else
		    pktin += la.ull;
	    }
	}

	pktout = -1;
	if (desclist[PACK_OUT].pmid != PM_ID_NULL) {
	    for (i = 0; i < resp->vset[PACK_OUT]->numval; i++) {
		pmExtractValue(resp->vset[PACK_OUT]->valfmt,
			&resp->vset[PACK_OUT]->vlist[i], desclist[PACK_IN].type,
			&la, PM_TYPE_U64);
		if (pktout == -1)
		    pktout = la.ull;
		else
		    pktout += la.ull;
	    }
	}
#endif

	if (iter % 22 == 0 || reconnecting == 1) {
	    /*
	     * heading
	     */
	    now_t = (time_t)((double)resp->timestamp.tv_sec + 0.5 + (double)resp->timestamp.tv_usec / 1000000);

	    printf("# %s load avg: ", host);

	    /* load average */
	    if (resp->vset[LOADAV]->pmid != PM_ID_NULL && resp->vset[LOADAV]->numval == 1) {
		pmExtractValue(resp->vset[LOADAV]->valfmt,
			&resp->vset[LOADAV]->vlist[0], desclist[LOADAV].type,
			&la, PM_TYPE_FLOAT);
		printf("%.2f", (double)la.f);
	    }
	    else
		/* load average, not available */
		printf("?");

	    printf(", interval: %g sec, %s", interval, pmCtime(&now_t, timebuf));

#if HAVE_NETWORK
printf("\
 queue |      memory |     system       |  disks  |net packets|      cpu\n\
run swp|    free page| scall ctxsw  intr|  rd   wr|   in   out|usr sys idl  wt\
\n");
/*
xxx xxx xxxxxxxx xxxx xxxxxx xxxxx xxxxx xxxx xxxx xxxxx xxxxx xxx xxx xxx xxx
*/
#else
printf("\
 queue |      memory |     system       |  disks  |      cpu\n\
run swp|    free page| scall ctxsw  intr|  rd   wr|usr sys idl  wt\
\n");
/*
xxx xxx xxxxxxxx xxxx xxxxxx xxxxx xxxxx xxxx xxxx xxx xxx xxx xxx
*/
#endif
	}

	if (reconnecting > 0) {
	    reconnecting--;
	    goto NEXT;
	}

	if (iter >= 0) {
	    /*
	     * Report
	     */
	    duration = __pmtimevalSub(&resp->timestamp, &prev->timestamp);

	    /* multiplier for percent cpu usage */
	    for (sum_time=0.0, i = CPU_USER; i <= CPU_WAIT; i++) {
		if (resp->vset[i]->numval == 1 && prev->vset[i]->numval == 1) {
		    tmp = (int)ulong_diff(firstval(resp, i, lval),
					     firstval(prev, i, lval));
		    if (tmp < 0) {
			sum_time = -1;
			break;
		    }
		    prev->vset[i]->vlist[0].value.lval = tmp;
		    sum_time += tmp;
		}
		else {
		    if ( i != CPU_SXBRK ) {
			sum_time = -1;
			break;
		    }
		}
	    }
	    if (sum_time > 0)
		sum_time = 100.0 / sum_time;

	    /*
	     * runq and swapq are strange ...
	     * at each clock tick, if the queue is not empty then
	     * add 1 to the occ count, and increment the q count by the
	     * current q length ...
	     * the best we can report is something between the stochastic
	     * and the time average
	     */
	    if (resp->vset[RUNQ_OCC]->numval == 1 && prev->vset[RUNQ_OCC]->numval == 1) {
		runocc = firstval(resp, RUNQ_OCC, lval) - firstval(prev, RUNQ_OCC, lval);
		if (runocc == 0)
		    runq = 0.0;
		else {
		    if (resp->vset[RUNQ]->numval == 1 && prev->vset[RUNQ]->numval == 1)
			runq = (double)(firstval(resp, RUNQ, lval) - firstval(prev, RUNQ, lval)) / runocc;
		    else
			runq = -1;
		}
	    }
	    else
		runq = -1;


	    if (resp->vset[SWAPQ_OCC]->numval == 1 && prev->vset[SWAPQ_OCC]->numval == 1) {
		swapocc = firstval(resp, SWAPQ_OCC, lval) - firstval(prev, SWAPQ_OCC, lval);
		if (swapocc == 0)
		    swapq = 0.0;
		else {
		    if (resp->vset[SWAPQ]->numval == 1 && prev->vset[SWAPQ]->numval == 1)

			swapq = (double)(firstval(resp, SWAPQ, lval) - firstval(prev, SWAPQ, lval)) / swapocc;
		    else
			swapq = -1;
		}
	    }
	    else
		swapq = -1;

	    if (interval > 0.0) {
		/* mean number of runnable processes in memory */
		if (runq != -1)
		    printf("%3.0f", runq);
		else
		    printf("%3.3s", "?");

		/* mean number of runnable processes in swap */
		if (swapq != -1)
		    printf(" %3.0f", swapq);
		else
		    printf(" %3.3s", "?");

		/* mean free kbytes of memory over the interval */
		if (resp->vset[FREEMEM]->numval == 1)
		    printf(" %8.0f", (double)firstval(resp, FREEMEM, lval));
		else
		    printf(" %8.8s", "?");

		/* 
		 * mean number of page out I/O operations during interval
		 * -- precise semantics need to be checked --
		 */
		tmp = -1;
		if (resp->vset[SWAPOUT]->numval == 1 && prev->vset[SWAPOUT]->numval == 1)
		    tmp = ulong_diff(firstval(resp, SWAPOUT, lval), firstval(prev, SWAPOUT, lval)) / duration;
		if (tmp >= 0)
		    printf(" %4.0f", tmp);
		else
		    printf(" %4.4s", "?");

		/* system call rate (per second) */
		tmp = -1;
		if (resp->vset[SYSCALL]->numval == 1 && prev->vset[SYSCALL]->numval == 1)
		    tmp = ulong_diff(firstval(resp, SYSCALL, lval), firstval(prev, SYSCALL, lval)) / duration;
		if (tmp >= 0)
		    printf(" %6.0f", tmp);
		else
		    printf(" %6.6s", "?");

		/* context switch rate (per second) */
		tmp = -1;
		if (resp->vset[CONTEXTSW]->numval == 1 && prev->vset[CONTEXTSW]->numval == 1)
		    tmp = ulong_diff(firstval(resp, CONTEXTSW, lval), firstval(prev, CONTEXTSW, lval)) / duration;
		if (tmp >= 0)
		    printf(" %5.0f", tmp);
		else
		    printf(" %5.5s", "?");

		/* interrupt rate (per second) */
		tmp = -1;
		if (resp->vset[INTR]->numval == 1 && prev->vset[INTR]->numval == 1)
		    tmp = ulong_diff(firstval(resp, INTR, lval), firstval(prev, INTR, lval)) / duration;
		if (tmp >= 0)
		    printf(" %5.0f", tmp);
		else
		    printf(" %5.5s", "?");

		/* disk reads (per second) */
		if (dkread != -1 && lastdkread != -1)
		    printf(" %4.0f", (double)(dkread - lastdkread) / duration);
		else
		    printf(" %4.4s", "?");

		/* disk writes (per second) */
		if (dkwrite != -1 && lastdkwrite != -1)
		    printf(" %4.0f", (double)(dkwrite - lastdkwrite) / duration);
		else
		    printf(" %4.4s", "?");
#if HAVE_NETWORK

		/* network packets in (per second) */
	        if (pktin != -1 && lastpktin != -1)
		    printf(" %5.0f",
			(double)(pktin - lastpktin) /duration);
		else
		    printf(" %5.5s", "?");

		/* network packets out (per second) */
	        if (pktout != -1 && lastpktout != -1)
		    printf(" %5.0f",
			(double)(pktout - lastpktout) /duration);
		else
		    printf(" %5.5s", "?");
#endif

		/* percentage time spent executing in user mode */
		if (sum_time != -1)
		    printf(" %3.0f",
			sum_time * firstval(prev, CPU_USER, lval));
		    else
			printf(" %3.3s", "?");

		/* percentage time spent executing in kernel mode */
		if (sum_time != -1)
		    printf(" %3.0f",
			sum_time * (firstval(prev, CPU_KERNEL, lval) + firstval(prev, CPU_SXBRK, lval) + firstval(prev, CPU_INTR, lval)));
		    else
			printf(" %3.3s", "?");

		/* percentage time spent in idle loop */
		if (sum_time != -1)
		    printf(" %3.0f",
			sum_time * firstval(prev, CPU_IDLE, lval));
		    else
			printf(" %3.3s", "?");

		/* percentage time spent in wait */
		if (sum_time != -1)
		    printf(" %3.0f",
			sum_time * firstval(prev, CPU_WAIT, lval));
		    else
			printf(" %3.3s", "?");

		putchar('\n');
	    }
	}

	if (iter >= 0)
	    pmFreeResult(prev);
	prev = resp;
	lastdkread = dkread;
	lastdkwrite = dkwrite;

#if HAVE_NETWORK
	lastpktin = pktin;
	lastpktout = pktout;
#endif

NEXT:
	if (samples == 0 || iter < samples-1) {
	    if (type != PM_CONTEXT_ARCHIVE || pauseFlag) {
		sginap((long)((double)CLK_TCK * interval));
	    }
	}
    } /* for */

    if (iter > -1)
	pmFreeResult(prev);

    exit(0);
    /*NOTREACHED*/
}

/* return ulong result of difference between two ulong */
static double
ulong_diff(__uint32_t this, __uint32_t prev)
{
    double	v = (double)(this) - (double)(prev);
    static int	dowrap = -1;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	fprintf(stderr, "\nulong_diff: prev = %u, this = %u, v = %f\n", prev, this, v);
    }
#endif

    if (v < 0.0) {
	if (dowrap == -1) {
	    /* PCP_COUNTER_WRAP in environment enables "counter wrap" logic */
	    if (getenv("PCP_COUNTER_WRAP") == NULL)
		dowrap = 0;
	    else
		dowrap = 1;
	}
	if (dowrap) {
	    /* wrapped, assume just once ... */
	    v += (double)UINT_MAX+1;
	}
    }

    return v;
}
