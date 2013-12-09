/*
 * Copyright (c) 2013 Red Hat.
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

#include "pmapi.h"
#include "impl.h"
#include "fault.h"
#include "internal.h"
#include <stddef.h>

/* bytes for a length field in a header/trailer, or a string length field */
#define LENSIZE	4

#ifdef PCP_DEBUG
static void
StrTimeval(__pmTimeval *tp)
{
    if (tp == NULL)
	fprintf(stderr, "<null timeval>");
    else
	__pmPrintTimeval(stderr, tp);
}
#endif

static int
addindom(__pmLogCtl *lcp, pmInDom indom, const __pmTimeval *tp, int numinst, 
         int *instlist, char **namelist, int *indom_buf, int allinbuf)
{
    __pmLogInDom	*idp;
    __pmHashNode	*hp;
    int		sts;

PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_ALLOC);
    if ((idp = (__pmLogInDom *)malloc(sizeof(__pmLogInDom))) == NULL)
	return -oserror();
    idp->stamp = *tp;		/* struct assignment */
    idp->numinst = numinst;
    idp->instlist = instlist;
    idp->namelist = namelist;
    idp->buf = indom_buf;
    idp->allinbuf = allinbuf;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOGMETA) {
	char	strbuf[20];
	fprintf(stderr, "addindom( ..., %s, ", pmInDomStr_r(indom, strbuf, sizeof(strbuf)));
	StrTimeval((__pmTimeval *)tp);
	fprintf(stderr, ", numinst=%d)\n", numinst);
    }
#endif


    if ((hp = __pmHashSearch((unsigned int)indom, &lcp->l_hashindom)) == NULL) {
	idp->next = NULL;
	sts = __pmHashAdd((unsigned int)indom, (void *)idp, &lcp->l_hashindom);
    }
    else {
	idp->next = (__pmLogInDom *)hp->data;
	hp->data = (void *)idp;
	sts = 0;
    }
    return sts;
}

/*
 * Load _all_ of the hashed pmDesc and __pmLogInDom structures from the metadata
 * log file -- used at the initialization (NewContext) of an archive.
 * Also load all the metric names from the metadata log file and create l_pmns.
 */
int
__pmLogLoadMeta(__pmLogCtl *lcp)
{
    int		rlen;
    int		check;
    pmDesc	*dp;
    int		sts = 0;
    __pmLogHdr	h;
    FILE	*f = lcp->l_mdfp;
    int		numpmid = 0;
    int		n;
    
    if ((sts = __pmNewPMNS(&(lcp->l_pmns))) < 0)
      goto end;

    fseek(f, (long)(sizeof(__pmLogLabel) + 2*sizeof(int)), SEEK_SET);
    for ( ; ; ) {
	n = (int)fread(&h, 1, sizeof(__pmLogHdr), f);

	/* swab hdr */
	h.len = ntohl(h.len);
	h.type = ntohl(h.type);

	if (n != sizeof(__pmLogHdr) || h.len <= 0) {
            if (feof(f)) {
		clearerr(f);
                sts = 0;
		goto end;
            }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOGMETA) {
		fprintf(stderr, "__pmLogLoadMeta: header read -> %d: expected: %d or len=%d\n",
			n, (int)sizeof(__pmLogHdr), h.len);
	    }
#endif
	    if (ferror(f)) {
		clearerr(f);
		sts = -oserror();
	    }
	    else
		sts = PM_ERR_LOGREC;
	    goto end;
	}
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOGMETA) {
	    fprintf(stderr, "__pmLogLoadMeta: record len=%d, type=%d @ offset=%d\n",
		h.len, h.type, (int)(ftell(f) - sizeof(__pmLogHdr)));
	}
