/*
 * pmprobe - light-weight pminfo for configuring monitor apps
 *
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
 */

#include <unistd.h>
#include "pmapi.h"
#include "impl.h"

static char	**namelist;
static pmID	*pmidlist;
static int	listsize;
static int	numpmid;
static int	*instlist;
static char	**instnamelist;
static int	type = 0;

static void
dometric(const char *name)
{
    if (numpmid >= listsize) {
	listsize = listsize == 0 ? 16 : listsize * 2;
	if ((pmidlist = (pmID *)realloc(pmidlist, listsize * sizeof(pmidlist[0]))) == NULL) {
	    __pmNoMem("realloc pmidlist", listsize * sizeof(pmidlist[0]), PM_FATAL_ERR);
	}
	if ((namelist = (char **)realloc(namelist, listsize * sizeof(namelist[0]))) == NULL) {
	    __pmNoMem("realloc namelist", listsize * sizeof(namelist[0]), PM_FATAL_ERR);
	}
    }

    namelist[numpmid]= strdup(name);
    if (namelist[numpmid] == NULL) {
	__pmNoMem("strdup name", strlen(name), PM_FATAL_ERR);
    }

    numpmid++;
}

static int
lookup(pmInDom indom)
{
    static pmInDom	last = PM_INDOM_NULL;
    static int		numinst;

    if (indom != last) {
	if (numinst > 0) {
	    free(instlist);
	    free(instnamelist);
	}
	if (type == PM_CONTEXT_ARCHIVE)
	    numinst = pmGetInDomArchive(indom, &instlist, &instnamelist);
	else
	    numinst = pmGetInDom(indom, &instlist, &instnamelist);
	last = indom;
    }
    return numinst;
}

