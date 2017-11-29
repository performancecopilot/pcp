/*
 * Service routines for managing a packed array of event records
 *
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

typedef struct {
    char		*baddr;	/* base address of the buffer */
    char		*bptr;	/* next location to be filled in the buffer */
    void		*berp;	/* current pmEvent[HighRes]Record in buffer */
    int			blen;	/* buffer size */
    int			bstate;
} bufctl_t;

#define B_FREE	0
#define B_INUSE	1

static int	nbuf;
static bufctl_t	*bufs;

static int
check_buf(bufctl_t *bp, int need)
{
    int		offset = bp->bptr - bp->baddr;
    int		er_offset = (char *)bp->berp - bp->baddr;
    char	*tmp_baddr;

    while (bp->blen == 0 || &bp->bptr[need] >= &bp->baddr[bp->blen-1]) {
	if (bp->blen == 0)
	    /* first time, punt on a 512 byte buffer */
	    bp->blen = 512;
	else
	    bp->blen *= 2;
	if ((tmp_baddr = (char *)realloc(bp->baddr, bp->blen)) == NULL) {
	    free(bp->baddr);
	    bp->baddr = NULL;
	    return -oserror();
	}
	bp->baddr = tmp_baddr;
	bp->bptr = &bp->baddr[offset];
	bp->berp = (void *)&bp->baddr[er_offset];
    }
    return 0;
}

static int
event_array(void)
{
    int		i;
    bufctl_t	*tmp_bufs;

    for (i = 0; i < nbuf; i++) {
	if (bufs[i].bstate == B_FREE)
	    break;
    }

    if (i == nbuf) {
	nbuf++;
	tmp_bufs = (bufctl_t *)realloc(bufs, nbuf*sizeof(bufs[0]));
	if (tmp_bufs == NULL) {
	    free(bufs);
	    bufs = NULL;
	    nbuf = 0;
	    return -oserror();
	}
	bufs = tmp_bufs;
    }

    bufs[i].bptr = bufs[i].baddr = NULL;
    bufs[i].blen = 0;
    bufs[i].bstate = B_INUSE;
    return i;
}

int
pmdaEventNewArray(void)
{
    int		sts = event_array();

    if (sts >= 0)
	pmdaEventResetArray(sts);
    return sts;
}

int
pmdaEventNewHighResArray(void)
{
    int		sts = event_array();

    if (sts >= 0)
	pmdaEventResetHighResArray(sts);
    return sts;
}

/* prepare to reuse an array */
int
pmdaEventResetArray(int idx)
{
    bufctl_t		*bp;
    pmEventArray	*eap;
    int			sts;

    if (idx < 0 || idx >= nbuf || bufs[idx].bstate == B_FREE)
	return PM_ERR_NOCONTEXT;
    bp = &bufs[idx];
    if ((sts = check_buf(bp, sizeof(pmEventArray) - sizeof(pmEventRecord))) < 0)
	return sts;

    eap = (pmEventArray *)bp->baddr;
    eap->ea_nrecords = 0;
    bp->bptr = bp->baddr + sizeof(pmEventArray) - sizeof(pmEventRecord);
    return 0;
}

int
pmdaEventResetHighResArray(int idx)
{
    bufctl_t		*bp;
    pmHighResEventArray	*hreap;
    int			sts;

    if (idx < 0 || idx >= nbuf || bufs[idx].bstate == B_FREE)
	return PM_ERR_NOCONTEXT;
    bp = &bufs[idx];
    if ((sts = check_buf(bp, sizeof(*hreap) - sizeof(pmHighResEventRecord))) < 0)
	return sts;

    hreap = (pmHighResEventArray *)bp->baddr;
    hreap->ea_nrecords = 0;
    bp->bptr = bp->baddr + sizeof(*hreap) - sizeof(pmHighResEventRecord);
    return 0;
}

/* release buffer space associated with a packed event array */
int
pmdaEventReleaseArray(int idx)
{
    if (idx < 0 || idx >= nbuf || bufs[idx].bstate == B_FREE)
	return PM_ERR_NOCONTEXT;

    free(bufs[idx].baddr);
    bufs[idx].bstate = B_FREE;
    return 0;
}

