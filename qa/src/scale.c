/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * scale - exercise scale conversion, pmConvScale and pmUnitsStr
 */

#include <pcp/pmapi.h>
#include <pcp/pmda.h>

static pmUnits	iu = PMDA_PMUNITS( 0, 1, 1, 0, PM_TIME_SEC, 0 );
static pmUnits	ou = PMDA_PMUNITS( 0, 1, 1, 0, PM_TIME_SEC, 0 );

static int tval[] = { 1, 7200 };
static int tscale[] =
    { PM_TIME_NSEC, PM_TIME_USEC, PM_TIME_MSEC, PM_TIME_SEC, PM_TIME_MIN, PM_TIME_HOUR };
static int sval[] = { 1, 1024*1024 };
static int sscale[] =
    { PM_SPACE_BYTE, PM_SPACE_KBYTE, PM_SPACE_MBYTE, PM_SPACE_GBYTE, PM_SPACE_TBYTE };

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    const char	*u;
    static char	*usage = "[-D debugspec] [-v]";
    int		vflag = 0;
    pmAtomValue	iv;
    pmAtomValue	ov;
    pmAtomValue	tv;
    int		d;
    int		i;
    int		j;
    int		k;
    int		l;
    int		underflow = 0;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:v")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'v':	/* verbose */
	    vflag++;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr, "Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */

    for (d = 1; d >= -1; d -=2) {
	iu.dimTime = d;
	ou.dimTime = d;
	if (d == 1) {
	    iu.dimCount = -1;
	    ou.dimCount = -1;
	}
	else {
	    iu.dimCount = 1;
	    ou.dimCount = 1;
	}
	for (i = 0; i < sizeof(tval)/sizeof(tval[0]); i++) {
	    iv.ll = tval[i];
	    for (j = 0; j < sizeof(tscale)/sizeof(tscale[0]); j++) {
		iu.scaleTime = tscale[j];
		for (k = 0; k < sizeof(tscale)/sizeof(tscale[0]); k++) {
		    ou.scaleTime = tscale[k];

		    if ((sts = pmConvScale(PM_TYPE_64, &iv, &iu, &ov, &ou)) < 0) {
			printf("convert: %s\n", pmErrStr(sts));
		    }
		    else {
			if (vflag) {
			    u = pmUnitsStr(&iu);
			    printf("%lld %s", (long long)iv.ll, *u == '\0' ? "none" : u);
			    u = pmUnitsStr(&ou);
			    printf(" -> %lld %s\n", (long long)ov.ll, *u == '\0' ? "none" : u);
			}
			if ((sts = pmConvScale(PM_TYPE_64, &ov, &ou, &tv, &iu)) < 0) {
			    printf("reconvert: %s\n", pmErrStr(sts));
			}
			else {
			    if (tv.ll != iv.ll) {
				if (ov.ll == 0)
				    underflow++;
				else {
				    u = pmUnitsStr(&iu);
				    printf("error?  %lld %s", (long long)iv.ll, *u == '\0' ? "none" : u);
				    u = pmUnitsStr(&ou);
				    printf(" -> %lld %s", (long long)ov.ll, *u == '\0' ? "none" : u);
				    u = pmUnitsStr(&iu);
				    printf(" -> %lld %s\n", (long long)tv.ll, *u == '\0' ? "none" : u);
				}
			    }
			}
		    }
		}
	    }
	}
    }
    printf("\nPass 1: plus %d underflows to zero\n\n", underflow);
    underflow = 0;

    ou.dimTime = 0;
    iu.dimTime = 0;
    ou.dimCount = 0;
    iu.dimCount = 0;
    ou.dimSpace = 1;
    iu.dimSpace = 1;
    iu.scaleTime = PM_TIME_SEC;
    for (d = 0; d < 4; d +=2) {
	ou.dimTime = -d;
	iu.dimTime = -d;
	for (i = 0; i < sizeof(sval)/sizeof(sval[0]); i++) {
	    iv.ll = sval[i];
	    for (j = 0; j < sizeof(sscale)/sizeof(sscale[0]); j++) {
		iu.scaleSpace = sscale[j];
		for (k = 0; k < sizeof(sscale)/sizeof(sscale[0]); k++) {
		    ou.scaleSpace = sscale[k];
		    for (l = 2; l < sizeof(tscale)/sizeof(tscale[0]); l += 2) {
			ou.scaleTime = tscale[l];

			if ((sts = pmConvScale(PM_TYPE_64, &iv, &iu, &ov, &ou)) < 0) {
			    printf("convert: %s\n", pmErrStr(sts));
			}
			else {
			    if (vflag) {
				u = pmUnitsStr(&iu);
				printf("%lld %s", (long long)iv.ll, *u == '\0' ? "none" : u);
				u = pmUnitsStr(&ou);
				printf(" -> %lld %s\n", (long long)ov.ll, *u == '\0' ? "none" : u);
			    }
			    if ((sts = pmConvScale(PM_TYPE_64, &ov, &ou, &tv, &iu)) < 0) {
				printf("reconvert: %s\n", pmErrStr(sts));
			    }
			    else {
				if (tv.ll != iv.ll) {
				    if (ov.ll == 0)
					underflow++;
				    else {
					u = pmUnitsStr(&iu);
					printf("error?  %lld %s", (long long)iv.ll, *u == '\0' ? "none" : u);
					u = pmUnitsStr(&ou);
					printf(" -> %lld %s", (long long)ov.ll, *u == '\0' ? "none" : u);
					u = pmUnitsStr(&iu);
					printf(" -> %lld %s\n", (long long)tv.ll, *u == '\0' ? "none" : u);
				    }
				}
			    }
			}
			if (d == 0)
			    break;
		    }
		}
	    }
	}
    }
    printf("\nPass 2: plus %d underflows to zero\n", underflow);

    iu.dimSpace = 0;
    iu.scaleSpace = 0;
    iu.scaleTime = PM_TIME_MSEC;
    for (d = 1; d > -2; d -= 2) {
	iu.dimTime = d;
	iu.dimCount = -d;
	for (i = 3; i >= -3; i--) {
	    iu.scaleCount = i;
	    printf("{ %3d,%3d,%3d,%3d,%3d,%3d } %s\n",
		    iu.dimSpace, iu.dimTime, iu.dimCount,
		    iu.scaleSpace, iu.scaleTime, iu.scaleCount,
		    pmUnitsStr(&iu));
	}
    }
    printf("\nPass 3:\n");

    exit(0);
}
