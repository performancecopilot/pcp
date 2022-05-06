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

/*
 * pack an indom into a physical metadata record
 * - lcp required to provide archive version (else NULL)
 * - type controls format (and must match version from lcp if not NULL)
 * - caller must free allocated buf[]
 */
int
__pmLogEncodeInDom(__pmLogCtl *lcp, int type, const __pmLogInDom * const lidp, __int32_t **buf)
{
    char		*str;
    int			i;
    size_t		len;
    __int32_t		*lenp;
    int			*inst;
    int			*stridx;
    __int32_t		*out;
    char		strbuf[20];

    if (lcp != NULL) {
	/*
	 * for V3 expect TYPE_INDOM or TYPE_INDOM_DELTA
	 * for V2 expect TYPE_INDOM_V2
	 */
	if (__pmLogVersion(lcp) != PM_LOG_VERS03 && __pmLogVersion(lcp) != PM_LOG_VERS02) {
	    pmprintf("__pmLogEncodeInDom(...,indom=%s,type=%d,...): Botch: bad archive version %d\n",
		pmInDomStr_r(lidp->indom, strbuf, sizeof(strbuf)), type, __pmLogVersion(lcp));
	    pmflush();
	    return PM_ERR_GENERIC;
	}
	if ((__pmLogVersion(lcp) == PM_LOG_VERS03 &&
	     type != TYPE_INDOM && type != TYPE_INDOM_DELTA) ||
	    (__pmLogVersion(lcp) == PM_LOG_VERS02 && type != TYPE_INDOM_V2)) {
	    pmprintf("__pmLogEncodeInDom(...,indom=%s,type=%d,...): Botch: bad type for archive version %d\n",
		pmInDomStr_r(lidp->indom, strbuf, sizeof(strbuf)), type, __pmLogVersion(lcp));
	    pmflush();
	    return PM_ERR_GENERIC;
	}
    }

    /*
     * Common leader fields on disk (before instances) ...
     * V2: 32-bits for len, type, usec, indom, numinst
     *     + 32 bits for sec
     * V3: 32-bits for len, type, nsec, indom, numinst
     *     + 64 bits for sec
     */
    len = 5 * sizeof(__int32_t);
    if (type == TYPE_INDOM || type == TYPE_INDOM_DELTA)
	len += sizeof(__int64_t);
    else if (type == TYPE_INDOM_V2)
	len += sizeof(__int32_t);
    else
	return PM_ERR_RECTYPE;

    len += (lidp->numinst > 0 ? lidp->numinst : 0) * (sizeof(lidp->instlist[0]) + sizeof(stridx[0]))
	    + sizeof(__int32_t);
    for (i = 0; i < lidp->numinst; i++) {
	if (lidp->namelist[i] != NULL)
	    len += strlen(lidp->namelist[i]) + 1;
    }

PM_FAULT_POINT("libpcp/" __FILE__ ":6", PM_FAULT_ALLOC);
    if (type == TYPE_INDOM || type == TYPE_INDOM_DELTA) {
	/* Version 3 */
	__pmInDom_v3	*v3;
	if ((v3 = (__pmInDom_v3 *)malloc(len)) == NULL)
	    return -oserror();
	/* swab all output fields */
	v3->len = htonl(len);
	v3->type = htonl(type);
	__pmPutTimestamp(&lidp->stamp, &v3->sec[0]);
	v3->indom = __htonpmInDom(lidp->indom);
	v3->numinst = htonl(lidp->numinst);
	out = (__int32_t *)v3;
	inst = (int *)&v3->data;
	lenp = &v3->len;
    }
    else {
	/* Version 2 */
	__pmInDom_v2	*v2;
	if ((v2 = (__pmInDom_v2 *)malloc(len)) == NULL)
	    return -oserror();
	/* swab all output fields */
	v2->len = htonl(len);
	v2->type = htonl(TYPE_INDOM_V2);
	__pmPutTimeval(&lidp->stamp, &v2->sec);
	v2->indom = __htonpmInDom(lidp->indom);
	v2->numinst = htonl(lidp->numinst);
	out = (__int32_t *)v2;
	inst = (int *)&v2->data;
	lenp = &v2->len;
    }

    stridx = (int *)&inst[lidp->numinst];
    str = (char *)&stridx[lidp->numinst];
    for (i = 0; i < lidp->numinst; i++) {
	inst[i] = htonl(lidp->instlist[i]);
	if (lidp->namelist[i] != NULL) {
	    int	slen = strlen(lidp->namelist[i])+1;
	    memmove((void *)str, (void *)lidp->namelist[i], slen);
	    stridx[i] = htonl((int)((ptrdiff_t)str - (ptrdiff_t)&stridx[lidp->numinst]));
	    str += slen;
	}
	else {
	    /* deleted instance for TYPE_INDOM_DELTA */
	    stridx[i] = htonl(-1);
	}
    }

    /* trailer record length */
    memcpy((void *)str, lenp, sizeof(*lenp));

    *buf = out;
    return 0;
}

