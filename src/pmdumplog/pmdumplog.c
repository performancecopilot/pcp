/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>
#include <limits.h>
#include "pmapi.h"
#include "impl.h"

extern int	errno;

static struct timeval	tv;
static int		numpmid;
static pmID		*pmid;
static char		*archbasename;
static int		sflag;

static int
do_size(pmResult *rp)
{
    int		nbyte = 0;
    int		i;
    int		j;
    /*
     *  Externally the log record looks like this ...
     *  :----------:-----------:..........:---------:
     *  | int len  | timestamp | pmResult | int len |
     *  :----------:-----------:..........:---------:
     *
     * start with sizes of the header len, timestamp, numpmid, and
     * trailer len
     */
    nbyte = sizeof(int) + sizeof(__pmTimeval) + sizeof(int);
    						/* len + timestamp + len */
    nbyte += sizeof(int);
    							/* numpmid */
    for (i = 0; i < rp->numpmid; i++) {
	pmValueSet	*vsp = rp->vset[i];
	nbyte += sizeof(pmID) + sizeof(int);		/* + pmid[i], numval */
	if (vsp->numval > 0) {
	    nbyte += sizeof(int);			/* + valfmt */
	    for (j = 0; j < vsp->numval; j++) {
		nbyte += sizeof(__pmValue_PDU);		/* + pmValue[j] */
		if (vsp->valfmt != PM_VAL_INSITU)
							/* + pmValueBlock */
							/* rounded up */
		    nbyte += sizeof(__pmPDU)*((vsp->vlist[j].value.pval->vlen - 1 + sizeof(__pmPDU))/sizeof(__pmPDU));
	    }
	}
    }

    return nbyte;
}

void
dumpresult(pmResult *resp)
{
    int		i;
    int		j;
    int		n;
    char	*p;
    pmDesc	desc;

    if (sflag) {
	int		nbyte;
	nbyte = do_size(resp);
	printf("[%d bytes]\n", nbyte);
    }

    __pmPrintStamp(stdout, &resp->timestamp);

    if (resp->numpmid == 0) {
	printf("  <mark>\n");
	return;
    }

    for (i = 0; i < resp->numpmid; i++) {
	pmValueSet	*vsp = resp->vset[i];
	if (i > 0)
	    printf("            ");
	n = pmNameID(vsp->pmid, &p);
	if (n < 0)
	    printf("  %s (%s):", pmIDStr(vsp->pmid), "<noname>");
	else {
	    printf("  %s (%s):", pmIDStr(vsp->pmid), p);
	    free(p);
	}
	if (vsp->numval == 0) {
	    printf( " No values returned!\n");
	    continue;
	}
	else if (vsp->numval < 0) {
	    printf( " %s\n", pmErrStr(vsp->numval));
	    continue;
	}

	if (pmLookupDesc(vsp->pmid, &desc) < 0) {
	    /* don't know, so punt on the most common cases */
	    desc.indom = PM_INDOM_NULL;
	    if (vsp->valfmt == PM_VAL_INSITU)
		desc.type = PM_TYPE_32;
	    else
		desc.type = PM_TYPE_AGGREGATE;
	}
	if (vsp->numval > 1)
	    printf("\n              ");
	for (j = 0; j < vsp->numval; j++) {
	    pmValue	*vp = &vsp->vlist[j];
	    if (vsp->numval > 1 || desc.indom != PM_INDOM_NULL) {
		printf("  inst [%d", vp->inst);
		if (pmNameInDom(desc.indom, vp->inst, &p) < 0)
		    printf(" or ???]");
		else {
		    printf(" or \"%s\"]", p);
		    free(p);
		}
		putchar(' ');
	    }
	    else
		printf(" ");
	    printf("value ");
	    pmPrintValue(stdout, vsp->valfmt, desc.type, vp, 1);
	    if (j < vsp->numval - 1)
		printf("\n              ");
	    else
		putchar('\n');
	}
    }
}

