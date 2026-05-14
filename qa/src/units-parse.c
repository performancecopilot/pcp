// Copyright (C) 2014 Red Hat, Inc.
// Exercise pmParseUnitsStr()

#include <pcp/pmapi.h>

void
dump(pmUnits *in, pmUnits *out)
{
    fprintf(stderr, "(%d,%d,%d,%u,%u,%u,%d,%u)",
	   in->dimSpace, in->dimTime, in->dimCount,
	   in->scaleSpace, in->scaleTime, in->scaleCount,
	   in->extraUnit, in->extraScale);
    if (out != NULL) {
	fprintf(stderr, " = > (%d,%d,%d,%u,%u,%u,%d,%u)",
	       out->dimSpace, out->dimTime, out->dimCount,
	       out->scaleSpace, out->scaleTime, out->scaleCount,
	       out->extraUnit, out->extraScale);
    }
    fputc('\n', stderr);
}

void
pmunits_roundtrip(int print_p, int ds, int dt, int dc, int ss, int st, int sc, int x1, int x2)
{
    pmUnits victim = {.dimSpace = ds,
	.dimTime = dt,
	.dimCount = dc,
	.scaleSpace = ss,
	.scaleTime = st,
	.scaleCount = sc,
	.extraUnit = x1,
	.extraScale = x2
    };

    char converted[100] = "";
    char convertedt[100] = "";
    pmUnits reversed;
    double reversed_multiplier;
    int sts;
    int dodge = 0;
    char *errmsg = NULL;

    (void) pmUnitsStr_r(&victim, converted, sizeof(converted));
    sts = pmParseUnitsStr(converted, &reversed, &reversed_multiplier, &errmsg);
    (void) pmUnitsStr_r(&reversed, convertedt, sizeof(convertedt));

    if (print_p) {
	fprintf(stderr, "(%d,%d,%d,%d,%d,%d,%d,%d) => \"%s\" => conv rc %d%s%s => (%d,%d,%d,%d,%d,%d,%d,%d)*%g => \"%s\" \n",
	       victim.dimSpace, victim.dimTime, victim.dimCount,
	       victim.scaleSpace, victim.scaleTime, victim.scaleCount,
	       victim.extraUnit, victim.extraScale,
	       converted, sts, (sts < 0 ? " " : ""), (sts < 0 ? errmsg : ""),
	       reversed.dimSpace, reversed.dimTime, reversed.dimCount,
	       reversed.scaleSpace, reversed.scaleTime, reversed.scaleCount,
	       reversed.extraUnit, reversed.extraScale,
	       reversed_multiplier,
	       convertedt);
    }

    if (sts != 0) {
	if (!print_p) {
	    dump(&victim, NULL);
	    fprintf(stderr, "pmParseUnitsStr(\"%s\") -> %d (%s)\n", converted, sts, pmErrStr(sts));
	}
    }
    else {
	int	bad = 0;
	if (strcmp(converted, convertedt) != 0) {
	    if (!bad) {
		dump(&victim, &reversed);
		bad = 1;
	    }
	    fprintf(stderr, "Botch: string first \"%s\" != second \"%s\"\n", converted, convertedt);
	}
	if (reversed_multiplier != 1.0) {	// FP equality ok
	    if (!bad) {
		dump(&victim, &reversed);
		bad = 1;
	    }
	    fprintf(stderr, "Botch: multiplier: %f != 1\n", reversed_multiplier);
	}
	/* dodge "special " historical case */
	if (victim.dimSpace + victim.dimTime + victim.dimCount == 0)
	    dodge = 1;

	if (reversed.dimSpace != victim.dimSpace && !dodge) {
	    if (!bad) {
		dump(&victim, &reversed);
		bad = 1;
	    }
	    fprintf(stderr, "Botch: dimSpace: in %d != out %d\n", victim.dimSpace, reversed.dimSpace);
	}
	if (reversed.dimTime != victim.dimTime && !dodge) {
	    if (!bad) {
		dump(&victim, &reversed);
		bad = 1;
	    }
	    fprintf(stderr, "Botch: dimTime: in %d != out %d\n", victim.dimTime, reversed.dimTime);
	}
	// The case of 'count' is more relaxed because of the ambiguity:
	// "count x 10^6" => (dim=6 scale=1) or (scale=1 dim=6)
	if (reversed.dimCount * reversed.scaleCount != victim.dimCount * victim.scaleCount && !dodge) {
	    if (!bad) {
		dump(&victim, &reversed);
		bad = 1;
	    }
	    fprintf(stderr, "Botch: dimCount: in %d * %d != %d * %d\n", victim.dimCount, victim.scaleCount, reversed.dimCount, reversed.scaleCount);
	}
	if (reversed.scaleSpace != victim.scaleSpace && !dodge) {
	    if (!bad) {
		dump(&victim, &reversed);
		bad = 1;
	    }
	    fprintf(stderr, "Botch: scaleSpace: in %d != out %d\n", victim.scaleSpace, reversed.scaleSpace);
	}
	if (reversed.scaleTime != victim.scaleTime && !dodge) {
	    if (!bad) {
		dump(&victim, &reversed);
		bad = 1;
	    }
	    fprintf(stderr, "Botch: scaleTime: in %d != out %d\n", victim.scaleTime, reversed.scaleTime);
	}
	if (reversed.scaleCount != victim.scaleCount && !dodge) {
	    if (!bad) {
		dump(&victim, &reversed);
		bad = 1;
	    }
	    fprintf(stderr, "Botch: scaleCount: in %d != out %d\n", victim.scaleCount, reversed.scaleCount);
	}
	if (reversed.extraUnit != victim.extraUnit) {
	    if (!bad) {
		dump(&victim, &reversed);
		bad = 1;
	    }
	    fprintf(stderr, "Botch: extraUnit: in %d != out %d\n", victim.extraUnit, reversed.extraUnit);
	}
	if (reversed.extraScale != victim.extraScale) {
	    if (!bad) {
		dump(&victim, &reversed);
		bad = 1;
	    }
	    fprintf(stderr, "Botch: extraScale: in %d != out %d\n", victim.extraScale, reversed.extraScale);
	}
    }

    if (sts < 0)
	free(errmsg);
}


