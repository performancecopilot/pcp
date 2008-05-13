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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: pminfo.c,v 1.4 2003/02/20 05:28:13 kenmcd Exp $"

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <limits.h>
#include "pmapi.h"
#include "impl.h"

extern int	errno;

static int	p_mid = 0;		/* Print metric IDs of leaf nodes */
static int	p_fullmid = 0;		/* Print verbose metric IDs of leaf nodes */
static int	p_desc = 0;		/* Print descriptions for metrics */
static int	verify = 0;		/* Only print error messages */
static int	p_oneline = 0;		/* fetch oneline text? */
static int	p_help = 0;		/* fetch help text? */
static int	p_value = 0;		/* pmFetch and print value(s)? */
static int	p_force = 0;		/* pmFetch and print value(s)? for non-enumerable indoms too */

static int	need_context = 0;	/* set if need a pmapi context */
static int	need_pmid = 0;		/* set if need to lookup names */
static int	type = 0;
static char	*hostname;
static char	*pmnsfile = PM_NS_DEFAULT;

static char	**namelist;
static pmID	*pmidlist;
static int	batchsize = 20;
static int	batchidx = 0;

static char	*Oflag = NULL;		/* argument of -O flag */
static int	zflag = 0;		/* for -z */
static char 	*tz = NULL;		/* for -Z timezone */
static struct timeval 	start;		/* start of time window */

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
void
mydump(pmDesc *dp, pmValueSet *vsp)
{
    int		j;
    char	*p;

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
    }
}

static void
report(void)
{
    int		i;
    int		sts;
    pmDesc	desc;
    pmResult	*result = NULL;
    pmResult	*xresult = NULL;
    pmValueSet	*vsp;
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
	    mydump(&desc, vsp);
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
	fprintf(stderr, "%s: namelist string malloc: %s\n", pmProgname, strerror(errno));
	exit(1);
    }

    batchidx++;
    if (batchidx >= batchsize)
	report();
}

extern int optind;

static void
PrintUsage(void)
{
    fprintf(stderr,
"Usage: %s [options] [metricname ...]\n\
\n\
Options:\n\
  -a archive    metrics source is a PCP log archive\n\
  -b batchsize	fetch this many metrics at a time for -f or -v (default 20)\n\
  -d		get and print metric description\n\
  -f		fetch and print value(s) for all instances\n\
  -F		fetch and print values for non-enumerable indoms too\n\
  -h host	metrics source is PMCD on host\n"
#ifdef PM_USE_CONTEXT_LOCAL
"  -L		metrics source is local, no PMCD\n"
#endif
"  -m		print PMID\n\
  -M		print PMID in verbose format\n\
  -n pmnsfile 	use an alternative PMNS\n\
  -O time	origin for a fetch from the archive\n\
  -t		get and display (terse) oneline text\n\
  -T		get and display (verbose) help text\n\
  -v		verify mode, be quiet and only report errors\n\
		(forces other output control options off)\n\
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
#ifdef PM_USE_CONTEXT_LOCAL
    char	*opts = "a:b:dD:Ffn:h:LMmO:tTvzZ:?";
#else
    char	*opts = "a:b:dD:Ffn:h:MmO:tTvzZ:?";
#endif
    extern char	*optarg;
    extern int	pmDebug;

    while ((c = getopt(argc, argv, opts)) != EOF) {
	switch (c) {

	    case 'a':	/* archive name */
		if (type != 0) {
#ifdef PM_USE_CONTEXT_LOCAL
		    fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
#else
		    fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmProgname);
#endif
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
#ifdef PM_USE_CONTEXT_LOCAL
		    fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
#else
		    fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmProgname);
#endif
		    errflag++;
		}
		hostname = optarg;
		type = PM_CONTEXT_HOST;
		need_context = 1;
		break;

#ifdef PM_USE_CONTEXT_LOCAL
	    case 'L':
		if (type != 0) {
		    fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		    errflag++;
		}
		hostname = NULL;
		type = PM_CONTEXT_LOCAL;
		need_context = 1;
		break;
#endif

	    case 'M':
		p_fullmid = 1;
		p_mid = 1;
		need_pmid = 1;
		break;

	    case 'm':
		p_mid = 1;
		need_pmid = 1;
		break;

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
    char	*p;
    pmLogLabel	label;
    char	*host;
    char	*msg;
    struct timeval	first;		/* initial sample time */
    struct timeval	last;		/* final sample time */
    int		tzh;			/* initial timezone handle */

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; pmProgname && *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    ParseOptions(argc, argv);

    if ((namelist = (char **)malloc(batchsize * sizeof(char *))) == NULL) {
	fprintf(stderr, "%s: namelist malloc: %s\n", pmProgname, strerror(errno));
	exit(1);
    }

    if ((pmidlist = (pmID *)malloc(batchsize * sizeof(pmID))) == NULL) {
	fprintf(stderr, "%s: pmidlist malloc: %s\n", pmProgname, strerror(errno));
	exit(1);
    }

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((sts = pmLoadNameSpace(pmnsfile)) < 0) {
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
#ifdef PM_USE_CONTEXT_LOCAL
	    else if (type == PM_CONTEXT_LOCAL)
		fprintf(stderr, "%s: Cannot make standalone connection on localhost: %s\n",
			pmProgname, pmErrStr(sts));
#endif
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
    /*NOTREACHED*/
}
