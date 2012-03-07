/*
 * Copyright (c) 1995-2001,2003 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <limits.h>

static int	p_mid;		/* Print metric IDs of leaf nodes */
static int	p_fullmid;	/* Print verbose metric IDs of leaf nodes */
static int	p_desc;		/* Print descriptions for metrics */
static int	verify;		/* Only print error messages */
static int	p_oneline;	/* fetch oneline text? */
static int	p_help;		/* fetch help text? */
static int	p_value;	/* pmFetch and print value(s)? */
static int	p_force;	/* pmFetch and print value(s)? for non-enumerable indoms too */

static int	need_context;	/* set if need a pmapi context */
static int	need_pmid;	/* set if need to lookup names */
static int	type;
static char	*hostname;
static char	*pmnsfile = PM_NS_DEFAULT;
static int	dupok = 0;

static char	**namelist;
static pmID	*pmidlist;
static int	batchsize = 20;
static int	batchidx;

static char	*Oflag;		/* argument of -O flag */
static int	xflag;		/* for -x */
static int	zflag;		/* for -z */
static char 	*tz;		/* for -Z timezone */
static struct timeval 	start;	/* start of time window */

static void myeventdump(pmValueSet *, int);

/*
 * stolen from pmprobe.c ... cache all of the most recently requested
 * pmInDom ...
 */
static char *
lookup(pmInDom indom, int inst)
{
    static pmInDom	last = PM_INDOM_NULL;
    static int		numinst = -1;
    static int		*instlist;
    static char		**namelist;
    int			i;

    if (indom != last) {
	if (numinst > 0) {
	    free(instlist);
	    free(namelist);
	}
	numinst = pmGetInDom(indom, &instlist, &namelist);
	last = indom;
    }

    for (i = 0; i < numinst; i++) {
	if (instlist[i] == inst)
	    return namelist[i];
    }

    return NULL;
}

/* 
 * we only ever have one metric
 */
static void
mydump(pmDesc *dp, pmValueSet *vsp, char *indent)
{
    int		j;
    char	*p;

    if (indent != NULL)
	printf("%s", indent);
    if (vsp->numval == 0) {
	printf("No value(s) available!\n");
	return;
    }
    else if (vsp->numval < 0) {
	printf("Error: %s\n", pmErrStr(vsp->numval));
	return;
    }

    for (j = 0; j < vsp->numval; j++) {
	pmValue	*vp = &vsp->vlist[j];
	if (dp->indom != PM_INDOM_NULL) {
	    if ((p = lookup(dp->indom, vp->inst)) == NULL) {
		if (p_force) {
		    /* the instance disappeared; ignore it */
		    printf("    inst [%d \"%s\"]\n", vp->inst, "DISAPPEARED");
		    continue;
		}
		else {
		    /* report the error and give up */
		    printf("pmNameIndom: indom=%s inst=%d: %s\n",
			    pmInDomStr(dp->indom), vp->inst, pmErrStr(PM_ERR_INST));
		    printf("    inst [%d]", vp->inst);
		}
	    }
	    else
		printf("    inst [%d or \"%s\"]", vp->inst, p);
	}
	else
	    printf("   ");
	printf(" value ");
	pmPrintValue(stdout, vsp->valfmt, dp->type, vp, 1);
	putchar('\n');
	if (dp->type == PM_TYPE_EVENT && xflag)
	    myeventdump(vsp, j);
    }
}

