/*
 * Copyright (c) 2016-2020,2022 Red Hat.
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
#include "libpcp.h"
#include "internal.h"
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

const char *
__pmLabelTypeString(int type)
{
    switch (type & ~(PM_LABEL_COMPOUND|PM_LABEL_OPTIONAL)) {
    case PM_LABEL_CONTEXT:  return "context";
    case PM_LABEL_DOMAIN:   return "domain";
    case PM_LABEL_INDOM:    return "indom";
    case PM_LABEL_CLUSTER:  return "cluster";
    case PM_LABEL_ITEM:	    return "pmid";
    case PM_LABEL_INSTANCES:return "instances";
    default:
	break;
    }
    return "?";
}

char *
__pmLabelIdentString(int ident, int type, char *buf, size_t buflen)
{
    char	*p, id[32];

    switch (type & ~(PM_LABEL_COMPOUND|PM_LABEL_OPTIONAL)) {
    case PM_LABEL_DOMAIN:
	pmsprintf(buf, buflen, "Domain %u", ident);
	break;
    case PM_LABEL_INDOM:
    case PM_LABEL_INSTANCES:
	pmsprintf(buf, buflen, "InDom %s", pmInDomStr_r(ident, id, sizeof(id)));
	break;
    case PM_LABEL_CLUSTER:
	pmIDStr_r(ident, id, sizeof(id));
	p = rindex(id, '.');
	*p = '\0';
	pmsprintf(buf, buflen, "Cluster %s", id);
	break;
    case PM_LABEL_ITEM:
	pmsprintf(buf, buflen, "PMID %s", pmIDStr_r(ident, id, sizeof(id)));
	break;
    case PM_LABEL_CONTEXT:
	pmsprintf(buf, buflen, "Context");
	break;
    default:
	buf[0] = '\0';
	break;
    }
    return buf;
}

char *
__pmLabelFlagString(int flags, char *buf, int buflen)
{
    /*
     * buffer needs to be long enough to hold label source
     * and any optional flag strings, separated by commas.
     */
    if (buflen <= 16)
	return NULL;
    strcpy(buf, __pmLabelTypeString(flags));
    if (flags & PM_LABEL_COMPOUND)
	strcat(buf, ",compound");
    if (flags & PM_LABEL_OPTIONAL)
	strcat(buf, ",optional");
    return buf;
}

void
__pmDumpLabelSet(FILE *fp, const pmLabelSet *set)
{
    char	buffer[PM_MAXLABELJSONLEN];
    const char	type[] = "pmLabelSet";
    char	*fls;
    int		i;

    for (i = 0; i < set->jsonlen; i++)
	buffer[i] = isprint((int)set->json[i]) ? set->json[i] : '.';
    buffer[set->jsonlen] = buffer[PM_MAXLABELJSONLEN-1] = '\0';

    fprintf(fp, "%s dump from "PRINTF_P_PFX"%p inst=%d nlabels=%d\n",
	    type, set, set->inst, set->nlabels);
    if (set->jsonlen)
	fprintf(fp, "%s "PRINTF_P_PFX"%p json:\n    %s\n", type, set, buffer);
    if (set->nlabels)
	fprintf(fp, "%s "PRINTF_P_PFX"%p index:\n", type, set);
    for (i = 0; i < set->nlabels; i++) {
	fls = __pmLabelFlagString(set->labels[i].flags, buffer, sizeof(buffer));
	fprintf(fp, "    [%d] name(%d,%d) : value(%d,%d) [%s]\n", i,
	        set->labels[i].name, set->labels[i].namelen,
	        set->labels[i].value, set->labels[i].valuelen, fls);
    }
}

void
__pmDumpLabelSets(FILE *fp, const pmLabelSet *sets, int nsets)
{
    int		n;

    for (n = 0; n < nsets; n++) {
	fprintf(fp, "[%d] ", n);
	__pmDumpLabelSet(fp, &sets[n]);
    }
}

static void
DumpLabelSets(char *func, int ident, int type, pmLabelSet *sets, int nsets)
{
    fprintf(stderr, "%s(ident=%d,type=0x%x[%s], %d sets @"PRINTF_P_PFX"%p)\n",
	    func, ident, type, __pmLabelTypeString(type), nsets, sets);
    __pmDumpLabelSets(stderr, sets, nsets);
}

/*
 * PDU contents for label metadata request (PDU_LABEL_REQ)
 */
