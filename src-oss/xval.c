/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <stdio.h>
#include <math.h>
#include <pcp/pmapi.h>

static int type[] = {
    PM_TYPE_32, PM_TYPE_U32, PM_TYPE_64, PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE, PM_TYPE_STRING, PM_TYPE_AGGREGATE };
static char *name[] = {
    "long", "ulong", "longlong", "ulonglong", "float", "double", "char *", "void *" };

static void
_y(pmAtomValue *ap, int type)
{
    long	*lp;

    switch (type) {
	case PM_TYPE_32:
	    printf("%d", ap->l);
	    break;
	case PM_TYPE_U32:
	    printf("%u", ap->ul);
	    break;
	case PM_TYPE_64:
	    printf("%lld", ap->ll);
	    break;
	case PM_TYPE_U64:
	    printf("%llu", ap->ull);
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
	    lp = ap->vp;
	    if (lp == (long *)0)
		printf("(void *)0");
	    else
		printf("%08x %08x %08x", lp[0], lp[1], lp[2]);
	    break;
    }
}

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
    pmValueBlock	*vb;
    int		foo[7];		/* foo[0] = len, foo[1]... = vbuf */
    int		valfmt;
    int		vflag = 0;	/* set to 1 to show all results */
    long long 	lv;
    double	v;
    int		match;

    vb = (pmValueBlock *)&foo;

    while (argc > 1) {
	sscanf(argv[1], "%llx", &lv);
	printf("\nValue: %lld 0x%016llx\n", lv, lv);
	argc--;
	argv++;

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
		    pv.value.pval = vb;
		    vb->vlen = PM_VAL_HDR_SIZE + sizeof(__int64_t);
		    vb->vtype = PM_TYPE_64;
		    ap = (pmAtomValue *)vb->vbuf;
		    ap->ll = lv;
		    break;
		case PM_TYPE_U64:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vb;
		    vb->vlen = PM_VAL_HDR_SIZE + sizeof(__uint64_t);
		    vb->vtype = PM_TYPE_U64;
		    ap = (pmAtomValue *)vb->vbuf;
		    ap->ull = lv;
		    break;
		case PM_TYPE_FLOAT:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vb;
		    vb->vlen = PM_VAL_HDR_SIZE + sizeof(float);
		    vb->vtype = PM_TYPE_FLOAT;
		    ap = (pmAtomValue *)vb->vbuf;
		    ap->f = lv;
		    break;
		case PM_TYPE_DOUBLE:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vb;
		    vb->vlen = PM_VAL_HDR_SIZE + sizeof(double);
		    vb->vtype = PM_TYPE_DOUBLE;
		    ap = (pmAtomValue *)vb->vbuf;
		    ap->d = lv;
		    break;
		case PM_TYPE_STRING:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vb;
		    sprintf(vb->vbuf, "%e", v);
		    vb->vlen = PM_VAL_HDR_SIZE + strlen(vb->vbuf) + 1;
		    vb->vtype = PM_TYPE_STRING;
		    ap = &bv;
		    bv.cp = (char *)vb->vbuf;
		    break;
		case PM_TYPE_AGGREGATE:
		    valfmt = PM_VAL_SPTR;
		    pv.value.pval = vb;
		    vb->vlen = PM_VAL_HDR_SIZE + 3 * sizeof(foo[0]);
		    vb->vtype = PM_TYPE_AGGREGATE;
		    foo[1] = foo[2] = foo[3] = lv;
		    ap = &bv;
		    bv.vp = (void *)vb->vbuf;
		    break;
	    }
	    for (o = 0; o < sizeof(type)/sizeof(type[0]); o++) {
		if ((e = pmExtractValue(valfmt, &pv, type[i], &av, type[o])) < 0) {
		    if (vflag == 0 && type[i] != type[o] &&
			(type[i] == PM_TYPE_STRING || type[o] == PM_TYPE_STRING ||
			 type[i] == PM_TYPE_AGGREGATE || type[o] == PM_TYPE_AGGREGATE))
			    continue;
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
			    for (k = 0; match == 0 && k < vb->vlen - PM_VAL_HDR_SIZE; k++)
				match = (bv.cp[k] == av.cp[k]);
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
			free(av.vp);
		}
	    }
	}
    }

    exit(0);
}