extern int do_local_spec(const char *);

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		fetch_sts;
    int		numinst;
    int		fflag = 0;		/* -f for all instances from */
					/*    pmGetIndom or pmGetIndomArchive */
    int		iflag = 0;		/* -i for instance numbers */
    int		Iflag = 0;		/* -I for instance names */
    int		vflag = 0;		/* -v for values */
    int		Vflag = 0;		/* -V for verbose */
    int		zflag = 0;		/* -z for archive timezone */
    int		errflag = 0;
    char	*host = NULL;
    char	*msg;
    pmLogLabel	label;			/* get hostname for archives */
    struct timeval	start;		/* start of sample window */
    struct timeval	first;		/* initial sample time */
    struct timeval	last;		/* final sample time */
    char	*pmnsfile = PM_NS_DEFAULT;
    char	*Oflag = NULL;		/* -O for archive offset */
    char	*tz = NULL;
    int		i;
    pmResult	*result;
    pmValueSet	*vsp;
    pmDesc	desc;
    char        *opts = "a:D:fh:IiK:Ln:O:VvZ:z?";

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, opts)) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
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

	case 'f':	/* pmGetIndom or pmGetInDomArchive for instances with -i or -I */
	    fflag++;
	    break;

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'i':	/* report internal instance numbers */
	    if (vflag) {
		fprintf(stderr, "%s: at most one of -i and -v allowed\n", pmProgname);
		errflag++;
	    }
	    iflag++;
	    break;

	case 'I':	/* report external instance names */
	    if (vflag) {
		fprintf(stderr, "%s: at most one of -I and -v allowed\n", pmProgname);
		errflag++;
	    }
	    Iflag++;
	    break;

	case 'K':	/* update local PMDA table */
	    if ((msg = __pmSpecLocalPMDA(optarg)) != NULL) {
		fprintf(stderr, "%s: __pmSpecLocalPMDA failed\n%s\n", pmProgname, msg);
		errflag++;
	    }
	    break;

	case 'L':	/* local PMDA connection, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h and -L allowed\n", pmProgname);
		errflag++;
	    }
	    host = NULL;
	    type = PM_CONTEXT_LOCAL;
	    break;

	case 'n':	/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 'O':	/* sample origin */
	    Oflag = optarg;
	    break;

	case 'V':	/* verbose */
	    Vflag++;
	    break;

	case 'v':	/* cheap values */
	    if (iflag || Iflag) {
		fprintf(stderr, "%s: at most one of -v and (-i or -I) allowed\n", pmProgname);
		errflag++;
	    }
	    vflag++;
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

    if (type != PM_CONTEXT_ARCHIVE && zflag) {
	fprintf(stderr, "%s: -z requires an archive source\n", pmProgname);
	errflag++;
    }

    if (type != PM_CONTEXT_ARCHIVE && Oflag != NULL) {
	fprintf(stderr, "%s: -O requires an archive source\n", pmProgname);
	errflag++;
    }


    if (errflag) {
	fprintf(stderr,
"Usage: %s [options] [metricname ...]\n\
\n\
Options:\n\
  -a archive   metrics source is a PCP log archive\n\
  -f           report all instances from pmGetIndom or pmGetInDomArchive\n\
	       (with -i or -I)\n\
               [default is to report instances from pmFetch]\n\
  -h host      metrics source is PMCD on host\n\
  -I           list external instance names\n\
  -i           list internal instance numbers\n\
  -K spec      optional additional PMDA spec for local connection\n\
               spec is of the form op,domain,dso-path,init-routine\n\
  -L           metrics source is local connection to PMDA, no PMCD\n\
  -n pmnsfile  use an alternative PMNS\n\
  -O time      origin for a fetch from the archive\n\
  -V           report PDU operations (verbose)\n\
  -v           list metric values\n\
  -Z timezone  set timezone for -O\n\
  -z           set timezone for -O to local time for host from -a\n",
		pmProgname);
	exit(1);
    }

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((sts = pmLoadNameSpace(pmnsfile)) < 0) {
	    printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, pmnsfile, pmErrStr(sts));
	    exit(1);
	}
    }

    if (type == 0) {
	type = PM_CONTEXT_HOST;
	host = "local:";
    }
    if ((sts = pmNewContext(type, host)) < 0) {
	if (type == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	else if (type == PM_CONTEXT_LOCAL)
	    fprintf(stderr, "%s: Cannot make standalone connection on localhost: %s\n",
		    pmProgname, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    if (type == PM_CONTEXT_ARCHIVE) {
	if ((sts = pmGetArchiveLabel(&label)) < 0) {
	    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmProgname, pmErrStr(sts));
	    exit(1);
	}
	first = label.ll_start;
	if ((sts = pmGetArchiveEnd(&last)) < 0) {
	    last.tv_sec = INT_MAX;
	    last.tv_usec = 0;
	}
	if (zflag) {
	    if ((sts = pmNewContextZone()) < 0) {
		fprintf(stderr, "%s: Cannot set context timezone: %s\n",
			pmProgname, pmErrStr(sts));
		exit(1);
	    }
	}
	else if (tz != NULL) {
	    if ((sts = pmNewZone(tz)) < 0) {
		fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
			pmProgname, tz, pmErrStr(sts));
		exit(1);
	    }
	}
	else
	    pmNewContextZone();

	if (pmParseTimeWindow(NULL, NULL, NULL, Oflag,
				   &first, &last,
				   &last, &first, &start, &msg) < 0) {
	    fprintf(stderr, "%s: %s", pmProgname, msg);
	    exit(1);
	}
    }

    if (optind >= argc) {
	pmTraversePMNS("", dometric);
    }
    else {
	int	a;
	for (a = optind; a < argc; a++) {
	    sts = pmTraversePMNS(argv[a], dometric);
	    if (sts < 0)
		printf("%s %d %s\n", argv[a], sts, pmErrStr(sts));
	}
    }

    /* Lookup names.
     * Cull out names that were unsuccessfully looked up.
     * However, it is unlikely to fail because names come from a traverse PMNS.
     */
    if ((sts = pmLookupName(numpmid, namelist, pmidlist)) < 0) {
        int i, j = 0;
	for (i = 0; i < numpmid; i++) {
	    if (pmidlist[i] == PM_ID_NULL) {
		printf("%s %d %s\n", namelist[i], sts, pmErrStr(sts));
		free(namelist[i]);
	    }
	    else {
		/* assert(j <= i); */
		pmidlist[j] = pmidlist[i];
		namelist[j] = namelist[i];
		j++;
	    }	
	}
	numpmid = j;
    }

    fetch_sts = 0;
    for (i = 0; i < numpmid; i++) {
	printf("%s ", namelist[i]);

	if (iflag || Iflag || vflag) {
	    if ((sts = pmLookupDesc(pmidlist[i], &desc)) < 0) {
		printf("%d %s (pmLookupDesc)\n", sts, pmErrStr(sts));
		continue;
	    }
	}

	if (fflag && (iflag || Iflag)) {
	    /*
	     * must be -i or -I with -f ... don't even fetch a result
	     * with pmFetch, just go straight to the instance domain with
	     * pmGetInDom or pmGetInDomArchive
	     */
	    if (desc.indom == PM_INDOM_NULL) {
		printf("1 PM_IN_NULL");
		if ( iflag && Iflag )
		    printf(" PM_IN_NULL");
	    }
	    else {
		if ((numinst = lookup(desc.indom)) < 0) {
		    printf("%d %s (pmGetInDom)", numinst, pmErrStr(numinst));
		}
		else {
		    int		j;
		    printf("%d", numinst);
		    for (j = 0; j < numinst; j++) {
			if (iflag)
			    printf(" ?%d", instlist[j]);
			if (Iflag)
			    printf(" \"%s\"", instnamelist[j]);
		    }
		}
	    }
	    putchar('\n');
	    continue;
	}

	if (type == PM_CONTEXT_ARCHIVE) {
	    /*
	     * merics from archives are fetched one at a time, otherwise
	     * get them all at once
	     */
	    if ((sts = pmSetMode(PM_MODE_FORW, &start, 0)) < 0) {
		printf("%d %s (pmSetMode)\n", sts, pmErrStr(sts));
		continue;
	    }
	    fetch_sts = pmFetch(1, &pmidlist[i], &result);
	}
	else {
	    if (i == 0)
		fetch_sts = pmFetch(numpmid, pmidlist, &result);
	}

	if (fetch_sts < 0) {
	    printf("%d %s (pmFetch)", fetch_sts, pmErrStr(fetch_sts));
	}
	else {
	    if (type == PM_CONTEXT_ARCHIVE)
	    	vsp = result->vset[0];
	    else
	    	vsp = result->vset[i];

	    if (vsp->numval < 0) {
		printf("%d %s", vsp->numval, pmErrStr(vsp->numval));
	    }
	    else if (vsp->numval == 0) {
		printf("0");
		;
	    }
	    else if (vsp->numval > 0) {
		printf("%d", vsp->numval);
		if (vflag) {
		    int		j;
		    for (j = 0; j < vsp->numval; j++) {
			pmValue	*vp = &vsp->vlist[j];
			putchar(' ');
			pmPrintValue(stdout, vsp->valfmt, desc.type, vp, 1);
		    }
		}
		if (iflag || Iflag) {
		    /*
		     * must be without -f
		     * get instance domain for reporting to minimize PDU
		     * round trips ... state should be the same as of the
		     * pmResult, so each instance in the pmResult should be
		     * found by pmGetInDom or pmGetInDomArchive
		     */
		    int		j;
		    if (desc.indom == PM_INDOM_NULL) {
			printf(" PM_IN_NULL");
			if ( iflag && Iflag )
			    printf(" PM_IN_NULL");
			putchar ('\n');
			continue;
		    }
		    if ((numinst = lookup(desc.indom)) < 0) {
			printf("%d %s (pmGetInDom)\n", numinst, pmErrStr(numinst));
			continue;
		    }
		    for (j = 0; j < vsp->numval; j++) {
			pmValue		*vp = &vsp->vlist[j];
			if (iflag)
			    printf(" ?%d", vp->inst);
			if (Iflag) {
			    int		k;
			    for (k = 0; k < numinst; k++) {
				if (instlist[k] == vp->inst)
				    break;
			    }
			    if (k < numinst)
				printf(" \"%s\"", instnamelist[k]);
			    else
				printf(" ?%d", vp->inst);
			}
		    }
		}
	    }
	}
	putchar('\n');
    }

   if (Vflag) {
	int j;

	/* Warning: 16 is magic and from libpcp/src/pdu.c */
	printf("PDUs send");
	j = 0;
	for (i = 0; i < 16; i++) {
	    printf(" %3d", __pmPDUCntOut[i]);
	    j += __pmPDUCntOut[i];
	}
	printf("\nTotal: %d\n", j);

	printf("PDUs recv");
	j = 0;
	for (i = 0; i < 16; i++) {
	    printf(" %3d",__pmPDUCntIn[i]);
	    j += __pmPDUCntIn[i];
	}
	printf("\nTotal: %d\n", j);
    }

    exit(0);
}