/*
 * output the metadata record for an indom
 */
int
__pmLogPutInDom(__pmArchCtl *acp, int type, const __pmLogInDom * const lidp)
{
    __pmLogCtl		*lcp = acp->ac_log;
    int			sts;
    __int32_t		*buf;
    size_t		len;
    char		strbuf[20];

    sts = __pmLogEncodeInDom(lcp, type, lidp, &buf);
    if (sts < 0)
	return sts;

    len = ntohl(buf[0]);
    if ((sts = __pmFwrite(buf, 1, len, lcp->mdfp)) != len) {
	char	errmsg[PM_MAXERRMSGLEN];
	pmprintf("__pmLogPutInDom(...,indom=%s,numinst=%d): write failed: returned %d expecting %zd: %s\n",
	    pmInDomStr_r(lidp->indom, strbuf, sizeof(strbuf)), lidp->numinst, sts, len,
	    osstrerror_r(errmsg, sizeof(errmsg)));
	pmflush();
	free(buf);
	return -oserror();
    }
    free(buf);

    /*
     * Update the indom's hash data structures ...
     *
     * For INDOM_DELTA, it is the caller's responsibility to call
     * addindom(), or more likely the wrapper __pmLogAddInDom() for
     * callers outside libpcp, with the _full_ indom, not the _delta_
     * indom we've just written to the external metadata file above.
     */
    if (type != TYPE_INDOM_DELTA)
	sts = addindom(lcp, type, lidp, NULL);

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
 * The InDom is unpacked into *lidp, and the caller should later call
 * __pmFreeLogInDom() to release the storage associated with *lidp.
 */

int
__pmLogLoadInDom(__pmArchCtl *acp, int rlen, int type, __pmLogInDom *lidp, __int32_t **buf)
{
    int			i;
    int			k;
    int			n;
    __int32_t		*stridx;
    int			idx;
    int			max_idx = -1;		/* pander to gcc */
    char		*namebase;
    __int32_t		*lbuf;

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
	v3 = (__pmInDom_v3 *)&lbuf[-2];	/* len+type not in buf */
	__pmLoadTimestamp(&v3->sec[0], &lidp->stamp);
	k = (sizeof(v3->sec)+sizeof(v3->nsec))/sizeof(__int32_t);
	lidp->indom = __ntohpmInDom(v3->indom);
	k++;
	lidp->numinst = ntohl(v3->numinst);
	k++;
	lidp->instlist = (int *)&v3->data;
	if (acp != NULL) {
	    /* rlen minus fixed fields (plus len+type), minus instlist[], minus strindex[] */
	    max_idx = rlen - 5*sizeof(__int32_t) - 2*lidp->numinst*sizeof(__int32_t);
	}
    }
    else if (type == TYPE_INDOM_V2) {
	__pmInDom_v2	*v2;
	v2 = (__pmInDom_v2 *)&lbuf[-2];	/* len+type not in lbuf */
	__pmLoadTimeval(&v2->sec, &lidp->stamp);
	k = (sizeof(v2->sec)+sizeof(v2->usec))/sizeof(__int32_t);
	lidp->indom = __ntohpmInDom(v2->indom);
	k++;
	lidp->numinst = ntohl(v2->numinst);
	k++;
	lidp->instlist = (int *)&v2->data;
	if (acp != NULL) {
	    /* rlen minus fixed fields (plus len+type), minus instlist[], minus strindex[] */
	    max_idx = rlen - 4*sizeof(__int32_t) - 2*lidp->numinst*sizeof(__int32_t);
	}
    }
    else {
	if (pmDebugOptions.logmeta)
	    fprintf(stderr, "__pmLogLoadInDom: botch type=%d\n", type);
	goto bad;
    }
    lidp->next = lidp->prior = NULL;
    lidp->buf = NULL;
    lidp->isdelta = 0;
    lidp->alloc = 0;
    if (lidp->numinst > 0) {
	k += lidp->numinst;
	stridx = (__int32_t *)&lbuf[k];
#if defined(HAVE_32BIT_PTR)
	lidp->namelist = (char **)stridx;
#else
	/* need to allocate to hold the pointers */
PM_FAULT_POINT("libpcp/" __FILE__ ":4", PM_FAULT_ALLOC);
	lidp->namelist = (char **)malloc(lidp->numinst * sizeof(char*));
	if (lidp->namelist == NULL) {
	    if (acp != NULL)
		free(lbuf);
	    return -oserror();
	}
	lidp->alloc |= PMLID_NAMELIST;
#endif
	k += lidp->numinst;
	namebase = (char *)&lbuf[k];
	for (i = 0; i < lidp->numinst; i++) {
	    lidp->instlist[i] = ntohl(lidp->instlist[i]);
	    if (lidp->instlist[i] < 0) {
		/* bad internal instance identifier */
		if (pmDebugOptions.logmeta) {
		    char	strbuf[20];
		    fprintf(stderr, "__pmLogLoadInDom: InDom: %s: instance[%d]: bad instance identifier (%d)\n", 
			pmInDomStr_r(lidp->indom, strbuf, sizeof(strbuf)),
			i, lidp->instlist[i]);
		}
		goto bad;
	    }
	    idx = ntohl(stridx[i]);
	    if (idx >= 0) {
		if (acp != NULL) {
		    /*
		     * crude sanity check ... if the index points to the
		     * start of the name that is past the end of the input
		     * record, the record is corrupted
		     */
		    if (idx > max_idx) {
			if (pmDebugOptions.logmeta) {
			    char	strbuf[20];
			    fprintf(stderr, "__pmLogLoadInDom: InDom: %s instance[%d]: bad string index (%d) > max index based on record length (%d)\n",
				pmInDomStr_r(lidp->indom, strbuf, sizeof(strbuf)),
				i, idx, max_idx);
			}
			goto bad;
		    }
		}
		lidp->namelist[i] = &namebase[idx];
		if (pmDebugOptions.logmeta && pmDebugOptions.desperate)
		    fprintf(stderr, "inst[%d] %d or \"%s\" (idx=%d)\n", i, lidp->instlist[i], lidp->namelist[i], idx);
	    }
	    else if (type == TYPE_INDOM_DELTA && idx == -1) {
		/* instance deleted ... */
		lidp->namelist[i] = NULL;
		if (pmDebugOptions.logmeta && pmDebugOptions.desperate)
		    fprintf(stderr, "inst[%d] %d (delta indom)\n", i, lidp->instlist[i]);
	    }
	    else {
		/* NOT (TYPE_INDOM_DELTA and idx == -1) and idx < 0 */
		if (pmDebugOptions.logmeta) {
		    char	strbuf[20];
		    fprintf(stderr, "__pmLogLoadInDom: InDom: %s instance[%d]: bad string index (%d)\n",
			pmInDomStr_r(lidp->indom, strbuf, sizeof(strbuf)),
			i, idx);
		}
		goto bad;
	    }
	}
    }
    else {
	lidp->namelist = NULL;
    }

    if (acp != NULL)
	*buf = lbuf;

    return 0;

bad:
    if (acp != NULL)
	free(lbuf);
    __pmFreeLogInDom(lidp);
    return PM_ERR_LOGREC;
}
