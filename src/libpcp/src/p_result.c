/*
 * Copyright (c) 2012-2014 Red Hat.
 * Copyright (c) 1995-2000 Silicon Graphics, Inc.  All Rights Reserved.
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
 * Thread-safe note
 *
 * Because __pmFindPDUBuf() returns with a pinned pdu buffer, the
 * buffer passed back from __pmEncodeResult() must also remain pinned
 * (otherwise another thread could clobber the buffer after returning
 * from __pmEncodeResult()) ... it is the caller of __pmEncodeResult()
 * who is responsible for (a) not pinning the buffer again, and (b)
 * ensuring _someone_ will unpin the buffer when it is safe to do so.
 *
 * Similarly, __pmDecodeResult() accepts a pinned buffer and returns
 * a pmResult that (on 64-bit pointer platforms) may contain pointers
 * into a second underlying pinned buffer.  The input buffer remains
 * pinned, the second buffer will be pinned if it is used.  The caller
 * will typically call pmFreeResult(), but also needs to call
 * __pmUnpinPDUBuf() for the input PDU buffer.  When the result contains
 * pointers back into the input PDU buffer, this will be pinned _twice_
 * so the pmFreeResult() and __pmUnpinPDUBuf() calls will still be
 * required.
 */

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "internal.h"

/*
 * PDU for pmResult (PDU_RESULT)
 */

typedef struct {
    pmID		pmid;
    int			numval;		/* no. of vlist els to follow, or error */
    int			valfmt;		/* insitu or pointer */
    __pmValue_PDU	vlist[1];	/* zero or more */
} vlist_t;

typedef struct {
    __pmPDUHdr		hdr;
    __pmTimeval		timestamp;	/* when returned */
    int			numpmid;	/* no. of PMIDs to follow */
    __pmPDU		data[1];	/* zero or more */
} result_t;

int
__pmEncodeResult(int targetfd, const pmResult *result, __pmPDU **pdubuf)
{
    int		i;
    int		j;
    size_t	need;	/* bytes for the PDU */
    size_t	vneed;	/* additional bytes for the pmValueBlocks on the end */
    __pmPDU	*_pdubuf;
    __pmPDU	*vbp;
    result_t	*pp;
    vlist_t	*vlp;

    need = sizeof(result_t) - sizeof(__pmPDU);
    vneed = 0;
    /* now add space for each vlist_t (data in result_t) */
    for (i = 0; i < result->numpmid; i++) {
	pmValueSet	*vsp = result->vset[i];
	/* need space for PMID and count of values (defer valfmt until
	 * we know numval > 0, which means there should be a valfmt)
	 */
	need += sizeof(pmID) + sizeof(int);
	for (j = 0; j < vsp->numval; j++) {
	    /* plus value, instance pair */
	    need += sizeof(__pmValue_PDU);
	    if (vsp->valfmt != PM_VAL_INSITU) {
		/* plus pmValueBlock */
		vneed += PM_PDU_SIZE_BYTES(vsp->vlist[j].value.pval->vlen);
	    }
	}
	if (j)
	    /* optional value format, if any values present */
	    need += sizeof(int);
    }
    /*
     * Need to reserve additonal space for trailer (an int) in case the
     * PDU buffer is used by __pmLogPutResult2()
     */
    if ((_pdubuf = __pmFindPDUBuf((int)(need+vneed+sizeof(int)))) == NULL)
	return -oserror();
    pp = (result_t *)_pdubuf;
    pp->hdr.len = (int)(need+vneed);
    pp->hdr.type = PDU_RESULT;
    pp->timestamp.tv_sec = htonl((__int32_t)(result->timestamp.tv_sec));
    pp->timestamp.tv_usec = htonl((__int32_t)(result->timestamp.tv_usec));
    pp->numpmid = htonl(result->numpmid);
    vlp = (vlist_t *)pp->data;
    /*
     * Note: vbp, and hence offset in sent PDU is in units of __pmPDU
     */
    vbp = _pdubuf + need/sizeof(__pmPDU);
    for (i = 0; i < result->numpmid; i++) {
	pmValueSet	*vsp = result->vset[i];
	vlp->pmid = __htonpmID(vsp->pmid);
	if (vsp->numval > 0)
	    vlp->valfmt = htonl(vsp->valfmt);
	for (j = 0; j < vsp->numval; j++) {
	    vlp->vlist[j].inst = htonl(vsp->vlist[j].inst);
	    if (vsp->valfmt == PM_VAL_INSITU)
		vlp->vlist[j].value.lval = htonl(vsp->vlist[j].value.lval);
	    else {
		/*
		 * pmValueBlocks are harder!
		 * -- need to copy the len field (len) + len bytes (vbuf)
		 */
		int	nb;
		nb = vsp->vlist[j].value.pval->vlen;
		memcpy((void *)vbp, (void *)vsp->vlist[j].value.pval, nb);
		if ((nb % sizeof(__pmPDU)) != 0) {
		    /* clear the padding bytes, lest they contain garbage */
		    int	pad;
		    char	*padp = (char *)vbp + nb;
		    for (pad = sizeof(__pmPDU) - 1; pad >= (nb % sizeof(__pmPDU)); pad--)
			*padp++ = '~';	/* buffer end */
		}
		__htonpmValueBlock((pmValueBlock *)vbp);
		/* point to the value block at the end of the PDU */
		vlp->vlist[j].value.lval = htonl((int)(vbp - _pdubuf));
		vbp += PM_PDU_SIZE(nb);
	    }
	}
	vlp->numval = htonl(vsp->numval);
	if (j > 0)
	    vlp = (vlist_t *)((__psint_t)vlp + sizeof(*vlp) + (j-1)*sizeof(vlp->vlist[0]));
	else
	    vlp = (vlist_t *)((__psint_t)vlp + sizeof(vlp->pmid) + sizeof(vlp->numval));
    }
    *pdubuf = _pdubuf;

    /* Note _pdubuf remains pinned ... see thread-safe comments above */
    return 0;
}