static void
dumpDesc(__pmContext *ctxp)
{
    int		i;
    int		sts;
    char	*p;
    __pmHashNode	*hp;
    pmDesc	*dp;

    printf("\nDescriptions for Metrics in the Log ...\n");
    for (i = 0; i < ctxp->c_archctl->ac_log->l_hashpmid.hsize; i++) {
	for (hp = ctxp->c_archctl->ac_log->l_hashpmid.hash[i]; hp != NULL; hp = hp->next) {
	    dp = (pmDesc *)hp->data;
	    sts = pmNameID(dp->pmid, &p);
	    if (sts < 0)
		printf("PMID: %s (%s)\n", pmIDStr(dp->pmid), "<noname>");
	    else {
		printf("PMID: %s (%s)\n", pmIDStr(dp->pmid), p);
		free(p);
	    }
	    __pmPrintDesc(stdout, dp);
	}
    }
}

static void
dumpInDom(__pmContext *ctxp)
{
    int		i;
    int		j;
    __pmHashNode	*hp;
    __pmLogInDom	*idp;
    __pmLogInDom	*ldp;

    printf("\nInstance Domains in the Log ...\n");
    for (i = 0; i < ctxp->c_archctl->ac_log->l_hashindom.hsize; i++) {
	for (hp = ctxp->c_archctl->ac_log->l_hashindom.hash[i]; hp != NULL; hp = hp->next) {
	    printf("InDom: %s\n", pmInDomStr((pmInDom)hp->key));
	    /*
	     * in reverse chronological order, so iteration is a bit funny
	     */
	    ldp = NULL;
	    for ( ; ; ) {
		for (idp = (__pmLogInDom *)hp->data; idp->next != ldp; idp =idp->next)
			;
		tv.tv_sec = idp->stamp.tv_sec;
		tv.tv_usec = idp->stamp.tv_usec;
		__pmPrintStamp(stdout, &tv);
		printf(" %d instances\n", idp->numinst);
		for (j = 0; j < idp->numinst; j++) {
		    printf("                 %d or \"%s\"\n",
			idp->instlist[j], idp->namelist[j]);
		}
		if (idp == (__pmLogInDom *)hp->data)
		    break;
		ldp = idp;
	    }
	}
    }
}

static void
dumpTI(__pmContext *ctxp)
{
    int		i;
    char	path[MAXPATHLEN];
    off_t	meta_size;
    off_t	log_size;
    struct stat	sbuf;
    __pmLogTI	*tip;
    __pmLogTI	*lastp;

    printf("\nTemporal Index\n");
    printf("             Log Vol    end(meta)     end(log)\n");
    lastp = NULL;
    for (i = 0; i < ctxp->c_archctl->ac_log->l_numti; i++) {
	tip = &ctxp->c_archctl->ac_log->l_ti[i];
	tv.tv_sec = tip->ti_stamp.tv_sec;
	tv.tv_usec = tip->ti_stamp.tv_usec;
	__pmPrintStamp(stdout, &tv);
	printf("    %4d  %11d  %11d\n", tip->ti_vol, tip->ti_meta, tip->ti_log);
	if (i == 0) {
	    sprintf(path, "%s.meta", archbasename);
	    if (stat(path, &sbuf) == 0)
		meta_size = sbuf.st_size;
	    else
		meta_size = -1;
	}
	if (lastp == NULL || tip->ti_vol != lastp->ti_vol) { 
	    sprintf(path, "%s.%d", archbasename, tip->ti_vol);
	    if (stat(path, &sbuf) == 0)
		log_size = sbuf.st_size;
	    else {
		log_size = -1;
		printf("             Warning: file missing for log volume %d\n", tip->ti_vol);
	    }
	}
	/*
	 * Integrity Errors
	 *
	 * this(tv_sec) < 0
	 * this(tv_usec) < 0 || this(tv_usec) > 999999
	 * this(timestamp) < last(timestamp)
	 * this(vol) < last(vol)
	 * this(vol) == last(vol) && this(meta) <= last(meta)
	 * this(vol) == last(vol) && this(log) <= last(log)
	 * file_exists(<base>.meta) && this(meta) > file_size(<base>.meta)
	 * file_exists(<base>.this(vol)) &&
	 *		this(log) > file_size(<base>.this(vol))
	 *
	 * Integrity Warnings
	 *
	 * this(vol) != last(vol) && !file_exists(<base>.this(vol))
	 */
	if (tip->ti_stamp.tv_sec < 0 ||
	    tip->ti_stamp.tv_usec < 0 || tip->ti_stamp.tv_usec > 999999)
	    printf("             Error: illegal timestamp value (%d sec, %d usec)\n",
		tip->ti_stamp.tv_sec, tip->ti_stamp.tv_usec);
	if (meta_size != -1 && tip->ti_meta > meta_size)
	    printf("             Error: offset to meta file past end of file (%ld)\n",
		(long)meta_size);
	if (log_size != -1 && tip->ti_log > log_size)
	    printf("             Error: offset to log file past end of file (%ld)\n",
		(long)log_size);
	if (i > 0) {
	    if (tip->ti_stamp.tv_sec + (double)tip->ti_stamp.tv_usec / 1000000.0 <
		lastp->ti_stamp.tv_sec + (double)lastp->ti_stamp.tv_usec / 1000000.0)
		printf("             Error: timestamp went backwards in time\n");
	    if (tip->ti_vol < lastp->ti_vol)
		printf("             Error: volume number decreased\n");
	    if (tip->ti_vol == lastp->ti_vol && tip->ti_meta < lastp->ti_meta)
		printf("             Error: offset to meta file decreased\n");
	    if (tip->ti_vol == lastp->ti_vol && tip->ti_log < lastp->ti_log)
		printf("             Error: offset to log file decreased\n");
	}
	lastp = tip;
    }
}

