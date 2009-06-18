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
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * check help files build by newhelp
 *
 * Usage:
 *    chkhelp helpfile metric-name ...
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#define VERSION 2
static int	version = VERSION;

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

    if (pi->pad == 0) {
	*ident = (int)pmid;
	*type = 1;
    }
    else {
	pi->pad = 0;
	*ident = (int)pmid;
	*type = 2;
    }

    return 1;
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
    int		allpmid = 0;
    int		allindom = 0;
    char	*filename;
    int		handle;
    char	*tp;
    char	*name;
    int		id;
    int		next_type;
    char	*endnum;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:Hin:Opv:?")) != EOF) {
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

    if (aflag && optind != argc - 1) {
	fprintf(stderr, "%s: metric-name arguments cannot be used with -i or -p\n\n",
	    pmProgname);
	errflag = 1;
    }

    if (aflag == 0 && optind == argc-1 && oneline+help != 0) {
	fprintf(stderr, "%s: -O or -H require metric-name arguments or -i or -p\n\n",
	    pmProgname);
	errflag = 1;
    }

    if (errflag || optind >= argc) {
	fprintf(stderr,
"Usage: %s helpfile\n"
"       %s [options] helpfile [metricname ...]\n"
"\n"
"Options:\n"
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

    if ((sts = pmLoadNameSpace(pmnsfile)) < 0) {
	fprintf(stderr, "pmLoadNameSpace: %s\n", pmErrStr(sts));
	exit(1);
    }

    if (help + oneline == 0 && (optind < argc || aflag))
	/* if metric names, -p or -i => -O is default */
	oneline = 1;

    if (optind == argc && aflag == 0)
	/* no metric names, process all entries */
	aflag = 1;

    while (optind < argc || aflag) {
	if (aflag) {
	    if (next(&id, &next_type) == 0)
		break;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0 && allindom+allpmid == 0)
		fprintf(stderr, "next_type=%d id=0x%x\n", next_type, id);
#endif
	    if (next_type == 2) {
		if (!allindom)
		    continue;
		printf("\nInDom %s:", pmInDomStr((pmInDom)id));
	    }
	    else {
		char		*p;
		if (!allpmid)
		    continue;

		printf("\nPMID %s", pmIDStr((pmID)id));
		sts = pmNameID(id, &p);
		if (sts == 0) {
		    printf(" %s", p);
		    free(p);
		}
		putchar(':');
	    }
	}
	else {
	    next_type = 1;
	    name = argv[optind++];
	    printf("\nPMID");
	    if ((sts = pmLookupName(1, &name, (pmID *)&id)) < 0) {
		printf(" %s: %s\n", name, pmErrStr(sts));
		continue;
	    }
	    printf(" %s %s:", pmIDStr((pmID)id), name);
	    if (id == PM_ID_NULL) {
		printf(" unknown metric\n");
		continue;
	    }
	}

	if (oneline) {
	    if (next_type == 1)
		tp = pmdaGetHelp(handle, (pmID)id, PM_TEXT_ONELINE);
	    else
		tp = pmdaGetInDomHelp(handle, (pmInDom)id, PM_TEXT_ONELINE);
	    if (tp != NULL)
		printf(" %s\n", tp);
	    else
		putchar('\n');
	}
	else
	    putchar('\n');

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