int
__pmSendResult(int fd, int from, const pmResult *result)
{
    int		sts;
    __pmPDU	*pdubuf;
    result_t	*pp;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU)
	__pmDumpResult(stderr, result);
#endif
    if ((sts = __pmEncodeResult(fd, result, &pdubuf)) < 0)
	return sts;
    pp = (result_t *)pdubuf;
    pp->hdr.from = from;
    sts = __pmXmitPDU(fd, pdubuf);
    __pmUnpinPDUBuf(pdubuf);
    return sts;
}

/*
 * enter here with pdubuf already pinned ... result may point into
 * _another_ pdu buffer that is pinned on exit
 */
int
__pmDecodeResult(__pmPDU *pdubuf, pmResult **result)
{
    int		numpmid;	/* number of metrics */
    int		i;		/* range of metrics */
    int		j;		/* range over values */
    int		index;
    int		vsize;		/* size of vlist_t's in PDU buffer */
    char	*pduend;	/* end pointer for incoming buffer */
    char	*vsplit;	/* vlist/valueblock division point */
    result_t	*pp;
    vlist_t	*vlp;
    pmResult	*pr;
#if defined(HAVE_64BIT_PTR)
    char	*newbuf;
    int		valfmt;
    int		numval;
    int		need;
/*
 * Note: all sizes are in units of bytes ... beware that pp->data is in
 *	 units of __pmPDU
 */
    int		nvsize;		/* size of pmValue's after decode */
    int		offset;		/* differences in sizes */
    int		vbsize;		/* size of pmValueBlocks */
    pmValueSet	*nvsp;
#elif defined(HAVE_32BIT_PTR)
    pmValueSet	*vsp;		/* vlist_t == pmValueSet */
#else
    Bozo - unexpected sizeof pointer!!
#endif

    pp = (result_t *)pdubuf;
    pduend = (char *)pdubuf + pp->hdr.len;
    if (pduend - (char *)pdubuf < sizeof(result_t) - sizeof(__pmPDU)) {
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    fprintf(stderr, "__pmDecodeResult: Bad: len=%d smaller than min %d\n", pp->hdr.len, (int)(sizeof(result_t) - sizeof(__pmPDU)));
	}
#endif
	return PM_ERR_IPC;
    }

    numpmid = ntohl(pp->numpmid);
    if (numpmid < 0 || numpmid > pp->hdr.len) {
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    fprintf(stderr, "__pmDecodeResult: Bad: numpmid=%d negative or not smaller than PDU len %d\n", numpmid, pp->hdr.len);
	}
