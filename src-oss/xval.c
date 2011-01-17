/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * xval [-D] [-e] [-u] [-v] value-in-hex
 */

#include <stdio.h>
#include <math.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

static int type[] = {
    PM_TYPE_32, PM_TYPE_U32, PM_TYPE_64, PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE, PM_TYPE_STRING, PM_TYPE_AGGREGATE, PM_TYPE_EVENT };
static char *name[] = {
    "long", "ulong", "longlong", "ulonglong", "float", "double", "char *", "pmValueBlock *", "pmEventArray" };

static void
_y(pmAtomValue *ap, int type)
{
    long		*lp;
    pmEventArray	*eap;

    switch (type) {
	case PM_TYPE_32:
	    printf("%d", ap->l);
	    break;
	case PM_TYPE_U32:
	    printf("%u", ap->ul);
	    break;
	case PM_TYPE_64:
	    printf("%lld", (long long)ap->ll);
	    break;
	case PM_TYPE_U64:
	    printf("%llu", (unsigned long long)ap->ull);
	    break;
	case PM_TYPE_FLOAT:
	    printf("%e", (double)ap->f);
	    break;
	case PM_TYPE_DOUBLE:
	    printf("%e", ap->d);
	    break;
	case PM_TYPE_STRING:
	    if (ap->cp == (char *)0)
		printf("(char *)0");
	    else
		printf("\"%s\"", ap->cp);
	    break;
	case PM_TYPE_AGGREGATE:
	    printf("[vlen=%d] ", ap->vbp->vlen - PM_VAL_HDR_SIZE);
	    lp = (long *)ap->vbp->vbuf;
	    if (ap->vbp->vlen - PM_VAL_HDR_SIZE == 0)
		;
	    else if (ap->vbp->vlen - PM_VAL_HDR_SIZE <= 4)
		printf("%08x", (unsigned)lp[0]);
	    else if (ap->vbp->vlen - PM_VAL_HDR_SIZE <= 8)
		printf("%08x %08x", (unsigned)lp[0], (unsigned)lp[1]);
	    else if (ap->vbp->vlen - PM_VAL_HDR_SIZE <= 12)
		printf("%08x %08x %08x",
			(unsigned)lp[0], (unsigned)lp[1], (unsigned)lp[2]);
	    else
		printf("%08x %08x %08x ...",
			(unsigned)lp[0], (unsigned)lp[1], (unsigned)lp[2]);
	    break;
	case PM_TYPE_EVENT:
	    eap = (pmEventArray *)ap;
		printf("[%d event records]", eap->ea_nrecords);
	    break;
    }
}

static pmEventArray	ea;

