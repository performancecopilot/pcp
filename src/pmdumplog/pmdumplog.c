/*
 * Copyright (c) 2014 Red Hat.
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
 */

#include "pmapi.h"
#include "impl.h"
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>

static struct timeval	tv;
static char		timebuf[32];	/* for pmCtime result + .xxx */
static int		numpmid;
static pmID		*pmid;
static pmID		pmid_flags;
static pmID		pmid_missed;
static int		sflag;
static int		xflag;		/* for -x (long timestamps) */
static pmLogLabel	label;

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "all", 0, 'a', 0, "dump everything" },
    { "descs", 0, 'd', 0, "dump metric descriptions" },
    { "insts", 0, 'i', 0, "dump instance domain descriptions" },
    { "", 0, 'L', 0, "more verbose form of archive label dump" },
    { "label", 0, 'l', 0, "dump the archive label" },
    { "metrics", 0, 'm', 0, "dump values of the metrics (default)" },
    PMOPT_NAMESPACE,
    { "reverse", 0, 'r', 0, "process archive in reverse chronological order" },
    PMOPT_START,
    { "sizes", 0, 's', 0, "report size of data records in archive" },
    PMOPT_FINISH,
    { "", 0, 't', 0, "dump the temporal index" },
    { "", 1, 'v', "FILE", "verbose hex dump of a physical file in raw format" },
    { "", 0, 'x', 0, "include date in reported timestamps" },
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static int overrides(int, pmOptions *);
static pmOptions opts = {
    .flags = PM_OPTFLAG_DONE | PM_OPTFLAG_STDOUT_TZ | PM_OPTFLAG_BOUNDARIES,
    .short_options = "aD:dilLmMn:rS:sT:tv:xZ:z?",
    .long_options = longopts,
    .short_usage = "[options] [archive [metricname ...]]",
    .override = overrides,
};

/*
 * return -1, 0 or 1 as the struct timeval's compare
 * a < b, a == b or a > b
 */
static int
tvcmp(struct timeval a, struct timeval b)
{
    if (a.tv_sec < b.tv_sec)
	return -1;
    if (a.tv_sec > b.tv_sec)
	return 1;
    if (a.tv_usec < b.tv_usec)
	return -1;
    if (a.tv_usec > b.tv_usec)
	return 1;
    return 0;
}

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
		    nbyte += PM_PDU_SIZE_BYTES(vsp->vlist[j].value.pval->vlen);
	    }
	}
    }

    return nbyte;
}

static void
setup_event_derived_metrics(void)
{
    int sts;

    if (pmid_flags == 0) {
	/*
	 * get PMID for event.flags and event.missed
	 * note that pmUnpackEventRecords() will have called
	 * __pmRegisterAnon(), so the anon metrics
	 * should now be in the PMNS
	 */
	char	*name_flags = "event.flags";
	char	*name_missed = "event.missed";

	if ((sts = pmLookupName(1, &name_flags, &pmid_flags)) < 0) {
	    /* should not happen! */
	    fprintf(stderr, "Warning: cannot get PMID for %s: %s\n",
		    name_flags, pmErrStr(sts));
	    /* avoid subsequent warnings ... */
	    __pmid_int(&pmid_flags)->item = 1;
	}
	sts = pmLookupName(1, &name_missed, &pmid_missed);
	if (sts < 0) {
	    /* should not happen! */
	    fprintf(stderr, "Warning: cannot get PMID for %s: %s\n",
		    name_missed, pmErrStr(sts));
	    /* avoid subsequent warnings ... */
	    __pmid_int(&pmid_missed)->item = 1;
	}
    }
}

static int
dump_nrecords(int nrecords, int nmissed)
{
    printf("%d", nrecords);
    if (nmissed > 0)
	printf(" (and %d missed)", nmissed);
    if (nrecords + nmissed == 1)
	printf(" event record\n");
    else
	printf(" event records\n");
    return 0;
}

static int
dump_nparams(int numpmid)
{
    if (numpmid == 0) {
	printf(" ---\n");
	printf("	          No parameters\n");
	return -1;
    }
    if (numpmid < 0) {
	printf(" ---\n");
	printf("	          Error: illegal number of parameters (%d)\n",
		numpmid);
	return -1;
    }
    return 0;
}

