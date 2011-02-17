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
 * p_result.c
 */


