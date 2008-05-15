/*
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"

char __pmAbuf[ABUFSIZE] = { '\0' };	/* line buffer for PDU_ASCII Sends and Decodes */

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
    __pmIPC	*ipc;
    size_t	need;	/* bytes for the PDU */
    size_t	vneed;	/* additional bytes for the pmValueBlocks on the end */
    __pmPDU	*_pdubuf;
    __pmPDU	*vbp;
    result_t	*pp;
    vlist_t	*vlp;

    if ((i = __pmFdLookupIPC(targetfd, &ipc)) < 0)
	return i;

    /* to start with, need space for result_t with no data (__pmPDU) */
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
		vneed += sizeof(__pmPDU)*((vsp->vlist[j].value.pval->vlen - 1 + sizeof(__pmPDU))/sizeof(__pmPDU));
	    }
	}
	if (j)
	    /* optional value format, if any values present */
	    need += sizeof(int);
    }
    if ((_pdubuf = __pmFindPDUBuf((int)(need+vneed))) == NULL)
	return -errno;
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
#ifdef PCP_DEBUG
		if ((nb % sizeof(__pmPDU)) != 0) {
		    /* for Purify */
		    int	pad;
		    char	*padp = (char *)vbp + nb;
		    for (pad = sizeof(__pmPDU) - 1; pad >= (nb % sizeof(__pmPDU)); pad--)
			*padp++ = '~';	/* buffer end */
		}
#endif
		__htonpmValueBlock((pmValueBlock *)vbp);
		/* point to the value block at the end of the PDU */
		vlp->vlist[j].value.lval = htonl((int)(vbp - _pdubuf));
		vbp += (nb - 1 + sizeof(__pmPDU))/sizeof(__pmPDU);
	    }
	}
	vlp->numval = htonl(vsp->numval);
	if (j > 0)
	    vlp = (vlist_t *)((__psint_t)vlp + sizeof(*vlp) + (j-1)*sizeof(vlp->vlist[0]));
	else
	    vlp = (vlist_t *)((__psint_t)vlp + sizeof(vlp->pmid) + sizeof(vlp->numval));
    }
    *pdubuf = _pdubuf;
    return 0;
}

int
__pmSendResult(int fd, int mode, const pmResult *result)
{
    int		i;
    int		j;
    int		sts;
    __pmPDU	*pdubuf;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU)
	__pmDumpResult(stderr, result);
#endif
    if (mode == PDU_BINARY) {
	if ((sts = __pmEncodeResult(fd, result, &pdubuf)) < 0) return sts;
	return __pmXmitPDU(fd, pdubuf);
    }
    else {
	/* assume PDU_ASCII */
	int	nbytes;
	char	*q;

	snprintf(__pmAbuf, sizeof(__pmAbuf), "RESULT %d\n", result->numpmid);
	nbytes = (int)strlen(__pmAbuf);
	sts = __pmXmitAscii(fd, __pmAbuf, nbytes);
	if (sts < 0)
	    return sts;
	for (i = 0; i < result->numpmid; i++) {
	    pmValueSet	*vsp = result->vset[i];

	    snprintf(__pmAbuf, sizeof(__pmAbuf), ". %d %d %d", vsp->pmid, vsp->valfmt, vsp->numval);
	    for (j = 0; j < vsp->numval; j++) {
		q = &__pmAbuf[strlen(__pmAbuf)];
		snprintf(q, sizeof(__pmAbuf)-(q-__pmAbuf), " %d %d", vsp->vlist[j].inst, vsp->vlist[j].value.lval);
	    }
	    nbytes = (int)strlen(__pmAbuf);
	    __pmAbuf[nbytes++] = '\n';
	    sts = __pmXmitAscii(fd, __pmAbuf, nbytes);
	    if (sts < 0)
		return sts;
	}
	return 0;
    }
}

