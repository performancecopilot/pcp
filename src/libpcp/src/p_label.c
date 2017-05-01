/*
 * Copyright (c) 2016-2017 Red Hat.
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

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "internal.h"

void
__pmDumpLabelSet(FILE *fp, const __pmLabelSet *set)
{
    char	buffer[MAXLABELJSONLEN];
    const char	type[] = "__pmLabelSet";
    int		i;

    for (i = 0; i < set->jsonlen; i++)
	buffer[i] = isprint((int)set->json[i]) ? set->json[i] : '.';
    buffer[set->jsonlen] = buffer[MAXLABELJSONLEN-1] = '\0';

    fprintf(fp, "%s dump from "PRINTF_P_PFX"%p inst=%d nlabels=%d\n",
	    type, set, set->inst, set->nlabels);
    if (set->jsonlen)
	fprintf(fp, "%s "PRINTF_P_PFX"%p json:\n    %s\n", type, set, buffer);
    if (set->nlabels)
	fprintf(fp, "%s "PRINTF_P_PFX"%p index:\n", type, set);
    for (i = 0; i < set->nlabels; i++)
	fprintf(fp, "    [%d] name(%d,%d) : value(%d,%d)\n", i,
	        set->labels[i].name, set->labels[i].namelen,
	        set->labels[i].value, set->labels[i].valuelen);
}

void
__pmDumpLabelSetArray(FILE *fp, const __pmLabelSet *sets, int nsets)
{
    int		n;

    for (n = 0; n < nsets; n++) {
	fprintf(stderr, "[%d] ", n);
	__pmDumpLabelSet(stderr, &sets[n]);
    }
}

#ifdef PCP_DEBUG
static const char *
LabelTypeString(int type)
{
    switch (type) {
    case PM_LABEL_CONTEXT:  return "CONTEXT";
    case PM_LABEL_DOMAIN:   return "DOMAIN";
    case PM_LABEL_INDOM:    return "INDOM";
    case PM_LABEL_PMID:	    return "PMID";
    case PM_LABEL_INSTS:    return "INSTS";
    default:
	break;
    }
    return "?";
}

static void
DumpLabelSets(char *func, int ident, int type, __pmLabelSet *sets, int nsets)
{
    fprintf(stderr, "%s(ident=%d,type=0x%x[%s], %d sets @"PRINTF_P_PFX"%p)\n",
	    func, ident, type, LabelTypeString(type), nsets, sets);
    __pmDumpLabelSetArray(stderr, sets, nsets);
}
#endif

/*
 * PDU contents for label metadata request (PDU_LABEL_REQ)
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		ident;		/* domain, PMID or pmInDom identifier */
    int		type;		/* context/domain/metric/indom/insts */
} label_req_t;