static void
rawdump(FILE *f)
{
    long	old;
    int		len;
    int		check;
    int		i;
    int		sts;

    old = ftell(f);
    fseek(f, 0, SEEK_SET);

    while ((sts = fread(&len, 1, sizeof(len), f)) == sizeof(len)) {
	printf("Dump ... record len: %d @ offset: %ld", len, ftell(f) - sizeof(len));
	len -= 2 * sizeof(len);
	for (i = 0; i < len; i++) {
	    check = fgetc(f);
	    if (check == EOF) {
		printf("Unexpected EOF\n");
		break;
	    }
	    if (i % 32 == 0) putchar('\n');
	    if (i % 4 == 0) putchar(' ');
	    printf("%02x", check &0xff);
	}
	putchar('\n');
	if ((sts = fread(&check, 1, sizeof(check), f)) != sizeof(check)) {
	    if (sts == 0)
		printf("Unexpected EOF\n");
	    break;
	}
	len += 2 * sizeof(len);
	if (check != len) {
	    printf("Trailer botch: %d != %d\n", check, len);
	    break;
	}
    }
    if (sts < 0)
	printf("fread fails: %s\n", strerror(errno));
    fseek(f, old, SEEK_SET);
}

static void
dometric(const char *name)
{
    int		sts;

    if (*name == '\0') {
	printf("PMNS appears to be empty!\n");
	return;
    }
    numpmid++;
    pmid = (pmID *)realloc(pmid, numpmid * sizeof(pmID));
    if ((sts = pmLookupName(1, (char **)&name, &pmid[numpmid-1])) < 0) {
	fprintf(stderr, "%s: pmLookupName(%s): %s\n", pmProgname, name, pmErrStr(sts));
	numpmid--;
    }
}