#endif
	return PM_ERR_IPC;
    }
    if (numpmid >= (INT_MAX - sizeof(pmResult)) / sizeof(pmValueSet *)) {
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    fprintf(stderr, "__pmDecodeResult: Bad: numpmid=%d larger than max %ld\n", numpmid, (long)(INT_MAX - sizeof(pmResult) / sizeof(pmValueSet *)));
	}
#endif
	return PM_ERR_IPC;
    }
    if ((pr = (pmResult *)malloc(sizeof(pmResult) +
			     (numpmid - 1) * sizeof(pmValueSet *))) == NULL) {
	return -oserror();
    }
    pr->numpmid = numpmid;
    pr->timestamp.tv_sec = ntohl(pp->timestamp.tv_sec);
    pr->timestamp.tv_usec = ntohl(pp->timestamp.tv_usec);

#if defined(HAVE_64BIT_PTR)
    vsplit = pduend;	/* smallest observed value block pointer */
    nvsize = vsize = vbsize = 0;
    for (i = 0; i < numpmid; i++) {
	vlp = (vlist_t *)&pp->data[vsize/sizeof(__pmPDU)];

	if (sizeof(*vlp) - sizeof(vlp->vlist) - sizeof(int) > (pduend - (char *)vlp)) {
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] outer vlp past end of PDU buffer\n", i);
	}
#endif
	    goto corrupt;
	}

	vsize += sizeof(vlp->pmid) + sizeof(vlp->numval);
	nvsize += sizeof(pmValueSet);
	numval = ntohl(vlp->numval);
	valfmt = ntohl(vlp->valfmt);
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    pmID	pmid = __ntohpmID(vlp->pmid);
	    char	strbuf[20];
	    fprintf(stderr, "vlist[%d] pmid: %s numval: %d",
			i, pmIDStr_r(pmid, strbuf, sizeof(strbuf)), numval);
	}
#endif
	/* numval may be negative - it holds an error code in that case */
	if (numval > pp->hdr.len) {
#ifdef PCP_DEBUG
	    if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
		fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] numval=%d > len=%d\n", i, numval, pp->hdr.len);
	    }
#endif
	    goto corrupt;
	}
	if (numval > 0) {
	    if (sizeof(*vlp) - sizeof(vlp->vlist) > (pduend - (char *)vlp)) {
#ifdef PCP_DEBUG
		if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
		    fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] inner vlp past end of PDU buffer\n", i);
		}
#endif
		goto corrupt;
	    }
	    if (numval >= (INT_MAX - sizeof(*vlp)) / sizeof(__pmValue_PDU)) {
#ifdef PCP_DEBUG
		if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
		    fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] numval=%d > max=%ld\n", i, numval, (long)((INT_MAX - sizeof(*vlp)) / sizeof(__pmValue_PDU)));
		}
#endif
		goto corrupt;
	    }
	    vsize += sizeof(vlp->valfmt) + numval * sizeof(__pmValue_PDU);
	    nvsize += (numval - 1) * sizeof(pmValue);
#ifdef PCP_DEBUG
	    if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
		fprintf(stderr, " valfmt: %s",
			valfmt == PM_VAL_INSITU ? "insitu" : "ptr");
	    }
#endif
	    if (valfmt != PM_VAL_INSITU) {
		for (j = 0; j < numval; j++) {
		    __pmValue_PDU *pduvp;
		    pmValueBlock *pduvbp;

		    pduvp = &vlp->vlist[j];
		    if (sizeof(__pmValue_PDU) > (size_t)(pduend - (char *)pduvp)) {
#ifdef PCP_DEBUG
			if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
			    fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] value[%d] initial pduvp past end of PDU buffer\n", i, j);
			}
#endif
			goto corrupt;
		    }
		    index = ntohl(pduvp->value.lval);
		    if (index < 0 || index > pp->hdr.len) {
#ifdef PCP_DEBUG
			if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
			    fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] value[%d] index=%d\n", i, j, index);
			}