int
__pmSendLabelReq(int fd, int from, int ident, int type)
{
    label_req_t	*pp;
    int		sts;
    int		nid;

    if (type & PM_LABEL_CONTEXT)
	nid = htonl(PM_ID_NULL);
    else if (type & PM_LABEL_DOMAIN)
	nid = htonl(ident);
    else if (type & PM_LABEL_INDOM)
	nid = __htonpmInDom((pmInDom)ident);
    else if (type & (PM_LABEL_PMID | PM_LABEL_INSTS))
	nid = __htonpmID((pmID)ident);
    else
	return -EINVAL;

    if ((pp = (label_req_t *)__pmFindPDUBuf(sizeof(label_req_t))) == NULL)
	return -oserror();
    pp->hdr.len = sizeof(label_req_t);
    pp->hdr.type = PDU_LABEL_REQ;
    pp->hdr.from = from;
    pp->ident = nid;
    pp->type = htonl(type);

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmDecodeLabelReq(__pmPDU *pdubuf, int *ident, int *otype)
{
    label_req_t	*pp;
    char	*pduend;
    int		type;

    pp = (label_req_t *)pdubuf;
    pduend = (char *)pdubuf + pp->hdr.len;

    if (pduend - (char*)pp < sizeof(label_req_t))
	return PM_ERR_IPC;

    type = *otype = ntohl(pp->type);
    if (type & PM_LABEL_DOMAIN)
        *ident = ntohl(pp->ident);
    else if (type & (PM_LABEL_PMID|PM_LABEL_INSTS))
	*ident = __ntohpmID(pp->ident);
    else if (type & PM_LABEL_INDOM)
        *ident = __ntohpmInDom(pp->ident);
    else
        *ident = PM_ID_NULL;

    return 0;
}

/*
 * PDU contents for label metadata response (PDU_LABEL)
 */
typedef struct {
    int		inst;		/* instance identifier or PM_IN_NULL */
    int		nlabels;	/* number of labels or an error code */
    int		json;		/* offset to start of the JSON string */
    int		jsonlen;	/* length in bytes of the JSON string */
} labelset_t;

typedef struct {
    __pmPDUHdr	hdr;
    int		ident;		/* domain, PMID or pmInDom identifier */
    int		type;		/* context/domain/metric/indom/insts */
    int		padding;
    int		nsets;
    labelset_t	sets[1];
} label_t;

int
__pmSendLabel(int fd, int from, int ident, int type, __pmLabelSet *sets, int nsets)
{
    size_t	json_offset;
    size_t	need;
    label_t	*pp;
    int		sts;
    int		i;

    if (nsets < 0)
	return -EINVAL;
    need = sizeof(label_t) + (sizeof(labelset_t) * (nsets - 1));
    for (i = 0; i < nsets; i++) {
	if (sets[i].jsonlen < 0)
	    return -EINVAL;
	need += sets[i].jsonlen;
    }

    if ((pp = (label_t *)__pmFindPDUBuf((int)need)) == NULL)
	return -oserror();

    pp->hdr.len = (int)need;
    pp->hdr.type = PDU_LABEL;
    pp->hdr.from = from;

    if (type & PM_LABEL_DOMAIN)
	pp->ident = htonl(ident);
    else if (type & (PM_LABEL_PMID | PM_LABEL_INSTS))
	pp->ident = __htonpmID((pmID)ident);
    else if (type & PM_LABEL_INDOM)
	pp->ident = __htonpmInDom((pmInDom)ident);
    else
	pp->ident = htonl(PM_ID_NULL);

    pp->type = htonl(type);
    pp->padding = 0;
    pp->nsets = htonl(nsets);

    json_offset = sizeof(label_t) + (sizeof(labelset_t) * (nsets - 1));
    for (i = 0; i < nsets; i++) {
	pp->sets[i].inst = htonl(sets[i].inst);
	pp->sets[i].nlabels = htonl(sets[i].nlabels);
	pp->sets[i].json = htonl(json_offset);
	pp->sets[i].jsonlen = htonl(sets[i].jsonlen);

	if (sets[i].jsonlen) {
	    memcpy((char *)pp + json_offset, sets[i].json, sets[i].jsonlen);
	    json_offset += sets[i].jsonlen;
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LABEL)
	DumpLabelSets("__pmSendLabel", ident, type, sets, nsets);
#endif

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmRecvLabel(int fd, __pmContext *ctxp, int timeout,
	 	int *ident, int *type, __pmLabelSet **sets, int *nsets)
{
    __pmPDU	*pb;
    int		oident = *ident;
    int		otype = *type;
    int		pinpdu;
    int		sts;

    sts = pinpdu = __pmGetPDU(fd, ANY_SIZE, timeout, &pb);
    if (sts == PDU_LABEL) {
	sts = __pmDecodeLabel(pb, ident, type, sets, nsets);
	if (sts >= 0) {
	    /* verify response is for a matching request */
	    if (oident != *ident || otype != *type)
		sts = PM_ERR_IPC;
	}
    }
    else if (sts == PDU_ERROR)
	__pmDecodeError(pb, &sts);
    else {
	__pmCloseChannelbyContext(ctxp, PDU_LABEL, sts);
	if (sts != PM_ERR_TIMEOUT)
	    sts = PM_ERR_IPC;
    }
    if (pinpdu > 0)
	__pmUnpinPDUBuf(pb);
    return sts;
}

int
__pmDecodeLabel(__pmPDU *pdubuf, int *ident, int *type, __pmLabelSet **setsp, int *nsetp)
{
    __pmLabelSet *sets, *sp;
    label_t	*label_pdu;
    char	*pdu_end;
    char	*json;
    int		jsonlen;
    int		jsonoff;
    int		nlabels;
    int		nsets;
    int		sts;
    int		i;

    label_pdu = (label_t *)pdubuf;
    pdu_end = (char *)pdubuf + label_pdu->hdr.len;

    if (pdu_end - (char *)label_pdu < sizeof(label_t) - sizeof(labelset_t))
	return PM_ERR_IPC;

    *ident = ntohl(label_pdu->ident);
    *type = ntohl(label_pdu->type);
    nsets = ntohl(label_pdu->nsets);
    if (nsets < 0 || nsets > 0x7fffffff)  /* maximum #instances per indom */
	return PM_ERR_IPC;

    if (!nsets) {
	sets = NULL;
	goto success;
    }

    /* allocate space for label set and name/value indices */
    if ((sets = (__pmLabelSet *)calloc(nsets, sizeof(__pmLabelSet))) == NULL)
	return -ENOMEM;

    for (i = 0; i < nsets; i++) {
	nlabels = ntohl(label_pdu->sets[i].nlabels);
	jsonlen = ntohl(label_pdu->sets[i].jsonlen);
	jsonoff = ntohl(label_pdu->sets[i].json);
	json = (char *)label_pdu + jsonoff;

	/* validity checks - none of these conditions should happen */
	if (nlabels >= MAXLABELS)
	    goto corrupt;
	if (jsonlen < 0 || jsonlen >= MAXLABELJSONLEN)
	    goto corrupt;

	/* check JSON content fits within the PDU bounds */
	if (pdu_end - (char *)label_pdu < jsonoff + jsonlen)
	    goto corrupt;

	if ((sts = __pmParseLabelSet(json, jsonlen, &sp)) < 0)
	    goto corrupt;
	if (sts > 0 && sp->nlabels != nlabels)
	    goto corrupt;
	sp->nlabels = nlabels;
	sp->inst = ntohl(label_pdu->sets[i].inst);
	sets[i] = *sp;
	free(sp);
    }

success:
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LABEL)
	DumpLabelSets("__pmDecodeLabel", *ident, *type, sets, nsets);
#endif

    *nsetp = nsets;
    *setsp = sets;
    return 0;

corrupt:
    __pmFreeLabelSetArray(sets, nsets);
    return PM_ERR_IPC;
}
