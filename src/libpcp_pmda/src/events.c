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
#include "impl.h"
#include "pmda.h"

typedef struct {
    char		*baddr;	/* base address of the buffer */
    char		*bptr;	/* next location to be filled in the buffer */
    pmEventRecord	*berp;	/* current record in buffer */
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

    while (bp->blen == 0 || &bp->bptr[need] >= &bp->baddr[bp->blen-1]) {
	if (bp->blen == 0)
	    /* first time, punt on a 512 byte buffer */
	    bp->blen = 512;
	else
	    bp->blen *= 2;
	if ((bp->baddr = (char *)realloc(bp->baddr, bp->blen)) == NULL)
	    return -oserror();
	bp->bptr = &bp->baddr[offset];
	bp->berp = (pmEventRecord *)&bp->baddr[er_offset];
    }
    return 0;
}

int
pmdaEventNewArray(void)
{
    int		i;

    for (i = 0; i < nbuf; i++) {
	if (bufs[i].bstate == B_FREE)
	    break;
    }

    if (i == nbuf) {
	nbuf++;
	bufs = (bufctl_t *)realloc(bufs, nbuf*sizeof(bufs[0]));
	if (bufs == NULL) {
	    nbuf = 0;
	    return -oserror();
	}
    }

    bufs[i].bptr = bufs[i].baddr = NULL;
    bufs[i].blen = 0;
    bufs[i].bstate = B_INUSE;
    pmdaEventResetArray(i);
    return i;
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
pmdaEventAddRecord(int idx, struct timeval *tp, int flags)
{
    int			sts;
    bufctl_t		*bp;
    pmEventArray	*eap;

    if (idx < 0 || idx >= nbuf || bufs[idx].bstate == B_FREE)
	return PM_ERR_NOCONTEXT;
    bp = &bufs[idx];

    /* use pmdaEventAddMissedRecord for missed records ... */
    if (flags & PM_EVENT_FLAG_MISSED)
	return PM_ERR_CONV;

    if ((sts = check_buf(bp, sizeof(pmEventRecord) - sizeof(pmEventParameter))) < 0)
	return sts;
    eap = (pmEventArray *)bp->baddr;
    eap->ea_nrecords++;
    bp->berp = (pmEventRecord *)bp->bptr;
    bp->berp->er_timestamp.tv_sec = (__int32_t)tp->tv_sec;
    bp->berp->er_timestamp.tv_usec = (__int32_t)tp->tv_usec;
    bp->berp->er_nparams = 0;
    bp->berp->er_flags = flags;
    bp->bptr += sizeof(pmEventRecord) - sizeof(pmEventParameter);
    return 0;
}

int
pmdaEventAddMissedRecord(int idx, struct timeval *tp, int missed)
{
    int			sts;
    bufctl_t		*bp;
    pmEventArray	*eap;

    if (idx < 0 || idx >= nbuf || bufs[idx].bstate == B_FREE)
	return PM_ERR_NOCONTEXT;
    bp = &bufs[idx];

    if ((sts = check_buf(bp, sizeof(pmEventRecord) - sizeof(pmEventParameter))) < 0)
	return sts;
    eap = (pmEventArray *)bp->baddr;
    eap->ea_nrecords++;
    bp->berp = (pmEventRecord *)bp->bptr;
    bp->berp->er_timestamp.tv_sec = (__int32_t)tp->tv_sec;
    bp->berp->er_timestamp.tv_usec = (__int32_t)tp->tv_usec;
    bp->berp->er_nparams = missed;
    bp->berp->er_flags = PM_EVENT_FLAG_MISSED;
    bp->bptr += sizeof(pmEventRecord) - sizeof(pmEventParameter);
    return 0;
}

int
pmdaEventAddParam(int idx, pmID pmid, int type, pmAtomValue *avp)
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
    bp->berp->er_nparams++;
    return 0;
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