typedef struct {
    __pmPDUHdr	hdr;
    int		ident;		/* domain, PMID or pmInDom identifier */
    int		type;		/* context/domain/indom/cluster/item/insts */
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
    else if (type & (PM_LABEL_INDOM|PM_LABEL_INSTANCES))
	nid = __htonpmInDom((pmInDom)ident);
    else if (type & (PM_LABEL_CLUSTER|PM_LABEL_ITEM))
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
    int		type;

    pp = (label_req_t *)pdubuf;

    if (pp->hdr.len < sizeof(label_req_t)) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeLabelReq: PM_ERR_IPC: short PDU %d < min size %d\n",
		pp->hdr.len, (int)sizeof(label_req_t));
	}
	return PM_ERR_IPC;
    }
    else if (pp->hdr.len > sizeof(label_req_t)) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeLabelReq: PM_ERR_IPC: PDU too long %d > required size %d\n",
			pp->hdr.len, (int)sizeof(label_req_t));
	}
	return PM_ERR_IPC;
    }

    type = *otype = ntohl(pp->type);
    if (type & PM_LABEL_DOMAIN)
        *ident = ntohl(pp->ident);
    else if (type & (PM_LABEL_CLUSTER|PM_LABEL_ITEM))
	*ident = __ntohpmID(pp->ident);
    else if (type & (PM_LABEL_INDOM|PM_LABEL_INSTANCES))
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
    pmLabel	labels[0];	/* zero or more label indices + flags */
} labelset_t;

typedef struct {
    __pmPDUHdr	hdr;
    int		ident;		/* domain, PMID or pmInDom identifier */
    int		type;		/* context/domain/indom/cluster/item/insts */
    int		padding;
    int		nsets;
    labelset_t	sets[1];
} labels_t;

int
__pmSendLabel(int fd, int from, int ident, int type, pmLabelSet *sets, int nsets)
{
    size_t	labels_offset;
    size_t	labels_need;
    size_t	json_offset;
    size_t	json_need;
    labelset_t	*lsp;
    labels_t	*pp;
    pmLabel	*lp;
    int		sts;
    int		i, j;

    if (nsets < 0)
	return -EINVAL;
    labels_need = sizeof(labels_t) + (sizeof(labelset_t) * (nsets - 1));
    json_need = 0;
    for (i = 0; i < nsets; i++) {
	json_need += sets[i].jsonlen;
	if (sets[i].nlabels > 0)
	    labels_need += sets[i].nlabels * sizeof(pmLabel);
    }

    if ((pp = (labels_t *)__pmFindPDUBuf((int)labels_need + json_need)) == NULL)
	return -oserror();

    pp->hdr.len = (int)(labels_need + json_need);
    pp->hdr.type = PDU_LABEL;
    pp->hdr.from = from;

    if (type & PM_LABEL_DOMAIN)
	pp->ident = htonl(ident);
    else if (type & (PM_LABEL_CLUSTER | PM_LABEL_ITEM))
	pp->ident = __htonpmID((pmID)ident);
    else if (type & (PM_LABEL_INDOM | PM_LABEL_INSTANCES))
	pp->ident = __htonpmInDom((pmInDom)ident);
    else
	pp->ident = htonl(PM_ID_NULL);

    pp->type = htonl(type);
    pp->padding = 0;
    pp->nsets = htonl(nsets);

    labels_offset = (char *)&pp->sets[0] - (char *)pp;
    json_offset = labels_need;	/* JSONB immediately follows labelsets */

    for (i = 0; i < nsets; i++) {
	lsp = (labelset_t *)((char *)pp + labels_offset);
	lsp->inst = htonl(sets[i].inst);
	lsp->nlabels = htonl(sets[i].nlabels);
	lsp->json = htonl(json_offset);
	lsp->jsonlen = htonl(sets[i].jsonlen);

	if (sets[i].nlabels > 0) {
	    for (j = 0; j < sets[i].nlabels; j++) {
		lp = &lsp->labels[j];
		lp->name = htons(sets[i].labels[j].name);
		lp->namelen = sets[i].labels[j].namelen;	/* byte copy */
		lp->flags = sets[i].labels[j].flags;		/* byte copy */
		lp->value = htons(sets[i].labels[j].value);
		lp->valuelen = htons(sets[i].labels[j].valuelen);
	    }
	    labels_offset += sets[i].nlabels * sizeof(pmLabel);
	}
	labels_offset += sizeof(labelset_t);

	if (sets[i].jsonlen) {
	    memcpy((char *)pp + json_offset, sets[i].json, sets[i].jsonlen);
	    json_offset += sets[i].jsonlen;
	}
    }

    if (pmDebugOptions.labels)
	DumpLabelSets("__pmSendLabel", ident, type, sets, nsets);

    sts = __pmXmitPDU(fd, (__pmPDU *)pp);
    __pmUnpinPDUBuf(pp);
    return sts;
}

