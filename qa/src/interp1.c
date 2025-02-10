/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * interp1 - backward PM_MODE_INTERP exercises
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		j;
    int		k;
    double	tdiff;
    int		dflag = 0;
    int		errflag = 0;
    int		ahtype = 0;
    char	*host = NULL;			/* pander to gcc */
    pmLogLabel	label;				/* get hostname for archives */
    char	*namespace = PM_NS_DEFAULT;
    int		samples = 10;
    double	delta = 1.0;
    int		msec;
    char	*endnum;
    pmResult	*result;
    pmResult	*prev = (pmResult *)0;
    int		i;
    int		numpmid = 3;
    pmID	pmid[3];
    const char	*name[] = { "sample.seconds", "sample.drift", "sample.milliseconds" };
    pmDesc	desc[3];
    struct timeval tend = {PM_MAX_TIME_T, 0};

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:dn:s:t:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (ahtype != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmGetProgname());
		errflag++;
	    }
	    ahtype = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

	case 'd':	/* use metric descriptor to decide on rate conversion */
	    dflag++;
	    break;

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case 's':	/* sample count */
	    samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples < 0) {
		fprintf(stderr, "%s: -s requires numeric argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case 't':	/* delta seconds (double) */
	    delta = strtod(optarg, &endnum);
	    if (*endnum != '\0' || delta <= 0.0) {
		fprintf(stderr, "%s: -t requires floating point argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s options ...\n\
\n\
Options\n\
  -a   archive	  metrics source is an archive\n\
  -d              use metric descriptors to decide on value or delta\n\
  -D   debugspec  standard PCP debugging options\n\
  -n   namespace  use an alternative PMNS\n\
  -s   samples	  terminate after this many iterations\n\
  -t   delta	  sample interval in seconds(float) [default 1.0]\n",
		pmGetProgname());
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmGetProgname(), namespace, pmErrStr(sts));
	exit(1);
    }

    if (ahtype == 0) {
	fprintf(stderr, "%s: -a is not optional!\n", pmGetProgname());
	exit(1);
    }
    if ((sts = pmNewContext(ahtype, host)) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
	    pmGetProgname(), host, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmGetArchiveLabel(&label)) < 0) {
	fprintf(stderr, "%s: Cannot get archive label record: %s\n",
	    pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    if (optind < argc) {
	numpmid = 0;
	while (optind < argc && numpmid < 3) {
	    name[numpmid] = argv[optind];
	    printf("metric[%d]: %s\n", numpmid, name[numpmid]);
	    optind++;
	    numpmid++;
	}
    }

    sts = pmSetMode(PM_MODE_BACK, &tend, 0);
    if (sts < 0) {
	printf("pmSetMode: %s\n", pmErrStr(sts));
	exit(1);
    }

    /*
     * we want to get to the last data record ... but there may be
     * an epilogue record at the end of the archive, so need some
     * heuristics to optionally read back before the epilogue ...
     * if at least 5 pmids and all are from the pmcd PMDA, then
     * there's a good chance this is an epilogue record.
     */
    sts = pmFetchArchive(&result);
    if (sts < 0) {
	printf("pmFetchArchive end: %s\n", pmErrStr(sts));
	exit(1);
    }
    if (result->numpmid >= 5) {
	for (j = 0; j < result->numpmid; j++) {
	    if (pmID_domain(result->vset[0]->pmid) != 2)
		break;
	}
	if (j == result->numpmid) {
	    /* suck back one record earlier in the archive */
	    pmFreeResult(result);
	    sts = pmFetchArchive(&result);
	    if (sts < 0) {
		printf("pmFetchArchive skip epilogue: %s\n", pmErrStr(sts));
		exit(1);
	    }
	}
    }

    msec = -delta * 1000;
    sts = pmSetMode(PM_MODE_INTERP, &result->timestamp, msec);
    if (sts < 0) {
	printf("pmSetMode: %s\n", pmErrStr(sts));
	exit(1);
    }
    pmFreeResult(result);

    sts = pmLookupName(numpmid, name, pmid);
    if (sts < 0) {
	printf("pmLookupName: %s\n", pmErrStr(sts));
	exit(1);
    }

    for (i = 0; i < numpmid; i++) {
	sts = pmLookupDesc(pmid[i], &desc[i]);
	if (sts < 0) {
	    printf("Warning: pmLookupDesc(%s): %s\n", pmIDStr(pmid[i]), pmErrStr(sts));
	    desc[i].type = -1;
	}
    }

    for (i = 0; i < samples; i++) {
	sts = pmFetch(numpmid, pmid, &result);
	if (sts < 0) {
	    printf("sample[%d] pmFetch: %s\n", i, pmErrStr(sts));
	    break;
	}
	if (prev) {
	    tdiff = pmtimevalSub(&result->timestamp, &prev->timestamp);
	    printf("\nsample %d, delta time=%.3f secs\n", i, tdiff);
	    for (j = 0; j < numpmid; j++) {
		printf("%s: ", name[j]);
		if (result->vset[j]->numval == 0)
		    printf("no current values ");
		if (prev->vset[j]->numval == 0)
		    printf("no prior values ");
		if (result->vset[j]->numval < 0)
		    printf("current error %s ", pmErrStr(result->vset[j]->numval));
		if (prev->vset[j]->numval < 0 && prev->vset[j]->numval != result->vset[j]->numval)
		    printf("prior error %s ", pmErrStr(prev->vset[j]->numval));
		putchar('\n');
		for (k = 0; k < prev->vset[j]->numval && k < result->vset[j]->numval; k++) {
		    if (result->vset[j]->vlist[k].inst != prev->vset[j]->vlist[k].inst) {
			printf("inst[%d]: mismatch, prior=%d, current=%d\n", k,
				prev->vset[j]->vlist[k].inst,
				result->vset[j]->vlist[k].inst);
			continue;
		    }
		    if (desc[j].type == PM_TYPE_32 || desc[j].type == PM_TYPE_U32) {
			if (!dflag || desc[j].sem == PM_SEM_COUNTER)
			    printf("delta[%d]: %d\n", k,
				result->vset[j]->vlist[k].value.lval -
				prev->vset[j]->vlist[k].value.lval);
			else
			    printf("value[%d]: %d\n", k,
				result->vset[j]->vlist[k].value.lval);
		    }
		    else if (desc[j].type == PM_TYPE_DOUBLE) {
			void		*cp = (void *)result->vset[j]->vlist[k].value.pval->vbuf;
			void		*pp = (void *)prev->vset[j]->vlist[k].value.pval->vbuf;
			double		cv, pv;
			pmAtomValue	av;

			memcpy((void *)&av, cp, sizeof(pmAtomValue));
			cv = av.d;
			if (!dflag || desc[j].sem == PM_SEM_COUNTER) {
			    memcpy((void *)&av, pp, sizeof(pmAtomValue));
			    pv = av.d;
			    printf("delta[%d]: %.0f\n", k, cv - pv);
			}
			else
			    printf("value[%d]: %.0f\n", k, cv);
		    }
		    else if (desc[j].type == PM_TYPE_STRING) {
			printf("value[%d]: %s\n", k,
			    result->vset[j]->vlist[k].value.pval->vbuf);
		    }
		    else {
			printf("don't know how to display type %d for PMID %s\n",
			    desc[j].type, pmIDStr(pmid[j]));
			break;
		    }
		}
	    }
	    pmFreeResult(prev);
	}
	prev = result;
    }

    printf("\n%d samples required %d log reads\n", i, __pmLogReads);

    exit(0);
}