#endif
	rlen = h.len - (int)sizeof(__pmLogHdr) - (int)sizeof(int);
	if (h.type == TYPE_DESC) {
            numpmid++;
PM_FAULT_POINT("libpcp/" __FILE__ ":2", PM_FAULT_ALLOC);
	    if ((dp = (pmDesc *)malloc(sizeof(pmDesc))) == NULL) {
		sts = -oserror();
		goto end;
	    }
	    if ((n = (int)fread(dp, 1, sizeof(pmDesc), f)) != sizeof(pmDesc)) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOGMETA) {
		    fprintf(stderr, "__pmLogLoadMeta: pmDesc read -> %d: expected: %d\n",
			    n, (int)sizeof(pmDesc));
		}
#endif
		if (ferror(f)) {
		    clearerr(f);
		    sts = -oserror();
		}
		else
		    sts = PM_ERR_LOGREC;
		free(dp);
		goto end;
	    }
	    else {
		/* swab desc */
		dp->type = ntohl(dp->type);
		dp->sem = ntohl(dp->sem);
		dp->indom = __ntohpmInDom(dp->indom);
		dp->units = __ntohpmUnits(dp->units);
		dp->pmid = __ntohpmID(dp->pmid);
	    }

	    if ((sts = __pmHashAdd((int)dp->pmid, (void *)dp, &lcp->l_hashpmid)) < 0) {
		free(dp);
		goto end;
	    }

            else {
                char name[MAXPATHLEN];
                int numnames;
		int i;
		int len;

                /* read in the names & store in PMNS tree ... */
		if ((n = (int)fread(&numnames, 1, sizeof(numnames), f)) != 
                     sizeof(numnames)) {
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_LOGMETA) {
			fprintf(stderr, "__pmLogLoadMeta: numnames read -> %d: expected: %d\n",
				n, (int)sizeof(numnames));
		    }
#endif
		    if (ferror(f)) {
			clearerr(f);
			sts = -oserror();
		    }
		    else
			sts = PM_ERR_LOGREC;
		    goto end;
		}
		else {
		    /* swab numnames */
		    numnames = ntohl(numnames);
		}

 		for (i = 0; i < numnames; i++) {
		    if ((n = (int)fread(&len, 1, sizeof(len), f)) != 
			 sizeof(len)) {
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_LOGMETA) {
			    fprintf(stderr, "__pmLogLoadMeta: len name[%d] read -> %d: expected: %d\n",
				    i, n, (int)sizeof(len));
			}
#endif
			if (ferror(f)) {
			    clearerr(f);
			    sts = -oserror();
			}
			else
			    sts = PM_ERR_LOGREC;
			goto end;
		    }
		    else {
			/* swab len */
			len = ntohl(len);
		    }

		    if ((n = (int)fread(name, 1, len, f)) != len) {
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_LOGMETA) {
			    fprintf(stderr, "__pmLogLoadMeta: name[%d] read -> %d: expected: %d\n",
				    i, n, len);
			}
#endif
			if (ferror(f)) {
			    clearerr(f);
			    sts = -oserror();
			}
			else
			    sts = PM_ERR_LOGREC;
			goto end;
		    }
                    name[len] = '\0';
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_LOGMETA) {
			char	strbuf[20];
			fprintf(stderr, "__pmLogLoadMeta: PMID: %s name: %s\n",
				pmIDStr_r(dp->pmid, strbuf, sizeof(strbuf)), name);
		    }
#endif

                    if ((sts = __pmAddPMNSNode(lcp->l_pmns, dp->pmid, name)) < 0) {
			/*
			 * If we see a duplicate PMID, its a recoverable error.
			 * We wont be able to see all of the data in the log, but
			 * its better to provide access to some rather than none,
			 * esp. when only one or two metric IDs may be corrupted
			 * in this way (which we may not be interested in anyway).
			 */
			if (sts != PM_ERR_PMID)
			    goto end;
			sts = 0;
		    } 
		}/*for*/
            }
	}
	else if (h.type == TYPE_INDOM) {
	    int			*tbuf;
	    pmInDom		indom;
	    __pmTimeval		*when;
	    int			numinst;
	    int			*instlist;
	    char		**namelist;
	    char		*namebase;
	    int			*stridx;
	    int			i;
	    int			k;
	    int			allinbuf;

PM_FAULT_POINT("libpcp/" __FILE__ ":3", PM_FAULT_ALLOC);
	    if ((tbuf = (int *)malloc(rlen)) == NULL) {
		sts = -oserror();
		goto end;
	    }
	    if ((n = (int)fread(tbuf, 1, rlen, f)) != rlen) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOGMETA) {
		    fprintf(stderr, "__pmLogLoadMeta: indom read -> %d: expected: %d\n",
			    n, rlen);
		}
