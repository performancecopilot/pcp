/*
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * 
 * check help files build by newhelp
 *
 * Usage:
 *    chkhelp helpfile metricname ...
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#define VERSION 2
static int	version = VERSION;

static int	handle;

/*
 * Note: these two are from libpcp_pmda/src/help.c
 */
typedef struct {
    pmID	pmid;
    __uint32_t	off_oneline;
    __uint32_t	off_text;
} help_idx_t;

typedef struct {
    int		dir_fd;
    int		pag_fd;
    int		numidx;
    help_idx_t	*index;
    char	*text;
    int		textlen;
} help_t;

static int
next(int *ident, int *type)
{
    static help_t	*hp = NULL;
    static int		nextidx;
    pmID		pmid;
    __pmID_int		*pi = (__pmID_int *)&pmid;
    extern void		*__pmdaHelpTab(void);

    if (hp == NULL) {
	hp = (help_t *)__pmdaHelpTab();
	/*
	 * Note, skip header and version info at index[0]
	 */
	nextidx = 1;
    }

    if (nextidx > hp->numidx)
	return 0;

    pmid = hp->index[nextidx].pmid;
    nextidx++;

    if (pi->flag == 0) {
	/* real PMID */
	*ident = (int)pmid;
	*type = 1;
    }
    else {
        /* special hack, this is encoding a domain id, not a PMID */
	pi->flag = 0;
	*ident = (int)pmid;
	*type = 2;
    }

    return 1;
}


/*
 * with -e come here for every metric in the PMNS ...
 */
void
dometric(const char *name)
{
    int		sts;
    pmID	pmid;
    char	*tp;

    sts = pmLookupName(1, (char **)&name, &pmid);
    if (sts < 0) {
	fprintf(stderr, "pmLookupName: failed for \"%s\": %s\n", name, pmErrStr(sts));
	return;
    }
    if (sts == 0) {
	fprintf(stderr, "pmLookupName: failed for \"%s\"\n", name);
	return;
    }

    tp = pmdaGetHelp(handle, pmid, PM_TEXT_ONELINE);
    if (tp != NULL)
	return;
    tp = pmdaGetHelp(handle, pmid, PM_TEXT_HELP);
    if (tp != NULL)
	return;

    /* no help text, report metric */
    printf("%s\n", name);
}

