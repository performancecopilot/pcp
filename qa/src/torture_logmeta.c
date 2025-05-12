/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

int
main(int argc, char **argv)
{
    int		c;
    int		i;
    int		sts = 0;			/* pander to gcc */
    int		fault = 0;
    int		errflag = 0;
    char	*offset = NULL;
    pmLogLabel	label;				/* get hostname for archives */
    int		tzh;				/* initial timezone handle */
    char	local[MAXHOSTNAMELEN];
    char	*endnum;
    struct timespec startTime;
    struct timespec endTime;
    struct timespec appStart;
    struct timespec appEnd;
    struct timespec appOffset;
    int		*instlist;
    char	**namelist;
    char	*name;
    int		xinst = -1;
    char	*xname = NULL;
    char	*xxname = NULL;
    pmInDom	indom[3];	/* null, good, bad */

    /* trim cmd name of leading directory components */
    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "fD:O:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'f':	/* fault injection mode */
	    fault = 1;
	    break;

	case 'O':	/* sample offset time */
	    offset = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc-4) {
	fprintf(stderr,
"Usage: %s [options] archive domain good-serial bad-serial\n\
\n\
Options:\n\
  -f             fault injection mode ... only do good archive ops\n\
  -O offset      initial offset into the time window\n",
                pmGetProgname());
        exit(1);
    }
    indom[0] = PM_INDOM_NULL;
    indom[1] = pmInDom_build(atoi(argv[optind+1]), atoi(argv[optind+2]));
    indom[2] = pmInDom_build(atoi(argv[optind+1]), atoi(argv[optind+3]));

    /*
     * once per context type ... invalid, localhost, archive
     */
    for (c = 0; c < 3; c++) {
	if (fault && c != 2)
	    continue;
	if (c == 0) {
	    /* invalid context, none created yet */
	    sts = -1;
	}
	else if (c == 1) {
	    /* pmcd on localhost */
	    (void)gethostname(local, MAXHOSTNAMELEN);
	    local[MAXHOSTNAMELEN-1] = '\0';
	    if ((sts = pmNewContext(PM_CONTEXT_HOST, local)) < 0) {
		fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		    pmGetProgname(), local, pmErrStr(sts));
		exit(1);
	    }
	}
	else if (c == 2) {
	    if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, argv[optind])) < 0) {
		fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmGetProgname(), argv[optind], pmErrStr(sts));
		exit(1);
	    }
	}
	fprintf(stderr, "\n=== iteration %d context %d ===\n", c, sts);

	if (c == 2) {
	    if ((sts = pmGetArchiveLabel(&label)) < 0) {
		fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		    pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }
	    startTime = label.start;
	    if ((sts = pmGetArchiveEnd(&endTime)) < 0) {
		fprintf(stderr, "%s: Cannot locate end of archive: %s\n",
		    pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }
	    if ((tzh = pmNewContextZone()) < 0) {
		fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		    pmGetProgname(), pmErrStr(tzh));
		exit(1);
	    }
	    sts = pmParseTimeWindow(NULL, NULL, NULL, offset, &startTime,
			    &endTime, &appStart, &appEnd, &appOffset,
			    &endnum);
	    if (sts < 0) {
		fprintf(stderr, "%s: illegal time window specification\n%s",
		    pmGetProgname(), endnum);
		exit(1);
	    }
	    if ((sts = pmSetModeHighRes(PM_MODE_FORW, &appOffset, NULL)) < 0) {
		fprintf(stderr, "%s: pmSetMode: %s\n",
		    pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }
	}

	/*
	 * routines to be tested ...
	 *
	 * PMAPI
	 * [y] pmGetInDomArchive
	 * [y] pmLookupInDomArchive
	 * [y] pmNameInDomArchive
	 *
	 * internal
	 * [ ] __pmLogPutDesc
	 * [ ] __pmLogPutInDom
	 * [ ] __pmLogPutIndex
	 * [ ] __pmLogLookupDesc
	 * [y] __pmLogGetInDom
	 * [y] __pmLogLookupInDom
	 * [y] __pmLogNameInDom
	 */ 

	/* once per indom ... null, good and bad */
	for (i = 0; i < 3; i++) {
	    __pmContext	*ctxp;

	    if (fault && i != 1)
		continue;
	    if (i == 0) {
		/* tests that do not use the indom ... */
		;
	    }

	    if ((sts = pmGetInDomArchive(indom[i], &instlist, &namelist)) < 0) {
		fprintf(stderr, "pmGetInDomArchive(%s) -> ", pmInDomStr(indom[i]));
		fprintf(stderr, "%s\n", pmErrStr(sts));
	    }
	    else {
		int	j;
		fprintf(stderr, "pmGetInDomArchive(%s) -> ", pmInDomStr(indom[i]));
		fprintf(stderr, "%d\n", sts);
		for (j = 0; j < sts; j++)
		    fprintf(stderr, "   [%d] %s\n", instlist[j], namelist[j]);
		if (xname == NULL) {
		    char	*q;
		    xname = strdup(namelist[0]);
		    for (q = xname; *q; q++) {
			if (*q == ' ') {
			    xxname = strdup(xname);
			    xxname[q-xname] = '\0';
			    break;
			}
		    }
		}
		free(instlist);
		free(namelist);
	    }

	    if (!fault) {
		if ((sts = pmLookupInDomArchive(indom[i], "foobar")) < 0) {
		    fprintf(stderr, "pmLookupInDomArchive(%s, foobar) -> ", pmInDomStr(indom[i]));
		    fprintf(stderr, "%s\n", pmErrStr(sts));
		}
		else {
		    fprintf(stderr, "pmLookupInDomArchive(%s, foobar) -> ", pmInDomStr(indom[i]));
		    fprintf(stderr, "%d\n", sts);
		}
	    }

	    if (xname != NULL) {
		if ((sts = pmLookupInDomArchive(indom[i], xname)) < 0) {
		    fprintf(stderr, "pmLookupInDomArchive(%s, %s) -> ", pmInDomStr(indom[i]), xname);
		    fprintf(stderr, "%s\n", pmErrStr(sts));
		}
		else {
		    fprintf(stderr, "pmLookupInDomArchive(%s, %s) -> ", pmInDomStr(indom[i]), xname);
		    fprintf(stderr, "%d\n", sts);
		    if (xinst == -1)
			xinst = sts;
		}
	    }

	    if (xxname != NULL) {
		if ((sts = pmLookupInDomArchive(indom[i], xxname)) < 0) {
		    fprintf(stderr, "pmLookupInDomArchive(%s, %s) -> ", pmInDomStr(indom[i]), xxname);
		    fprintf(stderr, "%s\n", pmErrStr(sts));
		}
		else {
		    fprintf(stderr, "pmLookupInDomArchive(%s, %s) -> ", pmInDomStr(indom[i]), xxname);
		    fprintf(stderr, "%d\n", sts);
		}
	    }

	    if (!fault) {
		if ((sts = pmNameInDomArchive(indom[i], 1234567, &name)) < 0) {
		    fprintf(stderr, "pmNameInDomArchive(%s, 1234567) -> ", pmInDomStr(indom[i]));
		    fprintf(stderr, "%s\n", pmErrStr(sts));
		}
		else {
		    fprintf(stderr, "pmNameInDomArchive(%s, 1234567) -> ", pmInDomStr(indom[i]));
		    fprintf(stderr, "%s\n", name);
		    free(name);
		}
	    }

	    if (xinst != -1) {
		if ((sts = pmNameInDomArchive(indom[i], xinst, &name)) < 0) {
		    fprintf(stderr, "pmNameInDomArchive(%s, %d) -> ", pmInDomStr(indom[i]), xinst);
		    fprintf(stderr, "%s\n", pmErrStr(sts));
		}
		else {
		    fprintf(stderr, "pmNameInDomArchive(%s, %d) -> ", pmInDomStr(indom[i]), xinst);
		    fprintf(stderr, "%s\n", name);
		    free(name);
		}
	    }

	    if ((sts = pmWhichContext()) < 0) {
		/* not unexpected in this QA application */
		continue;
	    }
	    ctxp = __pmHandleToPtr(sts);
	    if (ctxp == NULL) {
		/* not unexpected in this QA application */
		continue;
	    }
	    if (ctxp->c_archctl == NULL) {
		/* not unexpected in this QA application */
		PM_UNLOCK(ctxp->c_lock);
		continue;
	    }
	    else {
		__pmTimestamp	now;
		int		iter;
		__pmArchCtl	*acp = ctxp->c_archctl;
		PM_UNLOCK(ctxp->c_lock);
		for (iter=0; iter < 2; iter++) {
		    /*
		     * use -O time (defaults to start time) on first
		     * iteration, then half the time between there
		     * and the end
		     */
		     if (fault && iter != 1)
			continue;
		     if (iter == 0) {
			now.sec = appOffset.tv_sec;
			now.nsec = appOffset.tv_nsec;
		     }
		     else {
			/*
			 * danger! need to promote arithmetic to 64-bit
			 * for platforms where tv_sec is 32-bit and
			 * tv_sec + tv_sec => overflow
			 */
			now.sec = ((__int64_t)appOffset.tv_sec+(__int64_t)appEnd.tv_sec)/2;
			now.nsec = (((__int64_t)appOffset.tv_nsec+(__int64_t)appEnd.tv_nsec)/2) * 1000;
		     }
		    if ((sts = __pmLogGetInDom(acp, indom[i], &now, &instlist, &namelist)) < 0) {
			fprintf(stderr, "__pmLogGetInDom(%s) -> ", pmInDomStr(indom[i]));
			fprintf(stderr, "%s\n", pmErrStr(sts));
		    }
		    else {
			int	j;
			fprintf(stderr, "__pmLogGetInDom(%s) -> ", pmInDomStr(indom[i]));
			fprintf(stderr, "%d\n", sts);
			for (j = 0; j < sts; j++)
			    fprintf(stderr, "   [%d] %s\n", instlist[j], namelist[j]);
		    }
		    if (!fault) {
			if ((sts = __pmLogLookupInDom(acp, indom[i], &now, "foobar")) < 0) {
			    fprintf(stderr, "__pmLogLookupInDom(%s, foobar) -> ", pmInDomStr(indom[i]));
			    fprintf(stderr, "%s\n", pmErrStr(sts));
			}
			else {
			    fprintf(stderr, "__pmLogLookupInDom(%s, foobar) -> ", pmInDomStr(indom[i]));
			    fprintf(stderr, "%d\n", sts);
			}
		    }
		    if (xname != NULL) {
			if ((sts = __pmLogLookupInDom(acp, indom[i], &now, xname)) < 0) {
			    fprintf(stderr, "__pmLogLookupInDom(%s, %s) -> ", pmInDomStr(indom[i]), xname);
			    fprintf(stderr, "%s\n", pmErrStr(sts));
			}
			else {
			    fprintf(stderr, "__pmLogLookupInDom(%s, %s) -> ", pmInDomStr(indom[i]), xname);
			    fprintf(stderr, "%d\n", sts);
			}
		    }
		    if (xxname != NULL) {
			if ((sts = __pmLogLookupInDom(acp, indom[i], &now, xxname)) < 0) {
			    fprintf(stderr, "__pmLogLookupInDom(%s, %s) -> ", pmInDomStr(indom[i]), xxname);
			    fprintf(stderr, "%s\n", pmErrStr(sts));
			}
			else {
			    fprintf(stderr, "__pmLogLookupInDom(%s, %s) -> ", pmInDomStr(indom[i]), xxname);
			    fprintf(stderr, "%d\n", sts);
			}
		    }
		    if (!fault) {
			if ((sts = __pmLogNameInDom(acp, indom[i], &now, 1234567, &name)) < 0) {
			    fprintf(stderr, "__pmLogNameInDom(%s, 1234567) -> ", pmInDomStr(indom[i]));
			    fprintf(stderr, "%s\n", pmErrStr(sts));
			}
			else {
			    fprintf(stderr, "__pmLogNameInDom(%s, 1234567) -> ", pmInDomStr(indom[i]));
			    fprintf(stderr, "%s\n", name);
			}
		    }
		    if (xinst != -1) {
			if ((sts = __pmLogNameInDom(acp, indom[i], &now, xinst, &name)) < 0) {
			    fprintf(stderr, "__pmLogNameInDom(%s, %d) -> ", pmInDomStr(indom[i]), xinst);
			    fprintf(stderr, "%s\n", pmErrStr(sts));
			}
			else {
			    fprintf(stderr, "__pmLogNameInDom(%s, %d) -> ", pmInDomStr(indom[i]), xinst);
			    fprintf(stderr, "%s\n", name);
			}
		    }
		}
	    }
	}
    }

    return 0;
}
