/*
 * Copyright (c) 1995-2002,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pcp/pmapi.h"
#include "pcp/impl.h"

#define PM_LOG_VERS01 1

/*
 * Routines in this file are lifted from libpcp to allow obsolete
 * functionality to be restored to read V1 archives ...
 */

/*
 * logutil.c
 */

int
__pmLogChkLabel(__pmLogCtl *lcp, FILE *f, __pmLogLabel *lp, int vol)
{
    int		len;
    int		version = UNKNOWN_VERSION;
    int		xpectlen = sizeof(__pmLogLabel) + 2 * sizeof(len);
    int		n;

    if (vol >= 0 && vol < lcp->l_numseen && lcp->l_seen[vol]) {
	/* FastPath, cached result of previous check for this volume */
	fseek(f, (long)(sizeof(__pmLogLabel) + 2*sizeof(int)), SEEK_SET);
	return 0;
    }

    if (vol >= 0 && vol >= lcp->l_numseen) {
	lcp->l_seen = (int *)realloc(lcp->l_seen, (vol+1)*(int)sizeof(lcp->l_seen[0]));
	if (lcp->l_seen == NULL)
	    lcp->l_numseen = 0;
	else {
	    int 	i;
	    for (i = lcp->l_numseen; i < vol; i++)
		lcp->l_seen[i] = 0;
	    lcp->l_numseen = vol+1;
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG)
	fprintf(stderr, "__pmLogChkLabel: fd=%d vol=%d", fileno(f), vol);
#endif

    fseek(f, (long)0, SEEK_SET);
    n = (int)fread(&len, 1, sizeof(len), f);
    len = ntohl(len);
    if (n != sizeof(len) || len != xpectlen) {
	if (feof(f)) {
	    clearerr(f);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG)
		fprintf(stderr, " file is empty\n");
#endif
	    return PM_ERR_NODATA;
	}
	else {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG)
		fprintf(stderr, " header read -> %d (expect %d) or bad header len=%d (expected %d)\n",
		    n, (int)sizeof(len), len, xpectlen);
#endif
	    if (ferror(f)) {
		clearerr(f);
		return -errno;
	    }
	    else
		return PM_ERR_LABEL;
	}
    }

    if ((n = (int)fread(lp, 1, sizeof(__pmLogLabel), f)) != sizeof(__pmLogLabel)) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, " bad label len=%d: expected %d\n",
		n, (int)sizeof(__pmLogLabel));
#endif
	if (ferror(f)) {
	    clearerr(f);
	    return -errno;
	}
	else
	    return PM_ERR_LABEL;
    }
    else {
	/* swab internal log label */
	lp->ill_magic = ntohl(lp->ill_magic);
	lp->ill_pid = ntohl(lp->ill_pid);
	lp->ill_start.tv_sec = ntohl(lp->ill_start.tv_sec);
	lp->ill_start.tv_usec = ntohl(lp->ill_start.tv_usec);
	lp->ill_vol = ntohl(lp->ill_vol);
    }

    n = (int)fread(&len, 1, sizeof(len), f);
    len = ntohl(len);
    if (n != sizeof(len) || len != xpectlen) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, " trailer read -> %d (expect %d) or bad trailer len=%d (expected %d)\n",
		n, (int)sizeof(len), len, xpectlen);
#endif
	if (ferror(f)) {
	    clearerr(f);
	    return -errno;
	}
	else
	    return PM_ERR_LABEL;
    }

    version = lp->ill_magic & 0xff;
    if ((lp->ill_magic & 0xffffff00) != PM_LOG_MAGIC ||
	(version != PM_LOG_VERS01 && version != PM_LOG_VERS02) ||
	lp->ill_vol != vol) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, " version %d not supported\n", version);
#endif
	return PM_ERR_LABEL;
    }
    else {
	if (__pmSetVersionIPC(fileno(f), version) < 0)
	    return -errno;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, " [magic=%8x version=%d vol=%d pid=%d host=%s]\n",
		lp->ill_magic, version, lp->ill_vol, (int)lp->ill_pid, lp->ill_hostname);