int
main(int argc, char *argv[])
{
    int		e;
    int		k;
    int		i;
    int		o;
    pmValue	pv;
    pmAtomValue	av;
    pmAtomValue	bv;
    pmAtomValue	*ap;
    pmValueBlock	*vbp;
    int		foo[7];		/* foo[0] = len/type, foo[1]... = vbuf */
    int		valfmt;
    int		vflag = 0;	/* set to 1 to show all results */
    int		sgn = 1;	/* -u to force 64 unsigned from command line value */
    long long 	llv;
    int		match;

    ea.ea_nrecords = 1;
    ea.ea_record[0].er_nparams = 0;
    vbp = (pmValueBlock *)&ea;
    vbp->vtype = PM_TYPE_EVENT;
    vbp->vlen = sizeof(ea);	/* not quite correct, but close enough */

    vbp = (pmValueBlock *)&foo;

    while (argc > 1) {
	argc--;
	argv++;
	if (strcmp(argv[0], "-D") == 0) {
	    pmDebug = DBG_TRACE_VALUE;
	    continue;
	}
	if (strcmp(argv[0], "-e") == 0) {
	    goto error_cases;
	}
	if (strcmp(argv[0], "-u") == 0) {
	    sgn = 0;
	    continue;
	}
	if (strcmp(argv[0], "-v") == 0) {
	    vflag = 1;
	    continue;
	}
	/* note value is in hex! */
	sscanf(argv[0], "%llx", &llv);
	if (sgn)
	    printf("\nValue: %lld 0x%016llx\n", llv, llv);
	else
	    printf("\nValue: %llu 0x%016llx\n", (unsigned long long)llv, llv);

	for (i = 0; i < sizeof(type)/sizeof(type[0]); i++) {
	    valfmt = PM_VAL_INSITU;
	    ap = (pmAtomValue *)&pv.value.lval;
	    switch (type[i]) {
		case PM_TYPE_32:
		case PM_TYPE_U32:
		    pv.value.lval = llv;
		    break;
		case PM_TYPE_64:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vbp;
		    vbp->vlen = PM_VAL_HDR_SIZE + sizeof(__int64_t);
		    vbp->vtype = PM_TYPE_64;
		    ap = (pmAtomValue *)vbp->vbuf;
		    if (sgn)
			ap->ll = llv;
		    else
			ap->ll = (unsigned long long)llv;
		    break;
		case PM_TYPE_U64:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vbp;
		    vbp->vlen = PM_VAL_HDR_SIZE + sizeof(__uint64_t);
		    vbp->vtype = PM_TYPE_U64;
		    ap = (pmAtomValue *)vbp->vbuf;
		    if (sgn)
			ap->ull = llv;
		    else
			ap->ull = (unsigned long long)llv;
		    break;
		case PM_TYPE_FLOAT:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vbp;
		    vbp->vlen = PM_VAL_HDR_SIZE + sizeof(float);
		    vbp->vtype = PM_TYPE_FLOAT;
		    ap = (pmAtomValue *)vbp->vbuf;
		    if (sgn)
			ap->f = llv;
		    else
			ap->f = (unsigned long long)llv;
		    break;
		case PM_TYPE_DOUBLE:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vbp;
		    vbp->vlen = PM_VAL_HDR_SIZE + sizeof(double);
		    vbp->vtype = PM_TYPE_DOUBLE;
		    ap = (pmAtomValue *)vbp->vbuf;
		    if (sgn)
			ap->d = llv;
		    else
			ap->d = (unsigned long long)llv;
		    break;
		case PM_TYPE_STRING:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vbp;
		    sprintf(vbp->vbuf, "%lld", llv);
		    vbp->vlen = PM_VAL_HDR_SIZE + strlen(vbp->vbuf) + 1;
		    vbp->vtype = PM_TYPE_STRING;
		    ap = &bv;
		    bv.cp = (char *)vbp->vbuf;
		    break;
		case PM_TYPE_AGGREGATE:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vbp;
		    /* foo[0] is for vlen/vtype */
		    vbp->vlen = PM_VAL_HDR_SIZE + 3 * sizeof(foo[0]);
		    vbp->vtype = PM_TYPE_AGGREGATE;
		    foo[1] = foo[2] = foo[3] = llv;
		    foo[1]--;
		    foo[3]++;
		    ap = &bv;
		    bv.vbp = vbp;
		    break;
		case PM_TYPE_EVENT:
		    valfmt = PM_VAL_DPTR;
		    ap = (void *)&ea;
		    break;
	    }
	    for (o = 0; o < sizeof(type)/sizeof(type[0]); o++) {
		if ((e = pmExtractValue(valfmt, &pv, type[i], &av, type[o])) < 0) {
		    if (vflag == 0) {
			/* silently ignore the expected failures */
			if (type[i] != type[o] &&
			    (type[i] == PM_TYPE_STRING || type[o] == PM_TYPE_STRING ||
			     type[i] == PM_TYPE_AGGREGATE || type[o] == PM_TYPE_AGGREGATE))
			    continue;
			 if (type[i] == PM_TYPE_EVENT || type[o] == PM_TYPE_EVENT)
			    continue;
		    }
		    printf("(%s) ", name[i]);
		    _y(ap, type[i]);
		    printf(" => (%s) ", name[o]);
		    printf(": %s\n", pmErrStr(e));
		}
		else {
		    switch (type[o]) {
			case PM_TYPE_32:
			    if (type[i] == PM_TYPE_32)
				match = (pv.value.lval == av.l);
			    else if (type[i] == PM_TYPE_U32)
				match = ((unsigned)pv.value.lval == av.l);
			    else if (type[i] == PM_TYPE_FLOAT)
				match = (fabs((ap->f - av.l)/ap->f) < 0.00001);
			    else
				match = (llv == av.l);
			    break;
			case PM_TYPE_U32:
			    if (type[i] == PM_TYPE_32)
				match = (pv.value.lval == av.ul);
			    else if (type[i] == PM_TYPE_U32)
				match = ((unsigned)pv.value.lval == av.ul);
			    else if (type[i] == PM_TYPE_FLOAT)
				match = (fabs((ap->f - av.ul)/ap->f) < 0.00001);
			    else
				match = (llv == av.ul);
			    break;
			case PM_TYPE_64:
			    if (type[i] == PM_TYPE_32)
				match = (pv.value.lval == av.ll);
			    else if (type[i] == PM_TYPE_U32)
				match = ((unsigned)pv.value.lval == av.ll);
			    else if (type[i] == PM_TYPE_FLOAT)
				match = (fabs((ap->f - av.ll)/ap->f) < 0.00001);
			    else if (type[i] == PM_TYPE_DOUBLE)
				match = (fabs((ap->d - av.ll)/ap->d) < 0.00001);
			    else
				match = (llv == av.ll);
			    break;
			case PM_TYPE_U64:
			    if (type[i] == PM_TYPE_32)
				match = (pv.value.lval == av.ull);
			    else if (type[i] == PM_TYPE_U32)
				match = ((unsigned)pv.value.lval == av.ull);
			    else if (type[i] == PM_TYPE_FLOAT)
				match = (fabs((ap->f - av.ull)/ap->f) < 0.00001);
			    else if (type[i] == PM_TYPE_DOUBLE)
				match = (fabs((ap->d - av.ull)/ap->d) < 0.00001);
			    else
				match = (llv == av.ull);
			    break;
			case PM_TYPE_FLOAT:
			    if (type[i] == PM_TYPE_32)
				match = (fabs((pv.value.lval - av.f)/pv.value.lval) < 0.00001);
			    else if (type[i] == PM_TYPE_U32)
				match = (fabs(((unsigned)pv.value.lval - av.f)/(unsigned)pv.value.lval) < 0.00001);
			    else if (type[i] == PM_TYPE_64)
				match = (fabs((llv - av.f)/llv) < 0.00001);
			    else if (type[i] == PM_TYPE_U64)
				match = (fabs(((unsigned long long)llv - av.f)/(unsigned long long)llv) < 0.00001);
			    else if (type[i] == PM_TYPE_DOUBLE)
				match = (fabs((ap->d - av.f)/ap->d) < 0.00001);
			    else
				match = (ap->f == av.f);
			    break;
			case PM_TYPE_DOUBLE:
			    if (type[i] == PM_TYPE_32)
				match = (fabs((pv.value.lval - av.d)/pv.value.lval) < 0.00001);
			    else if (type[i] == PM_TYPE_U32)
				match = (fabs(((unsigned)pv.value.lval - av.d)/(unsigned)pv.value.lval) < 0.00001);
			    else if (type[i] == PM_TYPE_64)
				match = (fabs((llv - av.d)/llv) < 0.00001);
			    else if (type[i] == PM_TYPE_U64)
				match = (fabs(((unsigned long long)llv - av.d)/(unsigned long long)llv) < 0.00001);
			    else if (type[i] == PM_TYPE_FLOAT)
				match = (fabs((ap->f - av.d)/ap->f) < 0.00001);
			    else
				match = (ap->d == av.d);
			    break;
			case PM_TYPE_STRING:
			    match = (strcmp(bv.cp, av.cp) == 0);
			    break;
			case PM_TYPE_AGGREGATE:
			    match = 0;
			    for (k = 0; match == 0 && k < vbp->vlen - PM_VAL_HDR_SIZE; k++)
				match = (bv.cp[k] == av.cp[k]);
			    break;
			case PM_TYPE_EVENT:
			    /* should never get here */
			    match = 0;
			    break;
		    }
		    if (match == 0 || vflag) {
			printf("(%s) ", name[i]);
			_y(ap, type[i]);
			printf(" => (%s) ", name[o]);
			_y(&av, type[o]);
			if (match == 0)
			    printf(" : value mismatch");
			putchar('\n');
		    }
		    if (type[o] == PM_TYPE_STRING)
			free(av.cp);
		    else if (type[o] == PM_TYPE_AGGREGATE)
			free(av.vbp);
		}
	    }
	}
    }

    exit(0);

error_cases:
    /*
     * a series of error and odd cases driven by QA coverage analysis
     */
    ap = (pmAtomValue *)&pv.value.lval;
    ap->f = 123.456;
    printf("old FLOAT: %15.3f -> 32: ", ap->f);
    if ((e = pmExtractValue(PM_VAL_INSITU, &pv, PM_TYPE_FLOAT, &av, PM_TYPE_32)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%d\n", av.l);

    ap->f = (float)0xf7777777;
    printf("old FLOAT: %15.3f -> 32: ", ap->f);
    if ((e = pmExtractValue(PM_VAL_INSITU, &pv, PM_TYPE_FLOAT, &av, PM_TYPE_32)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%d\n", av.l);

    ap->f = 123.456;
    printf("old FLOAT: %15.3f -> U32: ", ap->f);
    if ((e = pmExtractValue(PM_VAL_INSITU, &pv, PM_TYPE_FLOAT, &av, PM_TYPE_U32)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%u\n", av.ul);

    ap->f = (float)((unsigned)0xffffffff);
    printf("old FLOAT: %15.3f -> U32: ", ap->f);
    if ((e = pmExtractValue(PM_VAL_INSITU, &pv, PM_TYPE_FLOAT, &av, PM_TYPE_U32)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%u\n", av.ul);

    ap->f = -123.456;
    printf("old FLOAT: %15.3f -> U32: ", ap->f);
    if ((e = pmExtractValue(PM_VAL_INSITU, &pv, PM_TYPE_FLOAT, &av, PM_TYPE_U32)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%u\n", av.ul);

    ap->f = 123.456;
    printf("old FLOAT: %15.3f -> 64: ", ap->f);
    if ((e = pmExtractValue(PM_VAL_INSITU, &pv, PM_TYPE_FLOAT, &av, PM_TYPE_64)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%lld\n", av.ll);

    ap->f = (float)0x7fffffffffffffffLL;
    printf("old FLOAT: %22.1f -> 64: ", ap->f);
    if ((e = pmExtractValue(PM_VAL_INSITU, &pv, PM_TYPE_FLOAT, &av, PM_TYPE_64)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%lld\n", av.ll);

    ap->f = 123.456;
    printf("old FLOAT: %15.3f -> U64: ", ap->f);
    if ((e = pmExtractValue(PM_VAL_INSITU, &pv, PM_TYPE_FLOAT, &av, PM_TYPE_U64)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%llu\n", av.ull);

    ap->f = (float)((unsigned long long)0xffffffffffffffffLL);
    printf("old FLOAT: %22.1f -> U64: ", ap->f);
    if ((e = pmExtractValue(PM_VAL_INSITU, &pv, PM_TYPE_FLOAT, &av, PM_TYPE_U64)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%llu\n", av.ull);

    ap->f = -123.456;
    printf("old FLOAT: %15.3f -> U64: ", ap->f);
    if ((e = pmExtractValue(PM_VAL_INSITU, &pv, PM_TYPE_FLOAT, &av, PM_TYPE_U64)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%llu\n", av.ull);

    ap->f = 123.45678;
    printf("old FLOAT: %15.5f -> DOUBLE: ", ap->f);
    if ((e = pmExtractValue(PM_VAL_INSITU, &pv, PM_TYPE_FLOAT, &av, PM_TYPE_DOUBLE)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%15.5f\n", av.d);

    ap->f = 123.45678;
    printf("old FLOAT: %15.5f -> STRING: ", ap->f);
    if ((e = pmExtractValue(PM_VAL_INSITU, &pv, PM_TYPE_FLOAT, &av, PM_TYPE_STRING)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%15.6f\n", av.d);

    pv.value.pval = vbp;
    vbp->vlen = PM_VAL_HDR_SIZE + sizeof(ap->ll);
    vbp->vtype = PM_TYPE_NOSUPPORT;
    ap = (pmAtomValue *)vbp->vbuf;
    ap->ll = 12345;
    printf("bad 64: %12lld -> 64: ", ap->ll);
    if ((e = pmExtractValue(PM_VAL_SPTR, &pv, PM_TYPE_64, &av, PM_TYPE_64)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%12lld\n", av.ll);

    vbp->vlen = PM_VAL_HDR_SIZE + sizeof(ap->ull);
    ap->ull = 12345;
    printf("bad U64: %12llu -> U64: ", ap->ull);
    if ((e = pmExtractValue(PM_VAL_SPTR, &pv, PM_TYPE_U64, &av, PM_TYPE_U64)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%12llu\n", av.ull);

    vbp->vlen = PM_VAL_HDR_SIZE + sizeof(ap->f);
    ap->f = 123.456;
    printf("bad FLOAT: %15.3f -> FLOAT: ", ap->f);
    if ((e = pmExtractValue(PM_VAL_SPTR, &pv, PM_TYPE_FLOAT, &av, PM_TYPE_FLOAT)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%15.3f\n", av.f);

    vbp->vlen = PM_VAL_HDR_SIZE + sizeof(ap->d);
    ap->d = 123.456;
    printf("bad DOUBLE: %15.3f -> DOUBLE: ", ap->d);
    if ((e = pmExtractValue(PM_VAL_SPTR, &pv, PM_TYPE_DOUBLE, &av, PM_TYPE_DOUBLE)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%15.3f\n", av.d);

    ap->cp = "not me";
    vbp->vlen = PM_VAL_HDR_SIZE + strlen(ap->cp);
    printf("bad STRING: %s -> STRING: ", ap->cp);
    if ((e = pmExtractValue(PM_VAL_SPTR, &pv, PM_TYPE_STRING, &av, PM_TYPE_STRING)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("%s\n", av.cp);

    vbp->vlen = PM_VAL_HDR_SIZE;
    printf("bad AGGREGATE: len=%d -> AGGREGATE: ", vbp->vlen - PM_VAL_HDR_SIZE);
    if ((e = pmExtractValue(PM_VAL_SPTR, &pv, PM_TYPE_AGGREGATE, &av, PM_TYPE_AGGREGATE)) < 0)
	printf("%s\n", pmErrStr(e));
    else
	printf("len=%d\n", av.vbp->vlen);

    exit(0);
}