static void
myeventdump(pmValueSet *vsp, int inst)
{
    int		r;		/* event records */
    int		p;		/* event parameters */
    int		nrecords;
    int		flags;
    pmResult	**res;
    static pmID	pmid_flags;
    static pmID	pmid_missed;

    nrecords = pmUnpackEventRecords(vsp, inst, &res);
    if (nrecords < 0) {
	fprintf(stderr, "pmUnpackEventRecords: %s\n", pmErrStr(nrecords));
	return;
    }

    if (pmid_flags == 0) {
	/*
	 * get PMID for event.flags and event.missed
	 * note that pmUnpackEventRecords() will have called
	 * __pmRegisterAnon(), so the anonymous metrics
	 * should now be in the PMNS
	 */
	char	*name_flags = "event.flags";
	char	*name_missed = "event.missed";
	int	sts;

	sts = pmLookupName(1, &name_flags, &pmid_flags);
	if (sts < 0) {
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

    for (r = 0; r < nrecords; r++) {
	printf("    --- event record [%d] timestamp ", r);
	__pmPrintStamp(stdout, &res[r]->timestamp);
	if (res[r]->numpmid == 0) {
	    printf(" ---\n");
	    printf("	No parameters\n");
	    continue;
	}
	if (res[r]->numpmid < 0) {
	    printf(" ---\n");
	    printf("	Error: illegal number of parameters (%d)\n",
			res[r]->numpmid);
	    continue;
	}
	flags = 0;
	for (p = 0; p < res[r]->numpmid; p++) {
	    pmValueSet	*xvsp = res[r]->vset[p];
	    int		sts;
	    pmDesc	desc;
	    char	*name;

	    if (pmNameID(xvsp->pmid, &name) >= 0) {
		if (p == 0) {
		    if (xvsp->pmid == pmid_flags) {
			flags = xvsp->vlist[0].value.lval;
			printf(" flags 0x%x", flags);
			printf(" (%s) ---\n", pmEventFlagsStr(flags));
			free(name);
			continue;
		    }
		    else
			printf(" ---\n");
		}
		if ((flags & PM_EVENT_FLAG_MISSED) &&
		    (p == 1) &&
		    (xvsp->pmid == pmid_missed)) {
		    printf("        ==> %d missed event records\n",
				xvsp->vlist[0].value.lval);
		    free(name);
		    continue;
		}
		printf("    %s (%s)\n", name, pmIDStr(xvsp->pmid));
		free(name);
	    }
	    else
		printf("	PMID: %s\n", pmIDStr(xvsp->pmid));
	    if ((sts = pmLookupDesc(xvsp->pmid, &desc)) < 0) {
		printf("	pmLookupDesc: %s\n", pmErrStr(sts));
		continue;
	    }
	    mydump(&desc, xvsp, "    ");
	}
    }
    if (nrecords >= 0)
	pmFreeEventResult(res);
}

static void
report(void)
{
    int		i;
    int		sts;
    pmDesc	desc;
    pmResult	*result = NULL;
    pmResult	*xresult = NULL;
    pmValueSet	*vsp = NULL;
    char	*buffer;
    int		all_count;
    int		*all_inst;
    char	**all_names;

    if (batchidx == 0)
	return;

    /* Lookup names. 
     * Cull out names that were unsuccessfully looked up. 
     * However, it is unlikely to fail because names come from a traverse PMNS. 
     */
    if (need_pmid) {
        if ((sts = pmLookupName(batchidx, namelist, pmidlist)) < 0) {
	    int j = 0;
	    for (i = 0; i < batchidx; i++) {
		if (pmidlist[i] == PM_ID_NULL) {
		    printf("%s: pmLookupName: %s\n", namelist[i], pmErrStr(sts));
		    free(namelist[i]);
		}
		else {
		    /* assert(j <= i); */
		    pmidlist[j] = pmidlist[i];
		    namelist[j] = namelist[i];
		    j++;
		}
	    }
	    batchidx = j;
	}
    }

    if (p_value || verify) {
	if (type == PM_CONTEXT_ARCHIVE) {
	    if ((sts = pmSetMode(PM_MODE_FORW, &start, 0)) < 0) {
		fprintf(stderr, "%s: pmSetMode failed: %s\n", pmProgname, pmErrStr(sts));
		exit(1);
	    }
	}
    	if ((sts = pmFetch(batchidx, pmidlist, &result)) < 0) {
	    for (i = 0; i < batchidx; i++)
		printf("%s: pmFetch: %s\n", namelist[i], pmErrStr(sts));
	    goto done;
	}
    }

    for (i = 0; i < batchidx; i++) {

	if (p_desc || p_value || verify) {
	    if ((sts = pmLookupDesc(pmidlist[i], &desc)) < 0) {
		printf("%s: pmLookupDesc: %s\n", namelist[i], pmErrStr(sts));
		continue;
	    }
	}

	if (p_desc || p_help || p_value)
	    /* Not doing verify, output separator  */
	    putchar('\n');


	if (p_value || verify) {
	    vsp = result->vset[i];
	    if (p_force) {
		if (result->vset[i]->numval == PM_ERR_PROFILE) {
		    /* indom is non-enumerable; try harder */
		    if ((all_count = pmGetInDom(desc.indom, &all_inst, &all_names)) > 0) {
			pmDelProfile(desc.indom, 0, NULL);
			pmAddProfile(desc.indom, all_count, all_inst);
			if (xresult != NULL) {
			    pmFreeResult(xresult);
			    xresult = NULL;
			}
			if (type == PM_CONTEXT_ARCHIVE) {
			    if ((sts = pmSetMode(PM_MODE_FORW, &start, 0)) < 0) {
				fprintf(stderr, "%s: pmSetMode failed: %s\n", pmProgname, pmErrStr(sts));
				exit(1);
			    }
			}
			if ((sts = pmFetch(1, &pmidlist[i], &xresult)) < 0) {
			    printf("%s: pmFetch: %s\n", namelist[i], pmErrStr(sts));
			    continue;
			}
			vsp = xresult->vset[0];
			/* leave the profile in the default state */
			free(all_inst);
			free(all_names);
			pmDelProfile(desc.indom, 0, NULL);
			pmAddProfile(desc.indom, 0, NULL);
		    }
		    else if (all_count == 0) {
			printf("%s: pmGetIndom: No instances?\n", namelist[i]);
			continue;
		    }
		    else {
			printf("%s: pmGetIndom: %s\n", namelist[i], pmErrStr(all_count));
			continue;
		    }
		}
	    }
	}

	if (verify) {
	    if (desc.type == PM_TYPE_NOSUPPORT)
		printf("%s: Not Supported\n", namelist[i]);
	    else if (vsp->numval < 0)
		printf("%s: %s\n", namelist[i], pmErrStr(vsp->numval));
	    else if (vsp->numval == 0)
		printf("%s: No value(s) available\n", namelist[i]);
	    continue;
	}
	else
	    /* not verify */
	    printf("%s", namelist[i]);

	if (p_mid)
	    printf(" PMID: %s", pmIDStr(pmidlist[i]));
	if (p_fullmid)
	    printf(" = %d = 0x%x", pmidlist[i], pmidlist[i]);

	if (p_oneline) {
	    if ((sts = pmLookupText(pmidlist[i], PM_TEXT_ONELINE, &buffer)) == 0) {
		if (p_fullmid)
		    printf("\n    ");
		else
		    putchar(' ');
		printf("[%s]", buffer);
		free(buffer);
	    }
	    else
		printf(" One-line Help: Error: %s\n", pmErrStr(sts));
	}
	putchar('\n');

	if (p_desc)
	    __pmPrintDesc(stdout, &desc);

	if (p_help) {
	    if ((sts = pmLookupText(pmidlist[i], PM_TEXT_HELP, &buffer)) == 0) {
		char	*p;
		for (p = buffer; *p; p++)
		    ;
		while (p > buffer && p[-1] == '\n') {
		    p--;
		    *p = '\0';
		}
		if (*buffer != '\0') {
		    printf("Help:\n");
		    printf("%s", buffer);
		    putchar('\n');
		}
		else
		    printf("Help: <empty entry>\n");
		free(buffer);
	    }
	    else
		printf("Full Help: Error: %s\n", pmErrStr(sts));
	}

	if (p_value) {
	    mydump(&desc, vsp, NULL);
	}
    }

    if (result != NULL) {
	pmFreeResult(result);
	result = NULL;
    }
    if (xresult != NULL) {
	pmFreeResult(xresult);
	xresult = NULL;
    }

done:
    for (i = 0; i < batchidx; i++)
	free(namelist[i]);
    batchidx = 0;
}

static void
dometric(const char *name)
{
    if (*name == '\0') {
	printf("PMNS appears to be empty!\n");
	return;
    }

    namelist[batchidx]= strdup(name);
    if (namelist[batchidx] == NULL) {
	fprintf(stderr, "%s: namelist string malloc: %s\n", pmProgname, osstrerror());
	exit(1);
    }

    batchidx++;
    if (batchidx >= batchsize)
	report();
}

static void
PrintUsage(void)
{
    fprintf(stderr,
"Usage: %s [options] [metricname ...]\n\
\n\
Options:\n\
  -a archive    metrics source is a PCP log archive\n\
  -b batchsize	fetch this many metrics at a time for -f or -v (default 20)\n\
  -c dmfile	load derived metric definitions from dmfile\n\
  -d		get and print metric description\n\
  -f		fetch and print value(s) for all instances\n\
  -F		fetch and print values for non-enumerable indoms too\n\
  -h host	metrics source is PMCD on host\n\
  -K spec	optional additional PMDA spec for local connection\n\
		spec is of the form op,domain,dso-path,init-routine\n\
  -L		metrics source is local connection to PMDA, no PMCD\n\
  -m		print PMID\n\
  -M		print PMID in verbose format\n\
  -n pmnsfile 	use an alternative PMNS\n\
  -N pmnsfile 	use an alternative PMNS (duplicate PMIDs are allowed)\n\
  -O time	origin for a fetch from the archive\n\
  -t		get and display (terse) oneline text\n\
  -T		get and display (verbose) help text\n\
  -v		verify mode, be quiet and only report errors\n\
		(forces other output control options off)\n\
  -x		like -f and expand event records\n\
  -Z timezone   set timezone for -O\n\
  -z            set timezone for -O to local time for host from -a\n",
		pmProgname);
}

static void
ParseOptions(int argc, char *argv[])
{
    int		c;
    int		sts;
    int		errflag = 0;
    char	*endnum;
    char	*errmsg;
    char	*opts = "a:b:c:dD:Ffh:K:LMmN:n:O:tTvxzZ:?";

    while ((c = getopt(argc, argv, opts)) != EOF) {
	switch (c) {

	    case 'a':	/* archive name */
		if (type != 0) {
		    fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		    errflag++;
		}
		type = PM_CONTEXT_ARCHIVE;
		hostname = optarg;
		need_context = 1;
		break;

	    case 'b':		/* batchsize */
		batchsize = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, "%s: -b requires numeric argument\n", pmProgname);
		    errflag++;
		}
		break;

	    case 'c':		/* derived metrics config file */
		sts = pmLoadDerivedConfig(optarg);
		if (sts < 0) {
		    fprintf(stderr, "%s: -c error: %s\n", pmProgname, pmErrStr(sts));
		    /* errors are not necessarily fatal ... */
		}
		break;

	    case 'd':
		p_desc = 1;
		need_context = 1;
		need_pmid = 1;
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

	    case 'F':
		p_force = p_value = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'f':
		p_value = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'h':	/* contact PMCD on this hostname */
		if (type != 0) {
		    fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		    errflag++;
		}
		hostname = optarg;
		type = PM_CONTEXT_HOST;
		need_context = 1;
		break;

	    case 'K':	/* update local PMDA table */
		if ((errmsg = __pmSpecLocalPMDA(optarg)) != NULL) {
		    fprintf(stderr, "%s: __pmSpecLocalPMDA failed: %s\n", pmProgname, errmsg);
		    errflag++;
		}
		break;

	    case 'L':	/* local PMDA connection, no PMCD */
		if (type != 0) {
		    fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		    errflag++;
		}
		hostname = NULL;
		type = PM_CONTEXT_LOCAL;
		need_context = 1;
		break;

	    case 'M':
		p_fullmid = 1;
		p_mid = 1;
		need_pmid = 1;
		break;

	    case 'm':
		p_mid = 1;
		need_pmid = 1;
		break;

	    case 'N':
		dupok = 1;
		/*FALLTHROUGH*/
	    case 'n':
		pmnsfile = optarg;
		break;

	    case 'O':		/* sample origin */
		Oflag = optarg;
		break;

	    case 'T':
		p_help = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 't':
		p_oneline = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'v':
		verify = 1;
		need_context = 1;
		need_pmid = 1;
		break;

	    case 'x':
		xflag = p_value = 1;
		need_context = 1;
		need_pmid = 1;
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
		if (errflag == 0) {
		    PrintUsage();
		    exit(0);
		}
	}
    }

    if (zflag && type == 0) {
	fprintf(stderr, "%s: -z requires an explicit -a or -h option\n", pmProgname);
	errflag++;
    }

    if (errflag) {
	PrintUsage();
	exit(1);
    }

    if (type != PM_CONTEXT_ARCHIVE && Oflag != NULL) {
	fprintf(stderr, "%s: Warning: -O option requires archive source, and will be ignored\n", pmProgname);
	Oflag = NULL;
    }

    if (type == PM_CONTEXT_ARCHIVE)
	/*
	 * for archives, one metric per batch and start at beginning of
	 * archive for each batch so metric will be found if it is in
	 * the archive
	 */
	batchsize = 1;

    if (verify)
	p_desc = p_mid = p_fullmid = p_help = p_oneline = p_value = p_force = 0;
}

