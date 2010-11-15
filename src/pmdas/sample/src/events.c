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
fprintf(stderr, "check_buf increase to %d\n", ebuflen);
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
	case PM_TYPE_STRING:
	    vlen = strlen(avp->cp);
	    need += PM_PDU_SIZE_BYTES(vlen);
	    src = avp->cp;
fprintf(stderr, "string vlen=%d pdulen=%d need=%d string=\"%s\"\n", vlen, PM_PDU_SIZE_BYTES(vlen), need, avp->cp);
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
    eap->ea_nmissed = 0;
    eptr += sizeof(pmEventArray) - sizeof(pmEventRecord);
}

static int
add_record(struct timeval *tp)
{
    int				sts;

    if ((sts = check_buf(sizeof(pmEventRecord) - sizeof(pmEventParameter))) < 0)
	return sts;
    eap->ea_nrecords++;
    erp = (pmEventRecord *)eptr;
    erp->er_timestamp = *tp;	/* struct assignment */
    erp->er_nparams = 0;
    eptr += sizeof(pmEventRecord) - sizeof(pmEventParameter);
    return 0;
}

unsigned int
event_get_c(void)
{
printf("event_get_c: %d %u\n", nfetch, (unsigned int)(nfetch % 4));
    return nfetch % 4;
}

void
event_set_c(unsigned int c)
{
    nfetch = c;
}

int
sample_fetch_events(pmEventArray **eapp)
{
    int			sts;
    struct timeval	stamp;
    int			c = nfetch % 4;
    pmAtomValue		atom;
    static pmID		pmid_type = PMDA_PMID(0,126);	/* event.type */
    static pmID		pmid_u32 = PMDA_PMID(0,127);	/* event.param_u32 */
    static pmID		pmid_u64 = PMDA_PMID(0,128);	/* event.param_u64 */
    static pmID		pmid_string = PMDA_PMID(0,129);	/* event.param_string */
    static pmID		pmid_float = PMDA_PMID(0,130);	/* event.param_float */
    static pmID		pmid_double = PMDA_PMID(0,131);	/* event.param_double */

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
	((__pmID_int *)&pmid_u32)->domain = mydomain;
fprintf(stderr, "mydomain %d %s %s\n", mydomain, pmIDStr(pmid_type), pmIDStr(pmid_u32));
	((__pmID_int *)&pmid_u64)->domain = mydomain;
	((__pmID_int *)&pmid_string)->domain = mydomain;
	((__pmID_int *)&pmid_float)->domain = mydomain;
	((__pmID_int *)&pmid_double)->domain = mydomain;
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
	    if ((sts = add_record(&stamp)) < 0)
		return sts;
	    stamp.tv_sec++;
	    break;
	case 2:
	    /*
	     * 3rd fetch
	     * 1 event with one U32 parameter
	     * 1 event with 2 parameters(U32 and U64 types)
	     */
	    if ((sts = add_record(&stamp)) < 0)
		return sts;
	    stamp.tv_sec++;
	    atom.ul = 1;
	    if ((sts = add_param(pmid_type, PM_TYPE_U32, &atom)) < 0)
		return sts;
	    if ((sts = add_record(&stamp)) < 0)
		return sts;
	    stamp.tv_sec++;
	    atom.ul = 2;
	    if ((sts = add_param(pmid_type, PM_TYPE_U32, &atom)) < 0)
		return sts;
	    atom.ull = 3;
	    if ((sts = add_param(pmid_u64, PM_TYPE_U64, &atom)) < 0)
		return sts;
	    break;
	case 3:
	    /*
	     * 4th fetch
	     * 1 event with 3 parameters (U32, U64 and STRING types)
	     * 1 event with 3 parameters (U32 and 2 DOUBLE types)
	     * 1 event with 5 (U32, U64, STRING, STRING, U32 types)
	     * 1 event with 2 parameters (U32 and FLOAT types)
	     * + 7 "missed" events
	     */
	    if ((sts = add_record(&stamp)) < 0)
		return sts;
	    stamp.tv_sec++;
	    atom.ul = 4;
	    if ((sts = add_param(pmid_type, PM_TYPE_U32, &atom)) < 0)
		return sts;
	    atom.ull = 4;
	    if ((sts = add_param(pmid_u64, PM_TYPE_U64, &atom)) < 0)
		return sts;
	    atom.cp = "5";
	    if ((sts = add_param(pmid_string, PM_TYPE_STRING, &atom)) < 0)
		return sts;
	    if ((sts = add_record(&stamp)) < 0)
		return sts;
	    stamp.tv_sec++;
	    atom.ul = 6;
	    if ((sts = add_param(pmid_type, PM_TYPE_U32, &atom)) < 0)
		return sts;
	    atom.d = 7;
	    if ((sts = add_param(pmid_double, PM_TYPE_DOUBLE, &atom)) < 0)
		return sts;
	    atom.d = 8;
	    if ((sts = add_param(pmid_double, PM_TYPE_DOUBLE, &atom)) < 0)
		return sts;
	    if ((sts = add_record(&stamp)) < 0)
		return sts;
	    stamp.tv_sec++;
	    atom.ul = 9;
	    if ((sts = add_param(pmid_type, PM_TYPE_U32, &atom)) < 0)
		return sts;
	    atom.ull = 10;
	    if ((sts = add_param(pmid_u64, PM_TYPE_U64, &atom)) < 0)
		return sts;
	    atom.cp = "eleven";
	    if ((sts = add_param(pmid_string, PM_TYPE_STRING, &atom)) < 0)
		return sts;
	    atom.cp = "twelve!!";
	    if ((sts = add_param(pmid_string, PM_TYPE_STRING, &atom)) < 0)
		return sts;
	    atom.ul = 13;
	    if ((sts = add_param(pmid_u32, PM_TYPE_U32, &atom)) < 0)
		return sts;
	    if ((sts = add_record(&stamp)) < 0)
		return sts;
	    stamp.tv_sec++;
	    atom.ul = 14;
	    if ((sts = add_param(pmid_type, PM_TYPE_U32, &atom)) < 0)
		return sts;
	    atom.f = 15;
	    if ((sts = add_param(pmid_float, PM_TYPE_FLOAT, &atom)) < 0)
		return sts;
	    eap->ea_nmissed = 7;
	    break;
    }
    nfetch++;

    *eapp = (pmEventArray *)ebuf;
    return eptr - ebuf;
}