#endif
			goto corrupt;
		    }
		    pduvbp = (pmValueBlock *)&pdubuf[index];
		    if (vsplit > (char *)pduvbp)
			vsplit = (char *)pduvbp;
		    if (sizeof(unsigned int) > (size_t)(pduend - (char *)pduvbp)) {
#ifdef PCP_DEBUG
			if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
			    fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] value[%d] second pduvp past end of PDU buffer\n", i, j);
			}
#endif
			goto corrupt;
		    }
		    __ntohpmValueBlock(pduvbp);
		    if (pduvbp->vlen < PM_VAL_HDR_SIZE || pduvbp->vlen > pp->hdr.len) {
#ifdef PCP_DEBUG
			if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
			    fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] value[%d] vlen=%d\n", i, j, pduvbp->vlen);
			}
#endif
			goto corrupt;
		    }
		    if (pduvbp->vlen > (size_t)(pduend - (char *)pduvbp)) {
#ifdef PCP_DEBUG
			if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
			    fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] value[%d] third pduvp past end of PDU buffer\n", i, j);
			}
#endif
			goto corrupt;
		    }
		    vbsize += PM_PDU_SIZE_BYTES(pduvbp->vlen);
#ifdef PCP_DEBUG
		    if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
			fprintf(stderr, " len: %d type: %d",
			    pduvbp->vlen - PM_VAL_HDR_SIZE, pduvbp->vtype);
		    }
#endif
		}
	    }
	}
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    fputc('\n', stderr);
	}
#endif
    }

    need = nvsize + vbsize;
    offset = sizeof(result_t) - sizeof(__pmPDU) + vsize;

#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
	fprintf(stderr, "need: %d vsize: %d nvsize: %d vbsize: %d offset: %d hdr.len: %d pduend: %p vsplit: %p (diff %d) pdubuf: %p (diff %d)\n", need, vsize, nvsize, vbsize, offset, pp->hdr.len, pduend, vsplit, (int)(pduend-vsplit), pdubuf, (int)(pduend-(char *)pdubuf));
    }
#endif

    if (need < 0 ||
	vsize > INT_MAX / sizeof(__pmPDU) ||
	vbsize > INT_MAX / sizeof(pmValueBlock) ||
	offset != pp->hdr.len - (pduend - vsplit) ||
	offset + vbsize != pduend - (char *)pdubuf) {
	goto corrupt;
    }

    /* the original pdubuf is already pinned so we won't allocate that again */
    if ((newbuf = (char *)__pmFindPDUBuf(need)) == NULL) {
	free(pr);
	return -oserror();
    }

    /*
     * At this point, we have verified the contents of the incoming PDU and
     * the following is set up ...
     *
     * From the original PDU buffer ...
     * :-----:---------:-----------:----------------:---------------------:
     * : Hdr :timestamp:  numpmid  : ... vlists ... : .. pmValueBlocks .. :
     * :-----:---------:-----------:----------------:---------------------:
     *                              <---  vsize ---> <----   vbsize  ---->
     *                                    bytes              bytes
     *
     * and in the new PDU buffer we are going to build ...
     * :---------------------:---------------------:
     * : ... pmValueSets ... : .. pmValueBlocks .. :
     * :---------------------:---------------------:
     *  <---   nvsize    ---> <----   vbsize  ---->
     *         bytes                  bytes
     */

    if (vbsize) {
	/* pmValueBlocks (if any) are copied across "as is" */
	index = vsize / sizeof(__pmPDU);
	memcpy((void *)&newbuf[nvsize], (void *)&pp->data[index], vbsize);
    }

    /*
     * offset is a bit tricky ... _add_ the expansion due to the
     * different sizes of the vlist_t and pmValueSet, and _subtract_
     * the PDU header and pmResult fields ...
     */
    offset = nvsize - vsize
		    - (int)sizeof(pp->hdr) - (int)sizeof(pp->timestamp)
		    - (int)sizeof(pp->numpmid);
    nvsize = vsize = 0;
    for (i = 0; i < numpmid; i++) {
	vlp = (vlist_t *)&pp->data[vsize/sizeof(__pmPDU)];
	nvsp = (pmValueSet *)&newbuf[nvsize];
	pr->vset[i] = nvsp;
	nvsp->pmid = __ntohpmID(vlp->pmid);
	nvsp->numval = ntohl(vlp->numval);
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    char	strbuf[20];
	    fprintf(stderr, "new vlist[%d] pmid: %s numval: %d",
			i, pmIDStr_r(nvsp->pmid, strbuf, sizeof(strbuf)), nvsp->numval);
	}