int
__pmDecodeResult(__pmPDU *pdubuf, int mode, pmResult **result)
{
    int		numpmid;	/* number of metrics */
    int		numval;		/* number of values */
    int		sts;		/* function status */
    int		i;		/* range of metrics */
    int		j;		/* range over values */
    result_t	*pp;
    vlist_t	*vlp;
    char	*q;
    pmResult	*pr = NULL;
    pmID	pmid;
    __pmIPC	*ipc;

#if defined(HAVE_64BIT_PTR)
    char	*newbuf;
    int		valfmt;
/*
 * Note: all sizes are in units of bytes ... beware that pp->data is in
 *	 units of __pmPDU
 */
    int			vsize;		/* size of vlist_t's in PDU buffer */
    int			nvsize;		/* size of pmValue's after decode */
    int			offset;		/* differences in sizes */
    int			vbsize;		/* size of pmValueBlocks */
    pmValueSet		*nvsp;
#elif defined(HAVE_32BIT_PTR)
    pmValueSet		*vsp;		/* vlist_t == pmValueSet */
#endif

    if ((sts = __pmLookupIPC(&ipc)) < 0)
	return sts;

    if (mode == PDU_BINARY) {
	pp = (result_t *)pdubuf;
	numpmid = ntohl(pp->numpmid);
	if ((pr = (pmResult *)malloc(sizeof(pmResult) +
				     (numpmid - 1) * sizeof(pmValueSet *))) == NULL) {
	    sts = -errno;
	    goto badsts;
	}
	pr->numpmid = numpmid;
	pr->timestamp.tv_sec = ntohl(pp->timestamp.tv_sec);
	pr->timestamp.tv_usec = ntohl(pp->timestamp.tv_usec);

#if defined(HAVE_64BIT_PTR)

	nvsize = vsize = vbsize = 0;
	for (i = 0; i < numpmid; i++) {
	    vlp = (vlist_t *)&pp->data[vsize/sizeof(__pmPDU)];
	    vsize += sizeof(vlp->pmid) + sizeof(vlp->numval);
	    numval = ntohl(vlp->numval);
	    valfmt = ntohl(vlp->valfmt);
	    pmid = __ntohpmID(vlp->pmid);
#ifdef DESPERATE
	    fprintf(stderr, "vlist[%d] pmid: %s numval: %d",
			    i, pmIDStr(pmid), numval);
#endif
	    nvsize += sizeof(pmValueSet);
	    if (numval > 0) {
		vsize += sizeof(vlp->valfmt) + numval * sizeof(__pmValue_PDU);
		nvsize += (numval - 1) * sizeof(pmValue);
#ifdef DESPERATE
		fprintf(stderr, " valfmt: %s",
			    valfmt == PM_VAL_INSITU ? "insitu" : "ptr");
#endif
		if (valfmt != PM_VAL_INSITU) {
		    for (j = 0; j < numval; j++) {
			int		index = ntohl(vlp->vlist[j].value.pval);
			pmValueBlock	*pduvbp = (pmValueBlock *)&pdubuf[index];
			__ntohpmValueBlock(pduvbp);
			vbsize += sizeof(__pmPDU) * ((pduvbp->vlen + sizeof(__pmPDU) - 1) / sizeof(__pmPDU));
#ifdef DESPERATE
			fprintf(stderr, " len: %d type: %d",
			    pduvbp->vlen - PM_VAL_HDR_SIZE, pduvbp->vtype);
#endif
		    }
		}
	    }
#ifdef DESPERATE
	    fputc('\n', stderr);
#endif
	}
#ifdef DESPERATE
	fprintf(stderr, "vsize: %d nvsize: %d vbsize: %d\n", vsize, nvsize, vbsize);
#endif

	/* pin the original pdubuf so we don't just allocate that again */
	__pmPinPDUBuf(pdubuf);
	if ((newbuf = (char *)__pmFindPDUBuf((int)nvsize + vbsize)) == NULL) {
	    free(pr);
	    __pmUnpinPDUBuf(pdubuf);
	    return -errno;
	}

	/*
	 * At this point, we have the following set up ...
	 *
	 * From the original PDU buffer ...
	 * :-----:---------:-----------:----------------:--------------------:
	 * : Hdr : numpmid : timestamp : ... vlists ... : .. pmValueBocks .. :
	 * :-----:---------:-----------:----------------:--------------------:
	 *                              <---  vsize ---> <---   vbsize   --->
	 *                                    bytes             bytes
	 *
	 * and in the new PDU buffer we are going to build ...
	 * :---------------------:--------------------:
	 * : ... pmValueSets ... : .. pmValueBocks .. :
	 * :---------------------:--------------------:
	 *  <---   nvsize    ---> <---   vbsize   --->
	 *         bytes                 bytes
	 */

	if (vbsize) {
	    /* pmValueBlocks (if any) are copied across "as is" */
	    memcpy((void *)&newbuf[nvsize], (void *)&pp->data[vsize/sizeof(__pmPDU)], vbsize);
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
#ifdef DESPERATE
	    fprintf(stderr, "new vlist[%d] pmid: %s numval: %d",
			    i, pmIDStr(nvsp->pmid), nvsp->numval);
#endif

	    vsize += sizeof(nvsp->pmid) + sizeof(nvsp->numval);
	    nvsize += sizeof(pmValueSet);
	    if (nvsp->numval > 0) {
		nvsp->valfmt = ntohl(vlp->valfmt);
#ifdef DESPERATE
		fprintf(stderr, " valfmt: %s",
			    nvsp->valfmt == PM_VAL_INSITU ? "insitu" : "ptr");
#endif
		vsize += sizeof(nvsp->valfmt) + nvsp->numval * sizeof(__pmValue_PDU);
		nvsize += (nvsp->numval - 1) * sizeof(pmValue);
		for (j = 0; j < nvsp->numval; j++) {
		    __pmValue_PDU	*vp = &vlp->vlist[j];
		    pmValue		*nvp = &nvsp->vlist[j];
		    nvp->inst = ntohl(vp->inst);
		    if (nvsp->valfmt == PM_VAL_INSITU) {
			nvp->value.lval = ntohl(vp->value.lval);
#ifdef DESPERATE
			fprintf(stderr, " value: %d", nvp->value.lval);
#endif
		    }
		    else {
			/*
			 * in the input PDU buffer, pval is an index to the
			 * start of the pmValueBlock, in units of __pmPDU
			 */
			nvp->value.pval = (pmValueBlock *)&newbuf[sizeof(__pmPDU) * ntohl(vp->value.pval) + offset];
#ifdef DESPERATE
			{
			    int		k, len;
			    len = nvp->value.pval->vlen - PM_VAL_HDR_SIZE;
			    fprintf(stderr, " len: %d type: %dvalue: 0x", len,
				nvp->value.pval->vtype;
			    for (k = 0; k < len; k++)
				fprintf(stderr, "%02x", nvp->value.pval->vbuf[k]);
			}
#endif
		    }
		}
	    }
	    else if (nvsp->numval < 0 && ipc && ipc->version == PDU_VERSION1) {
		nvsp->numval = XLATE_ERR_1TO2(nvsp->numval);
	    }
#ifdef DESPERATE
	    fputc('\n', stderr);
#endif
	}

	__pmUnpinPDUBuf(pdubuf);	/* release it now that data copied */
	if (numpmid)
	    __pmPinPDUBuf(newbuf);	/* pin the new buffer containing data */

#elif defined(HAVE_32BIT_PTR)

	pr->timestamp.tv_sec = ntohl(pp->timestamp.tv_sec);
	pr->timestamp.tv_usec = ntohl(pp->timestamp.tv_usec);
	vlp = (vlist_t *)pp->data;
	/*
	 * Now fix up any pointers in pmValueBlocks (currently offsets into
	 * the PDU buffer) by adding the base address of the PDU buffer.
	 */
	for (i = 0; i < numpmid; i++) {
	    vsp = pr->vset[i] = (pmValueSet *)vlp;
#ifndef HAVE_NETWORK_BYTEORDER
	    vsp->pmid = __ntohpmID(vsp->pmid);
	    vsp->numval = ntohl(vsp->numval);
	    if (vsp->numval > 0) {
		vsp->valfmt = ntohl(vsp->valfmt);
		for (j = 0; j < vsp->numval; j++) {
		    vsp->vlist[j].inst = ntohl(vsp->vlist[j].inst);
		    if (vsp->valfmt == PM_VAL_INSITU)
			vsp->vlist[j].value.lval = ntohl(vsp->vlist[j].value.lval);
		}
	    }
#endif
	    if (vsp->numval > 0) {
		if (vsp->valfmt != PM_VAL_INSITU) {
		    /* salvage pmValueBlocks from end of PDU */
		    for (j = 0; j < vsp->numval; j++) {
			vsp->vlist[j].value.pval = (pmValueBlock *)&pdubuf[ntohl(vsp->vlist[j].value.lval)];
			__ntohpmValueBlock(vsp->vlist[j].value.pval);
		    }
		}
		vlp = (vlist_t *)((__psint_t)vlp + sizeof(*vlp) + (vsp->numval-1)*sizeof(vlp->vlist[0]));
	    }
	    else {
		if (vsp->numval < 0 && ipc && ipc->version == PDU_VERSION1)
		    vsp->numval = XLATE_ERR_1TO2(vsp->numval);
		vlp = (vlist_t *)((__psint_t)vlp + sizeof(vlp->pmid) + sizeof(vlp->numval));
	    }
	}
	if (numpmid)
	    __pmPinPDUBuf(pdubuf);
#else

	Ark! Unexpected sizeof pointer!!

#endif
    }
    else {
	/* assume PDU_ASCII */
	pmValueSet		*avsp;		/* pmValueSet constructor */
	if ((sts = __pmRecvLine(pdubuf, ABUFSIZE, __pmAbuf)) <= 0) goto badsts;
	if (sscanf(__pmAbuf, "RESULT %d", &numpmid) != 1) goto bad;
	if ((pr = (pmResult *)malloc(sizeof(pmResult) + numpmid * sizeof(pmValueSet *))) == NULL) {
	    sts = -errno;
	    goto badsts;
	}
	pr->numpmid = numpmid;
	for (i = 0; i < numpmid; i++) {
	    if ((sts = __pmRecvLine(pdubuf, ABUFSIZE, __pmAbuf)) <= 0) goto badsts;
	    if (sscanf(__pmAbuf, ". %d %d", &pmid, &j) != 2) goto bad;

	    if (j < 0 && ipc && ipc->version == PDU_VERSION1)
		numval = XLATE_ERR_1TO2(j);
	    else
		numval = j;

	    if (j < 0)
		j = 0;
	    if (j == 1)
		avsp = (pmValueSet *)__pmPoolAlloc(sizeof(pmValueSet));
	    else
		avsp = (pmValueSet *)malloc(sizeof(pmValueSet) + (j-1) * sizeof(pmValue));
	    if (avsp == NULL) {
		sts = -errno;
		goto badsts;
	    }
	    pr->vset[i] = avsp;
	    avsp->pmid = pmid;
	    avsp->numval = numval;
	    if (numval > 0) {
		q = &__pmAbuf[2];
		while (*q && !isspace((int)*q)) q++;		/* pmid */
		if (*q == '\0') goto bad;
		while (*q && isspace((int)*q)) q++;
		if (*q == '\0') goto bad;
		while (*q && !isspace((int)*q)) q++;		/* numval */
		if (*q == '\0') goto bad;
		while (*q && isspace((int)*q)) q++;
		if (*q == '\0') goto bad;
		if ((sts = sscanf(q, "%d", &avsp->valfmt)) != 1)
		    goto bad;
		while (*q && !isspace((int)*q)) q++;		/* valfmt */
		for (j = 0; j < numval && *q; j++) {
		    while (*q && isspace((int)*q)) q++;
		    if (*q == '\0') goto bad;
		    if (sscanf(q, "%d %d", &avsp->vlist[j].inst, &avsp->vlist[j].value.lval) != 2) goto
		        bad;
		    while (*q && !isspace((int)*q)) q++;		/* inst */
		    if (*q == '\0') goto bad;
		    while (*q && isspace((int)*q)) q++;
		    if (*q == '\0') goto bad;
		    while (*q && !isspace((int)*q)) q++;		/* value */
		}
		if (j < numval) goto bad;
	    }
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU)
	__pmDumpResult(stderr, pr);
#endif
    *result = pr;
    return 0;

bad:
    sts = PM_ERR_IPC;
    __pmNotifyErr(LOG_WARNING, "__pmDecodeResult: ASCII botch @ \"%s\"\n", __pmAbuf);
    /* FALLTHRU */

badsts:
    if (pr != NULL) {
	/* clean up partial malloc's */
	pr->numpmid = i;
	pmFreeResult(pr);
    }
    /* FALLTHRU */

    return sts;
}
