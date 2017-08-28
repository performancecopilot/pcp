/*
 * utils for pmlogconv
 *
 * Copyright (c) 2017 Red Hat.
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
 */

/* pinched from pmlogextract and libpcp */

#include "pcp/pmapi.h"
#include "pcp/impl.h"

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

    offset = __pmFtell(f);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	fprintf(stderr, "_pmLogGet: fd=%d vol=%d posn=%ld ",
	    fileno(f), vol, offset);
    }
#endif

again:
    sts = (int)__pmFread(&head, 1, sizeof(head), f);
    if (sts != sizeof(head)) {
	if (sts == 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG)
		fprintf(stderr, "AFTER end\n");
#endif
	    __pmFseek(f, offset, SEEK_SET);
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
	__pmFseek(f, offset, SEEK_SET);
	return -errno;
    }

    lpb[0] = head;
    if ((sts = (int)__pmFread(&lpb[1], 1, ntohl(head) - sizeof(head), f)) != ntohl(head) - sizeof(head)) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "Error: data fread=%d %s\n", sts, strerror(errno));
#endif
	if (sts == 0) {
	    __pmFseek(f, offset, SEEK_SET);
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
	    fprintf(stderr, "0x%x ", lpb[j]);
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

/*
 * like __pmDecodeDesc but pmDesc in metadata not PDU
 */
typedef struct {
    __pmLogHdr	hdr;
    pmDesc	desc;
} desc_t;

void
_pmUnpackDesc(__pmPDU *pdubuf, pmDesc *desc)
{
    desc_t	*pp;

    pp = (desc_t *)pdubuf;
    desc->type = ntohl(pp->desc.type);
    desc->sem = ntohl(pp->desc.sem);
    desc->indom = __ntohpmInDom(pp->desc.indom);
    desc->units = __ntohpmUnits(pp->desc.units);
    desc->pmid = __ntohpmID(pp->desc.pmid);
    return;
}

/*
 * like rewrite_pdu() from pmlogger
 */

typedef struct {
    pmID		pmid;
    int			numval;		/* no. of vlist els to follow, or error */
    int			valfmt;		/* insitu or pointer */
    __pmValue_PDU	vlist[1];	/* zero or more */
} vlist_t;

typedef struct {
    int			hdr;
    // __pmPDUHdr		hdr;
    __pmTimeval		timestamp;	/* when returned */
    int			numpmid;	/* no. of PMIDs to follow */
    __pmPDU		data[1];	/* zero or more */
} result_t;

#define PM_ERR_BASE1 1000
#define XLATE_ERR_1TO2(e) \
        ((e) <= -PM_ERR_BASE1 ? (e)+PM_ERR_BASE1-PM_ERR_BASE2 : (e))

void
rewrite_pdu(__pmPDU *pb)
{
    result_t		*pp = (result_t *)pb;
    int			vsize;
    int			numpmid;
    int			numval;
    vlist_t		*vlp;
    int			i;

    numpmid = ntohl(pp->numpmid);
    vsize = 0;
    for (i = 0; i < numpmid; i++) {
	vlp = (vlist_t *)&pp->data[vsize/sizeof(__pmPDU)];
	numval = ntohl(vlp->numval);
	vsize += sizeof(vlp->pmid) + sizeof(vlp->numval);
	if (numval > 0)
	    vsize += sizeof(vlp->valfmt) + numval * sizeof(__pmValue_PDU);
	if (numval < 0)
	    vlp->numval = htonl(XLATE_ERR_1TO2(numval));
    }

    return;
}

