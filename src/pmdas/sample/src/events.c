/*
 * The "event" records here are all fake.  But the logic does show
 * how a real PMDA could deliver values for metrics of type
 * PM_TYPE_EVENT.
 *
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "events.h"

static int		nfetch = 0;
static char		*ebuf;
static int		ebuflen;
static char		*eptr = NULL;
static char		*ebufend;
static pmEventArray	*eap;
static pmEventRecord	*erp;

static int
check_buf(int need)
{
    int		offset = eptr - ebuf;

    while (&eptr[need] >= ebufend) {
	ebuflen *= 2;
	if ((ebuf = (char *)realloc(ebuf, ebuflen)) == NULL)
	    return -errno;
	eptr = &ebuf[offset];
	ebufend = &ebuf[ebuflen-1];
    }
    return 0;
}

static int
add_param(pmID pmid, int type, pmAtomValue *avp)
{
    int			need;		/* bytes in the buffer */
    int			vlen;		/* value only length */
    int			sts;
    pmEventParameter	*epp;
    void		*src;

    need = sizeof(pmEventParameter);
    switch (type) {
	case PM_TYPE_32:
	case PM_TYPE_U32:
	    vlen = sizeof(avp->l);
	    need += vlen;
	    src = &avp->l;
	    break;
	case PM_TYPE_64:
	case PM_TYPE_U64:
	    vlen = sizeof(avp->ll);
	    need += vlen;
	    src = &avp->ll;
	    break;
	case PM_TYPE_FLOAT:
	    vlen = sizeof(avp->f);
	    need += vlen;
	    src = &avp->f;
	    break;
	case PM_TYPE_DOUBLE:
	    vlen = sizeof(avp->d);
	    need += vlen;
	    src = &avp->d;
	    break;
	case PM_TYPE_STRING:
	    vlen = strlen(avp->cp);
	    need += PM_PDU_SIZE_BYTES(vlen);
	    src = avp->cp;
	    break;
	case PM_TYPE_AGGREGATE:
	    vlen = 8;		/* hardcoded for aggr[] */
	    need += PM_PDU_SIZE_BYTES(vlen);
	    src = avp->vp;
	    break;
	default:
	    return PM_ERR_TYPE;
    }
    if ((sts = check_buf(need)) < 0)
	return sts;
    epp = (pmEventParameter *)eptr;
    epp->ep_pmid = pmid;
    epp->ep_len = PM_VAL_HDR_SIZE + vlen;
    epp->ep_type = type;
    memcpy((void *)(eptr + sizeof(pmEventParameter)), src, vlen);
    eptr += need;
    erp->er_nparams++;
    return 0;
}

static void
reset(void)
{
    eptr = ebuf;
    eap = (pmEventArray *)eptr;
    eap->ea_nrecords = 0;
    eptr += sizeof(pmEventArray) - sizeof(pmEventRecord);
}

static int
add_record(struct timeval *tp, int flags)
{
    int				sts;

    if ((sts = check_buf(sizeof(pmEventRecord) - sizeof(pmEventParameter))) < 0)
	return sts;
    eap->ea_nrecords++;
    erp = (pmEventRecord *)eptr;
    erp->er_timestamp.tv_sec = (__int32_t)tp->tv_sec;
    erp->er_timestamp.tv_usec = (__int32_t)tp->tv_usec;
    erp->er_nparams = 0;
    erp->er_flags = flags;
    eptr += sizeof(pmEventRecord) - sizeof(pmEventParameter);
    return 0;
}

int
event_get_c(void)
{
    return nfetch % 4;
}

void
event_set_c(int c)
{
    nfetch = c;
}

