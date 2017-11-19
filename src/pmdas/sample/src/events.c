/*
 * The "event" records here are all fake.  But the logic does show how
 * a real PMDA could deliver values for metrics of type PM_TYPE_EVENT
 * and PM_TYPE_HIGHRES_EVENT.
 *
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
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
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pcp/pmda.h>
#include "events.h"

static int		nfetch;
static int		xnfetch;
static int		myarray[2];

static int		nhrfetch;
static int		xnhrfetch;
static int		hrarray[2];

/* event parameters */
static pmValueBlock	*aggr;
static char		aggrval[] = { '\01', '\03', '\07', '\017', '\037', '\077', '\177', '\377' };
static pmID		pmid_type = PMDA_PMID(0,127);	/* event.type */
static pmID		pmid_32 = PMDA_PMID(0,128);	/* event.param_32 */
static pmID		pmid_u32 = PMDA_PMID(0,129);	/* event.param_u32 */
static pmID		pmid_64 = PMDA_PMID(0,130);	/* event.param_64 */
static pmID		pmid_u64 = PMDA_PMID(0,131);	/* event.param_u64 */
static pmID		pmid_float = PMDA_PMID(0,132);	/* event.param_float */
static pmID		pmid_double = PMDA_PMID(0,133);	/* event.param_double */
static pmID		pmid_string = PMDA_PMID(0,134);	/* event.param_string */
static pmID		pmid_aggregate = PMDA_PMID(0,135);	/* event.param_aggregate */

void
init_events(int domain)
{
    int	i, sts;

    /*
     * fix the domain field in the event parameter PMIDs ...
     * note these PMIDs must match the corresponding metrics in
     * desctab[] and this cannot easily be done automatically
     */
    pmid_type = pmID_build(domain, pmID_cluster(pmid_type), pmID_item(pmid_type));
    pmid_32 = pmID_build(domain, pmID_cluster(pmid_32), pmID_item(pmid_32));
    pmid_u32 = pmID_build(domain, pmID_cluster(pmid_u32), pmID_item(pmid_u32));
    pmid_64 = pmID_build(domain, pmID_cluster(pmid_64), pmID_item(pmid_64));
    pmid_u64 = pmID_build(domain, pmID_cluster(pmid_u64), pmID_item(pmid_u64));
    pmid_float = pmID_build(domain, pmID_cluster(pmid_float), pmID_item(pmid_float));
    pmid_double = pmID_build(domain, pmID_cluster(pmid_double), pmID_item(pmid_double));
    pmid_string = pmID_build(domain, pmID_cluster(pmid_string), pmID_item(pmid_string));
    pmid_aggregate = pmID_build(domain, pmID_cluster(pmid_aggregate), pmID_item(pmid_aggregate));

    /* build pmValueBlock for aggregate value */
    aggr = (pmValueBlock *)malloc(PM_VAL_HDR_SIZE + sizeof(aggrval));
    aggr->vtype = PM_TYPE_AGGREGATE;
    aggr->vlen = PM_VAL_HDR_SIZE + sizeof(aggrval);
    memcpy(aggr->vbuf, (void *)aggrval, sizeof(aggrval));

    /* setup event arrays for 2 instances */
    for (i = 0; i < 2; i++) {
	if ((sts = myarray[i] = pmdaEventNewArray()) < 0)
	    fprintf(stderr, "pmdaEventNewArray: %s\n", pmErrStr(sts));
	if ((sts = hrarray[i] = pmdaEventNewHighResArray()) < 0)
	    fprintf(stderr, "pmdaEventNewHighResArray: %s\n", pmErrStr(sts));
    }
}

int
event_get_fetch_count(void)
{
    return nfetch % 4;
}

void
event_set_fetch_count(int c)
{
    xnfetch = nfetch = c;
}