int
__pmRecvLabel(int fd, __pmContext *ctxp, int timeout,
	 	int *ident, int *type, pmLabelSet **sets, int *nsets)
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
	    if (oident != *ident || otype != *type) {
		if (pmDebugOptions.pdu) {
		    fprintf(stderr, "__pmRecvLabel: PM_ERR_IPC: oident %d != *ident %d or  != *type %d\n",
			oident, *ident, *type);
		}
		sts = PM_ERR_IPC;
	    }
	}
    }
    else if (sts == PDU_ERROR)
	__pmDecodeError(pb, &sts);
    else if (sts != PM_ERR_TIMEOUT) {
	if (pmDebugOptions.pdu) {
	    char	strbuf[20];
	    char	errmsg[PM_MAXERRMSGLEN];
	    if (sts < 0)
		fprintf(stderr, "__pmRecvLabel: PM_ERR_IPC: expecting PDU_LABEL but__pmGetPDU returns %d (%s)\n",
		    sts, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    else
		fprintf(stderr, "__pmRecvLabel: PM_ERR_IPC: expecting PDU_LABEL but__pmGetPDU returns %d (type=%s)\n",
		    sts, __pmPDUTypeStr_r(sts, strbuf, sizeof(strbuf)));
	}
	sts = PM_ERR_IPC;
    }

    if (pinpdu > 0)
	__pmUnpinPDUBuf(pb);

    return sts;
}