#endif

	vsize += sizeof(nvsp->pmid) + sizeof(nvsp->numval);
	nvsize += sizeof(pmValueSet);
	if (nvsp->numval > 0) {
	    nvsp->valfmt = ntohl(vlp->valfmt);
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    fprintf(stderr, " valfmt: %s",
			    nvsp->valfmt == PM_VAL_INSITU ? "insitu" : "ptr");
	}
#endif
	    vsize += sizeof(nvsp->valfmt) + nvsp->numval * sizeof(__pmValue_PDU);
	    nvsize += (nvsp->numval - 1) * sizeof(pmValue);
	    for (j = 0; j < nvsp->numval; j++) {
		__pmValue_PDU	*vp = &vlp->vlist[j];
		pmValue		*nvp = &nvsp->vlist[j];

		nvp->inst = ntohl(vp->inst);
		if (nvsp->valfmt == PM_VAL_INSITU) {
		    nvp->value.lval = ntohl(vp->value.lval);
#ifdef PCP_DEBUG
		    if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
			fprintf(stderr, " value: %d", nvp->value.lval);
		    }
#endif
		}
		else {
		    /*
		     * in the input PDU buffer, pval is an index to the
		     * start of the pmValueBlock, in units of __pmPDU
		     */
		    index = sizeof(__pmPDU) * ntohl(vp->value.pval) + offset;
		    nvp->value.pval = (pmValueBlock *)&newbuf[index];
#ifdef PCP_DEBUG
		    if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
			int		k, len;
			len = nvp->value.pval->vlen - PM_VAL_HDR_SIZE;
			fprintf(stderr, " len: %d type: %d value: 0x", len,
				nvp->value.pval->vtype);
			for (k = 0; k < len; k++)
			    fprintf(stderr, "%02x", nvp->value.pval->vbuf[k]);
		    }
#endif
		}
	    }
	}
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    fputc('\n', stderr);
	}
#endif
    }
    if (numpmid == 0)
	__pmUnpinPDUBuf(newbuf);

#elif defined(HAVE_32BIT_PTR)

    pr->timestamp.tv_sec = ntohl(pp->timestamp.tv_sec);
    pr->timestamp.tv_usec = ntohl(pp->timestamp.tv_usec);
    vlp = (vlist_t *)pp->data;
    vsplit = pduend;
    vsize = 0;

    /*
     * Now fix up any pointers in pmValueBlocks (currently offsets into
     * the PDU buffer) by adding the base address of the PDU buffer.
     */
    for (i = 0; i < numpmid; i++) {
	if (sizeof(*vlp) - sizeof(vlp->vlist) - sizeof(int) > (pduend - (char *)vlp)) {
#ifdef PCP_DEBUG
	    if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
		fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] outer vlp past end of PDU buffer\n", i);
	    }
#endif
	    goto corrupt;
	}

	vsp = pr->vset[i] = (pmValueSet *)vlp;
	vsp->pmid = __ntohpmID(vsp->pmid);
	vsp->numval = ntohl(vsp->numval);
	/* numval may be negative - it holds an error code in that case */
	if (vsp->numval > pp->hdr.len) {
#ifdef PCP_DEBUG
	    if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
		fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] numval=%d > len=%d\n", i, vsp->numval, pp->hdr.len);
	    }
#endif
	    goto corrupt;
	}

	vsize += sizeof(vsp->pmid) + sizeof(vsp->numval);
	if (vsp->numval > 0) {
	    if (sizeof(*vlp) - sizeof(vlp->vlist) > (pduend - (char *)vlp)) {
#ifdef PCP_DEBUG
		if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
		    fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] inner vlp past end of PDU buffer\n", i);
		}
