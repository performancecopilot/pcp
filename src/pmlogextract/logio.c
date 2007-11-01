/*
 * utils for pmlogextract
 *
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: logio.c,v 1.13 2004/06/20 22:38:25 kenmcd Exp $"

#include <unistd.h>
#include <errno.h>
#include "pmapi.h"
#include "impl.h"

/*
 * raw read of next log record - largely stolen from __pmLogRead in libpcp
 */
int
_pmLogGet(__pmLogCtl *lcp, int vol, __pmPDU **pb)
{
    int		head;
    int		tail;
    int		sts;
    long	offset;
    char	*p;
    __pmPDU	*lpb;
    FILE	*f;

    if (vol == PM_LOG_VOL_META)
	f = lcp->l_mdfp;
    else
	f = lcp->l_mfp;

    offset = ftell(f);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	fprintf(stderr, "_pmLogGet: fd=%d vol=%d posn=%ld ",
	    fileno(f), vol, offset);
    }
#endif

again:
    sts = (int)fread(&head, 1, sizeof(head), f);
    if (sts != sizeof(head)) {
	if (sts == 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG)
		fprintf(stderr, "AFTER end\n");
#endif
	    fseek(f, offset, SEEK_SET);
	    if (vol != PM_LOG_VOL_META) {
		if (lcp->l_curvol < lcp->l_maxvol) {
		    if (__pmLogChangeVol(lcp, lcp->l_curvol+1) == 0) {
			f = lcp->l_mfp;
			goto again;
		    }
		}
	    }
	    return PM_ERR_EOL;
	}
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "Error: hdr fread=%d %s\n", sts, strerror(errno));
#endif
	if (sts > 0)
	    return PM_ERR_LOGREC;
	else
	    return -errno;
    }

    if ((lpb = (__pmPDU *)malloc(ntohl(head))) == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "Error: __pmFindPDUBuf(%d) %s\n",
		(int)ntohl(head), strerror(errno));
#endif
	fseek(f, offset, SEEK_SET);
	return -errno;
    }

    lpb[0] = head;
    if ((sts = (int)fread(&lpb[1], 1, ntohl(head) - sizeof(head), f)) != ntohl(head) - sizeof(head)) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "Error: data fread=%d %s\n", sts, strerror(errno));
#endif
	if (sts == 0) {
	    fseek(f, offset, SEEK_SET);
	    return PM_ERR_EOL;
	}
	else if (sts > 0)
	    return PM_ERR_LOGREC;
	else
	    return -errno;
    }


    p = (char *)lpb;
    memcpy(&tail, &p[ntohl(head) - sizeof(head)], sizeof(head));
    if (head != tail) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "Error: head-tail mismatch (%d-%d)\n",
		(int)ntohl(head), (int)ntohl(tail));
#endif
	return PM_ERR_LOGREC;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	if (vol != PM_LOG_VOL_META || ntohl(lpb[1]) == TYPE_INDOM) {
	    fprintf(stderr, "@");
	    if (sts >= 0) {
		struct timeval	stamp;
		__pmTimeval		*tvp = (__pmTimeval *)&lpb[vol == PM_LOG_VOL_META ? 2 : 1];
		stamp.tv_sec = ntohl(tvp->tv_sec);
		stamp.tv_usec = ntohl(tvp->tv_usec);
		__pmPrintStamp(stderr, &stamp);
	    }
	    else
		fprintf(stderr, "unknown time");
	}
	fprintf(stderr, " len=%d (incl head+tail)\n", (int)ntohl(head));
    }
#endif

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	int		i, j;
	struct timeval	stamp;
	__pmTimeval	*tvp = (__pmTimeval *)&lpb[vol == PM_LOG_VOL_META ? 2 : 1];
	fprintf(stderr, "_pmLogGet");
	if (vol != PM_LOG_VOL_META || ntohl(lpb[1]) == TYPE_INDOM) {
	    fprintf(stderr, " timestamp=");
	    stamp.tv_sec = ntohl(tvp->tv_sec);
	    stamp.tv_usec = ntohl(tvp->tv_usec);
	    __pmPrintStamp(stderr, &stamp);
	}
	fprintf(stderr, " " PRINTF_P_PFX "%p ... " PRINTF_P_PFX "%p", lpb, &lpb[ntohl(head)/sizeof(__pmPDU) - 1]);
	fputc('\n', stderr);
	fprintf(stderr, "%03d: ", 0);
	for (j = 0, i = 0; j < ntohl(head)/sizeof(__pmPDU); j++) {
	    if (i == 8) {
		fprintf(stderr, "\n%03d: ", j);
		i = 0;
	    }
	    fprintf(stderr, "" PRINTF_P_PFX "%p ", (unsigned int *)lpb[j]);
	    i++;
	}
	fputc('\n', stderr);
    }
#endif

    *pb = lpb;
    return 0;
}

int
_pmLogPut(FILE *f, __pmPDU *pb)
{
    int		rlen = ntohl(pb[0]);
    int		sts;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	fprintf(stderr, "_pmLogPut: fd=%d rlen=%d\n",
	    fileno(f), rlen);
    }
#endif

    if ((sts = (int)fwrite(pb, 1, rlen, f)) != rlen) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "_pmLogPut: fwrite=%d %s\n", sts, strerror(errno));
#endif
	return -errno;
    }
    return 0;
}