int
sample_fetch_events(pmValueBlock **vbpp, int inst)
{
    int			c;
    int			sts;
    int			flags;
    struct timeval	stamp;
    pmAtomValue		atom;

    if (nfetch >= 0)
	c = nfetch % 4;
    else /* one of the error injection cases */
	c = nfetch;

    if (inst == PM_IN_NULL || inst > 1 || inst < 0)
	inst = 1;

    pmdaEventResetArray(myarray[inst]);
    gettimeofday(&stamp, NULL);

    if (inst == 0) {
	/* original instance ... */
	/* rebase event records 10 secs in past, add 1 sec for each new record */
	stamp.tv_sec -= 10;

	switch (c) {
	    case 0:
		/*
		 * 1st fetch
		 * No events
		 */
		 break;
	    case 1:
		/*
		 * 2nd fetch
		 * 1 event with NO parameters
		 */
		flags = PM_EVENT_FLAG_POINT;
		if ((sts = pmdaEventAddRecord(myarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		break;
	    case 2:
		/*
		 * 3rd fetch
		 * 1 event with one U32 parameter
		 * 1 event with 2 parameters(U32 and 64 types)
		 */
		flags = PM_EVENT_FLAG_POINT;
		if ((sts = pmdaEventAddRecord(myarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		atom.ul = 1;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_type, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		if ((sts = pmdaEventAddRecord(myarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		atom.ul = 2;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_type, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		atom.ll = -3;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_64, PM_TYPE_64, &atom)) < 0)
		    return sts;
		break;
	    case 3:
		/*
		 * 4th fetch
		 * 1 event start with 3 parameters (U32, U64 and STRING types)
		 * 1 event with 3 parameters (U32 and 2 DOUBLE types)
		 * 1 event end with 6 (U32, U64, STRING, STRING, 32 and U32 types)
		 * 7 "missed" events
		 * 1 event with 3 parameters (U32, FLOAT and AGGREGATE types)
		 */
		flags = PM_EVENT_FLAG_START|PM_EVENT_FLAG_ID|PM_EVENT_FLAG_PARENT;
		if ((sts = pmdaEventAddRecord(myarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		atom.ul = 4;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_type, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		atom.ull = 5;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_u64, PM_TYPE_U64, &atom)) < 0)
		    return sts;
		atom.cp = "6";
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_string, PM_TYPE_STRING, &atom)) < 0)
		    return sts;
		flags = PM_EVENT_FLAG_POINT;
		if ((sts = pmdaEventAddRecord(myarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		atom.ul = 7;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_type, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		atom.d = 8;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_double, PM_TYPE_DOUBLE, &atom)) < 0)
		    return sts;
		atom.d = -9;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_double, PM_TYPE_DOUBLE, &atom)) < 0)
		    return sts;
		flags = PM_EVENT_FLAG_END;
		if ((sts = pmdaEventAddRecord(myarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		atom.ul = 10;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_type, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		atom.ull = 11;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_u64, PM_TYPE_U64, &atom)) < 0)
		    return sts;
		atom.cp = "twelve";
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_string, PM_TYPE_STRING, &atom)) < 0)
		    return sts;
		atom.cp = "thirteen";
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_string, PM_TYPE_STRING, &atom)) < 0)
		    return sts;
		atom.l = -14;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_32, PM_TYPE_32, &atom)) < 0)
		    return sts;
		atom.ul = 15;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_u32, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		/* "missed" 7 records */
		if ((sts = pmdaEventAddMissedRecord(myarray[inst], &stamp, 7)) < 0)
		    return sts;
		stamp.tv_sec++;
		flags = PM_EVENT_FLAG_POINT;
		if ((sts = pmdaEventAddRecord(myarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		atom.ul = 16;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_type, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		atom.f = -17;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_float, PM_TYPE_FLOAT, &atom)) < 0)
		    return sts;
		atom.vbp = aggr;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_aggregate, PM_TYPE_AGGREGATE, &atom)) < 0)
		    return sts;
		break;
	    case -1:
		/* error injection */
		flags = PM_EVENT_FLAG_POINT;
		if ((sts = pmdaEventAddRecord(myarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		atom.ul = c;
		if ((sts = pmdaEventAddParam(myarray[inst], pmid_type, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		/* pmid that is not in PMNS and not known to the PMDA */
		if ((sts = pmdaEventAddParam(myarray[inst], PMDA_PMID(100,200), PM_TYPE_U32, &atom)) < 0)
		    return sts;
		break;
	}
	nfetch++;
    }
    else {
	/*
	 * new, boring instance ..., either instance ["bogus"] for
	 * sample.events.rrecord or singular instance for
	 * sample.events.no_indom_records
	 */
	static char hrecord1[20];
	static char hrecord2[] = "bingo!";

	flags = PM_EVENT_FLAG_POINT;
	if ((sts = pmdaEventAddRecord(myarray[inst], &stamp, flags)) < 0)
	    return sts;
	pmsprintf(hrecord1, sizeof(hrecord1), "fetch #%d", xnfetch);
	atom.cp = hrecord1;
	if ((sts = pmdaEventAddParam(myarray[inst], pmid_string, PM_TYPE_STRING, &atom)) < 0)
	    return sts;
	if ((xnfetch % 3) == 0) {
	    if ((sts = pmdaEventAddRecord(myarray[inst], &stamp, flags)) < 0)
		return sts;
	    atom.cp = hrecord2;
	    if ((sts = pmdaEventAddParam(myarray[inst], pmid_string, PM_TYPE_STRING, &atom)) < 0)
		return sts;
	}
	xnfetch++;
    }

    *vbpp = (pmValueBlock *)pmdaEventGetAddr(myarray[inst]);

    return 0;
}

int
event_get_highres_fetch_count(void)
{
    return nhrfetch % 4;
}

void
event_set_highres_fetch_count(int c)
{
    xnhrfetch = nhrfetch = c;
}

int
sample_fetch_highres_events(pmValueBlock **vbpp, int inst)
{
    int			c;
    int			sts;
    int			flags;
    struct timespec	stamp;
    pmAtomValue		atom;

    if (nhrfetch >= 0)
	c = nhrfetch % 4;
    else /* one of the error injection cases */
	c = nhrfetch;

    if (inst == PM_IN_NULL || inst > 1 || inst < 0)
	inst = 1;

    pmdaEventResetHighResArray(hrarray[inst]);
    __pmGetTimespec(&stamp);

    if (inst == 0) {
	/* original instance ... */
	/* rebase event records 10 secs in past, add 1 sec for each new record */
	stamp.tv_sec -= 10;

	switch (c) {
	    case 0:
		/*
		 * 1st fetch
		 * No events
		 */
		 break;
	    case 1:
		/*
		 * 2nd fetch
		 * 1 event with NO parameters
		 */
		flags = PM_EVENT_FLAG_POINT;
		if ((sts = pmdaEventAddHighResRecord(hrarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		break;
	    case 2:
		/*
		 * 3rd fetch
		 * 1 event with one U32 parameter
		 * 1 event with 2 parameters(U32 and 64 types)
		 */
		flags = PM_EVENT_FLAG_POINT;
		if ((sts = pmdaEventAddHighResRecord(hrarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		atom.ul = 1;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_type, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		if ((sts = pmdaEventAddHighResRecord(hrarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		atom.ul = 2;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_type, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		atom.ll = -3;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_64, PM_TYPE_64, &atom)) < 0)
		    return sts;
		break;
	    case 3:
		/*
		 * 4th fetch
		 * 1 event start with 3 parameters (U32, U64 and STRING types)
		 * 1 event with 3 parameters (U32 and 2 DOUBLE types)
		 * 1 event end with 6 (U32, U64, STRING, STRING, 32 and U32 types)
		 * 7 "missed" events
		 * 1 event with 3 parameters (U32, FLOAT and AGGREGATE types)
		 */
		flags = PM_EVENT_FLAG_START|PM_EVENT_FLAG_ID|PM_EVENT_FLAG_PARENT;
		if ((sts = pmdaEventAddHighResRecord(hrarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		atom.ul = 4;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_type, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		atom.ull = 5;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_u64, PM_TYPE_U64, &atom)) < 0)
		    return sts;
		atom.cp = "6";
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_string, PM_TYPE_STRING, &atom)) < 0)
		    return sts;
		flags = PM_EVENT_FLAG_POINT;
		if ((sts = pmdaEventAddHighResRecord(hrarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		atom.ul = 7;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_type, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		atom.d = 8;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_double, PM_TYPE_DOUBLE, &atom)) < 0)
		    return sts;
		atom.d = -9;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_double, PM_TYPE_DOUBLE, &atom)) < 0)
		    return sts;
		flags = PM_EVENT_FLAG_END;
		if ((sts = pmdaEventAddHighResRecord(hrarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		atom.ul = 10;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_type, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		atom.ull = 11;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_u64, PM_TYPE_U64, &atom)) < 0)
		    return sts;
		atom.cp = "twelve";
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_string, PM_TYPE_STRING, &atom)) < 0)
		    return sts;
		atom.cp = "thirteen";
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_string, PM_TYPE_STRING, &atom)) < 0)
		    return sts;
		atom.l = -14;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_32, PM_TYPE_32, &atom)) < 0)
		    return sts;
		atom.ul = 15;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_u32, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		/* "missed" 7000 records */
		if ((sts = pmdaEventAddHighResMissedRecord(hrarray[inst], &stamp, 7000)) < 0)
		    return sts;
		stamp.tv_sec++;
		flags = PM_EVENT_FLAG_POINT;
		if ((sts = pmdaEventAddHighResRecord(hrarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		atom.ul = 16;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_type, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		atom.f = -17;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_float, PM_TYPE_FLOAT, &atom)) < 0)
		    return sts;
		atom.vbp = aggr;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_aggregate, PM_TYPE_AGGREGATE, &atom)) < 0)
		    return sts;
		break;
	    case -1:
		/* error injection */
		flags = PM_EVENT_FLAG_POINT;
		if ((sts = pmdaEventAddHighResRecord(hrarray[inst], &stamp, flags)) < 0)
		    return sts;
		stamp.tv_sec++;
		atom.ul = c;
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_type, PM_TYPE_U32, &atom)) < 0)
		    return sts;
		/* pmid that is not in PMNS and not known to the PMDA */
		if ((sts = pmdaEventHighResAddParam(hrarray[inst], PMDA_PMID(100,200), PM_TYPE_U32, &atom)) < 0)
		    return sts;
		break;
	}
	nhrfetch++;
    }
    else {
	/*
	 * new, boring instance ..., either instance ["bogus"] for
	 * sample.events.rrecord or singular instance for
	 * sample.events.no_indom_records
	 */
	static char record1[20];
	static char record2[] = "bingo!";

	flags = PM_EVENT_FLAG_POINT;
	if ((sts = pmdaEventAddHighResRecord(hrarray[inst], &stamp, flags)) < 0)
	    return sts;
	pmsprintf(record1, sizeof(record1), "fetch #%d", xnhrfetch);
	atom.cp = record1;
	if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_string, PM_TYPE_STRING, &atom)) < 0)
	    return sts;
	if ((xnhrfetch % 3) == 0) {
	    if ((sts = pmdaEventAddHighResRecord(hrarray[inst], &stamp, flags)) < 0)
		return sts;
	    atom.cp = record2;
	    if ((sts = pmdaEventHighResAddParam(hrarray[inst], pmid_string, PM_TYPE_STRING, &atom)) < 0)
		return sts;
	}
	xnhrfetch++;
    }

    *vbpp = (pmValueBlock *)pmdaEventHighResGetAddr(hrarray[inst]);

    return 0;
}
