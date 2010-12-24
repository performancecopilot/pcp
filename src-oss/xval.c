/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <stdio.h>
#include <math.h>
#include <pcp/pmapi.h>

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
    int		foo[7];		/* foo[0] = len, foo[1]... = vbuf */
    int		valfmt;
    int		vflag = 0;	/* set to 1 to show all results */
    long long 	lv;
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
	if (strcmp(argv[0], "-v") == 0) {
	    vflag = 1;
	    continue;
	}
	/* note value is in hex! */
	sscanf(argv[0], "%llx", &lv);
	printf("\nValue: %lld 0x%016llx\n", lv, lv);

	for (i = 0; i < sizeof(type)/sizeof(type[0]); i++) {
	    valfmt = PM_VAL_INSITU;
	    ap = (pmAtomValue *)&pv.value.lval;
	    switch (type[i]) {
		case PM_TYPE_32:
		case PM_TYPE_U32:
		    pv.value.lval = lv;
		    break;
		case PM_TYPE_64:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vbp;
		    vbp->vlen = PM_VAL_HDR_SIZE + sizeof(__int64_t);
		    vbp->vtype = PM_TYPE_64;
		    ap = (pmAtomValue *)vbp->vbuf;
		    ap->ll = lv;
		    break;
		case PM_TYPE_U64:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vbp;
		    vbp->vlen = PM_VAL_HDR_SIZE + sizeof(__uint64_t);
		    vbp->vtype = PM_TYPE_U64;
		    ap = (pmAtomValue *)vbp->vbuf;
		    ap->ull = lv;
		    break;
		case PM_TYPE_FLOAT:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vbp;
		    vbp->vlen = PM_VAL_HDR_SIZE + sizeof(float);
		    vbp->vtype = PM_TYPE_FLOAT;
		    ap = (pmAtomValue *)vbp->vbuf;
		    ap->f = lv;
		    break;
		case PM_TYPE_DOUBLE:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vbp;
		    vbp->vlen = PM_VAL_HDR_SIZE + sizeof(double);
		    vbp->vtype = PM_TYPE_DOUBLE;
		    ap = (pmAtomValue *)vbp->vbuf;
		    ap->d = lv;
		    break;
		case PM_TYPE_STRING:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vbp;
		    sprintf(vbp->vbuf, "%lld", lv);
		    vbp->vlen = PM_VAL_HDR_SIZE + strlen(vbp->vbuf) + 1;
		    vbp->vtype = PM_TYPE_STRING;
		    ap = &bv;
		    bv.cp = (char *)vbp->vbuf;
		    break;
		case PM_TYPE_AGGREGATE:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vbp;
		    vbp->vlen = PM_VAL_HDR_SIZE + 3 * sizeof(foo[0]);
		    vbp->vtype = PM_TYPE_AGGREGATE;
		    foo[1] = foo[2] = foo[3] = lv;
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
			    else
				match = (lv == av.l);
			    break;
			case PM_TYPE_U32:
			    if (type[i] == PM_TYPE_32)
				match = (pv.value.lval == av.ul);
			    else if (type[i] == PM_TYPE_U32)
				match = ((unsigned)pv.value.lval == av.ul);
			    else
				match = (lv == av.ul);
			    break;
			case PM_TYPE_64:
			    if (type[i] == PM_TYPE_32)
				match = (pv.value.lval == av.ll);
			    else if (type[i] == PM_TYPE_U32)
				match = ((unsigned)pv.value.lval == av.ll);
			    else
				match = (lv == av.ll);
			    break;
			case PM_TYPE_U64:
			    if (type[i] == PM_TYPE_32)
				match = (pv.value.lval == av.ull);
			    else if (type[i] == PM_TYPE_U32)
				match = ((unsigned)pv.value.lval == av.ull);
			    else
				match = (lv == av.ull);
			    break;
			case PM_TYPE_FLOAT:
			    match = (fabs((lv - av.f)/lv) < 0.00001);
			    break;
			case PM_TYPE_DOUBLE:
			    match = (fabs((lv - av.d)/lv) < 0.00001);
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
}