#endif
    }

    if (vol >= 0 && vol < lcp->l_numseen)
	lcp->l_seen[vol] = 1;

    return version;
}

/*
 * logmeta.c
 */

static char *
StrTimeval(__pmTimeval *tp)
{
    if (tp == NULL) {
	static char *null_timeval = "<null timeval>";
	return null_timeval;
    }
    else {
	static char		sbuf[13];
	static struct tm	*tmp;
	time_t 			t = tp->tv_sec;
	tmp = localtime(&t);
	pmsprintf(sbuf, sizeof(sbuf), "%02d:%02d:%02d.%03d",	/* safe */
	    tmp->tm_hour, tmp->tm_min, tmp->tm_sec, tp->tv_usec/1000);
	return sbuf;
    }
}

static int
addindom(__pmLogCtl *lcp, pmInDom indom, const __pmTimeval *tp, int numinst, 
         int *instlist, char **namelist, int *indom_buf, int allinbuf)
{
    __pmLogInDom	*idp;
    __pmHashNode	*hp;
    int		sts;

    if ((idp = (__pmLogInDom *)malloc(sizeof(__pmLogInDom))) == NULL)
	return -errno;
    idp->stamp = *tp;		/* struct assignment */
    idp->numinst = numinst;
    idp->instlist = instlist;
    idp->namelist = namelist;
    idp->buf = indom_buf;
    idp->allinbuf = allinbuf;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOGMETA)
	fprintf(stderr, "addindom( ..., %s, %s, numinst=%d)\n",
	    pmInDomStr(indom), StrTimeval((__pmTimeval *)tp), numinst);
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
 * load _all_ of the hashed pmDesc and __pmLogInDom structures from the metadata
 * log file -- used at the initialization (NewContext) of an archive
 * If version 2 then
 * load all the names from the meta data and create l_pmns.
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
    int         version2 = ((lcp->l_label.ill_magic & 0xff) == PM_LOG_VERS02);
    int		numpmid = 0;
    int		n;
    
    if (version2) {
       if ((sts = __pmNewPMNS(&(lcp->l_pmns))) < 0) {
          goto end;
       }
    }

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
		fprintf(stderr, "__pmLogLoadMeta: header read -> %d: expected: %d\n",
			n, (int)sizeof(__pmLogHdr));
	    }
#endif
	    if (ferror(f)) {
		clearerr(f);
		sts = -errno;
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
	    if ((dp = (pmDesc *)malloc(sizeof(pmDesc))) == NULL) {
		sts = -errno;
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
		    sts = -errno;
		}
		else
		    sts = PM_ERR_LOGREC;
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

	    if ((sts = __pmHashAdd((int)dp->pmid, (void *)dp, &lcp->l_hashpmid)) < 0)
		goto end;

            if (version2) {
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
			sts = -errno;
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
			    sts = -errno;
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
			    sts = -errno;
			}
			else
			    sts = PM_ERR_LOGREC;
			goto end;
		    }
                    name[len] = '\0';
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_LOGMETA) {
			fprintf(stderr, "__pmLogLoadMeta: PMID: %s name: %s\n",
				pmIDStr(dp->pmid), name);
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
            }/*version2*/
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

	    if ((tbuf = (int *)malloc(rlen)) == NULL) {
		sts = -errno;
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
		    sts = -errno;
		}
		else
		    sts = PM_ERR_LOGREC;
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
		namelist = (char **)malloc(numinst*sizeof(char*));
		if (namelist == NULL) {
		    sts = -errno;
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
	    if ((sts = addindom(lcp, indom, when, numinst, instlist, namelist, tbuf, allinbuf)) < 0)
		goto end;
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
		sts = -errno;
	    }
	    else
		sts = PM_ERR_LOGREC;
	    goto end;
	}
    }/*for*/
end:
    
    fseek(f, (long)(sizeof(__pmLogLabel) + 2*sizeof(int)), SEEK_SET);

    if (version2 && sts == 0) {
        __pmFixPMNSHashTab(lcp->l_pmns, numpmid, 1);
    }
    return sts;
}