void
pmunits_roundtrip_all(int print_p)
{
    int ds, dt, dc;
    unsigned ss, st;
    int sc;
    unsigned k = 0;

    for (ds = -8; ds < 8; ds += 4) {
	for (dt = -8; dt < 8; dt +=4) {
	    for (dc = -8; dc < 8; dc +=4) {
		// scale X only if dim X != 0
		for (ss = 0; ss < (ds ? 16 : 0); ss += 4) {
		    for (st = 0; st < (dt ? 16 : 0); st +=4) {
			for (sc = -8; sc < (dc ? 8 : -8); sc +=4) {
			    k++;
			    pmunits_roundtrip(print_p, ds, dt, dc, ss, st, sc, 0, 0);
			    if (ss == 0 && st == 0 && sc == 0) {
				/* extra units in the numerator */
				pmunits_roundtrip(print_p, 1, 0, 0, PM_SPACE_BYTE, 0, 0, PM_UNIT_TEMPERATURE, PM_TEMPERATURE_C);
				k++;
				pmunits_roundtrip(print_p, 0, 1, 0, 0, PM_TIME_SEC, 0, PM_UNIT_TEMPERATURE, PM_TEMPERATURE_F);
				k++;
				pmunits_roundtrip(print_p, 0, 0, 1, 0, 0, PM_COUNT_ONE, PM_UNIT_TEMPERATURE, PM_TEMPERATURE_K);
				k++;
				/* extra units in the denominator */
				pmunits_roundtrip(print_p, 1, -1, 0, PM_SPACE_KBYTE, PM_TIME_SEC, 0, -PM_UNIT_POWER, PM_POWER_kW);
				k++;
				pmunits_roundtrip(print_p, 1, 0, -1, PM_SPACE_MBYTE, 0, PM_COUNT_ONE, -PM_UNIT_POWER, PM_POWER_W);
				k++;
				pmunits_roundtrip(print_p, 0, -1, 1, 0, PM_TIME_USEC, PM_COUNT_ONE, -PM_UNIT_POWER, PM_POWER_mW);
				k++;
				pmunits_roundtrip(print_p, -1, 0, 1, PM_SPACE_GBYTE, 0, PM_COUNT_ONE, -PM_UNIT_POWER, PM_POWER_uW);
				k++;
			    }
			}
		    }
		}
	    }
	}
    }

    fprintf(stderr, "%u pmUnits tuples round-tripped.\n", k);
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

    fprintf(stderr, "\"%s\" => conv rc %d%s%s => (%d,%d,%d,%d,%d,%d,%d,%d)*%g => \"%s\"\n", str, sts, (sts < 0 ? " " : ""),
	   (sts < 0 ? errmsg : ""),
	   reversed.dimSpace, reversed.dimTime, reversed.dimCount,
	   reversed.scaleSpace, reversed.scaleTime, reversed.scaleCount,
	   reversed.extraUnit, reversed.extraScale,
	   reversed_multiplier, converted);

    if (sts < 0)
	free(errmsg);
}

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,	/* -D */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:?",
    .long_options = longopts,
    .short_usage = "[options] ...",
};

int
main(int argc, char *argv[])
{
    int		c;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }
    argc -= opts.optind-1;
    argv += opts.optind-1;

    if (argc == 8 && !strcmp(argv[1], "tuple"))
	pmunits_roundtrip(1, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), atoi(argv[7]), 0, 0);
    else if (argc == 10 && !strcmp(argv[1], "tuple"))
	pmunits_roundtrip(1, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), atoi(argv[7]), atoi(argv[8]), atoi(argv[9]));
    else if (argc == 2 && strcmp(argv[1], "scan") == 0)
	pmunits_roundtrip_all(0);
    else if (argc == 2 && strcmp(argv[1], "debug-scan") == 0)
	pmunits_roundtrip_all(1);
    else if (argc == 3 && strcmp(argv[1], "parse") == 0)
	pmunits_parse(argv[2]);
    else {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "    tuple dspace dtime dcount sspace stime scount\n");
	fprintf(stderr, "or  tuple dspace dtime dcount sspace stime scount xunit xscale\n");
	fprintf(stderr, "or  parse string\n");
	fprintf(stderr, "or  scan\n");
	fprintf(stderr, "or  debug-scan\n");
	return -1;
    }

    return 0;
}
