/*
 * Fiddle with __pmResult => pmResult
 *
 * Copyright (c) 2021 Ken McDonell.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * Debug flags:
 * -Dappl0	report added/dropped instances
 */
#include <pmapi.h>
#include <libpcp.h>
#include <assert.h>


// TODO -remove this when __pmResultAlloc is in libpcp
/*
 * Linked list of allocations made by __pmResultAlloc(), but not
 * yet released by pmFreeResult().
 */
typedef struct result_pool {
    struct result_pool	*next;
    __pmResult		*__rp;
    pmResult		*rp;
} result_pool;

// TODO -remove this when __pmResultAlloc is in libpcp
result_pool	*head_pool;

// TODO -remove this when __pmOffsetResult is in libpcp.h
/*
 * __pmResult and pmResult are identical after the timestamp.
 * If we're exporting a pmResult from a __pmResult, this function
 * returns the correct address for the pmResult at, or shortly
 * after, the start of the __pmResult (rp)
 */
inline pmResult *
__pmOffsetResult(__pmResult *rp)
{
    pmResult	*tmp;
   return (pmResult *)&(((char *)&rp->numpmid)[(char *)tmp - (char *)&tmp->numpmid]);
}

// TODO -remove this when __pmResultAlloc is in libpcp
__pmResult *
__pmResultAlloc(int need)
{
    result_pool	*new = (result_pool *)malloc(sizeof(*new));

    if (new == NULL) {
	pmNoMem("__pmResultAlloc: new", sizeof(*new), PM_FATAL_ERR);
	/* NOTREACHED */
    }

    new->__rp = (__pmResult *)malloc(need);
    if (new->__rp == NULL) {
	free(new);
	pmNoMem("__pmResultAlloc: __pmResult", need, PM_FATAL_ERR);
	/* NOTREACHED */
    }
    new->rp = __pmOffsetResult(new->__rp);

    new->next = head_pool;
    head_pool = new;

    return new->__rp;
}


// TODO -remove this when revised pmFreeResult is in libpcp
void
pmFreeResult(pmResult *result)
{
    result_pool	*pool;
    result_pool	*prior = NULL;

    if (pmDebugOptions.pdubuf)
	fprintf(stderr, "pmFreeResult(" PRINTF_P_PFX "%p)", result);

    for (pool = head_pool; pool != NULL; pool = pool->next) {
	if (pool->rp == result) {
	    if (pmDebugOptions.pdubuf)
		fprintf(stderr, " [in " PRINTF_P_PFX "%p]", pool->__rp);
	    break;
	}
    }
    if (pmDebugOptions.pdubuf)
	fputc('\n', stderr);

    __pmFreeResultValues(result);
    if (pool->rp == result) {
	free(pool->__rp);
	if (prior == NULL)
	    head_pool = pool->next;
	else
	    prior->next = pool->next;
	free(pool);
    }
    else
	free(result);
}


