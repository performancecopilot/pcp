/*
 * pmconvscale - tests bug in pmConvScale
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <unistd.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

/* default is the "no" dimension case */
pmUnits		units = {0, 0, 0, 0, 0, 0};
pmUnits		oldunits = {0, 0, 0, 0, 0, 0};

static int sscales[] = { PM_SPACE_GBYTE, PM_SPACE_TBYTE, PM_SPACE_GBYTE, PM_SPACE_MBYTE, PM_SPACE_KBYTE, PM_SPACE_BYTE, PM_SPACE_KBYTE, PM_SPACE_MBYTE };
static int n_sscales = sizeof(sscales) / sizeof(sscales[0]);

static int tscales[] = { PM_TIME_MIN, PM_TIME_HOUR, PM_TIME_MIN, PM_TIME_SEC, PM_TIME_MSEC, PM_TIME_USEC, PM_TIME_NSEC, PM_TIME_USEC, PM_TIME_MSEC, PM_TIME_SEC };
static int n_tscales = sizeof(tscales) / sizeof(tscales[0]);

static int cscales[] = {1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4, -5, -6, -7, -8, -7, -6, -5, -4, -3, -2, -1, 0 }; 
static int n_cscales = sizeof(cscales) / sizeof(cscales[0]);

/* just for PBYTE and EBYTE space extensions */
#ifdef HAVE_BITFIELDS_LTOR
pmUnits myunits = { 1, -1, 0, PM_SPACE_EBYTE, PM_TIME_SEC, 0 };
#else
pmUnits myunits = { 0, 0, PM_TIME_SEC, PM_SPACE_EBYTE, 0, -1, 1 };
#endif

int
main(int argc, char **argv)
{
    int i, sts;
    int	mode = 1;
    int	limit = 0;
    pmAtomValue value, newvalue;
    char olds[64], news[64];

    value.f = 12345678;

    if (argc > 1) {
	if (argv[1][0] == 'c') {
	    /* crude, but effective ... the "count" dimension case */
	    units.dimCount = 1;
	    mode = 1;
	}
	else if (argv[1][0] == 's') {
	    /* crude, but effective ... the "space" dimension case */
	    units.dimSpace = 1;
	    units.scaleSpace = PM_SPACE_MBYTE;
	    mode = 2;
	}
	else if (argv[1][0] == 'r') {
	    /* crude, but effective ... the "space/time" dimension case */
	    units.dimSpace = 1;
	    units.dimTime = -1;
	    units.scaleSpace = PM_SPACE_MBYTE;
	    units.scaleTime = PM_TIME_SEC;
	    mode = 2;
	}
	else if (argv[1][0] == 't') {
	    /* crude, but effective ... the "time" dimension case */
	    units.dimTime = 1;
	    units.scaleTime = PM_TIME_SEC;
	    mode = 3;
	}
	else if (argv[1][0] == 'x') {
	    /* PBYTE and EBYTE space extensions */
	    value.f = 1;
	    while (myunits.scaleSpace > 0) {
		units = myunits;
		units.scaleSpace--;
		sts = pmConvScale(PM_TYPE_FLOAT, &value, &myunits, &newvalue, &units);
		if (sts < 0) {
		    strcpy(olds, pmUnitsStr(&oldunits));
		    strcpy(news, pmUnitsStr(&units));
		    fprintf(stderr, "pmConvScale \"%s\" -> \"%s\" failed: %s\n", olds, news, pmErrStr(sts));
		    exit(1);
		}
		printf("%12.1f %s -> ", value.f, pmUnitsStr(&myunits));
		printf("%12.1f %s\n", newvalue.f, pmUnitsStr(&units));
		myunits.scaleSpace--;
		value.f = newvalue.f / 512;
	    }
	    value.f *= 512;
	    while (myunits.scaleSpace < PM_SPACE_EBYTE) {
		units = myunits;
		units.scaleSpace++;
		sts = pmConvScale(PM_TYPE_FLOAT, &value, &myunits, &newvalue, &units);
		if (sts < 0) {
		    strcpy(olds, pmUnitsStr(&oldunits));
		    strcpy(news, pmUnitsStr(&units));
		    fprintf(stderr, "pmConvScale \"%s\" -> \"%s\" failed: %s\n", olds, news, pmErrStr(sts));
		    exit(1);
		}
		printf("%12.1f %s -> ", value.f, pmUnitsStr(&myunits));
		printf("%12.1f %s\n", newvalue.f, pmUnitsStr(&units));
		myunits.scaleSpace++;
		value.f = newvalue.f * 512;
	    }
	    exit(0);
	}
    }

    oldunits = units;

    if (mode == 1)
	limit = n_cscales;
    else if (mode == 2)
	limit = n_sscales;
    else if (mode == 3)
	limit = n_tscales;

    for (i=0; i < limit; i++) {
	if (mode == 1)
	    units.scaleCount = cscales[i];
	else if (mode == 2)
	    units.scaleSpace = sscales[i];
	else if (mode == 3)
	    units.scaleTime = tscales[i];

	strcpy(olds, pmUnitsStr(&oldunits));
	strcpy(news, pmUnitsStr(&units));

	sts = pmConvScale(PM_TYPE_FLOAT, &value, &oldunits, &newvalue, &units);
	if (sts < 0) {
	    fprintf(stderr, "pmConvScale \"%s\" -> \"%s\" failed: %s\n", olds, news, pmErrStr(sts));
	    exit(1);
	}

	if (mode == 1)
	    printf("scaleCount=%d old: %12.1f \"%s\" new: %12.1f \"%s\"\n",
		units.scaleCount, value.f, olds, newvalue.f, news);
	else if (mode == 2)
	    printf("scaleSpace=%d old: %12.1f \"%s\" new: %12.1f \"%s\"\n",
		units.scaleSpace, value.f, olds, newvalue.f, news);
	else if (mode == 3)
	    printf("scaleTime=%d old: %12.1f \"%s\" new: %12.1f \"%s\"\n",
		units.scaleTime, value.f, olds, newvalue.f, news);

	value.f = newvalue.f;
	oldunits = units;
    }
    exit(0);
}