#endif
		if (ferror(f)) {
		    clearerr(f);
		    sts = -oserror();
		}
		else
		    sts = PM_ERR_LOGREC;
		free(tbuf);
		goto end;
	    }

	    k = 0;
	    when = (__pmTimeval *)&tbuf[k];
	    when->tv_sec = ntohl(when->tv_sec);
	    when->tv_usec = ntohl(when->tv_usec);
	    k += sizeof(*when)/sizeof(int);
	    indom = __ntohpmInDom((unsigned int)tbuf[k++]);
	    numinst = ntohl(tbuf[k++]);
	    if (numinst > 0) {
		instlist = &tbuf[k];
		k += numinst;
		stridx = &tbuf[k];
#if defined(HAVE_32BIT_PTR)
		namelist = (char **)stridx;
		allinbuf = 1; /* allocation is all in tbuf */
#else
		allinbuf = 0; /* allocation for namelist + tbuf */
		/* need to allocate to hold the pointers */
PM_FAULT_POINT("libpcp/" __FILE__ ":4", PM_FAULT_ALLOC);
		namelist = (char **)malloc(numinst*sizeof(char*));
		if (namelist == NULL) {
		    sts = -oserror();
		    free(tbuf);
		    goto end;
		}
#endif
		k += numinst;
		namebase = (char *)&tbuf[k];
	        for (i = 0; i < numinst; i++) {
		    instlist[i] = ntohl(instlist[i]);
	            namelist[i] = &namebase[ntohl(stridx[i])];
		}
	    }
	    else {
		/* no instances, or an error */
		instlist = NULL;
		namelist = NULL;
	    }
	    if ((sts = addindom(lcp, indom, when, numinst, instlist, namelist, tbuf, allinbuf)) < 0) {
		free(tbuf);
		if (allinbuf == 0)
		    free(namelist);
		goto end;
	    }
	}
	else
	    fseek(f, (long)rlen, SEEK_CUR);
	n = (int)fread(&check, 1, sizeof(check), f);
	check = ntohl(check);
	if (n != sizeof(check) || h.len != check) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOGMETA) {
		fprintf(stderr, "__pmLogLoadMeta: trailer read -> %d or len=%d: expected %d @ offset=%d\n",
		    n, check, h.len, (int)(ftell(f) - sizeof(check)));
	    }
#endif
	    if (ferror(f)) {
		clearerr(f);
		sts = -oserror();
	    }
	    else
		sts = PM_ERR_LOGREC;
	    goto end;
	}
    }/*for*/
end:
    
    fseek(f, (long)(sizeof(__pmLogLabel) + 2*sizeof(int)), SEEK_SET);

    if (sts == 0) {
	if (numpmid == 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOGMETA) {
		fprintf(stderr, "__pmLogLoadMeta: no metrics found?\n");
	    }
#endif
	    sts = PM_ERR_LOGREC;
	}
	else
	    __pmFixPMNSHashTab(lcp->l_pmns, numpmid, 1);
    }
    return sts;
}

/*
 * scan the hashed data structures to find a pmDesc, given a pmid
 */
int
__pmLogLookupDesc(__pmLogCtl *lcp, pmID pmid, pmDesc *dp)
{
    __pmHashNode	*hp;
    pmDesc	*tp;

    if ((hp = __pmHashSearch((unsigned int)pmid, &lcp->l_hashpmid)) == NULL)
	return PM_ERR_PMID_LOG;

    tp = (pmDesc *)hp->data;
    *dp = *tp;			/* struct assignment */
    return 0;
}