int
pmdaEventReleaseHighResArray(int idx)
{
    return pmdaEventReleaseArray(idx);
}

int
pmdaEventAddRecord(int idx, struct timeval *tp, int flags)
{
    int			sts;
    bufctl_t		*bp;
    pmEventArray	*eap;
    pmEventRecord	*erp;

    if (idx < 0 || idx >= nbuf || bufs[idx].bstate == B_FREE)
	return PM_ERR_NOCONTEXT;
    bp = &bufs[idx];

    /* use pmdaEventAddMissedRecord for missed records ... */
    if (flags & PM_EVENT_FLAG_MISSED)
	return PM_ERR_CONV;

    if ((sts = check_buf(bp, sizeof(*erp) - sizeof(pmEventParameter))) < 0)
	return sts;
    eap = (pmEventArray *)bp->baddr;
    eap->ea_nrecords++;
    erp = (pmEventRecord *)bp->bptr;
    erp->er_timestamp.tv_sec = (__int32_t)tp->tv_sec;
    erp->er_timestamp.tv_usec = (__int32_t)tp->tv_usec;
    erp->er_nparams = 0;
    erp->er_flags = flags;
    bp->berp = (void *)erp;
    bp->bptr += sizeof(pmEventRecord) - sizeof(pmEventParameter);
    return 0;
}

int
pmdaEventAddHighResRecord(int idx, struct timespec *ts, int flags)
{
    int			sts;
    bufctl_t		*bp;
    pmHighResEventArray	*hreap;
    pmHighResEventRecord *hrerp;

    if (idx < 0 || idx >= nbuf || bufs[idx].bstate == B_FREE)
	return PM_ERR_NOCONTEXT;
    bp = &bufs[idx];

    /* use pmdaEventAddMissedRecord for missed records ... */
    if (flags & PM_EVENT_FLAG_MISSED)
	return PM_ERR_CONV;

    if ((sts = check_buf(bp, sizeof(*hrerp) - sizeof(pmEventParameter))) < 0)
	return sts;
    hreap = (pmHighResEventArray *)bp->baddr;
    hreap->ea_nrecords++;
    hrerp = (pmHighResEventRecord *)bp->bptr;
    hrerp->er_timestamp.tv_sec = (__int64_t)ts->tv_sec;
    hrerp->er_timestamp.tv_nsec = (__int64_t)ts->tv_nsec;
    hrerp->er_nparams = 0;
    hrerp->er_flags = flags;
    bp->berp = (void *)hrerp;
    bp->bptr += sizeof(pmHighResEventRecord) - sizeof(pmEventParameter);
    return 0;
}

int
pmdaEventAddMissedRecord(int idx, struct timeval *tp, int missed)
{
    int			sts;
    bufctl_t		*bp;
    pmEventArray	*eap;
    pmEventRecord	*erp;

    if (idx < 0 || idx >= nbuf || bufs[idx].bstate == B_FREE)
	return PM_ERR_NOCONTEXT;
    bp = &bufs[idx];

    if ((sts = check_buf(bp, sizeof(*erp) - sizeof(pmEventParameter))) < 0)
	return sts;
    eap = (pmEventArray *)bp->baddr;
    eap->ea_nrecords++;
    erp = (pmEventRecord *)bp->bptr;
    erp->er_timestamp.tv_sec = (__int32_t)tp->tv_sec;
    erp->er_timestamp.tv_usec = (__int32_t)tp->tv_usec;
    erp->er_nparams = missed;
    erp->er_flags = PM_EVENT_FLAG_MISSED;
    bp->berp = (void *)erp;
    bp->bptr += sizeof(pmEventRecord) - sizeof(pmEventParameter);
    return 0;
}

