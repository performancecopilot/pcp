/*
 * Copyright (c) 2013 Ken McDonell, Inc.  All Rights Reserved.
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
 *
 */

#include "pmapi.h"
#include "libpcp.h"
#include "logcheck.h"
#include "../libpcp/src/internal.h"

int
pass2(__pmContext *ctxp, char *archname)
{
    __pmHashNode	*hp;
    pmDesc		*dp;
    char		*name;
    int			sts;
    __pmLogCtl		*lcp = ctxp->c_archctl->ac_log;
    __pmFILE		*f;
    __pmLogHdr		h;
    int			n;
    off_t		offset = 0;
    __pmTimestamp	stamp;
    __pmTimestamp	last_stamp = { 0, 0 };
    __pmTimestamp	end = { 0, 0 };
    int			nrec = 0;
    int			nread;

    if (vflag)
	fprintf(stderr, "%s: start pass2\n", archname);

    /*
     * Integrity checks on metric metadata (pmDesc and PMNS)
     * pmid -> name in PMNS
     * type is valid
     * indom is PM_INDOM_NULL or domain(indom) == domain(metric)
     * sem is valid
     * units are valid
     */
    hp = __pmHashWalk(&ctxp->c_archctl->ac_log->hashpmid, PM_HASH_WALK_START);
    while (hp != NULL) {
	dp = (pmDesc *)hp->data;
	if ((sts = pmNameID(dp->pmid, &name)) < 0) {
	    /*
	     * Note: this is really a non-test because the pmid <-> name
	     *       association is guaranteed at this point since the 
	     *       pmid and some name have to have been loaded from the
	     *       .meta file in order that there is a entry in the hash
	     *       table.
	     */
	    fprintf(stderr, "%s.meta: PMID %s: no name in PMNS: %s\n",
		archname, pmIDStr(dp->pmid), pmErrStr(sts));
	    name = NULL;
	}
	if (dp->type == PM_TYPE_NOSUPPORT)
	    goto next;
	if (dp->type < PM_TYPE_32 || dp->type > PM_TYPE_HIGHRES_EVENT) {
	    fprintf(stderr, "%s.meta: %s [%s]: bad type (%d) in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->type);
	}
	/*
	 * Note: dynamic metrics are special when logged in an archive and
         */
	if (dp->indom != PM_INDOM_NULL &&
	    !IS_DERIVED_LOGGED(dp->pmid) &&
	    pmID_domain(dp->pmid) != pmInDom_domain(dp->indom)) {
	    fprintf(stderr, "%s.meta: %s [%s]: domain of pmid (%d) != domain of indom (%d)\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		pmID_domain(dp->pmid), pmInDom_domain(dp->indom));
	}
	if (dp->sem != PM_SEM_COUNTER && dp->sem != PM_SEM_INSTANT &&
	    dp->sem != PM_SEM_DISCRETE) {
	    fprintf(stderr, "%s.meta: %s [%s]: bad semantics (%d) in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->sem);
	}
	/*
	 * Heuristic ... dimension should really be in the range -2,2
	 * (inclusive)
	 */
	if (dp->units.dimSpace < -2 || dp->units.dimSpace > 2) {
	    fprintf(stderr, "%s.meta: %s [%s]: bad dimension (%d) for Space in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.dimSpace);
	}
	if (dp->units.dimTime < -2 || dp->units.dimTime > 2) {
	    fprintf(stderr, "%s.meta: %s [%s]: bad dimension (%d) for Time in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.dimTime);
	}
	if (dp->units.dimCount < -2 || dp->units.dimCount > 2) {
	    fprintf(stderr, "%s.meta: %s [%s]: bad dimension (%d) for Count in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.dimCount);
	}
	/*
	 * only Space and Time have sensible upper bounds, but if dimension
	 * is 0, scale should also be 0
	 */
	if (dp->units.dimSpace == 0 && dp->units.scaleSpace != 0) {
	    fprintf(stderr, "%s.meta: %s [%s]: non-zero scale (%d) with zero dimension for Space in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.scaleSpace);
	}
	if (dp->units.scaleSpace > PM_SPACE_EBYTE) {
	    fprintf(stderr, "%s.meta: %s [%s]: bad scale (%d) for Space in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.scaleSpace);
	}
	if (dp->units.dimTime == 0 && dp->units.scaleTime != 0) {
	    fprintf(stderr, "%s.meta: %s [%s]: non-zero scale (%d) with zero dimension for Time in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.scaleTime);
	}
	if (dp->units.scaleTime > PM_TIME_HOUR) {
	    fprintf(stderr, "%s.meta: %s [%s]: bad scale (%d) for Time in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.scaleTime);
	}
	if (dp->units.dimCount == 0 && dp->units.scaleCount != 0) {
	    fprintf(stderr, "%s.meta: %s [%s]: non-zero scale (%d) with zero dimension for Count in pmDesc\n",
		archname, name == NULL ? "unknown" : name, pmIDStr(dp->pmid),
		dp->units.scaleCount);
	}


next:
	if (name != NULL)
	    free(name);

	hp = __pmHashWalk(&ctxp->c_archctl->ac_log->hashpmid, PM_HASH_WALK_NEXT);
    }

    /*
     * check timestamps in selected metadata records ... need to mimic
     * __pmLogLoadMeta() here
     */
    f = lcp->mdfp;
    __pmGetArchiveEnd(ctxp->c_archctl, &end);
    __pmFseek(f, (long)__pmLogLabelSize(lcp), SEEK_SET);
    for ( ; ; ) {
	if (offset != -1)
	    offset = __pmFtell(f);
	n = (int)__pmFread(&h, 1, sizeof(__pmLogHdr), f);
	if (n != sizeof(h)) {
	    /*
	     * assume end of file ... other conditions picked
	     * up in pass0
	     */
	    break;
	}
	/* swab hdr */
        h.len = ntohl(h.len);
        h.type = ntohl(h.type);
	nread = 0;
	if (h.type == TYPE_INDOM || h.type == TYPE_INDOM_DELTA || h.type == TYPE_INDOM_V2 || h.type == TYPE_LABEL || h.type == TYPE_LABEL_V2) {
	    /* timestamp in next 3 (or 2) 32-bit words + indom */
	    __int32_t	buf[4];
	    pmInDom	indom;
	    int		bad = 0;
	    n = (int)__pmFread(buf, 1, sizeof(buf), f);
	    if (n == sizeof(buf)) {
		nread += sizeof(buf);
		if (__pmLogVersion(lcp) == PM_LOG_VERS03) {
		    __pmLoadTimestamp(buf, &stamp);
		    indom = __ntohpmInDom(buf[3]);
		}
		else {
		    __pmLoadTimeval(buf, &stamp);
		    indom = __ntohpmInDom(buf[2]);
		}
		if (__pmTimestampSub(&stamp, &lcp->label.start) < 0) {
		    fprintf(stderr, "%s.meta[record %d]: pmInDom %s: timestamp ",
			archname, nrec, pmInDomStr(indom));
		    __pmPrintTimestamp(stderr, &stamp);
		    fprintf(stderr, " before archive start ");
		    __pmPrintTimestamp(stderr, &lcp->label.start);
		    fputc('\n', stderr);
		    bad = 1;
		}
		else if (end.sec > 0 && __pmTimestampSub(&end, &stamp) < 0) {
		    fprintf(stderr, "%s.meta[record %d]: pmInDom %s: timestamp ",
			archname, nrec, pmInDomStr(indom));
		    __pmPrintTimestamp(stderr, &stamp);
		    fprintf(stderr, " after archive end ");
		    __pmPrintTimestamp(stderr, &end);
		    fputc('\n', stderr);
		    bad = 1;
		}
		else if (last_stamp.sec > 0 && __pmTimestampSub(&stamp, &last_stamp) < 0) {
		    fprintf(stderr, "%s.meta[record %d]: pmInDom %s: timestamp ",
			archname, nrec, pmInDomStr(indom));
		    __pmPrintTimestamp(stderr, &stamp);
		    fprintf(stderr, " before previous metadata timestamp ");
		    __pmPrintTimestamp(stderr, &last_stamp);
		    fputc('\n', stderr);
		    bad = 1;
		}
		if (bad && offset != -1) {
		    char	path[MAXPATHLEN];
		    struct stat	sbuf;
		    pmsprintf(path, sizeof(path), "%s.meta", archname);
		    if (stat(path, &sbuf) >= 0) {
			fprintf(stderr, "%s: last valid record ends at offset %lld (of %lld)\n",
				path, (long long)offset, (long long)sbuf.st_size);
		    }
		    else {
			fprintf(stderr, "%s: last valid record ends at offset %lld of ??? bytes\n",
				path, (long long)offset);
			sbuf.st_size = 0;
		    }
		    if (try_truncate(path, offset, sbuf.st_size)) {
			/* we're done here ... */
			break;
		    }
		    /* one-trip, no further offset reporting or repairing */
		    offset = -1;
		}
		last_stamp = stamp;	/* struct assignment */
	    }
	}
	else if (h.type == TYPE_DESC || h.type == TYPE_TEXT) {
	    /* no timestamps for these ones */
	    ;
	}
	else {
	    fprintf(stderr, "%s.meta[record %d]: Botch BAD type meta off=%ld type=%d\n", archname, nrec, offset, h.type);
	}
	__pmFseek(f, (long)h.len - sizeof(h) - nread, SEEK_CUR);
	nrec++;
    }


    return 0;
}