#endif
		goto corrupt;
	    }
	    if (vsp->numval >= (INT_MAX - sizeof(*vlp)) / sizeof(__pmValue_PDU)) {
#ifdef PCP_DEBUG
		if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
		    fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] numval=%d > max=%ld\n", i, vsp->numval, (long)((INT_MAX - sizeof(*vlp)) / sizeof(__pmValue_PDU)));
		}
#endif
		goto corrupt;
	    }
	    vsp->valfmt = ntohl(vsp->valfmt);
	    vsize += sizeof(vsp->valfmt) + vsp->numval * sizeof(__pmValue_PDU);
	    for (j = 0; j < vsp->numval; j++) {
		__pmValue_PDU *pduvp;
		pmValueBlock *pduvbp;

		pduvp = &vsp->vlist[j];
		if (sizeof(__pmValue_PDU) > (size_t)(pduend - (char *)pduvp)) {
#ifdef PCP_DEBUG
		    if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
			fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] value[%d] initial pduvp past end of PDU buffer\n", i, j);
		    }
#endif
		    goto corrupt;
		}

		pduvp->inst = ntohl(pduvp->inst);
		if (vsp->valfmt == PM_VAL_INSITU) {
		    pduvp->value.lval = ntohl(pduvp->value.lval);
		} else {
		    /* salvage pmValueBlocks from end of PDU */
		    index = ntohl(pduvp->value.lval);
		    if (index < 0 || index > pp->hdr.len) {
#ifdef PCP_DEBUG
			if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
			    fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] value[%d] index=%d\n", i, j, index);
			}
#endif
			goto corrupt;
		    }
		    pduvbp = (pmValueBlock *)&pdubuf[index];
		    if (vsplit > (char *)pduvbp)
			vsplit = (char *)pduvbp;
		    if (sizeof(unsigned int) > (size_t)(pduend - (char *)pduvbp)) {
#ifdef PCP_DEBUG
			if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
			    fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] value[%d] second pduvp past end of PDU buffer\n", i, j);
			}
#endif
			goto corrupt;
		    }
		    __ntohpmValueBlock(pduvbp);
		    if (pduvbp->vlen < PM_VAL_HDR_SIZE || pduvbp->vlen > pp->hdr.len) {
#ifdef PCP_DEBUG
			if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
			    fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] value[%d] vlen=%d\n", i, j, pduvbp->vlen);
			}
#endif
			goto corrupt;
		    }
		    if (pduvbp->vlen > (size_t)(pduend - (char *)pduvbp)) {
#ifdef PCP_DEBUG
			if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
			    fprintf(stderr, "__pmDecodeResult: Bad: pmid[%d] value[%d] third pduvp past end of PDU buffer\n", i, j);
			}
#endif
			goto corrupt;
		    }
		    pduvp->value.pval = pduvbp;
		}
	    }
	    vlp = (vlist_t *)((__psint_t)vlp + sizeof(*vlp) + (vsp->numval-1)*sizeof(vlp->vlist[0]));
	}
	else {
	    vlp = (vlist_t *)((__psint_t)vlp + sizeof(vlp->pmid) + sizeof(vlp->numval));
	}
    }
    if (numpmid > 0) {
	if (sizeof(result_t) - sizeof(__pmPDU) + vsize != pp->hdr.len - (pduend - vsplit)) {
#ifdef PCP_DEBUG
	    if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
		fprintf(stderr, "__pmDecodeResult: Bad: vsplit past end of PDU buffer\n");
	    }
#endif
	    goto corrupt;
	}
	/*
	 * PDU buffer already pinned on entry, pin again so that
	 * the caller can safely call _both_ pmFreeResult() and
	 * __pmUnpinPDUBuf() ... refer to thread-safe notes above.
	 */
	__pmPinPDUBuf(pdubuf);
    }
#endif

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU)
	__pmDumpResult(stderr, pr);
#endif

    /*
     * Note we return with the input buffer (pdubuf) still pinned and
     * for the 64-bit pointer case the new buffer (newbuf) also pinned -
     * if numpmid != 0 see the thread-safe comments above
     */
    *result = pr;
    return 0;

corrupt:
    free(pr);
    return PM_ERR_IPC;
}
