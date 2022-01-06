/*
 * Copyright (c) 2013-2018, 2020 Red Hat.
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2021, Ken McDonell.  All Rights Reserved.
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
 *
 * Thread-safe notes
 *
 * - nothing to see here
 */

#include "pmapi.h"
#include "libpcp.h"
#include "fault.h"
#include "internal.h"

/*
 * On-Disk  InDom Record, Version 3
 */
typedef struct {
    __int32_t	len;		/* header */
    __int32_t	type;
    __int32_t	sec[2];		/* __pmTimestamp */
    __int32_t	nsec;
    __int32_t	indom;
    __int32_t	numinst;
    __int32_t	data[0];	/* inst[] then stridx[] then strings */
    				/* will be expanded if numinst > 0 */
} __pmInDom_v3;

/*
 * On-Disk  InDom Record, Version 2
 */
typedef struct {
    __int32_t	len;		/* header */
    __int32_t	type;
    __int32_t	sec;		/* pmTimeval */
    __int32_t	usec;
    __int32_t	indom;
    __int32_t	numinst;
    __int32_t	data[0];	/* inst[] then stridx[] then strings */
    				/* will be expanded if numinst > 0 */
} __pmInDom_v2;

int
__pmLogPutInDom(__pmArchCtl *acp, pmInDom indom, const __pmTimestamp * const tsp, 
		int type, int numinst, int *instlist, char **namelist)
{
    __pmLogCtl		*lcp = acp->ac_log;
    char		*str;
    int			sts = 0;
    int			i;
    size_t		len;
    __int32_t		*lenp;
    int			*inst;
    int			*stridx;
    void		*out;
    char		strbuf[20];

    /*
     * for V3 expect TYPE_INDOM or TYPE_INDOM_DELTA
     * for V2 expect TYPE_INDOM_V2
     */
    if (__pmLogVersion(lcp) != PM_LOG_VERS03 && __pmLogVersion(lcp) != PM_LOG_VERS02) {
	pmprintf("__pmLogPutInDom(...,indom=%s,type=%d,...): Botch: bad archive version %d\n",
	    pmInDomStr_r(indom, strbuf, sizeof(strbuf)), type, __pmLogVersion(lcp));
	pmflush();
	return PM_ERR_GENERIC;
    }
    if ((__pmLogVersion(lcp) == PM_LOG_VERS03 &&
	 type != TYPE_INDOM && type != TYPE_INDOM_DELTA) ||
	(__pmLogVersion(lcp) == PM_LOG_VERS02 && type != TYPE_INDOM_V2)) {
	pmprintf("__pmLogPutInDom(...,indom=%s,type=%d,...): Botch: bad type for archive version %d\n",
	    pmInDomStr_r(indom, strbuf, sizeof(strbuf)), type, __pmLogVersion(lcp));
	pmflush();
	return PM_ERR_GENERIC;
    }

    /*
     * Common leader fields on disk (before instances) ...
     * V2: 32-bits for len, type, usec, indom, numinst
     *     + 32 bits for sec
     * V3: 32-bits for len, type, nsec, indom, numinst
     *     + 64 bits for sec
     */
    len = 5 * sizeof(__int32_t);
    if (__pmLogVersion(lcp) == PM_LOG_VERS03)
	len += sizeof(__int64_t);
    else if (__pmLogVersion(lcp) == PM_LOG_VERS02)
	len += sizeof(__int32_t);
    else
	return PM_ERR_LABEL;

    len += (numinst > 0 ? numinst : 0) * (sizeof(instlist[0]) + sizeof(stridx[0]))
	    + sizeof(__int32_t);
    for (i = 0; i < numinst; i++) {
	if (namelist[i] != NULL)
	    len += strlen(namelist[i]) + 1;
    }

PM_FAULT_POINT("libpcp/" __FILE__ ":6", PM_FAULT_ALLOC);
    if (__pmLogVersion(lcp) == PM_LOG_VERS03) {
	__pmInDom_v3	*v3;
	if ((v3 = (__pmInDom_v3 *)malloc(len)) == NULL)
	    return -oserror();
	/* swab all output fields */
	v3->len = htonl(len);
	v3->type = htonl(type);
	__pmPutTimestamp(tsp, &v3->sec[0]);
	v3->indom = __htonpmInDom(indom);
	v3->numinst = htonl(numinst);
	out = (void *)v3;
	inst = (int *)&v3->data;
	lenp = &v3->len;
    }
    else {
	/* __pmLogVersion(lcp) == PM_LOG_VERS02 */
	__pmInDom_v2	*v2;
	if ((v2 = (__pmInDom_v2 *)malloc(len)) == NULL)
	    return -oserror();
	/* swab all output fields */
	v2->len = htonl(len);
	v2->type = htonl(TYPE_INDOM_V2);
	__pmPutTimeval(tsp, &v2->sec);
	v2->indom = __htonpmInDom(indom);
	v2->numinst = htonl(numinst);
	out = (void *)v2;
	inst = (int *)&v2->data;
	lenp = &v2->len;
    }

    stridx = (int *)&inst[numinst];
    str = (char *)&stridx[numinst];
    for (i = 0; i < numinst; i++) {
	inst[i] = htonl(instlist[i]);
	if (namelist[i] != NULL) {
	    int	slen = strlen(namelist[i])+1;
	    memmove((void *)str, (void *)namelist[i], slen);
	    stridx[i] = htonl((int)((ptrdiff_t)str - (ptrdiff_t)&stridx[numinst]));
	    str += slen;
	}
	else {
	    /* deleted instance for TYPE_INDOM_DELTA */
	    stridx[i] = -1;
	}
    }

    /* trailer record length */
    memcpy((void *)str, lenp, sizeof(*lenp));

    if ((sts = __pmFwrite(out, 1, len, lcp->mdfp)) != len) {
	char	errmsg[PM_MAXERRMSGLEN];
	pmprintf("__pmLogPutInDom(...,indom=%s,numinst=%d): write failed: returned %d expecting %zd: %s\n",
	    pmInDomStr_r(indom, strbuf, sizeof(strbuf)), numinst, sts, len,
	    osstrerror_r(errmsg, sizeof(errmsg)));
	pmflush();
	free(out);
	return -oserror();
    }
    free(out);

    /*
     * Update the indom's hash data structures ...
     *
     * For INDOM_DELTA, it is the caller's responsibility to call
     * addindom(), or more likely the wrapper __pmLogAddInDom() for
     * callers outside libpcp, with the _full_ indom, not the _delta_
     * indom we've just written to the external metadata file above.
     */
    if (type != TYPE_INDOM_DELTA)
	sts = addindom(lcp, indom, tsp, numinst, instlist, namelist, NULL, 0);

    return sts;
}