int
main(void)
{
    __pmResult		*in;
    pmResult		*out;
    __pmTimestamp	stamp;
    int			need;
    int			numpmid = 3;
    double		value;
    int			nch;

    pmSetDebug("pdubuf");

    need = sizeof(__pmResult) + (numpmid-1)*sizeof(pmValueSet *);
    in = __pmResultAlloc(need);
    in->timestamp.sec = 4321;
    in->timestamp.nsec = 123456789;
    in->numpmid = numpmid;
    printf("in: stamp=%" FMT_INT64 ".%09d numpmid=%d\n", in->timestamp.sec, in->timestamp.nsec, in->numpmid);

    /* 1st metric, singular, 32-bit insitu value */
    in->vset[0] = (pmValueSet *)malloc(sizeof(pmValueSet));
    in->vset[0]->pmid = pmID_build(12,34,56);
    in->vset[0]->numval = 1;
    in->vset[0]->valfmt = PM_VAL_INSITU;
    in->vset[0]->vlist[0].inst = PM_IN_NULL;
    in->vset[0]->vlist[0].value.lval = 42;

    /* 2nd metric, 2 instances, double value */
    in->vset[1] = (pmValueSet *)malloc(sizeof(pmValueSet)+sizeof(pmValue));
    in->vset[1]->pmid = pmID_build(23,45,56);
    in->vset[1]->numval = 2;
    in->vset[1]->valfmt = PM_VAL_DPTR;
    in->vset[1]->vlist[0].inst = 42;
    in->vset[1]->vlist[0].value.pval = (pmValueBlock *)malloc(sizeof(int)+sizeof(double));
    in->vset[1]->vlist[0].value.pval->vtype = PM_TYPE_DOUBLE;
    in->vset[1]->vlist[0].value.pval->vlen = sizeof(int)+sizeof(double);
    value = 1234.5678;
    memmove((void *)in->vset[1]->vlist[0].value.pval->vbuf, (void *)&value, sizeof(value));
    in->vset[1]->vlist[1].inst = 43;
    in->vset[1]->vlist[1].value.pval = (pmValueBlock *)malloc(sizeof(int)+sizeof(double));
    in->vset[1]->vlist[1].value.pval->vtype = PM_TYPE_DOUBLE;
    in->vset[1]->vlist[1].value.pval->vlen = sizeof(int)+sizeof(double);
    value = 8765.4321;
    memmove((void *)in->vset[1]->vlist[1].value.pval->vbuf, (void *)&value, sizeof(value));

    /* 3rd metric, 3 instances, dynamic string value */
    in->vset[2] = (pmValueSet *)malloc(sizeof(pmValueSet)+2*sizeof(pmValue));
    in->vset[2]->pmid = pmID_build(34,56,78);
    in->vset[2]->numval = 3;
    in->vset[2]->valfmt = PM_VAL_DPTR;
    in->vset[2]->vlist[0].inst = 0;
    nch = strlen("mary had");
    in->vset[2]->vlist[0].value.pval = (pmValueBlock *)malloc(sizeof(int)+nch);
    in->vset[2]->vlist[0].value.pval->vtype = PM_TYPE_STRING;
    in->vset[2]->vlist[0].value.pval->vlen = sizeof(int)+nch;
    memmove((void *)in->vset[2]->vlist[0].value.pval->vbuf, (void *)"mary had", nch);
    in->vset[2]->vlist[1].inst = 1;
    nch = strlen("a little");
    in->vset[2]->vlist[1].value.pval = (pmValueBlock *)malloc(sizeof(int)+nch);
    in->vset[2]->vlist[1].value.pval->vtype = PM_TYPE_STRING;
    in->vset[2]->vlist[1].value.pval->vlen = sizeof(int)+nch;
    memmove((void *)in->vset[2]->vlist[1].value.pval->vbuf, (void *)"a little", nch);
    in->vset[2]->vlist[2].inst = 2;
    nch = strlen("lanb");
    in->vset[2]->vlist[2].value.pval = (pmValueBlock *)malloc(sizeof(int)+nch);
    in->vset[2]->vlist[2].value.pval->vtype = PM_TYPE_STRING;
    in->vset[2]->vlist[2].value.pval->vlen = sizeof(int)+nch;
    memmove((void *)in->vset[2]->vlist[2].value.pval->vbuf, (void *)"lamb", nch);
    __pmPrintResult(stdout, in);

    out = __pmOffsetResult(in);
    assert(&in->numpmid == &out->numpmid);

    stamp.sec = in->timestamp.sec;
    stamp.nsec = in->timestamp.nsec;
    out->timestamp.tv_sec = stamp.sec;
    out->timestamp.tv_usec = stamp.nsec / 1000;
    printf("out: stamp=%ld.%06ld numpmid=%d\n", (long)out->timestamp.tv_sec, (long)out->timestamp.tv_usec, out->numpmid);
    __pmDumpResult(stdout, out);

    pmFreeResult(out);

    return(0);

}