int
pmdaEventAddHighResMissedRecord(int idx, struct timespec *ts, int missed)
{
    int			sts;
    bufctl_t		*bp;
    pmHighResEventArray	*hreap;
    pmHighResEventRecord *hrerp;

    if (idx < 0 || idx >= nbuf || bufs[idx].bstate == B_FREE)
	return PM_ERR_NOCONTEXT;
    bp = &bufs[idx];

    if ((sts = check_buf(bp, sizeof(*hrerp) - sizeof(pmEventParameter))) < 0)
	return sts;
    hreap = (pmHighResEventArray *)bp->baddr;
    hreap->ea_nrecords++;
    hrerp = (pmHighResEventRecord *)bp->bptr;
    hrerp->er_timestamp.tv_sec = (__int32_t)ts->tv_sec;
    hrerp->er_timestamp.tv_nsec = (__int32_t)ts->tv_nsec;
    hrerp->er_nparams = missed;
    hrerp->er_flags = PM_EVENT_FLAG_MISSED;
    bp->berp = (void *)hrerp;
    bp->bptr += sizeof(pmHighResEventRecord) - sizeof(pmEventParameter);
    return 0;
}

int
add_param(int idx, pmID pmid, int type, pmAtomValue *avp, bufctl_t **bpp)
{
    int			sts;
    int			need;	/* bytes in the buffer */
    int			vlen;	/* value only length */
    void		*src;
    pmEventParameter	*epp;
    bufctl_t		*bp;

    if (idx < 0 || idx >= nbuf || bufs[idx].bstate == B_FREE)
	return PM_ERR_NOCONTEXT;
    bp = &bufs[idx];

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
	    /* PM_VAL_HDR_SIZE is added back in below */
	    vlen = avp->vbp->vlen - PM_VAL_HDR_SIZE;
	    need += PM_PDU_SIZE_BYTES(vlen);
	    src = avp->vbp->vbuf;
	    break;
	default:
	    return PM_ERR_TYPE;
    }
    if ((sts = check_buf(bp, need)) < 0)
	return sts;
    epp = (pmEventParameter *)bp->bptr;
    epp->ep_pmid = pmid;
    epp->ep_len = PM_VAL_HDR_SIZE + vlen;
    epp->ep_type = type;
    memcpy((void *)(bp->bptr + sizeof(pmEventParameter)), src, vlen);
    bp->bptr += need;
    *bpp = bp;
    return 0;
}

int
pmdaEventAddParam(int idx, pmID pmid, int type, pmAtomValue *avp)
{
    int			sts;
    bufctl_t		*bp;
    pmEventRecord	*erp;

    if ((sts = add_param(idx, pmid, type, avp, &bp)) >= 0) {
	erp = (pmEventRecord *)bp->berp;
	erp->er_nparams++;
    }
    return sts;
}

int
pmdaEventHighResAddParam(int idx, pmID pmid, int type, pmAtomValue *avp)
{
    int			sts;
    bufctl_t		*bp;
    pmHighResEventRecord *hrerp;

    if ((sts = add_param(idx, pmid, type, avp, &bp)) >= 0) {
	hrerp = (pmHighResEventRecord *)bp->berp;
	hrerp->er_nparams++;
    }
    return sts;
}

/*
 * fill in the vlen/vtype header and return the address of the whole
 * structure
 */
pmEventArray *
pmdaEventGetAddr(int idx)
{
    pmEventArray	*eap;

    if (idx < 0 || idx >= nbuf || bufs[idx].bstate == B_FREE)
	return NULL;

    eap = (pmEventArray *)bufs[idx].baddr;
    eap->ea_type = PM_TYPE_EVENT;
    eap->ea_len = bufs[idx].bptr - bufs[idx].baddr;
    return eap;
}

pmHighResEventArray *
pmdaEventHighResGetAddr(int idx)
{
    pmHighResEventArray	*hreap;

    if (idx < 0 || idx >= nbuf || bufs[idx].bstate == B_FREE)
	return NULL;

    hreap = (pmHighResEventArray *)bufs[idx].baddr;
    hreap->ea_type = PM_TYPE_HIGHRES_EVENT;
    hreap->ea_len = bufs[idx].bptr - bufs[idx].baddr;
    return hreap;
}