static void
dump_parameter(pmValueSet *xvsp, int index, int *flagsp)
{
    int		sts, flags = *flagsp;
    pmDesc	desc;
    char	**names;

    if ((sts = pmNameAll(xvsp->pmid, &names)) >= 0) {
	if (index == 0) {
	    if (xvsp->pmid == pmid_flags) {
		flags = *flagsp = xvsp->vlist[0].value.lval;
		printf(" flags 0x%x", flags);
		printf(" (%s) ---\n", pmEventFlagsStr(flags));
		free(names);
		return;
	    }
	    printf(" ---\n");
	}
	if ((flags & PM_EVENT_FLAG_MISSED) && index == 1 &&
	    (xvsp->pmid == pmid_missed)) {
	    printf("              ==> %d missed event records\n",
		    xvsp->vlist[0].value.lval);
	    free(names);
	    return;
	}
	printf("                %s (", pmIDStr(xvsp->pmid));
	__pmPrintMetricNames(stdout, sts, names, " or ");
	printf("):");
	free(names);
    }
    else
	printf("	            PMID: %s:", pmIDStr(xvsp->pmid));
    if ((sts = pmLookupDesc(xvsp->pmid, &desc)) < 0) {
	printf(" pmLookupDesc: %s\n", pmErrStr(sts));
	return;
    }
    printf(" value ");
    pmPrintValue(stdout, xvsp->valfmt, desc.type, &xvsp->vlist[0], 1);
    putchar('\n');
}

static void
dump_event(int numnames, char **names, pmValueSet *vsp, int index, int indom, int type)
{
    int		r;		/* event records */
    int		p;		/* event parameters */
    int		flags;
    int		nrecords;
    int		nmissed = 0;
    int		highres = (type == PM_TYPE_HIGHRES_EVENT);
    char	*iname;
    pmValue	*vp = &vsp->vlist[index];

    if (index > 0)
	printf("            ");
    printf("  %s (", pmIDStr(vsp->pmid));
    __pmPrintMetricNames(stdout, numnames, names, " or ");
    if (indom != PM_INDOM_NULL) {
	putchar('[');
	if (pmNameInDom(indom, vp->inst, &iname) < 0)
	    printf("%d or ???])", vp->inst);
	else {
	    printf("%d or \"%s\"])", vp->inst, iname);
	    free(iname);
	}
    }
    else {
	printf(")");
    }
    printf(": ");

    if (highres) {
	pmHighResResult	**hr;

	if ((nrecords = pmUnpackHighResEventRecords(vsp, index, &hr)) < 0)
	    return;
	if (nrecords == 0) {
	    printf("No event records\n");
	    pmFreeHighResEventResult(hr);
	    return;
	}
	setup_event_derived_metrics();

	for (r = 0; r < nrecords; r++) {
	    if (hr[r]->numpmid == 2 && hr[r]->vset[0]->pmid == pmid_flags &&
		(hr[r]->vset[0]->vlist[0].value.lval & PM_EVENT_FLAG_MISSED) &&
		hr[r]->vset[1]->pmid == pmid_missed) {
		nmissed += hr[r]->vset[1]->vlist[0].value.lval;
	    }
	}
	dump_nrecords(nrecords, nmissed);

	for (r = 0; r < nrecords; r++) {
	    printf("              --- event record [%d] timestamp ", r);
	    __pmPrintHighResStamp(stdout, &hr[r]->timestamp);
	    if (dump_nparams(hr[r]->numpmid) < 0)
		continue;
	    flags = 0;
	    for (p = 0; p < hr[r]->numpmid; p++)
		dump_parameter(hr[r]->vset[p], p, &flags);
	}
	pmFreeHighResEventResult(hr);
    }
    else {
	pmResult	**res;

	if ((nrecords = pmUnpackEventRecords(vsp, index, &res)) < 0)
	    return;
	if (nrecords == 0) {
	    printf("No event records\n");
	    pmFreeEventResult(res);
	    return;
	}
	setup_event_derived_metrics();

	for (r = 0; r < nrecords; r++) {
	    if (res[r]->numpmid == 2 && res[r]->vset[0]->pmid == pmid_flags &&
		(res[r]->vset[0]->vlist[0].value.lval & PM_EVENT_FLAG_MISSED) &&
		res[r]->vset[1]->pmid == pmid_missed) {
		nmissed += res[r]->vset[1]->vlist[0].value.lval;
	    }
	}
	dump_nrecords(nrecords, nmissed);

	for (r = 0; r < nrecords; r++) {
	    printf("              --- event record [%d] timestamp ", r);
	    __pmPrintStamp(stdout, &res[r]->timestamp);
	    if (dump_nparams(res[r]->numpmid) < 0)
		continue;
	    flags = 0;
	    for (p = 0; p < res[r]->numpmid; p++)
		dump_parameter(res[r]->vset[p], p, &flags);
	}
	pmFreeEventResult(res);
    }
}