int
main(int argc, char *argv[])
{
    int			c;
    int			sts;
    int			errflag = 0;
    char		*pmnsfile = PM_NS_DEFAULT;
    char		*rawfile;
    int			i;
    int			n;
    int			first = 1;
    int			dflag = 0;
    int			iflag = 0;
    int			Lflag = 0;
    int			lflag = 0;
    int			mflag = 0;
    int			tflag = 0;
    int			vflag = 0;
    int			mode = PM_MODE_FORW;
    char		*p;
    __pmContext		*ctxp;
    pmResult		*result;
    char		*start = NULL;
    char		*finish = NULL;
    struct timeval	startTime = { 0, 0 }; 
    struct timeval 	endTime = { INT_MAX, 0 };
    struct timeval	appStart;
    struct timeval	appEnd;
    struct timeval	appOffset;
    pmLogLabel		label;			/* get hostname for archives */
    int			zflag = 0;		/* for -z */
    char 		*tz = NULL;		/* for -Z timezone */
    char		timebuf[32];		/* for pmCtime result + .xxx */
    double		current;
    double		done;
    extern char		*optarg;
    extern int		optind;
    extern int		pmDebug;

    /* trim cmd name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    while ((c = getopt(argc, argv, "aD:dilLmn:rS:sT:tv:Z:z?")) != EOF) {
	switch (c) {

	case 'a':	/* dump everything */
	    dflag = iflag = lflag = mflag = sflag = tflag = 1;
	    break;

	case 'd':	/* dump pmDesc structures */
	    dflag = 1;
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

	case 'i':	/* dump instance domains */
	    iflag = 1;
	    break;

	case 'L':	/* dump label, verbose */
	    Lflag = 1;
	    lflag = 1;
	    break;

	case 'l':	/* dump label */
	    lflag = 1;
	    break;

	case 'm':	/* dump metrics in log */
	    mflag = 1;
	    break;

	case 'n':	/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 'r':	/* read log in reverse chornological order */
	    mode = PM_MODE_BACK;
	    break;

	case 'S':	/* start time */
	    start = optarg;
	    break;

	case 's':	/* report data size in log */
	    sflag = 1;
	    break;

	case 'T':	/* terminate time */
	    finish = optarg;
	    break;

	case 't':	/* dump temporal index */
	    tflag = 1;
	    break;

	case 'v':	/* verbose, dump in raw format */
	    vflag = 1;
	    rawfile = optarg;
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

    if (errflag ||
	(vflag && optind != argc) ||
	(!vflag && optind > argc-1)) {
	fprintf(stderr,
"Usage: %s [options] archive [metricname ...]\n"
"       %s -v file\n"
"\n"
"Options:\n"
"  -a            dump everything\n"
"  -d            dump metric descriptions\n"
"  -i            dump instance domain descriptions\n"
"  -L            more verbose form of -l\n"
"  -l            dump the archive label\n"
"  -m            dump values of the metrics (default)\n"
"  -n pmnsfile   use an alternative local PMNS\n"
"  -r            process archive in reverse chronological order\n"
"  -S start      start of the time window\n"
"  -s            report size of data records in archive\n"
"  -T finish     end of the time window\n"
"  -t            dump the temporal index\n"
"  -v file       verbose hex dump of a physical file in raw format\n"
"  -Z timezone   set reporting timezone\n"
"  -z            set reporting timezone to local time for host from -a\n",
                pmProgname, pmProgname);
	exit(1);
    }

    if (vflag) {
	FILE	*f;
	if ((f = fopen(rawfile, "r")) == NULL) {
	    fprintf(stderr, "%s: Cannot open \"%s\": %s\n", pmProgname, rawfile, strerror(errno));
	    exit(1);
	}
	printf("Raw dump of physical archive file \"%s\" ...\n", rawfile);
	rawdump(f);
	exit(0);
    }

    if (dflag + iflag + lflag + mflag + tflag == 0)
	mflag = 1;	/* default */

    archbasename = argv[optind];
    if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, archbasename)) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n", pmProgname, argv[optind], pmErrStr(sts));
	exit(1);
    }
    optind++;

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((sts = pmLoadNameSpace(pmnsfile)) < 0) {
	    fprintf(stderr, "%s: Cannot load namespace from \"%s\": %s\n", pmProgname, pmnsfile, pmErrStr(sts));
	    exit(1);
	}
    }

    numpmid = argc - optind;
    if (numpmid) {
	numpmid = 0;
	pmid = NULL;
	for (i = 0 ; optind < argc; i++, optind++) {
	    numpmid++;
	    pmid = (pmID *)realloc(pmid, numpmid * sizeof(pmID));
	    if ((sts = pmLookupName(1, &argv[optind], &pmid[numpmid-1])) < 0) {
		if (sts == PM_ERR_NONLEAF) {
		    numpmid--;
		    if ((sts = pmTraversePMNS(argv[optind], dometric)) < 0)
			fprintf(stderr, "%s: pmTraversePMNS(%s): %s\n", pmProgname, argv[optind], pmErrStr(sts));
		}
		else
		    fprintf(stderr, "%s: pmLookupName(%s): %s\n", pmProgname, argv[optind], pmErrStr(sts));
		if (sts < 0)
		    numpmid--;
	    }
	}
	if (numpmid == 0) {
	    fprintf(stderr, "No metric names can be translated, dump abandoned\n");
	    exit(1);
	}
    }

    if ((n = pmWhichContext()) >= 0)
	ctxp = __pmHandleToPtr(n);
    else {
	fprintf(stderr, "%s: %s!\n", pmProgname, pmErrStr(PM_ERR_NOCONTEXT));
	exit(1);
    }

    if ((sts = pmGetArchiveLabel(&label)) < 0) {
	fprintf(stderr, "%s: Cannot get archive label record: %s\n",
	    pmProgname, pmErrStr(sts));
	exit(1);
    }
    startTime = label.ll_start;;

    if ((sts = pmGetArchiveEnd(&endTime)) < 0) {
	endTime.tv_sec = INT_MAX;
	endTime.tv_usec = 0;
	fflush(stdout);
	fprintf(stderr, "%s: Cannot locate end of archive: %s\n",
	    pmProgname, pmErrStr(sts));
	fprintf(stderr, "\nWARNING: This archive is sufficiently damaged that it may not be possible to\n");
	fprintf(stderr, "         produce complete information.  Continuing and hoping for the best.\n\n");
	fflush(stderr);
    }

    if (zflag) {
	if ((sts = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmProgname, pmErrStr(sts));
	    exit(1);
	}
	printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n",
	    label.ll_hostname);
    }
    else if (tz != NULL) {
	if ((sts = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		pmProgname, tz, pmErrStr(sts));
	    exit(1);
	}
	printf("Note: timezone set to \"TZ=%s\"\n\n", tz);
    }

    sts = pmParseTimeWindow(start, finish, NULL, NULL, &startTime,
			    &endTime, &appStart, &appEnd, &appOffset,
			    &p);
    pmSetMode(mode, &appStart, 0);

    if (sts < 0) {
	fprintf(stderr, "%s:\n%s\n", pmProgname, p);
	exit(1);
    }

    if (lflag) {
	char	       *ddmm;
	char	       *yr;

	printf("Log Label (Log Format Version %d)\n", label.ll_magic & 0xff);
	printf("Performance metrics from host %s\n", label.ll_hostname);

	ddmm = pmCtime((const time_t *)&label.ll_start.tv_sec, timebuf);
	ddmm[10] = '\0';
	yr = &ddmm[20];
	printf("  commencing %s ", ddmm);
	__pmPrintStamp(stdout, &label.ll_start);
	printf(" %4.4s\n", yr);

	if (endTime.tv_sec == INT_MAX) {
	    /* pmGetArchiveEnd() failed! */
	    printf("  ending     UNKNOWN\n");
	}
	else {
	    ddmm = pmCtime((const time_t *)&endTime.tv_sec, timebuf);
	    ddmm[10] = '\0';
	    yr = &ddmm[20];
	    printf("  ending     %s ", ddmm);
	    __pmPrintStamp(stdout, &endTime);
	    printf(" %4.4s\n", yr);
	}

	if (Lflag) {
	    printf("Archive timezone: %s\n", label.ll_tz);
	    printf("PID for pmlogger: %d\n", (int)label.ll_pid);
	}
    }

    if (dflag)
	dumpDesc(ctxp);

    if (iflag)
	dumpInDom(ctxp);

    if (tflag)
	dumpTI(ctxp);

    if (mflag) {
	if (mode == PM_MODE_FORW) {
	    sts = pmSetMode(mode, &appStart, 0);
	    done = appEnd.tv_sec + (double)(appEnd.tv_usec)/1000000;
	}
	else {
	    appEnd.tv_sec = INT_MAX;
	    sts = pmSetMode(mode, &appEnd, 0);
	    done = appStart.tv_sec + (double)(appStart.tv_usec)/1000000;
	}
	if (sts < 0) {
	    fprintf(stderr, "%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}
	sts = 0;
	for ( ; ; ) {
	    if (numpmid == 0)
		sts = pmFetchArchive(&result);
	    else
		sts = pmFetch(numpmid, pmid, &result);
	    if (sts < 0)
		break;
	    if (first && mode == PM_MODE_BACK) {
		first = 0;
		printf("\nLog finished at %24.24s - dump in reverse order\n", pmCtime((const time_t *)&result->timestamp.tv_sec, timebuf));
	    }
	    current = result->timestamp.tv_sec + (double)(result->timestamp.tv_usec)/1000000;
	    if ((mode == PM_MODE_FORW && current > done) ||
		(mode == PM_MODE_BACK && current < done)) {
		sts = PM_ERR_EOL;
		break;
	    }
	    putchar('\n');
	    dumpresult(result);
	    pmFreeResult(result);
	}
	if (sts != PM_ERR_EOL) {
	    fprintf(stderr, "%s: pmFetch: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}
    }

    exit(0);
    /*NOTREACHED*/
}