/*****************************************************************************/

int
main(int argc, char **argv)
{
    int		sts;
    int		exitsts = 0;
    char	local[MAXHOSTNAMELEN];
    pmLogLabel	label;
    char	*host;
    char	*msg;
    struct timeval	first;		/* initial sample time */
    struct timeval	last;		/* final sample time */

    __pmSetProgname(argv[0]);

    ParseOptions(argc, argv);

    if ((namelist = (char **)malloc(batchsize * sizeof(char *))) == NULL) {
	fprintf(stderr, "%s: namelist malloc: %s\n", pmProgname, osstrerror());
	exit(1);
    }

    if ((pmidlist = (pmID *)malloc(batchsize * sizeof(pmID))) == NULL) {
	fprintf(stderr, "%s: pmidlist malloc: %s\n", pmProgname, osstrerror());
	exit(1);
    }

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((sts = pmLoadASCIINameSpace(pmnsfile, dupok)) < 0) {
	    fprintf(stderr, "%s: Error loading namespace: %s\n",
		pmProgname, pmErrStr(sts));
	    exit(1);
	}
    }
    else {
   	need_context = 1; /* atleast for PMNS */
    }

    if (need_context) {
	if (type == 0) {
	    type = PM_CONTEXT_HOST;
	    (void)gethostname(local, MAXHOSTNAMELEN);
	    local[MAXHOSTNAMELEN-1] = '\0';
	    hostname = local;
	}
	if ((sts = pmNewContext(type, hostname)) < 0) {
	    if (type == PM_CONTEXT_HOST)
		fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
			pmProgname, hostname, pmErrStr(sts));
	    else if (type == PM_CONTEXT_LOCAL)
		fprintf(stderr, "%s: Cannot make standalone connection on localhost: %s\n",
			pmProgname, pmErrStr(sts));
	    else
		fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
			pmProgname, hostname, pmErrStr(sts));
	    exit(1);
	}

	if (type == PM_CONTEXT_ARCHIVE) {
	    pmTrimNameSpace();
	    if ((sts = pmGetArchiveLabel(&label)) < 0) {
		fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		    pmProgname, pmErrStr(sts));
		exit(1);
	    }
	    first = label.ll_start;
	    host = label.ll_hostname;

	    if ((sts = pmGetArchiveEnd(&last)) < 0) {
		last.tv_sec = INT_MAX;
		last.tv_usec = 0;
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
		printf("Note: timezone set to local timezone of host \"%s\"\n\n", host);
	    }
	    else if (tz != NULL) {
		if ((sts = pmNewZone(tz)) < 0) {
		    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
			pmProgname, tz, pmErrStr(sts));
		    exit(1);
		}
		printf("Note: timezone set to \"TZ=%s\"\n\n", tz);
	    }
	    else {
		pmNewContextZone();
	    }

	    if (pmParseTimeWindow(NULL, NULL, NULL, Oflag,
				   &first, &last,
				   &last, &first, &start, &msg) < 0) {
		fprintf(stderr, "%s: %s", pmProgname, msg);
		exit(1);
	    }
	}
    }

    if (optind >= argc) {
    	sts = pmTraversePMNS("", dometric);
	if (sts < 0) {
	    fprintf(stderr, "Error: %s\n", pmErrStr(sts));
		exitsts = 1;
	}
    }
    else {
	int	a;
	for (a = optind; a < argc; a++) {
	    sts = pmTraversePMNS(argv[a], dometric);
	    if (sts < 0) {
		fprintf(stderr, "Error: %s: %s\n", argv[a], pmErrStr(sts));
		exitsts = 1;
	    }
	}
    }
    report();

    exit(exitsts);
}