static void
dump_metric(int numnames, char **names, pmValueSet *vsp, int index, int indom, int type)
{
    pmValue	*vp = &vsp->vlist[index];
    char	*iname;

    if (index == 0) {
	printf("  %s (", pmIDStr(vsp->pmid));
	__pmPrintMetricNames(stdout, numnames, names, " or ");
	printf("):");
	if (vsp->numval > 1) {
	    putchar('\n');
	    printf("               ");
	}
    }
    else
	printf("               ");

    if (indom != PM_INDOM_NULL) {
	printf(" inst [");
	if (pmNameInDom(indom, vp->inst, &iname) < 0)
	    printf("%d or ???]", vp->inst);
	else {
	    printf("%d or \"%s\"]", vp->inst, iname);
	    free(iname);
	}
    }
    printf(" value ");
    pmPrintValue(stdout, vsp->valfmt, type, vp, 1);
    putchar('\n');
}

static void
dump_result(pmResult *resp)
{
    int		i;
    int		j;
    int		n;
    char	**names;
    pmDesc	desc;

    if (sflag) {
	int		nbyte;
	nbyte = do_size(resp);
	printf("[%d bytes]\n", nbyte);
    }

    if (xflag) {
	char	       *ddmm;
	char	       *yr;

	ddmm = pmCtime(&resp->timestamp.tv_sec, timebuf);
	ddmm[10] = '\0';
	yr = &ddmm[20];
	printf("%s ", ddmm);
	__pmPrintStamp(stdout, &resp->timestamp);
	printf(" %4.4s", yr);
	if (xflag >= 2)
	    printf(" (%.6f)", __pmtimevalSub(&resp->timestamp, &label.ll_start));
    }
    else
	__pmPrintStamp(stdout, &resp->timestamp);

    if (resp->numpmid == 0) {
	printf("  <mark>\n");
	return;
    }

    for (i = 0; i < resp->numpmid; i++) {
	pmValueSet	*vsp = resp->vset[i];

	if (i > 0)
	    printf("            ");
	n = pmNameAll(vsp->pmid, &names);
	if (vsp->numval == 0) {
	    printf("  %s (", pmIDStr(vsp->pmid));
	    __pmPrintMetricNames(stdout, n, names, " or ");
	    printf("): No values returned!\n");
	    goto next;
	}
	else if (vsp->numval < 0) {
	    printf("  %s (", pmIDStr(vsp->pmid));
	    __pmPrintMetricNames(stdout, n, names, " or ");
	    printf("): %s\n", pmErrStr(vsp->numval));
	    goto next;
	}

	if (pmLookupDesc(vsp->pmid, &desc) < 0) {
	    /* don't know, so punt on the most common cases */
	    desc.indom = PM_INDOM_NULL;
	    if (vsp->valfmt == PM_VAL_INSITU)
		desc.type = PM_TYPE_32;
	    else
		desc.type = PM_TYPE_AGGREGATE;
	}

	for (j = 0; j < vsp->numval; j++) {
	    if (desc.type == PM_TYPE_EVENT ||
		desc.type == PM_TYPE_HIGHRES_EVENT)
		dump_event(n, names, vsp, j, desc.indom, desc.type);
	    else
		dump_metric(n, names, vsp, j, desc.indom, desc.type);
	}
next:
	if (n > 0)
	    free(names);
    }
}

