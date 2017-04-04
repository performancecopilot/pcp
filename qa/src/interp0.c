/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * interp0 - basic PM_MODE_INTERP exercises
 */

#include <unistd.h>
#include <string.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		j;
    double	tdiff;
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
    char	*name[] = { "sample.seconds", "sample.drift", "sample.milliseconds" };
    pmDesc	desc;
    int		type[3];

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:n:s:t:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (ahtype != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
		errflag++;
	    }
	    ahtype = PM_CONTEXT_ARCHIVE;
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

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case 's':	/* sample count */
	    samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples < 0) {
		fprintf(stderr, "%s: -s requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case 't':	/* delta seconds (double) */
	    delta = strtod(optarg, &endnum);
	    if (*endnum != '\0' || delta <= 0.0) {
		fprintf(stderr, "%s: -t requires floating point argument\n", pmProgname);
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
  -a   archive	  metrics source is an archive log\n\
  -D   debug	  standard PCP debug flag\n\
  -n   namespace  use an alternative PMNS\n\
  -s   samples	  terminate after this many iterations\n\
  -t   delta	  sample interval in seconds(float) [default 1.0]\n",
		pmProgname);
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT) {
	if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	    printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	    exit(1);
	}
    }

    if (ahtype != PM_CONTEXT_ARCHIVE) {
	fprintf(stderr, "%s: -a is not optional!\n", pmProgname);
	exit(1);
    }
    if ((sts = pmNewContext(ahtype, host)) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
	    pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmGetArchiveLabel(&label)) < 0) {
	fprintf(stderr, "%s: Cannot get archive label record: %s\n",
	    pmProgname, pmErrStr(sts));
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

    /* skip preamble */
    sts = pmFetchArchive(&result);
    if (sts < 0) {
	printf("pmFetchArchive: %s\n", pmErrStr(sts));
	exit(1);
    }
    pmFreeResult(result);

    sts = pmFetchArchive(&result);
    if (sts < 0) {
	printf("pmFetchArchive: %s\n", pmErrStr(sts));
	exit(1);
    }


    msec = delta * 1000;
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
	sts = pmLookupDesc(pmid[i], &desc);
	if (sts < 0) {
	    printf("Warning: pmLookupDesc(%s): %s\n", name[i], pmErrStr(sts));
	    type[i] = -1;
	}
	else
	    type[i] = desc.type;
    }

    for (i = 0; i < samples; i++) {
	sts = pmFetch(numpmid, pmid, &result);
	if (sts < 0) {
	    printf("sample[%d] pmFetch: %s\n", i, pmErrStr(sts));
	    if (sts == PM_ERR_EOL)
		break;
	    printf("... this is unexpected and fatal!\n");
	    exit(1);
	}
	if (prev) {
	    struct timeval tmp = result->timestamp;
	    __pmtimevalDec(&tmp, &prev->timestamp);
	    tdiff = __pmtimevalToReal(&tmp);
	    printf("\nsample %d, delta time=%.3f secs\n", i, tdiff);
	    for (j = 0; j < numpmid; j++) {
		printf("%s: ", name[j]);
		if (result->vset[j]->numval != 1 || prev->vset[j]->numval != 1) {
		    if (result->vset[j]->numval == 0)
			printf("no current values ");
		    if (prev->vset[j]->numval == 0)
			printf("no prior values ");
		    if (result->vset[j]->numval < 0)
			printf("current error %s ", pmErrStr(result->vset[j]->numval));
		    if (prev->vset[j]->numval < 0 && prev->vset[j]->numval != result->vset[j]->numval)
			printf("prior error %s ", pmErrStr(prev->vset[j]->numval));
		    putchar('\n');
		}
		else {
		    if (type[j] == PM_TYPE_32 || type[j] == PM_TYPE_U32) {
			printf("delta: %d\n",
			    result->vset[j]->vlist[0].value.lval -
			    prev->vset[j]->vlist[0].value.lval);
		    }
		    else if (type[j] == PM_TYPE_DOUBLE) {
			void		*cp = (void *)result->vset[j]->vlist[0].value.pval->vbuf;
			void		*pp = (void *)prev->vset[j]->vlist[0].value.pval->vbuf;
			double		cv, pv;
			pmAtomValue	av;

			memcpy((void *)&av, cp, sizeof(pmAtomValue));
			cv = av.d;
			memcpy((void *)&av, pp, sizeof(pmAtomValue));
			pv = av.d;
			printf("delta: %.0f\n",
			    cv - pv);
		    }
		    else if (type[j] == PM_TYPE_EVENT) {
			pmResult **records;
			int r, param;

			printf("%d event records found\n", result->vset[j]->numval);
			sts = pmUnpackEventRecords(result->vset[j], 0, &records);
			if (sts < 0) {
			    printf("event decode error: %s\n", pmErrStr(sts));
			} else {
			    for (r = 0; r < sts; r++) {
				struct timeval tmp = records[r]->timestamp;
				__pmtimevalDec(&tmp, &prev->timestamp);
				tdiff = __pmtimevalToReal(&tmp);
				printf("\nevent %d, offset time=%.3f secs, param ids:", j+1, tdiff);
				for (param = 0; param < records[r]->numpmid; param++)
				    printf(" %s", pmIDStr(records[r]->vset[param]->pmid));
			    }
			    pmFreeEventResult(records);
			    putchar('\n');
			}
		    }
		    else if (type[j] == PM_TYPE_HIGHRES_EVENT) {
			pmHighResResult **hrecords;
			int r, param;

			printf("%d highres event records found\n", result->vset[j]->numval);
			sts = pmUnpackHighResEventRecords(result->vset[j], 0, &hrecords);
			if (sts < 0) {
			    printf("highres event decode error: %s\n", pmErrStr(sts));
			} else {
			    for (r = 0; r < sts; r++) {
				tdiff = hrecords[r]->timestamp.tv_sec - prev->timestamp.tv_sec +
				(long double)(hrecords[r]->timestamp.tv_nsec - prev->timestamp.tv_usec * 1000)
					/ (long double)1000000000;
				printf("\nhighres event %d, offset time=%.9f secs, param ids:", j+1, tdiff);
				for (param = 0; param < hrecords[r]->numpmid; param++)
				    printf(" %s", pmIDStr(hrecords[r]->vset[param]->pmid));
			    }
			    pmFreeHighResEventResult(hrecords);
			    putchar('\n');
			}
		    }
		    else
			printf("don't know how to display type %d for PMID %s\n",
			    type[j], pmIDStr(pmid[j]));
		}
	    }
	    pmFreeResult(prev);
	}
	prev = result;
    }

    printf("\n%d samples required %d log reads\n", i, __pmLogReads);

    exit(0);
}