int
main(int argc, char **argv)
{
    int		sts;
    int		c;
    int		help = 0;
    int		oneline = 0;
    char	*pmnsfile = PM_NS_DEFAULT;
    int		errflag = 0;
    int		aflag = 0;
    int		eflag = 0;
    int		allpmid = 0;
    int		allindom = 0;
    char	*filename;
    char	*tp;
    char	*name;
    int		id;
    int		next_type;
    char	*endnum;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:eHin:Opv:?")) != EOF) {
	switch (c) {

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

	case 'e':	/* help text exists? */
	    eflag = 1;
	    break;

	case 'H':	/* help text */
	    help = 1;
	    break;

	case 'i':
	    aflag++;
	    allindom = 1;
	    break;

	case 'n':	/* alternative namespace file */
	    pmnsfile = optarg;
	    break;

	case 'O':	/* oneline text */
	    oneline = 1;
	    break;

	case 'p':
	    aflag++;
	    allpmid = 1;
	    break;

	case 'v':	/* version 2 only these days */
	    version = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -v requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    if (version != 2) {
		fprintf(stderr 
                       ,"%s: deprecated option - only version 2 is supported\n"
                       , pmProgname);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (optind == argc) {
	fprintf(stderr, "%s: missing helpfile argument\n\n", pmProgname);
	errflag = 1;
    }

    if (aflag && optind < argc-1) {
	fprintf(stderr, "%s: metricname arguments cannot be used with -i or -p\n\n",
	    pmProgname);
	errflag = 1;
    }

    if (aflag == 0 && optind == argc-1 && oneline+help != 0) {
	fprintf(stderr, "%s: -O or -H require metricname arguments or -i or -p\n\n",
	    pmProgname);
	errflag = 1;
    }

    if (eflag && (allpmid || allindom)) {
	fprintf(stderr, "%s: -e cannot be used with -i or -p\n\n",
	    pmProgname);
	errflag = 1;
    }

    if (errflag || optind >= argc) {
	fprintf(stderr,
"Usage: %s helpfile\n"
"       %s [options] helpfile [metricname ...]\n"
"\n"
"Options:\n"
"  -e           exists check, only report metrics with no help text\n"
"  -H           display verbose help text\n"
"  -i           process all the instance domains\n"
"  -n pmnsfile  use an alternative PMNS\n"
"  -O           display the one line help summary\n"
"  -p           process all the metrics (PMIDs)\n"
"  -v version   deprecated (only version 2 format supported)\n"
"\n"
"No options implies silently check internal integrity of the helpfile.\n",
		pmProgname, pmProgname);
	exit(1);
    }

    filename = argv[optind++];
    if ((handle = pmdaOpenHelp(filename)) < 0) {
	fprintf(stderr, "pmdaOpenHelp: failed to open \"%s\": ", filename);
	if (handle == -EINVAL)
	    fprintf(stderr, "Bad format, not version %d PCP help text\n", version);
	else
	    fprintf(stderr, "%s\n", pmErrStr(handle));
	exit(1);
    }

    if ((sts = pmLoadASCIINameSpace(pmnsfile, 1)) < 0) {
	fprintf(stderr, "pmLoadASCIINameSpace(%s, 1): %s\n", pmnsfile, pmErrStr(sts));
	exit(1);
    }

    if (help + oneline == 0 && (optind < argc || aflag))
	/* if metric names, -p or -i => -O is default */
	oneline = 1;

    if (optind == argc && aflag == 0)
	/* no metric names, process all entries */
	aflag = 1;

    if (eflag) {
	if (optind == argc)
	    sts = pmTraversePMNS("", dometric);
	    if (sts < 0)
		fprintf(stderr, "Error: pmTraversePMNS(\"\", ...): %s\n", pmErrStr(sts));
	else {
	    for ( ; optind < argc; optind++) {
		sts = pmTraversePMNS(argv[optind], dometric);
		if (sts < 0)
		    fprintf(stderr, "Error: pmTraversePMNS(\"%s\", ...): %s\n", argv[optind], pmErrStr(sts));
	    }
	}
	exit(0);
    }

    while (optind < argc || aflag) {
	if (aflag) {
	    if (next(&id, &next_type) == 0)
		break;
#ifdef PCP_DEBUG
	    if ((pmDebug & DBG_TRACE_APPL0) && allindom+allpmid == 0)
		fprintf(stderr, "next_type=%d id=0x%x\n", next_type, id);
#endif
	    if (next_type == 2) {
		if (!allindom)
		    continue;
		printf("\nInDom %s:", pmInDomStr((pmInDom)id));
	    }
	    else {
		char		**names;
		if (!allpmid)
		    continue;

		printf("\nPMID %s", pmIDStr((pmID)id));
		sts = pmNameAll(id, &names);
		if (sts > 0) {
		    printf(" ");
		    __pmPrintMetricNames(stdout, sts, names, " or ");
		    free(names);
		}
		putchar(':');
	    }
	}
	else {
	    next_type = 1;
	    name = argv[optind++];
	    if ((sts = pmLookupName(1, &name, (pmID *)&id)) < 0) {
		printf("\n%s: %s\n", name, pmErrStr(sts));
		continue;
	    }
	    if (id == PM_ID_NULL) {
		printf("\n%s: unknown metric\n", name);
		continue;
	    }
	    printf("\nPMID %s %s:", pmIDStr((pmID)id), name);
	}

	if (oneline) {
	    if (next_type == 1)
		tp = pmdaGetHelp(handle, (pmID)id, PM_TEXT_ONELINE);
	    else
		tp = pmdaGetInDomHelp(handle, (pmInDom)id, PM_TEXT_ONELINE);
	    if (tp != NULL)
		printf(" %s", tp);
	    putchar('\n');
	}

	if (help) {
	    if (next_type == 1)
		tp = pmdaGetHelp(handle, (pmID)id, PM_TEXT_HELP);
	    else
		tp = pmdaGetInDomHelp(handle, (pmInDom)id, PM_TEXT_HELP);
	    if (tp != NULL && *tp)
		printf("%s\n", tp);
	}

    }

    return 0;
}