static void
dumpDesc(__pmContext *ctxp)
{
    int			i;
    int			sts;
    char		**names;
    __pmHashNode	*hp;
    pmDesc		*dp;

    printf("\nDescriptions for Metrics in the Log ...\n");
    for (i = 0; i < ctxp->c_archctl->ac_log->l_hashpmid.hsize; i++) {
	for (hp = ctxp->c_archctl->ac_log->l_hashpmid.hash[i]; hp != NULL; hp = hp->next) {
	    dp = (pmDesc *)hp->data;
	    sts = pmNameAll(dp->pmid, &names);
	    if (sts < 0)
		printf("PMID: %s (%s)\n", pmIDStr(dp->pmid), "<noname>");
	    else {
		printf("PMID: %s (", pmIDStr(dp->pmid));
		__pmPrintMetricNames(stdout, sts, names, " or ");
		printf(")\n");
		free(names);
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
    off_t	meta_size = -1;		/* initialize to pander to gcc */
    off_t	log_size = -1;		/* initialize to pander to gcc */
    struct stat	sbuf;
    __pmLogTI	*tip;
    __pmLogTI	*lastp;
    __pmLogCtl  *lcp;
    
    lcp = ctxp->c_archctl->ac_log;

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
	    sprintf(path, "%s.meta", lcp->l_name);
	    if (stat(path, &sbuf) == 0)
		meta_size = sbuf.st_size;
	    else
		meta_size = -1;
	}
	if (lastp == NULL || tip->ti_vol != lastp->ti_vol) { 
	    sprintf(path, "%s.%d", lcp->l_name, tip->ti_vol);
	    if (stat(path, &sbuf) == 0)
		log_size = sbuf.st_size;
	    else {
		log_size = -1;
		printf("             Warning: file missing or compressed for log volume %d\n", tip->ti_vol);
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
	    if (tip->ti_stamp.tv_sec < lastp->ti_stamp.tv_sec ||
	        (tip->ti_stamp.tv_sec == lastp->ti_stamp.tv_sec &&
	         tip->ti_stamp.tv_usec < lastp->ti_stamp.tv_usec))
		printf("             Error: timestamp went backwards in time %d.%06d -> %d.%06d\n",
			(int)lastp->ti_stamp.tv_sec, (int)lastp->ti_stamp.tv_usec,
			(int)tip->ti_stamp.tv_sec, (int)tip->ti_stamp.tv_usec);
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
dumpLabel(int verbose)
{
    char	*ddmm;
    char	*yr;

    printf("Log Label (Log Format Version %d)\n", label.ll_magic & 0xff);
    printf("Performance metrics from host %s\n", label.ll_hostname);

    ddmm = pmCtime(&label.ll_start.tv_sec, timebuf);
    ddmm[10] = '\0';
    yr = &ddmm[20];
    printf("  commencing %s ", ddmm);
    __pmPrintStamp(stdout, &label.ll_start);
    printf(" %4.4s\n", yr);

    if (opts.finish.tv_sec == INT_MAX) {
	/* pmGetArchiveEnd() failed! */
	printf("  ending     UNKNOWN\n");
    }
    else {
	ddmm = pmCtime(&opts.finish.tv_sec, timebuf);
	ddmm[10] = '\0';
	yr = &ddmm[20];
	printf("  ending     %s ", ddmm);
	__pmPrintStamp(stdout, &opts.finish);
	printf(" %4.4s\n", yr);
    }

    if (verbose) {
	printf("Archive timezone: %s\n", label.ll_tz);
	printf("PID for pmlogger: %" FMT_PID "\n", label.ll_pid);
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
    fseek(f, (long)0, SEEK_SET);

    while ((sts = fread(&len, 1, sizeof(len), f)) == sizeof(len)) {
	len = ntohl(len);
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
	    printf("%02x", check & 0xff);
	}
	putchar('\n');
	if ((sts = fread(&check, 1, sizeof(check), f)) != sizeof(check)) {
	    if (sts == 0)
		printf("Unexpected EOF\n");
	    break;
	}
	check = ntohl(check);
	len += 2 * sizeof(len);
	if (check != len) {
	    printf("Trailer botch: %d != %d\n", check, len);
	    break;
	}
    }
    if (sts < 0)
	printf("fread fails: %s\n", osstrerror());
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

static int
overrides(int opt, pmOptions *opts)
{
    if (opt == 'a' || opt == 'L' || opt == 's' || opt == 't')
	return 1;
    return 0;
}

int
main(int argc, char *argv[])
{
    int			c;
    int			sts;
    char		*rawfile = NULL;
    int			i;
    int			ctxid;
    int			first = 1;
    int			dflag = 0;
    int			iflag = 0;
    int			Lflag = 0;
    int			lflag = 0;
    int			Mflag = 0;
    int			mflag = 0;
    int			tflag = 0;
    int			vflag = 0;
    int			mode = PM_MODE_FORW;
    __pmContext		*ctxp;
    pmResult		*raw_result;
    pmResult		*skel_result = NULL;
    pmResult		*result;
    struct timeval	done;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'a':	/* dump everything */
	    dflag = iflag = lflag = mflag = sflag = tflag = 1;
	    break;

	case 'd':	/* dump pmDesc structures */
	    dflag = 1;
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

	case 'M':	/* report <mark> records */
	    Mflag = 1;
	    break;

	case 'r':	/* read log in reverse chornological order */
	    mode = PM_MODE_BACK;
	    break;

	case 's':	/* report data size in log */
	    sflag = 1;
	    break;

	case 't':	/* dump temporal index */
	    tflag = 1;
	    break;

	case 'v':	/* verbose, dump in raw format */
	    vflag = 1;
	    rawfile = opts.optarg;
	    break;

	case 'x':	/* report Ddd Mmm DD <timestamp> YYYY */
			/* -xx reports numeric timeval also */
	    xflag++;
	    break;
	}
    }

    if (opts.errors ||
	(vflag && opts.optind != argc) ||
	(!vflag && opts.optind > argc - 1 && !opts.narchives)) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if (vflag) {
	FILE	*f;
	if ((f = fopen(rawfile, "r")) == NULL) {
	    fprintf(stderr, "%s: Cannot open \"%s\": %s\n", pmProgname, rawfile, osstrerror());
	    exit(1);
	}
	printf("Raw dump of physical archive file \"%s\" ...\n", rawfile);
	rawdump(f);
	exit(0);
    }

    if (dflag + iflag + lflag + mflag + tflag == 0)
	mflag = 1;	/* default */

    /* delay option end processing until now that we have the archive name */
    if (opts.narchives == 0)
	__pmAddOptArchive(&opts, argv[opts.optind++]);
    opts.flags &= ~PM_OPTFLAG_DONE;
    __pmEndOptions(&opts);

    if ((sts = ctxid = pmNewContext(PM_CONTEXT_ARCHIVE, opts.archives[0])) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, opts.archives[0], pmErrStr(sts));
	exit(1);
    }
    /* complete TZ and time window option (origin) setup */
    if (pmGetContextOptions(ctxid, &opts)) {
	pmflush();
	exit(1);
    }

    numpmid = argc - opts.optind;
    if (numpmid) {
	numpmid = 0;
	pmid = NULL;
	for (i = 0; opts.optind < argc; i++, opts.optind++) {
	    numpmid++;
	    pmid = (pmID *)realloc(pmid, numpmid * sizeof(pmID));
	    if ((sts = pmLookupName(1, &argv[opts.optind], &pmid[numpmid-1])) < 0) {
		if (sts == PM_ERR_NONLEAF) {
		    numpmid--;
		    if ((sts = pmTraversePMNS(argv[opts.optind], dometric)) < 0)
			fprintf(stderr, "%s: pmTraversePMNS(%s): %s\n",
				pmProgname, argv[opts.optind], pmErrStr(sts));
		}
		else
		    fprintf(stderr, "%s: pmLookupName(%s): %s\n",
			    pmProgname, argv[opts.optind], pmErrStr(sts));
		if (sts < 0)
		    numpmid--;
	    }
	}
	if (numpmid == 0) {
	    fprintf(stderr, "No metric names can be translated, dump abandoned\n");
	    exit(1);
	}
    }

    if ((sts = pmGetArchiveLabel(&label)) < 0) {
	fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmProgname, pmErrStr(sts));
	exit(1);
    }

    if (numpmid > 0) {
	/*
	 * setup dummy pmResult
	 */
	skel_result = (pmResult *)malloc(sizeof(pmResult)+(numpmid-1)*sizeof(pmValueSet *));
	if (skel_result == NULL) {
	    fprintf(stderr, "%s: malloc(skel_result): %s\n", pmProgname, osstrerror());
	    exit(1);

	}
    }

    /*
     * Note: ctxp->c_lock remains locked throughout ... __pmHandleToPtr()
     *       is only called once, and a single context is used throughout
     *       ... so there is no PM_UNLOCK(ctxp->c_lock) anywhere in the
     *       pmdumplog code.
     *       This works because ctxp->c_lock is a recursive lock and
     *       pmdumplog is single-threaded.
     */
    if ((ctxp = __pmHandleToPtr(ctxid)) == NULL) {
	fprintf(stderr, "%s: botch: __pmHandleToPtr(%d) returns NULL!\n",
		pmProgname, ctxid);
	exit(1);
    }

    pmSetMode(mode, &opts.start, 0);

    if (lflag)
	dumpLabel(Lflag);

    if (dflag)
	dumpDesc(ctxp);

    if (iflag)
	dumpInDom(ctxp);

    if (tflag)
	dumpTI(ctxp);

    if (mflag) {
	if (mode == PM_MODE_FORW) {
	    if (opts.start_optarg != NULL || opts.finish_optarg != NULL) {
		/* -S or -T */
		sts = pmSetMode(mode, &opts.start, 0);
		done = opts.finish;
	    }
	    else {
		/* read the whole archive */
		done.tv_sec = 0;
		done.tv_usec = 0;
		sts = pmSetMode(mode, &done, 0);
		done.tv_sec = INT_MAX;
	    }
	}
	else {
	    if (opts.start_optarg != NULL || opts.finish_optarg != NULL) {
		/* -S or -T */
		done.tv_sec = INT_MAX;
		done.tv_usec = 0;
		sts = pmSetMode(mode, &done, 0);
		done = opts.start;
	    }
	    else {
		/* read the whole archive backwards */
		done.tv_sec = INT_MAX;
		done.tv_usec = 0;
		sts = pmSetMode(mode, &done, 0);
		done.tv_sec = 0;
	    }
	}
	if (sts < 0) {
	    fprintf(stderr, "%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}
	sts = 0;
	for ( ; ; ) {
	    sts = __pmLogFetch(ctxp, 0, NULL, &raw_result);
	    if (sts < 0)
		break;
	    if (numpmid == 0 || (raw_result->numpmid == 0 && Mflag)) {
		/*
		 * want 'em all or <mark> record ...
		 */
		result = raw_result;
	    }
	    else if (numpmid > 0) {
		/*
		 * cherry pick from raw_result if pmid matches one
		 * of interest
		 */
		int	picked = 0;
		int	j;
		skel_result->timestamp = raw_result->timestamp;
		for (j = 0; j < numpmid; j++)
		    skel_result->vset[j] = NULL;
		for (i = 0; i < raw_result->numpmid; i++) {
		    for (j = 0; j < numpmid; j++) {
			if (pmid[j] == raw_result->vset[i]->pmid) {
			    skel_result->vset[j] = raw_result->vset[i];
			    picked++;
			    break;
			}
		    }
		}
		if (picked == 0) {
		    /* no metrics of interest, skip this record */
		    pmFreeResult(raw_result);
		    continue;
		}
		skel_result->numpmid = picked;
		if (picked != numpmid) {
		    /* did not find 'em all ... shuffle time */
		    int		j;
		    i = 0;
		    for (j = 0; j < numpmid; j++) {
			if (skel_result->vset[j] != NULL)
			    skel_result->vset[i++] = skel_result->vset[j];
		    }
		}
		result = skel_result;
	    }
	    else {
		/* not interesting */
		pmFreeResult(raw_result);
		continue;
	    }
	    if (first && mode == PM_MODE_BACK) {
		first = 0;
		printf("\nLog finished at %24.24s - dump in reverse order\n",
			pmCtime(&result->timestamp.tv_sec, timebuf));
	    }
	    if ((mode == PM_MODE_FORW && tvcmp(result->timestamp, done) > 0) ||
		(mode == PM_MODE_BACK && tvcmp(result->timestamp, done) < 0)) {
		sts = PM_ERR_EOL;
		break;
	    }
	    putchar('\n');
	    dump_result(result);
	    pmFreeResult(raw_result);
	}
	if (sts != PM_ERR_EOL) {
	    fprintf(stderr, "%s: pmFetch: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}
    }

    exit(0);
}
