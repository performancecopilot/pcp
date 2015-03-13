/*
 * Copyright (c) 2012-2013 Red Hat.
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "internal.h"

/*
 * PDU for pm*InDom request (PDU_INSTANCE_REQ)
 */
typedef struct {
    __pmPDUHdr		hdr;
    pmInDom		indom;
    __pmTimeval		when;			/* desired time */
    int			inst;			/* may be PM_IN_NULL */
    int			namelen;		/* chars in name[], may be 0 */
    char		name[sizeof(int)];	/* may be missing */
} instance_req_t;

int
__pmSendInstanceReq(int fd, int from, const __pmTimeval *when, pmInDom indom, 
		    int inst, const char *name)
{
    instance_req_t	*pp;
    int			need;
    int			sts;

    need = sizeof(instance_req_t) - sizeof(int);
    if (name != NULL)
	need += PM_PDU_SIZE_BYTES(strlen(name));
    if ((pp = (instance_req_t *)__pmFindPDUBuf(need)) == NULL)
	return -oserror();
    pp->hdr.len = need;
    pp->hdr.type = PDU_INSTANCE_REQ;
    pp->hdr.from = from;
    pp->when.tv_sec = htonl((__int32_t)when->tv_sec);
    pp->when.tv_usec = htonl((__int32_t)when->tv_usec);
    pp->indom = __htonpmInDom(indom);
    pp->inst = htonl(inst);
    if (name == NULL)
	pp->namelen = 0;
    else {
	pp->namelen = (int)strlen(name);
	memcpy((void *)pp->name, (void *)name, pp->namelen);
	if ((pp->namelen % sizeof(__pmPDU)) != 0) {
            /* clear the padding bytes, lest they contain garbage */
	    int	pad;
	    char	*padp = pp->name + pp->namelen;

	    for (pad = sizeof(__pmPDU) - 1; pad >= (pp->namelen % sizeof(__pmPDU)); pad--)
		*padp++ = '~';	/* buffer end */
	}
	pp->namelen = htonl(pp->namelen);
    }

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmDecodeInstanceReq(__pmPDU *pdubuf, __pmTimeval *when, pmInDom *indom, int *inst, char **name)
{
    instance_req_t	*pp;
    char		*pdu_end;
    int			namelen;

    pp = (instance_req_t *)pdubuf;
    pdu_end = (char *)pdubuf + pp->hdr.len;

    if (pdu_end - (char *)pp < sizeof(instance_req_t) - sizeof(pp->name))
	return PM_ERR_IPC;

    when->tv_sec = ntohl(pp->when.tv_sec);
    when->tv_usec = ntohl(pp->when.tv_usec);
    *indom = __ntohpmInDom(pp->indom);
    *inst = ntohl(pp->inst);
    namelen = ntohl(pp->namelen);
    if (namelen > 0) {
	if (namelen >= INT_MAX - 1 || namelen > pp->hdr.len)
	    return PM_ERR_IPC;
	if (pdu_end - (char *)pp < sizeof(instance_req_t) - sizeof(pp->name) + namelen)
	    return PM_ERR_IPC;
	if ((*name = (char *)malloc(namelen+1)) == NULL)
	    return -oserror();
	strncpy(*name, pp->name, namelen);
	(*name)[namelen] = '\0';
    }
    else if (namelen < 0) {
	return PM_ERR_IPC;
    } else {
	*name = NULL;
    }
    return 0;
}

/*
 * PDU for pm*InDom result (PDU_INSTANCE)
 */
typedef struct {
    int		inst;			/* internal instance id */
    int		namelen;		/* chars in name[], may be 0 */
    char	name[sizeof(int)];	/* may be missing */
} instlist_t;

typedef struct {
    __pmPDUHdr	hdr;
    pmInDom	indom;
    int		numinst;	/* no. of elts to follow */
    __pmPDU	rest[1];	/* array of instlist_t */
} instance_t;

int
__pmSendInstance(int fd, int from, __pmInResult *result)
{
    instance_t	*rp;
    instlist_t		*ip;
    int			need;
    int			i;
    int			j;
    int			sts;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_INDOM)
	__pmDumpInResult(stderr, result);
#endif

    need = sizeof(*rp) - sizeof(rp->rest);
    /* instlist_t + name rounded up to a __pmPDU boundary */
    for (i = 0; i < result->numinst; i++) {
	need += sizeof(*ip) - sizeof(ip->name);
	if (result->namelist != NULL)
	    need += PM_PDU_SIZE_BYTES(strlen(result->namelist[i]));
    }

    if ((rp = (instance_t *)__pmFindPDUBuf(need)) == NULL)
	return -oserror();
    rp->hdr.len = need;
    rp->hdr.type = PDU_INSTANCE;
    rp->hdr.from = from;
    rp->indom = __htonpmInDom(result->indom);
    rp->numinst = htonl(result->numinst);

    for (i = j = 0; i < result->numinst; i++) {
	ip = (instlist_t *)&rp->rest[j/sizeof(__pmPDU)];
	if (result->instlist != NULL)
	    ip->inst = htonl(result->instlist[i]);
	else
	    /* weird, but this is going to be ignored at the other end */
	    ip->inst = htonl(PM_IN_NULL);
	if (result->namelist != NULL) {
	    ip->namelen = (int)strlen(result->namelist[i]);
	    memcpy((void *)ip->name, (void *)result->namelist[i], ip->namelen);
	    if ((ip->namelen % sizeof(__pmPDU)) != 0) {
                /* clear the padding bytes, lest they contain garbage */
		int	pad;
		char	*padp = ip->name + ip->namelen;
		for (pad = sizeof(__pmPDU) - 1; pad >= (ip->namelen % sizeof(__pmPDU)); pad--)
		    *padp++ = '~';	/* buffer end */
	    }
	    j += sizeof(*ip) - sizeof(ip->name) + PM_PDU_SIZE_BYTES(ip->namelen);
	    ip->namelen = htonl(ip->namelen);
	}
	else {
	    ip->namelen = 0;
	    j += sizeof(*ip) - sizeof(ip->name);
	}
    }

    sts = __pmXmitPDU(fd, (__pmPDU *)rp);
    __pmUnpinPDUBuf(rp);
    return sts;
}