/*
 * Load an InDom record from disk
 *
 * libpcp use, acp != NULL
 *	we've already read the record header, so we need rlen more bytes
 *	to be allocated, read from f; the buffer is returned via *buf
 *	(and the caller must free() this when done).
 * pmlogrewrite (and others) use, acp == NULL
 *	*buf already contains the on-disk record (after the header)
 *
 * The InDom is unpacked into *inp, and the return value is 1 if
 * inp->namelist[] entries points into buf[], otherwise the return
 * value is 0 and inp->namelist also needs to be freed by the caller
 * if it is not NULL
 */

int
__pmLogLoadInDom(__pmArchCtl *acp, int rlen, int type, pmInResult *inp, __pmTimestamp *tsp, __int32_t **buf)
{
    int			i;
    int			k;
    int			n;
    __int32_t		*stridx;
    char		*namebase;
    __int32_t		*lbuf;
    int			sts;

PM_FAULT_POINT("libpcp/" __FILE__ ":3", PM_FAULT_ALLOC);
    if (acp != NULL) {
	__pmLogCtl	*lcp = acp->ac_log;
	if ((lbuf = (__int32_t *)malloc(rlen)) == NULL)
	    return -oserror();
	if ((n = (int)__pmFread(lbuf, 1, rlen, lcp->mdfp)) != rlen) {
	    if (pmDebugOptions.logmeta)
		fprintf(stderr, "__pmLogLoadInDom: indom read -> %d: expected: %d\n", n, rlen);
	    free(lbuf);
	    if (__pmFerror(lcp->mdfp)) {
		__pmClearerr(lcp->mdfp);
		return -oserror();
	    }
	    else
		return PM_ERR_LOGREC;
	}
    }
    else
	lbuf = *buf;

    if (type == TYPE_INDOM || type == TYPE_INDOM_DELTA) {
	__pmInDom_v3	*v3;
	__pmTimestamp	stamp;
	v3 = (__pmInDom_v3 *)&lbuf[-2];	/* len+type not in buf */
	__pmLoadTimestamp(&v3->sec[0], &stamp);
	*tsp = stamp;	/* struct assignment */
	k = (sizeof(v3->sec)+sizeof(v3->nsec))/sizeof(__int32_t);
	inp->indom = __ntohpmInDom(v3->indom);
	k++;
	inp->numinst = ntohl(v3->numinst);
	k++;
	inp->instlist = (int *)&v3->data;
    }
    else if (type == TYPE_INDOM_V2) {
	__pmInDom_v2	*v2;
	v2 = (__pmInDom_v2 *)&lbuf[-2];	/* len+type not in lbuf */
	__pmLoadTimeval(&v2->sec, tsp);
	k = (sizeof(v2->sec)+sizeof(v2->usec))/sizeof(__int32_t);
	inp->indom = __ntohpmInDom(v2->indom);
	k++;
	inp->numinst = ntohl(v2->numinst);
	k++;
	inp->instlist = (int *)&v2->data;
    }
    else {
	fprintf(stderr, "__pmLogLoadInDom: botch type=%d\n", type);
	exit(1);
    }
    if (inp->numinst > 0) {
	k += inp->numinst;
	stridx = (__int32_t *)&lbuf[k];
#if defined(HAVE_32BIT_PTR)
	inp->namelist = (char **)stridx;
	sts = 1; /* allocation is all in lbuf */
#else
	sts = 0; /* allocation for namelist + lbuf */
	/* need to allocate to hold the pointers */
PM_FAULT_POINT("libpcp/" __FILE__ ":4", PM_FAULT_ALLOC);
	inp->namelist = (char **)malloc(inp->numinst * sizeof(char*));
	if (inp->namelist == NULL) {
	    free(lbuf);
	    return -oserror();
	}
#endif
	k += inp->numinst;
	namebase = (char *)&lbuf[k];
	for (i = 0; i < inp->numinst; i++) {
	    inp->instlist[i] = ntohl(inp->instlist[i]);
	    if (inp->instlist[i] >= 0 && ntohl(stridx[i]) >= 0) {
		inp->namelist[i] = &namebase[ntohl(stridx[i])];
		if (pmDebugOptions.logmeta && pmDebugOptions.desperate)
		    fprintf(stderr, "inst[%d] %d or \"%s\" (idx=%d)\n", i, inp->instlist[i], inp->namelist[i], ntohl(stridx[i]));
	    }
	    else {
		inp->namelist[i] = NULL;
		if (pmDebugOptions.logmeta && pmDebugOptions.desperate)
		    fprintf(stderr, "inst[%d] %d (idx=%d)\n", i, inp->instlist[i], ntohl(stridx[i]));
	    }
	}
    }
    else {
	inp->namelist = NULL;
	/*
	 * sts value here does not matter, because inp->numinst <= 0 and
	 * so inp->namelist will never be referenced
	 */
	sts = 1;
    }

    if (acp != NULL)
	*buf = lbuf;

    return sts;
}