int
sample_fetch_events(pmEventArray **eapp)
{
    int			sts;
    struct timeval	stamp;
    int			c;
    pmAtomValue		atom;
    static char		aggr[] = { '\01', '\03', '\07', '\017', '\037', '\077', '\177', '\377' };
    static pmID		pmid_type = PMDA_PMID(0,127);	/* event.type */
    static pmID		pmid_32 = PMDA_PMID(0,128);	/* event.param_32 */
    static pmID		pmid_u32 = PMDA_PMID(0,129);	/* event.param_u32 */
    static pmID		pmid_64 = PMDA_PMID(0,130);	/* event.param_64 */
    static pmID		pmid_u64 = PMDA_PMID(0,131);	/* event.param_u64 */
    static pmID		pmid_float = PMDA_PMID(0,132);	/* event.param_float */
    static pmID		pmid_double = PMDA_PMID(0,133);	/* event.param_double */
    static pmID		pmid_string = PMDA_PMID(0,134);	/* event.param_string */
    static pmID		pmid_aggregate = PMDA_PMID(0,135);	/* event.param_aggregate */

    if (nfetch >= 0)
	c = nfetch % 4;
    else {
	/* one of the error injection cases */
	c = nfetch;
    }

    if (eptr == NULL) {
	/* first time, punt on a 512 byte buffer */
	ebuflen = 512;
	ebuf = eptr = (char *)malloc(ebuflen);
	if (ebuf == NULL)
	    return -errno;
	ebufend = &ebuf[ebuflen-1];
	/*
	 * also, fix the domain field in the event parameter PMIDs ...
	 * note these PMIDs must match the corresponding metrics in
	 * desctab[] and this cannot easily be done automatically
	 */
	((__pmID_int *)&pmid_type)->domain = mydomain;
	((__pmID_int *)&pmid_32)->domain = mydomain;
	((__pmID_int *)&pmid_u32)->domain = mydomain;
	((__pmID_int *)&pmid_64)->domain = mydomain;
	((__pmID_int *)&pmid_u64)->domain = mydomain;
	((__pmID_int *)&pmid_float)->domain = mydomain;
	((__pmID_int *)&pmid_double)->domain = mydomain;
	((__pmID_int *)&pmid_string)->domain = mydomain;
	((__pmID_int *)&pmid_aggregate)->domain = mydomain;
    }
    reset();
    gettimeofday(&stamp, NULL);
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
	    if ((sts = add_record(&stamp, 0)) < 0)
		return sts;
	    stamp.tv_sec++;
	    break;
	case 2:
	    /*
	     * 3rd fetch
	     * 1 event with one U32 parameter
	     * 1 event with 2 parameters(U32 and 64 types)
	     */
	    if ((sts = add_record(&stamp, 0)) < 0)
		return sts;
	    stamp.tv_sec++;
	    atom.ul = 1;
	    if ((sts = add_param(pmid_type, PM_TYPE_U32, &atom)) < 0)
		return sts;
	    if ((sts = add_record(&stamp, 1)) < 0)
		return sts;
	    stamp.tv_sec++;
	    atom.ul = 2;
	    if ((sts = add_param(pmid_type, PM_TYPE_U32, &atom)) < 0)
		return sts;
	    atom.ll = -3;
	    if ((sts = add_param(pmid_64, PM_TYPE_64, &atom)) < 0)
		return sts;
	    break;
	case 3:
	    /*
	     * 4th fetch
	     * 1 event with 3 parameters (U32, U64 and STRING types)
	     * 1 event with 3 parameters (U32 and 2 DOUBLE types)
	     * 1 event with 6 (U32, U64, STRING, STRING, 32 and U32 types)
	     * 7 "missed" events
	     * 1 event with 3 parameters (U32, FLOAT and AGGREGATE types)
	     */
	    if ((sts = add_record(&stamp, 0)) < 0)
		return sts;
	    stamp.tv_sec++;
	    atom.ul = 4;
	    if ((sts = add_param(pmid_type, PM_TYPE_U32, &atom)) < 0)
		return sts;
	    atom.ull = 5;
	    if ((sts = add_param(pmid_u64, PM_TYPE_U64, &atom)) < 0)
		return sts;
	    atom.cp = "6";
	    if ((sts = add_param(pmid_string, PM_TYPE_STRING, &atom)) < 0)
		return sts;
	    if ((sts = add_record(&stamp, 0)) < 0)
		return sts;
	    stamp.tv_sec++;
	    atom.ul = 7;
	    if ((sts = add_param(pmid_type, PM_TYPE_U32, &atom)) < 0)
		return sts;
	    atom.d = 8;
	    if ((sts = add_param(pmid_double, PM_TYPE_DOUBLE, &atom)) < 0)
		return sts;
	    atom.d = -9;
	    if ((sts = add_param(pmid_double, PM_TYPE_DOUBLE, &atom)) < 0)
		return sts;
	    if ((sts = add_record(&stamp, 2)) < 0)
		return sts;
	    stamp.tv_sec++;
	    atom.ul = 10;
	    if ((sts = add_param(pmid_type, PM_TYPE_U32, &atom)) < 0)
		return sts;
	    atom.ull = 11;
	    if ((sts = add_param(pmid_u64, PM_TYPE_U64, &atom)) < 0)
		return sts;
	    atom.cp = "twelve";
	    if ((sts = add_param(pmid_string, PM_TYPE_STRING, &atom)) < 0)
		return sts;
	    atom.cp = "thirteen";
	    if ((sts = add_param(pmid_string, PM_TYPE_STRING, &atom)) < 0)
		return sts;
	    atom.l = -14;
	    if ((sts = add_param(pmid_32, PM_TYPE_32, &atom)) < 0)
		return sts;
	    atom.ul = 15;
	    if ((sts = add_param(pmid_u32, PM_TYPE_U32, &atom)) < 0)
		return sts;
	    /* "missed 7 records */
	    if ((sts = add_record(&stamp, PM_EVENT_FLAG_MISSED)) < 0)
		return sts;
	    stamp.tv_sec++;
	    erp->er_nparams = 7;
	    if ((sts = add_record(&stamp, 0)) < 0)
		return sts;
	    stamp.tv_sec++;
	    atom.ul = 16;
	    if ((sts = add_param(pmid_type, PM_TYPE_U32, &atom)) < 0)
		return sts;
	    atom.f = -17;
	    if ((sts = add_param(pmid_float, PM_TYPE_FLOAT, &atom)) < 0)
		return sts;
	    atom.vp = (void *)aggr;
	    if ((sts = add_param(pmid_aggregate, PM_TYPE_AGGREGATE, &atom)) < 0)
		return sts;
	    break;
	case -1:
	    /* error injection */
	    if ((sts = add_record(&stamp, 0)) < 0)
		return sts;
	    stamp.tv_sec++;
	    atom.ul = c;
	    if ((sts = add_param(pmid_type, PM_TYPE_U32, &atom)) < 0)
		return sts;
	    /* pmid that is not in PMNS and not known to the PMDA */
	    if ((sts = add_param(PMDA_PMID(100,200), PM_TYPE_U32, &atom)) < 0)
		return sts;
	    break;
    }
    nfetch++;

    *eapp = (pmEventArray *)ebuf;

    return eptr - ebuf;
}