int
__pmDecodeLabel(__pmPDU *pdubuf, int *ident, int *type, pmLabelSet **setsp, int *nsetp)
{
    pmLabelSet	*sets;
    pmLabelSet	*sp;
    pmLabel	*lp;
    labelset_t	*lsp;
    labels_t	*label_pdu;
    size_t	pdu_length;
    char	*pdu_end;
    char	*json;
    int		labeloff;
    int		labellen;
    int		jsonlen = 0;
    int		jsonoff = 0;
    int		nlabels;
    int		nsets;
    int		i, j;

    label_pdu = (labels_t *)pdubuf;
    pdu_end = (char *)pdubuf + label_pdu->hdr.len;
    pdu_length = pdu_end - (char *)label_pdu;

    if (pdu_length < sizeof(labels_t) - sizeof(labelset_t)) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeLabel: PM_ERR_IPC: short PDU %d < min size %d\n",
		(int)pdu_length, (int)(sizeof(labels_t) - sizeof(labelset_t)));
	}
	return PM_ERR_IPC;
    }

    *ident = ntohl(label_pdu->ident);
    *type = ntohl(label_pdu->type);
    nsets = ntohl(label_pdu->nsets);
    if (nsets < 0) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeLabel: PM_ERR_IPC: nsets %d < 0\n",
		nsets);
	}
	return PM_ERR_IPC;
    }
    else if (nsets > (label_pdu->hdr.len - sizeof(labelset_t)) / sizeof(labelset_t)) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeLabel: PM_ERR_IPC: nsets %d > max %d for PDU len %d\n",
		nsets, (int)((label_pdu->hdr.len - sizeof(labels_t)) / sizeof(labelset_t)), label_pdu->hdr.len);
	}
	return PM_ERR_IPC;
    }
    else if (nsets == 0) {
	sets = NULL;
	goto success;
    }

    /* allocate space for label set and name/value indices */
    if ((sets = (pmLabelSet *)calloc(nsets, sizeof(pmLabelSet))) == NULL)
	return -ENOMEM;

    labeloff = (char *)&label_pdu->sets[0] - (char *)label_pdu;

    for (i = 0; i < nsets; i++) {
	lsp = (labelset_t *)((char *)label_pdu + labeloff);
	if (pdu_end - (char *)lsp < sizeof(labelset_t)) {
	    if (pmDebugOptions.pdu) {
		fprintf(stderr, "__pmDecodeLabel: PM_ERR_IPC: labelset[%d] PDU too small remainder %d < required %d\n",
		    i, (int)(pdu_end - (char *)lsp), (int)sizeof(labelset_t));
	    }
	    goto corrupt;
	}

	nlabels = ntohl(lsp->nlabels);
	jsonlen = ntohl(lsp->jsonlen);
	jsonoff = ntohl(lsp->json);

	labellen = sizeof(labelset_t);
	if (nlabels > 0)
	    labellen += nlabels * sizeof(pmLabel);

	/* validity checks - these conditions should not happen */
	if (nlabels >= PM_MAXLABELS) {
	    if (pmDebugOptions.pdu) {
		fprintf(stderr, "__pmDecodeLabel: PM_ERR_IPC: labelset[%d] nlabels %d >= PM_MAXLABELS %d\n",
		    i, nlabels, PM_MAXLABELS);
	    }
	    goto corrupt;
	}
	if (jsonlen >= PM_MAXLABELJSONLEN) {
	    if (pmDebugOptions.pdu) {
		fprintf(stderr, "__pmDecodeLabel: PM_ERR_IPC: labelset[%d] jsonlen %d >= PM_MAXLABELJSONLEN %d\n",
		    i, jsonlen, PM_MAXLABELJSONLEN);
	    }
	    goto corrupt;
	}

	/* check JSON content fits within the PDU bounds */
	if (pdu_length < jsonoff + jsonlen) {
	    if (pmDebugOptions.pdu) {
		fprintf(stderr, "__pmDecodeLabel: PM_ERR_IPC: labelset[%d] pdu_length %d < jsonoff %d + jsonlen %d\n",
		    i, (int)pdu_length, jsonoff, jsonlen);
	    }
	    goto corrupt;
	}

	/* check label content fits within the PDU bounds */
	if (pdu_length < labeloff + labellen) {
	    if (pmDebugOptions.pdu) {
		fprintf(stderr, "__pmDecodeLabel: PM_ERR_IPC: labelset[%d] pdu_length %d < labeloff %d + labellen %d\n",
		    i, (int)pdu_length, labeloff, labellen);
	    }
	    goto corrupt;
	}

	sp = &sets[i];
	sp->inst = ntohl(lsp->inst);
	sp->nlabels = nlabels;
	if (nlabels > 0) {
	    if ((json = malloc(jsonlen + 1)) == NULL) {
		pmNoMem("__pmDecodeLabel json", jsonlen + 1, PM_RECOV_ERR);
		goto corrupt;
	    }
	    if ((lp = (pmLabel *)calloc(nlabels, sizeof(pmLabel))) == NULL) {
		free(json);
		pmNoMem("__pmDecodeLabel lp", nlabels * sizeof(pmLabel), PM_RECOV_ERR);
		goto corrupt;
	    }
	    memcpy(json, (char *)label_pdu + jsonoff, jsonlen);
	    json[jsonlen] = '\0';

	    sp->json = json;
	    sp->jsonlen = jsonlen;
	    sp->labels = lp;

	    for (j = 0; j < nlabels; j++) {
		lp = &sp->labels[j];
		lp->name = ntohs(lsp->labels[j].name);
		lp->namelen = lsp->labels[j].namelen;		/* byte copy */
		lp->flags = lsp->labels[j].flags;		/* byte copy */
		lp->value = ntohs(lsp->labels[j].value);
		lp->valuelen = ntohs(lsp->labels[j].valuelen);

		if (jsonlen < lp->name + lp->namelen) {
		    if (pmDebugOptions.pdu) {
			fprintf(stderr, "__pmDecodeLabel: PM_ERR_IPC: labelset[%d] label[%d] PDU too short jsonlen %d < name %d + namelen %d\n",
			    i, j, jsonlen, lp->name, lp->namelen);
		    }
		    goto corrupt;
		}
		if (jsonlen < lp->value + lp->valuelen) {
		    if (pmDebugOptions.pdu) {
			fprintf(stderr, "__pmDecodeLabel: PM_ERR_IPC: labelset[%d] label[%d] PDU too short jsonlen %d < value %d + valuelen %d\n",
			    i, j, jsonlen, lp->value, lp->valuelen);
		    }
		    goto corrupt;
		}
	    }
	    labeloff += nlabels * sizeof(pmLabel);
	}
	labeloff += sizeof(labelset_t);
    }

    /* and no extra junk at the end of the PDU after the last labelset */
    if (pdu_length > PM_PDU_SIZE_BYTES(jsonoff + jsonlen)) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmDecodeLabel: PM_ERR_IPC: PDU too long %d extra\n",
		(int)(pdu_length - PM_PDU_SIZE_BYTES(jsonoff + jsonlen)));
	}
	goto corrupt;
    }

success:
    if (pmDebugOptions.labels)
	DumpLabelSets("__pmDecodeLabel", *ident, *type, sets, nsets);

    *nsetp = nsets;
    *setsp = sets;
    return 0;

corrupt:
    pmFreeLabelSets(sets, nsets);
    return PM_ERR_IPC;
}