/*
 * Add a new pmDesc into the metadata log, and to the hashed data structures
 * If numnames is positive, then write out any associated PMNS names.
 */
int
__pmLogPutDesc(__pmLogCtl *lcp, const pmDesc *dp, int numnames, char **names)
{
    __pmLogHdr	h;
    FILE	*f = lcp->l_mdfp;
    pmDesc	*tdp;
    pmDesc	*odp;		/* pmDesc to write out */
    int		onumnames;	/* numnames to write out */
    int		olen;		/* length to write out */
    int		i;
    int		len;
    pmDesc	tmp;

    h.type = htonl(TYPE_DESC);
    len = sizeof(__pmLogHdr) + sizeof(pmDesc) + LENSIZE;

    tmp.type = htonl(dp->type);
    tmp.sem = htonl(dp->sem);
    tmp.indom = __htonpmInDom(dp->indom);
    tmp.units = __htonpmUnits(dp->units);
    tmp.pmid = __htonpmID(dp->pmid);
    odp = &tmp;

    if (numnames > 0) {
        len += sizeof(numnames);
        for (i = 0; i < numnames; i++)
            len += LENSIZE + (int)strlen(names[i]);

	h.len = htonl(len);
	onumnames = htonl(numnames);
	if (fwrite(&h, 1, sizeof(__pmLogHdr), f) != sizeof(__pmLogHdr) ||
	    fwrite(odp, 1, sizeof(pmDesc), f) != sizeof(pmDesc) ||
            fwrite(&onumnames, 1, sizeof(numnames), f) != sizeof(numnames))
		return -oserror();

        /* write out the names */
        for (i = 0; i < numnames; i++) {
	    int slen = (int)strlen(names[i]);
	    olen = htonl(slen);
            if (fwrite(&olen, 1, LENSIZE, f) != LENSIZE)
                return -oserror();
            if (fwrite(names[i], 1, slen, f) != slen)
                return -oserror();
        }

	olen = htonl(len);
	if (fwrite(&olen, 1, LENSIZE, f) != LENSIZE)
	    return -oserror();
    }
    else {
	h.len = htonl(len);
	olen = htonl(len);
	if (fwrite(&h, 1, sizeof(__pmLogHdr), f) != sizeof(__pmLogHdr) ||
	    fwrite(odp, 1, sizeof(pmDesc), f) != sizeof(pmDesc) ||
	    fwrite(&olen, 1, LENSIZE, f) != LENSIZE)
		return -oserror();
    }

    /*
     * need to make a copy of the pmDesc, and add this, since caller
     * may re-use *dp
     */
PM_FAULT_POINT("libpcp/" __FILE__ ":5", PM_FAULT_ALLOC);
    if ((tdp = (pmDesc *)malloc(sizeof(pmDesc))) == NULL)
	return -oserror();
    *tdp = *dp;		/* struct assignment */
    return __pmHashAdd((int)dp->pmid, (void *)tdp, &lcp->l_hashpmid);
}

static __pmLogInDom *
searchindom(__pmLogCtl *lcp, pmInDom indom, __pmTimeval *tp)
{
    __pmHashNode	*hp;
    __pmLogInDom	*idp;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOGMETA) {
	char	strbuf[20];
	fprintf(stderr, "searchindom( ..., %s, ", pmInDomStr_r(indom, strbuf, sizeof(strbuf)));
	StrTimeval(tp);
	fprintf(stderr, ")\n");
    }
#endif

    if ((hp = __pmHashSearch((unsigned int)indom, &lcp->l_hashindom)) == NULL)
	return NULL;

    idp = (__pmLogInDom *)hp->data;
    if (tp != NULL) {
	for ( ; idp != NULL; idp = idp->next) {
	    /*
	     * need first one at or earlier than the requested time
	     */
	    if (__pmTimevalSub(&idp->stamp, tp) <= 0)
		break;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOGMETA) {
		fprintf(stderr, "request @ ");
		StrTimeval(tp);
		fprintf(stderr, " is too early for indom @ ");
		StrTimeval(&idp->stamp);
		fputc('\n', stderr);
	    }
#endif
	}
	if (idp == NULL)
	    return NULL;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOGMETA) {
	fprintf(stderr, "success for indom @ ");
	StrTimeval(&idp->stamp);
	fputc('\n', stderr);
    }