int
__pmDecodeInstance(__pmPDU *pdubuf, __pmInResult **result)
{
    int			i;
    int			j;
    instance_t		*rp;
    instlist_t		*ip;
    __pmInResult	*res;
    int			sts;
    char		*p;
    char		*pdu_end;
    int			keep_instlist;
    int			keep_namelist;

    rp = (instance_t *)pdubuf;
    pdu_end = (char *)pdubuf + rp->hdr.len;

    if (pdu_end - (char *)pdubuf < sizeof(instance_t) - sizeof(__pmPDU))
	return PM_ERR_IPC;

    if ((res = (__pmInResult *)malloc(sizeof(*res))) == NULL)
	return -oserror();
    res->instlist = NULL;
    res->namelist = NULL;
    res->indom = __ntohpmInDom(rp->indom);
    res->numinst = ntohl(rp->numinst);

    if (res->numinst >= (INT_MAX / sizeof(res->instlist[0])) ||
	res->numinst >= (INT_MAX / sizeof(res->namelist[0])) ||
	res->numinst >= rp->hdr.len) {
	sts = PM_ERR_IPC;
	goto badsts;
    }
    if ((res->instlist = (int *)malloc(res->numinst * sizeof(res->instlist[0]))) == NULL) {
	sts = -oserror();
	goto badsts;
    }
    if ((res->namelist = (char **)malloc(res->numinst * sizeof(res->namelist[0]))) == NULL) {
	sts = -oserror();
	goto badsts;
    }
    /* required for __pmFreeInResult() in the event of a later error */
    memset(res->namelist, 0, res->numinst * sizeof(res->namelist[0]));

    if (res->numinst == 1)
	keep_instlist = keep_namelist = 0;
    else
	keep_instlist = keep_namelist = 1;

    for (i = j = 0; i < res->numinst; i++) {
	ip = (instlist_t *)&rp->rest[j/sizeof(__pmPDU)];
	if (sizeof(instlist_t) - sizeof(ip->name) > (size_t)(pdu_end - (char *)ip)) {
	    sts = PM_ERR_IPC;
	    goto badsts;
	}

	res->instlist[i] = ntohl(ip->inst);
	if (res->instlist[i] != PM_IN_NULL)
	    keep_instlist = 1;
	ip->namelen = ntohl(ip->namelen);
	if (ip->namelen > 0)
	    keep_namelist = 1;
	if (ip->namelen < 0) {
	    sts = PM_ERR_IPC;
	    goto badsts;
	}
	if (sizeof(instlist_t) - sizeof(int) + ip->namelen > (size_t)(pdu_end - (char *)ip)) {
	    sts = PM_ERR_IPC;
	    goto badsts;
	}
	if ((p = (char *)malloc(ip->namelen + 1)) == NULL) {
	    sts = -oserror();
	    goto badsts;
	}
	memcpy((void *)p, (void *)ip->name, ip->namelen);
	p[ip->namelen] = '\0';
	res->namelist[i] = p;
	j += sizeof(*ip) - sizeof(ip->name) + PM_PDU_SIZE_BYTES(ip->namelen);
    }
    if (keep_instlist == 0) {
	free(res->instlist);
	res->instlist = NULL;
    }
    if (keep_namelist == 0) {
	free(res->namelist[0]);
	free(res->namelist);
	res->namelist = NULL;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_INDOM)
	__pmDumpInResult(stderr, res);
#endif
    *result = res;
    return 0;

badsts:
    __pmFreeInResult(res);
    return sts;
}
