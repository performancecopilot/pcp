// Copyright (C) 2014 Red Hat, Inc.
// Exercise pmParseUnitsStr()

#include <pcp/pmapi.h>

#include <assert.h>
// #define assert(c) if(!(c)) printf("%s\n", #c)


void
pmunits_roundtrip(int print_p, int d1, int d2, int d3, int s1, int s2, int s3)
{
    pmUnits victim = {.dimSpace = d1,
	.dimCount = d2,
	.dimTime = d3,
	.scaleSpace = s1,
	.scaleCount = s2,
	.scaleTime = s3
    };

    char converted[100] = "";
    char converted2[100] = "";
    pmUnits reversed;
    double reversed_multiplier;
    int sts;
    char *errmsg = NULL;

    (void) pmUnitsStr_r(&victim, converted, sizeof(converted));
    sts = pmParseUnitsStr(converted, &reversed, &reversed_multiplier, &errmsg);
    (void) pmUnitsStr_r(&reversed, converted2, sizeof(converted2));

    if (print_p)
	printf("(%d,%d,%d,%d,%d,%d) => \"%s\" => conv rc %d%s%s => (%d,%d,%d,%d,%d,%d)*%g => \"%s\" \n",
	       victim.dimSpace, victim.dimCount, victim.dimTime, victim.scaleSpace, victim.scaleCount, victim.scaleTime,
	       converted, sts, (sts < 0 ? " " : ""), (sts < 0 ? errmsg : ""), reversed.dimSpace, reversed.dimCount,
	       reversed.dimTime, reversed.scaleSpace, reversed.scaleCount, reversed.scaleTime, reversed_multiplier,
	       converted2);
    else {
	assert(sts == 0);
	assert(strcmp(converted, converted2) == 0);
	assert(reversed_multiplier == 1.0);	// FP equality ok
	assert(reversed.dimSpace == victim.dimSpace);
	assert(reversed.dimTime == victim.dimTime);
	assert(reversed.scaleSpace == victim.scaleSpace);
	assert(reversed.scaleTime == victim.scaleTime);
	// The case of 'count' is more relaxed because of the ambiguity:
	// "count x 10^6" => (dim=6 scale=1) or (scale=1 dim=6)
	assert(reversed.dimCount * reversed.scaleCount == victim.dimCount * victim.scaleCount);
    }

    if (sts < 0)
	free(errmsg);
}


void
pmunits_roundtrip_all()
{
    int d1, d2, d3, s1, s2, s3;
    unsigned k = 0;

    for (d1 = -8; d1 < 8; d1++)
	for (d2 = -8; d2 < 8; d2++)
	    for (d3 = -8; d3 < 8; d3++)
		for (s1 = 0; s1 < (d1 ? 16 : 0); s1++)	// scale X only if dim X
		    for (s2 = -8; s2 < (d2 ? 8 : -8); s2++)
			for (s3 = 0; s3 < (d3 ? 16 : 0); s3++) {
			    k++;
			    pmunits_roundtrip(0, d1, d2, d3, s1, s2, s3);
			}

    printf("%u pmUnits tuples round-tripped.\n", k);
}


void
pmunits_parse(const char *str)
{
    pmUnits reversed;
    double reversed_multiplier;
    int sts;
    char converted[100] = "";
    char *errmsg;

    sts = pmParseUnitsStr(str, &reversed, &reversed_multiplier, &errmsg);
    (void) pmUnitsStr_r(&reversed, converted, sizeof(converted));

    printf("\"%s\" => conv rc %d%s%s => (%d,%d,%d,%d,%d,%d)*%g => \"%s\"\n", str, sts, (sts < 0 ? " " : ""),
	   (sts < 0 ? errmsg : ""), reversed.dimSpace, reversed.dimCount, reversed.dimTime, reversed.scaleSpace,
	   reversed.scaleCount, reversed.scaleTime, reversed_multiplier, converted);

    if (sts < 0)
	free(errmsg);
}



int
main(int argc, char *argv[])
{
    if (argc == 8 && !strcmp(argv[1], "tuple"))
	pmunits_roundtrip(1, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), atoi(argv[7]));
    else if (argc == 2 && !strcmp(argv[1], "scan"))
	pmunits_roundtrip_all();
    else if (argc == 3 && !strcmp(argv[1], "parse"))
	pmunits_parse(argv[2]);
    else
	return -1;

    return 0;
}