#endif
    return idp;
}

/*
 * for the given indom retrieve the instance domain that is correct
 * as of the latest time (tp == NULL) or at a designated
 * time
 */
int
__pmLogGetInDom(__pmLogCtl *lcp, pmInDom indom, __pmTimeval *tp, int **instlist, char ***namelist)
{
    __pmLogInDom	*idp = searchindom(lcp, indom, tp);

    if (idp == NULL)
	return PM_ERR_INDOM_LOG;

    *instlist = idp->instlist;
    *namelist = idp->namelist;

    return idp->numinst;
}

int
__pmLogLookupInDom(__pmLogCtl *lcp, pmInDom indom, __pmTimeval *tp, 
		   const char *name)
{
    __pmLogInDom	*idp = searchindom(lcp, indom, tp);
    int		i;

    if (idp == NULL)
	return PM_ERR_INDOM_LOG;

    if (idp->numinst < 0)
	return idp->numinst;

    /* full match */
    for (i = 0; i < idp->numinst; i++) {
	if (strcmp(name, idp->namelist[i]) == 0)
	    return idp->instlist[i];
    }

    /* half-baked match to first space */
    for (i = 0; i < idp->numinst; i++) {
	char	*p = idp->namelist[i];
	while (*p && *p != ' ')
	    p++;
	if (*p == ' ') {
	    if (strncmp(name, idp->namelist[i], p - idp->namelist[i]) == 0)
		return idp->instlist[i];
	}
    }

    return PM_ERR_INST_LOG;
}

int
__pmLogNameInDom(__pmLogCtl *lcp, pmInDom indom, __pmTimeval *tp, int inst, char **name)
{
    __pmLogInDom	*idp = searchindom(lcp, indom, tp);
    int		i;

    if (idp == NULL)
	return PM_ERR_INDOM_LOG;

    if (idp->numinst < 0)
	return idp->numinst;

    for (i = 0; i < idp->numinst; i++) {
	if (inst == idp->instlist[i]) {
	    *name = idp->namelist[i];
	    return 0;
	}
    }

    return PM_ERR_INST_LOG;
}

int
__pmLogPutInDom(__pmLogCtl *lcp, pmInDom indom, const __pmTimeval *tp, 
		int numinst, int *instlist, char **namelist)
{
    int		sts = 0;
    __pmLogHdr	h;
    int		i;
    int		wlen;
    int		strsize;
    int		*stridx;
    int		real_numinst;
    int		onuminst;
    pmInDom	oindom;
    __pmTimeval	otp;

    /*
     * Note: this routine depends upon the names pointed to by namelist[]
     *       being packed, and starting at namelist[0] ... this is exactly
     *       the format returned by pmGetInDom and __pmLogLoadMeta, so it
     *       should be OK
     */

    real_numinst = numinst > 0 ? numinst : 0;
PM_FAULT_POINT("libpcp/" __FILE__ ":6", PM_FAULT_ALLOC);
    if ((stridx = (int *)malloc(real_numinst * sizeof(stridx[0]))) == NULL)
	return -oserror();

    h.len = (int)sizeof(__pmLogHdr) + (int)sizeof(*tp) + (int)sizeof(indom) +
	    (int)sizeof(numinst) +
	    real_numinst * ((int)sizeof(instlist[0]) + (int)sizeof(stridx[0])) +
	    (int)sizeof(h.len);
    strsize = 0;
    for (i = 0; i < numinst; i++) {
	strsize += (int)strlen(namelist[i]) + 1;
	/* see Note */
	stridx[i] = (int)((ptrdiff_t)namelist[i] - (ptrdiff_t)namelist[0]);
	stridx[i] = htonl(stridx[i]); /* swab */
	instlist[i] = htonl(instlist[i]); /* swab: this is changed back after writing */
    }
    h.len += strsize;

    /* swab all output buffers */
    h.len = htonl(h.len);
    h.type = htonl(TYPE_INDOM);
    otp.tv_sec = htonl(tp->tv_sec);
    otp.tv_usec = htonl(tp->tv_usec);
    oindom = __htonpmInDom(indom);
    onuminst = htonl(numinst);

    wlen = (int)fwrite(&h, 1, sizeof(__pmLogHdr), lcp->l_mdfp);
    wlen += fwrite(&otp, 1, sizeof(otp), lcp->l_mdfp);
    wlen += fwrite(&oindom, 1, sizeof(oindom), lcp->l_mdfp);
    wlen += fwrite(&onuminst, 1, sizeof(onuminst), lcp->l_mdfp);
    if (numinst > 0) {
	wlen += fwrite(instlist, 1, real_numinst * sizeof(instlist[0]), lcp->l_mdfp);
	wlen += fwrite(stridx, 1, real_numinst * sizeof(stridx[0]), lcp->l_mdfp);
	/* see Note */
	wlen += fwrite(namelist[0], 1, strsize, lcp->l_mdfp);
    }
    wlen += fwrite(&h.len, 1, sizeof(h.len), lcp->l_mdfp);
    free(stridx);

    if (wlen != (int)ntohl(h.len)) {
	char	errmsg[PM_MAXERRMSGLEN];
	pmprintf("__pmLogPutInDom: wrote %d, expected %d: %s\n",
	    wlen, (int)ntohl(h.len), osstrerror_r(errmsg, sizeof(errmsg)));
	pmflush();
	return -oserror();
    }

    sts = addindom(lcp, indom, tp, numinst, instlist, namelist, NULL, 0);

    /* swab instance list back again so as to not upset the caller */
    for (i = 0; i < numinst; i++) {
	instlist[i] = ntohl(instlist[i]);
    }

    return sts;
}

int
pmLookupInDomArchive(pmInDom indom, const char *name)
{
    int		n;
    int		j;
    __pmHashNode	*hp;
    __pmLogInDom	*idp;
    __pmContext	*ctxp;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp == NULL)
	    return PM_ERR_NOCONTEXT;
	if (ctxp->c_type != PM_CONTEXT_ARCHIVE) {
	    PM_UNLOCK(ctxp->c_lock);
	    return PM_ERR_NOTARCHIVE;
	}

	if ((hp = __pmHashSearch((unsigned int)indom, &ctxp->c_archctl->ac_log->l_hashindom)) == NULL) {
	    PM_UNLOCK(ctxp->c_lock);
	    return PM_ERR_INDOM_LOG;
	}

	for (idp = (__pmLogInDom *)hp->data; idp != NULL; idp = idp->next) {
	    /* full match */
	    for (j = 0; j < idp->numinst; j++) {
		if (strcmp(name, idp->namelist[j]) == 0) {
		    PM_UNLOCK(ctxp->c_lock);
		    return idp->instlist[j];
		}
	    }
	    /* half-baked match to first space */
	    for (j = 0; j < idp->numinst; j++) {
		char	*p = idp->namelist[j];
		while (*p && *p != ' ')
		    p++;
		if (*p == ' ') {
		    if (strncmp(name, idp->namelist[j], p - idp->namelist[j]) == 0) {
			PM_UNLOCK(ctxp->c_lock);
			return idp->instlist[j];
		    }
		}
	    }
	}
	n = PM_ERR_INST_LOG;
	PM_UNLOCK(ctxp->c_lock);
    }

    return n;
}

int
pmNameInDomArchive(pmInDom indom, int inst, char **name)
{
    int		n;
    int		j;
    __pmHashNode	*hp;
    __pmLogInDom	*idp;
    __pmContext	*ctxp;

    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp == NULL)
	    return PM_ERR_NOCONTEXT;
	if (ctxp->c_type != PM_CONTEXT_ARCHIVE) {
	    PM_UNLOCK(ctxp->c_lock);
	    return PM_ERR_NOTARCHIVE;
	}

	if ((hp = __pmHashSearch((unsigned int)indom, &ctxp->c_archctl->ac_log->l_hashindom)) == NULL) {
	    PM_UNLOCK(ctxp->c_lock);
	    return PM_ERR_INDOM_LOG;
	}

	for (idp = (__pmLogInDom *)hp->data; idp != NULL; idp = idp->next) {
	    for (j = 0; j < idp->numinst; j++) {
		if (idp->instlist[j] == inst) {
		    if ((*name = strdup(idp->namelist[j])) == NULL)
			n = -oserror();
		    else
			n = 0;
		    PM_UNLOCK(ctxp->c_lock);
		    return n;
		}
	    }
	}
	n = PM_ERR_INST_LOG;
	PM_UNLOCK(ctxp->c_lock);
    }

    return n;
}

int
pmGetInDomArchive(pmInDom indom, int **instlist, char ***namelist)
{
    int			n;
    int			i;
    int			j;
    char		*p;
    __pmContext		*ctxp;
    __pmHashNode		*hp;
    __pmLogInDom		*idp;
    int			numinst = 0;
    int			strsize = 0;
    int			*ilist = NULL;
    char		**nlist = NULL;
    char		**olist;

    /* avoid ambiguity when no instances or errors */
    *instlist = NULL;
    *namelist = NULL;
    if (indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if ((n = pmWhichContext()) >= 0) {
	ctxp = __pmHandleToPtr(n);
	if (ctxp == NULL)
	    return PM_ERR_NOCONTEXT;
	if (ctxp->c_type != PM_CONTEXT_ARCHIVE) {
	    PM_UNLOCK(ctxp->c_lock);
	    return PM_ERR_NOTARCHIVE;
	}

	if ((hp = __pmHashSearch((unsigned int)indom, &ctxp->c_archctl->ac_log->l_hashindom)) == NULL) {
	    PM_UNLOCK(ctxp->c_lock);
	    return PM_ERR_INDOM_LOG;
	}

	for (idp = (__pmLogInDom *)hp->data; idp != NULL; idp = idp->next) {
	    for (j = 0; j < idp->numinst; j++) {
		for (i = 0; i < numinst; i++) {
		    if (idp->instlist[j] == ilist[i])
			break;
		}
		if (i == numinst) {
		    numinst++;
PM_FAULT_POINT("libpcp/" __FILE__ ":7", PM_FAULT_ALLOC);
		    if ((ilist = (int *)realloc(ilist, numinst*sizeof(ilist[0]))) == NULL) {
			__pmNoMem("pmGetInDomArchive: ilist", numinst*sizeof(ilist[0]), PM_FATAL_ERR);
		    }
PM_FAULT_POINT("libpcp/" __FILE__ ":8", PM_FAULT_ALLOC);
		    if ((nlist = (char **)realloc(nlist, numinst*sizeof(nlist[0]))) == NULL) {
			__pmNoMem("pmGetInDomArchive: nlist", numinst*sizeof(nlist[0]), PM_FATAL_ERR);
		    }
		    ilist[numinst-1] = idp->instlist[j];
		    nlist[numinst-1] = idp->namelist[j];
		    strsize += strlen(idp->namelist[j])+1;
		}
	    }
	}
PM_FAULT_POINT("libpcp/" __FILE__ ":9", PM_FAULT_ALLOC);
	if ((olist = (char **)malloc(numinst*sizeof(olist[0]) + strsize)) == NULL) {
	    __pmNoMem("pmGetInDomArchive: olist", numinst*sizeof(olist[0]) + strsize, PM_FATAL_ERR);
	}
	p = (char *)olist;
	p += numinst * sizeof(olist[0]);
	for (i = 0; i < numinst; i++) {
	    olist[i] = p;
	    strcpy(p, nlist[i]);
	    p += strlen(nlist[i]) + 1;
	}
	free(nlist);
	*instlist = ilist;
	*namelist = olist;
	n = numinst;
	PM_UNLOCK(ctxp->c_lock);
    }

    return n;
}
