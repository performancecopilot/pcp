/*
 * Copyright (c) 2012-2013 Red Hat.
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
 */

#include "pmcd.h"

/* Check returned error from a client.
 * If client returns ready/not_ready status change, check then update agent
 * status.
 * If the client goes from not_ready to ready, it sends an unsolicited error
 * PDU.  If this happens, the retry flag indicates that the expected response
 * is yet to arrive, and that the caller should try reading
 * and the expected response will follow it.  
 */
int
CheckError(AgentInfo *ap, int sts)
{
    int		retSts;

    if (sts == PM_ERR_PMDANOTREADY) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    __pmNotifyErr(LOG_INFO, "%s agent (%s) sent NOT READY\n",
			 ap->pmDomainLabel,
			 ap->status.notReady ? "not ready" : "ready");
#endif
	if (ap->status.notReady == 0) {
	    ap->status.notReady = 1;
	    retSts = PM_ERR_AGAIN;
	}
	else
	    retSts = PM_ERR_IPC;
    }
    else if (sts == PM_ERR_PMDAREADY) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    __pmNotifyErr(LOG_INFO, "%s agent (%s) sent unexpected READY\n",
			 ap->pmDomainLabel,
			 ap->status.notReady ? "not ready" : "ready");
#endif
	retSts = PM_ERR_IPC;
    }
    else
	retSts = sts;

    return retSts;
}

int
DoText(ClientInfo *cp, __pmPDU* pb)
{
    int		sts, s;
    int		ident;
    int		type;
    AgentInfo	*ap;
    char	*buffer = NULL;

    if ((sts = __pmDecodeTextReq(pb, &ident, &type)) < 0)
	return sts;

    if ((ap = FindDomainAgent(((__pmID_int *)&ident)->domain)) == NULL)
	return PM_ERR_PMID;
    else if (!ap->status.connected)
	return PM_ERR_NOAGENT;

    if (ap->ipcType == AGENT_DSO) {
	if (ap->ipc.dso.dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
	    ap->ipc.dso.dispatch.version.four.ext->e_context = cp - client;
	sts = ap->ipc.dso.dispatch.version.any.text(ident, type, &buffer,
					  ap->ipc.dso.dispatch.version.any.ext);
    }
    else {
	if (ap->status.notReady)
	    return PM_ERR_AGAIN;
	pmcd_trace(TR_XMIT_PDU, ap->inFd, PDU_TEXT_REQ, ident);
	sts = __pmSendTextReq(ap->inFd, cp - client, ident, type);
	if (sts >= 0) {
	    int		pinpdu;
	    pinpdu = sts = __pmGetPDU(ap->outFd, ANY_SIZE, _pmcd_timeout, &pb);
	    if (sts > 0)
		pmcd_trace(TR_RECV_PDU, ap->outFd, sts, (int)((__psint_t)pb & 0xffffffff));
	    if (sts == PDU_TEXT)
		sts = __pmDecodeText(pb, &ident, &buffer);
	    else if (sts == PDU_ERROR) {
		s = __pmDecodeError(pb, &sts);
		if (s < 0)
		    sts = s;
		else
		    sts = CheckError(ap, sts);
		pmcd_trace(TR_RECV_ERR, ap->outFd, PDU_TEXT, sts);
	    }
	    else {
		pmcd_trace(TR_WRONG_PDU, ap->outFd, PDU_TEXT, sts);
		sts = PM_ERR_IPC;	/* Wrong PDU type */
	    }
	    if (pinpdu > 0)
		__pmUnpinPDUBuf(pb);
	}
	else
	    pmcd_trace(TR_XMIT_ERR, ap->inFd, PDU_TEXT_REQ, sts);
    }

    if (ap->ipcType != AGENT_DSO &&
	(sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT || sts == -EPIPE))
	CleanupAgent(ap, AT_COMM, ap->inFd);	

    if (sts >= 0) {
	pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_TEXT, ident);
	sts = __pmSendText(cp->fd, FROM_ANON, ident, buffer);
	if (sts < 0 && ap->ipcType != AGENT_DSO) {
	    pmcd_trace(TR_XMIT_ERR, cp->fd, PDU_TEXT, sts);
	    CleanupClient(cp, sts);
	}
	if (ap->ipcType != AGENT_DSO) {
	    /* daemon PMDAs have a malloc'd buffer */
	    free(buffer);
	}
    }
    return sts;
}

int
DoProfile(ClientInfo *cp, __pmPDU *pb)
{
    __pmProfile	*newProf;
    int		ctxnum, sts, i;

    sts = __pmDecodeProfile(pb, &ctxnum, &newProf);
    if (sts >= 0) {
	/* Allocate more profile pointers if required */
	if (ctxnum >= cp->szProfile) {
	    __pmProfile	**newProfPtrs;
	    int		need, oldSize = cp->szProfile;

	    if (ctxnum - cp->szProfile < 4)
		cp->szProfile += 4;
	    else
		cp->szProfile = ctxnum + 1;
	    need = cp->szProfile * (int)sizeof(__pmProfile *);
	    if ((newProfPtrs = (__pmProfile **)malloc(need)) == NULL) {
		cp->szProfile = oldSize;
		__pmNoMem("DoProfile.newProfPtrs", need, PM_RECOV_ERR);
		__pmFreeProfile(newProf);
		return -oserror();
	    }

	    /* Copy any old pointers and zero the newly allocated ones */
	    if ((need = oldSize * (int)sizeof(__pmProfile *))) {
		memcpy(newProfPtrs, cp->profile, need);
		free(cp->profile);	/* But not the __pmProfile ptrs! */
	    }
	    need = (cp->szProfile - oldSize) * (int)sizeof(__pmProfile *);
	    memset(&newProfPtrs[oldSize], 0, need);
	    cp->profile = newProfPtrs;
	}
	else				/* cp->profile is big enough */
	    if (cp->profile[ctxnum] != NULL)
		__pmFreeProfile(cp->profile[ctxnum]);
	cp->profile[ctxnum] = newProf;

	/* "Invalidate" any references to the client context's profile in the
	 * agents to which the old profile was last sent
	 */
	for (i = 0; i < nAgents; i++) {
	    AgentInfo	*ap = &agent[i];

	    if (ap->profClient == cp && ap->profIndex == ctxnum)
		ap->profClient = NULL;
	}
    }
    return sts;
}

int
DoDesc(ClientInfo *cp, __pmPDU *pb)
{
    int		sts, s;
    pmID	pmid;
    AgentInfo	*ap;
    pmDesc	desc = {0};
    int		fdfail = -1;

    if ((sts = __pmDecodeDescReq(pb, &pmid)) < 0)
	return sts;

    if ((ap = FindDomainAgent(((__pmID_int *)&pmid)->domain)) == NULL)
	return PM_ERR_PMID;
    else if (!ap->status.connected)
	return PM_ERR_NOAGENT;

    if (ap->ipcType == AGENT_DSO) {
	if (ap->ipc.dso.dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
	    ap->ipc.dso.dispatch.version.four.ext->e_context = cp - client;
	sts = ap->ipc.dso.dispatch.version.any.desc(pmid, &desc,
					ap->ipc.dso.dispatch.version.any.ext);
    }
    else {
	if (ap->status.notReady)
	    return PM_ERR_AGAIN;
	pmcd_trace(TR_XMIT_PDU, ap->inFd, PDU_DESC_REQ, (int)pmid);
	sts = __pmSendDescReq(ap->inFd, cp - client, pmid);
	if (sts >= 0) {
	    int		pinpdu;
	    pinpdu = sts = __pmGetPDU(ap->outFd, ANY_SIZE, _pmcd_timeout, &pb);
	    if (sts > 0)
		pmcd_trace(TR_RECV_PDU, ap->outFd, sts, (int)((__psint_t)pb & 0xffffffff));
	    if (sts == PDU_DESC)
		sts = __pmDecodeDesc(pb, &desc);
	    else if (sts == PDU_ERROR) {
		s = __pmDecodeError(pb, &sts);
		if (s < 0)
		    sts = s;
		else
		    sts = CheckError(ap, sts);
		pmcd_trace(TR_RECV_ERR, ap->outFd, PDU_DESC, sts);
	    }
	    else {
		pmcd_trace(TR_WRONG_PDU, ap->outFd, PDU_DESC, sts);
		sts = PM_ERR_IPC;	/* Wrong PDU type */
		fdfail = ap->outFd;
	    }
	    if (pinpdu > 0)
		__pmUnpinPDUBuf(pb);
	}
	else {
	    pmcd_trace(TR_XMIT_ERR, ap->inFd, PDU_DESC_REQ, sts);
	    fdfail = ap->inFd;
	}
    }

    if (sts >= 0) {
	pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_DESC, (int)desc.pmid);
	sts = __pmSendDesc(cp->fd, FROM_ANON, &desc);
	if (sts < 0) {
	    pmcd_trace(TR_XMIT_ERR, cp->fd, PDU_DESC, sts);
	    CleanupClient(cp, sts);
	}
    }
    else
	if (ap->ipcType != AGENT_DSO &&
	    (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT || sts == -EPIPE) &&
	    fdfail != -1)
	    CleanupAgent(ap, AT_COMM, fdfail);

    return sts;
}

int
DoInstance(ClientInfo *cp, __pmPDU* pb)
{
    int			sts, s;
    __pmTimeval		when;
    pmInDom		indom;
    int			inst;
    char		*name;
    __pmInResult	*inresult = NULL;
    AgentInfo		*ap;
    int			fdfail = -1;

    sts = __pmDecodeInstanceReq(pb, &when, &indom, &inst, &name);
    if (sts < 0)
	return sts;
    if (when.tv_sec != 0 || when.tv_usec != 0) {
	/*
	 * we have no idea how to do anything but current, yet!
	 *
	 * TODO EXCEPTION PCP 2.0 ...
	 * this may be left over from the pmvcr days, and can be tossed?
	 * ... leaving it here is benign
	 */
	if (name != NULL) free(name);
	return PM_ERR_NYI;
    }
    if ((ap = FindDomainAgent(((__pmInDom_int *)&indom)->domain)) == NULL) {
	if (name != NULL) free(name);
	return PM_ERR_INDOM;
    }
    else if (!ap->status.connected) {
	if (name != NULL) free(name);
	return PM_ERR_NOAGENT;
    }

    if (ap->ipcType == AGENT_DSO) {
	if (ap->ipc.dso.dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
	    ap->ipc.dso.dispatch.version.four.ext->e_context = cp - client;
	sts = ap->ipc.dso.dispatch.version.any.instance(indom, inst, name,
					&inresult,
					ap->ipc.dso.dispatch.version.any.ext);
    }
    else {
	if (ap->status.notReady) {
	    if (name != NULL) free(name);
	    return PM_ERR_AGAIN;
	}
	pmcd_trace(TR_XMIT_PDU, ap->inFd, PDU_INSTANCE_REQ, (int)indom);
	sts = __pmSendInstanceReq(ap->inFd, cp - client, &when, indom, inst, name);
	if (sts >= 0) {
	    int		pinpdu;
	    pinpdu = sts = __pmGetPDU(ap->outFd, ANY_SIZE, _pmcd_timeout, &pb);
	    if (sts > 0)
		pmcd_trace(TR_RECV_PDU, ap->outFd, sts, (int)((__psint_t)pb & 0xffffffff));
	    if (sts == PDU_INSTANCE)
		sts = __pmDecodeInstance(pb, &inresult);
	    else if (sts == PDU_ERROR) {
		inresult = NULL;
		s = __pmDecodeError(pb, &sts);
		if (s < 0)
		    sts = s;
		else
		    sts = CheckError(ap, sts);
		pmcd_trace(TR_RECV_ERR, ap->outFd, PDU_INSTANCE, sts);
	    }
	    else {
		pmcd_trace(TR_WRONG_PDU, ap->outFd, PDU_INSTANCE, sts);
		sts = PM_ERR_IPC;	/* Wrong PDU type */
		fdfail = ap->outFd;
	    }
	    if (pinpdu > 0)
		__pmUnpinPDUBuf(pb);
	}
	else {
	    pmcd_trace(TR_XMIT_ERR, ap->inFd, PDU_INSTANCE_REQ, sts);
	    fdfail = ap->inFd;
	}
    }
    if (name != NULL) free(name);

    if (sts >= 0) {
	pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_INSTANCE, (int)(inresult->indom));
	sts = __pmSendInstance(cp->fd, FROM_ANON, inresult);
	if (sts < 0) {
	    pmcd_trace(TR_XMIT_ERR, cp->fd, PDU_INSTANCE, sts);
	    CleanupClient(cp, sts);
	}
	__pmFreeInResult(inresult);
    }
    else
	if (ap->ipcType != AGENT_DSO &&
	    (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT || sts == -EPIPE) &&
	    fdfail != -1)
	    CleanupAgent(ap, AT_COMM, fdfail);

    return sts;
}

/*
 * This handler is for remote versions of pmNameAll or pmNameID.
 * Note: only one pmid for the list should be sent.
 */
int
DoPMNSIDs(ClientInfo *cp, __pmPDU *pb)
{
    int		sts = 0;
    int		op_sts = 0;
    int 	numnames = 0;
    pmID	idlist[1];
    char	**namelist = NULL;
    AgentInfo	*ap = NULL;
    int		fdfail = -1;

    if ((sts = __pmDecodeIDList(pb, 1, idlist, &op_sts)) < 0)
	goto fail;

    if ((sts = pmNameAll(idlist[0], &namelist)) < 0) {
	/*
	 * failure may be a real failure, or could be a metric within a
	 * dynamic sutree of the PMNS
	 */
	if ((ap = FindDomainAgent(((__pmID_int *)&idlist[0])->domain)) == NULL) {
	    sts = PM_ERR_NOAGENT;
	    goto fail;
	}
	if (!ap->status.connected) {
	    sts = PM_ERR_NOAGENT;
	    goto fail;
	}
	if (ap->ipcType == AGENT_DSO) {
	    if (ap->ipc.dso.dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		ap->ipc.dso.dispatch.version.four.ext->e_context = cp - client;
	    if (ap->ipc.dso.dispatch.comm.pmda_interface >= PMDA_INTERFACE_4) {
		sts = ap->ipc.dso.dispatch.version.four.name(idlist[0], &namelist, 
				  ap->ipc.dso.dispatch.version.four.ext);
	    }
	    else {
		/* Not PMDA_INTERFACE_4 or later */
		sts = PM_ERR_PMID;
	    }
	}
	else {
	    /* daemon PMDA ... ship request on */
	    if (ap->status.notReady)
		return PM_ERR_AGAIN;
	    pmcd_trace(TR_XMIT_PDU, ap->inFd, PDU_PMNS_IDS, 1);
	    sts = __pmSendIDList(ap->inFd, cp - client, 1, &idlist[0], 0);
	    if (sts >= 0) {
		int		pinpdu;
		pinpdu = sts = __pmGetPDU(ap->outFd, ANY_SIZE, _pmcd_timeout, &pb);
		if (sts > 0)
		    pmcd_trace(TR_RECV_PDU, ap->outFd, sts, (int)((__psint_t)pb & 0xffffffff));
		if (sts == PDU_PMNS_NAMES) {
		    sts = __pmDecodeNameList(pb, &numnames, &namelist, NULL);
		}
		else if (sts == PDU_ERROR) {
		    __pmDecodeError(pb, &sts);
		    pmcd_trace(TR_RECV_ERR, ap->outFd, PDU_PMNS_NAMES, sts);
		}
		else {
		    pmcd_trace(TR_WRONG_PDU, ap->outFd, PDU_PMNS_NAMES, sts);
		    sts = PM_ERR_IPC;	/* Wrong PDU type */
		    fdfail = ap->outFd;
		}
		if (pinpdu > 0)
		    __pmUnpinPDUBuf(pb);
	    }
	    else {
		/* __pmSendIDList failed */
		sts = __pmMapErrno(sts);
		fdfail = ap->inFd;
	    }
	}
	if (sts < 0) goto fail;
    }

    numnames = sts;

    pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_PMNS_NAMES, numnames);
    if ((sts = __pmSendNameList(cp->fd, FROM_ANON, numnames, namelist, NULL)) < 0){
	pmcd_trace(TR_XMIT_ERR, cp->fd, PDU_PMNS_NAMES, sts);
	CleanupClient(cp, sts);
    	goto fail;
    }
    /* fall through OK */

fail:
    if (ap != NULL && ap->ipcType != AGENT_DSO &&
	(sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT || sts == -EPIPE) &&
	fdfail != -1)
	CleanupAgent(ap, AT_COMM, fdfail);
    if (namelist) free(namelist);
    return sts;
}

/*
 * This handler is for the remote version of pmLookupName.
 */
int
DoPMNSNames(ClientInfo *cp, __pmPDU *pb)
{
    int		sts;
    int		numids = 0;
    int		numok;
    int		lsts;
    int		domain;
    pmID	*idlist = NULL;
    char	**namelist = NULL;
    int		i;
    AgentInfo	*ap = NULL;

    if ((sts = __pmDecodeNameList(pb, &numids, &namelist, NULL)) < 0)
	goto done;

    if ((idlist = (pmID *)calloc(numids, sizeof(int))) == NULL) {
        sts = -oserror();
	goto done;
    }

    sts = pmLookupName(numids, namelist, idlist);
    /*
     * even if this fails, or looks up fewer than numids, we have to
     * check each PMID looking for dynamic metrics and process them
     * separately with the help of the PMDA, if possible
     */
    for (i = 0; i < numids; i++) {
	if (idlist[i] == PM_ID_NULL ||
	    pmid_domain(idlist[i]) != DYNAMIC_PMID || pmid_item(idlist[i]) != 0)
	    continue;
	lsts = 0;
	domain = pmid_cluster(idlist[i]);
	/*
	 * don't return <domain>.*.* ... all return paths from here
	 * must either set a valid PMID in idlist[i] or indicate
	 * an error via lsts
	 */
	idlist[i] = PM_ID_NULL;	/* default case if cannot translate */
	if ((ap = FindDomainAgent(domain)) == NULL) {
	    lsts = PM_ERR_NOAGENT;
	}
	else if (!ap->status.connected) {
	    lsts = PM_ERR_NOAGENT;
	}
	else {
	    if (ap->ipcType == AGENT_DSO) {
		if (ap->ipc.dso.dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		    ap->ipc.dso.dispatch.version.four.ext->e_context = cp - client;
		if (ap->ipc.dso.dispatch.comm.pmda_interface >= PMDA_INTERFACE_4) {
		    lsts = ap->ipc.dso.dispatch.version.four.pmid(namelist[i], &idlist[i],
				      ap->ipc.dso.dispatch.version.four.ext);
		}
		else {
		    /* Not PMDA_INTERFACE_4 or later */
		    lsts = PM_ERR_NONLEAF;
		}
	    }
	    else {
		/* daemon PMDA ... ship request on */
		int		fdfail = -1;
		if (ap->status.notReady)
		    lsts = PM_ERR_AGAIN;
		else {
		    pmcd_trace(TR_XMIT_PDU, ap->inFd, PDU_PMNS_NAMES, 1);
		    lsts = __pmSendNameList(ap->inFd, cp - client, 1, &namelist[i], NULL);
		    if (lsts >= 0) {
			int		pinpdu;
			pinpdu = lsts = __pmGetPDU(ap->outFd, ANY_SIZE, _pmcd_timeout, &pb);
			if (lsts > 0)
			    pmcd_trace(TR_RECV_PDU, ap->outFd, sts, (int)((__psint_t)pb & 0xffffffff));
			if (lsts == PDU_PMNS_IDS) {
			    int		xsts;
			    lsts = __pmDecodeIDList(pb, 1, &idlist[i], &xsts);
			    if (lsts >= 0)
				lsts = xsts;
			}
			else if (lsts == PDU_ERROR) {
			    __pmDecodeError(pb, &lsts);
			    pmcd_trace(TR_RECV_ERR, ap->outFd, PDU_PMNS_IDS, lsts);
			}
			else {
			    pmcd_trace(TR_WRONG_PDU, ap->outFd, PDU_PMNS_IDS, sts);
			    lsts = PM_ERR_IPC;	/* Wrong PDU type */
			    fdfail = ap->outFd;
			}
			if (pinpdu > 0)
			    __pmUnpinPDUBuf(pb);
		    }
		    else {
			/* __pmSendNameList failed */
			lsts = __pmMapErrno(lsts);
			pmcd_trace(TR_XMIT_ERR, ap->inFd, PDU_PMNS_NAMES, lsts);
			fdfail = ap->inFd;
		    }
		}
		if (ap != NULL && ap->ipcType != AGENT_DSO &&
		    (lsts == PM_ERR_IPC || lsts == PM_ERR_TIMEOUT || lsts == -EPIPE) &&
		    fdfail != -1)
		    CleanupAgent(ap, AT_COMM, fdfail);
	    }
	}
	/*
	 * only set error status to the current error status
	 * if this is the first error for this set of metrics,
	 * and it either it is a fatal error, or numids is 1
	 */
	if (lsts < 0 && sts > 0) {
	    if ((lsts != PM_ERR_NAME && lsts != PM_ERR_NOAGENT &&
		 lsts != PM_ERR_NONLEAF) || numids == 1)
	    sts = lsts;
	}
    }

    if (sts < 0)
	/* fatal error or explicit error in the numids == 1 case */
	goto done;

    numok = numids;
    for (i = 0; i < numids; i++) {
	if (idlist[i] == PM_ID_NULL)
	    numok--;
    }

    pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_PMNS_IDS, numok);
    if ((sts = __pmSendIDList(cp->fd, FROM_ANON, numids, idlist, numok)) < 0) {
	pmcd_trace(TR_XMIT_ERR, cp->fd, PDU_PMNS_IDS, sts);
	CleanupClient(cp, sts);
    }

done:
    if (idlist) free(idlist);
    if (namelist) free(namelist);

    return sts;
}

/*
 * This handler is for the remote version of pmGetChildren.
 */
int
DoPMNSChild(ClientInfo *cp, __pmPDU *pb)
{
    int		sts = 0;
    int		numnames = 0;
    char 	*name = NULL;
    char	**offspring = NULL;
    int		*statuslist = NULL;
    int		subtype;
    char	*namelist[1];
    pmID	idlist[1];

    if ((sts = __pmDecodeChildReq(pb, &name, &subtype)) < 0)
	goto done;

    namelist[0] = name;
    sts = pmLookupName(1, namelist, idlist);
    if (sts == 1 && pmid_domain(idlist[0]) == DYNAMIC_PMID && pmid_item(idlist[0]) == 0) {
	int		domain = pmid_cluster(idlist[0]);
	AgentInfo	*ap = NULL;
	if ((ap = FindDomainAgent(domain)) == NULL) {
	    sts = PM_ERR_NOAGENT;
	    goto done;
	}
	if (!ap->status.connected) {
	    sts = PM_ERR_NOAGENT;
	    goto done;
	}
	if (ap->ipcType == AGENT_DSO) {
	    if (ap->ipc.dso.dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		ap->ipc.dso.dispatch.version.four.ext->e_context = cp - client;
	    if (ap->ipc.dso.dispatch.comm.pmda_interface >= PMDA_INTERFACE_4) {
		sts = ap->ipc.dso.dispatch.version.four.children(name, 0, &offspring, &statuslist,
				  ap->ipc.dso.dispatch.version.four.ext);
		if (sts < 0)
		    goto done;
		if (subtype == 0) {
		    if (statuslist) free(statuslist);
		    statuslist = NULL;
		}
	    }
	    else {
		/* Not PMDA_INTERFACE_4 or later */
		sts = PM_ERR_NAME;
		goto done;
	    }
	}
	else {
	    /* daemon PMDA ... ship request on */
	    int		fdfail = -1;
	    if (ap->status.notReady)
		sts = PM_ERR_AGAIN;
	    else {
		pmcd_trace(TR_XMIT_PDU, ap->inFd, PDU_PMNS_CHILD, 1);
		sts = __pmSendChildReq(ap->inFd, cp - client, name, subtype);
		if (sts >= 0) {
		    int		pinpdu;
		    pinpdu = sts = __pmGetPDU(ap->outFd, ANY_SIZE, _pmcd_timeout, &pb);
		    if (sts > 0)
			pmcd_trace(TR_RECV_PDU, ap->outFd, sts, (int)((__psint_t)pb & 0xffffffff));
		    if (sts == PDU_PMNS_NAMES) {
			sts = __pmDecodeNameList(pb, &numnames,
			                               &offspring, &statuslist);
			if (sts >= 0) {
			    sts = numnames;
			    if (subtype == 0) {
				free(statuslist);
				statuslist = NULL;
			    }
			}
		    }
		    else if (sts == PDU_ERROR) {
			__pmDecodeError(pb, &sts);
			pmcd_trace(TR_RECV_ERR, ap->outFd, PDU_PMNS_NAMES, sts);
		    }
		    else {
			pmcd_trace(TR_WRONG_PDU, ap->outFd, PDU_PMNS_NAMES, sts);
			sts = PM_ERR_IPC;	/* Wrong PDU type */
			fdfail = ap->outFd;
		    }
		    if (pinpdu > 0)
			__pmUnpinPDUBuf(pb);
		}
		else {
		    /* __pmSendChildReq failed */
		    sts = __pmMapErrno(sts);
		    fdfail = ap->inFd;
		}
	    }
	    if (ap != NULL && ap->ipcType != AGENT_DSO &&
		(sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT || sts == -EPIPE) &&
		fdfail != -1)
		CleanupAgent(ap, AT_COMM, fdfail);
	}
    }
    else {
	if (subtype == 0) {
	    if ((sts = pmGetChildren(name, &offspring)) < 0)
		goto done;
	}
	else {
	    if ((sts = pmGetChildrenStatus(name, &offspring, &statuslist)) < 0)
		goto done;
	}
    }

    numnames = sts;
    pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_PMNS_NAMES, numnames);
    if ((sts = __pmSendNameList(cp->fd, FROM_ANON, numnames, offspring, statuslist)) < 0) {
	pmcd_trace(TR_XMIT_ERR, cp->fd, PDU_PMNS_NAMES, sts);
	CleanupClient(cp, sts);
    }

done:
    if (name) free(name);
    if (offspring) free(offspring);
    if (statuslist) free(statuslist);
    return sts;
}

/*************************************************************************/

static char **travNL;     /* list of names for traversal */
static char *travNL_ptr;  /* pointer into travNL */
static int travNL_num;    /* number of names in list */
static int travNL_strlen; /* number of bytes of names */
static int travNL_i;      /* array index */

static void
AddLengths(const char *name)
{
  travNL_strlen += strlen(name) + 1;
  travNL_num++;
}

static void
BuildNameList(const char *name)
{
  travNL[travNL_i++] = travNL_ptr;
  strcpy(travNL_ptr, name);
  travNL_ptr += strlen(name) + 1;
}

/*
 * handle dynamic PMNS entries in remote version of pmTraversePMNS.
 *
 * num_names and names[] is the result of pmTraversePMNS for the
 * loaded PMNS ... need to preserve the semantics of this in the
 * end result, so names[] and all of the name[i] strings are in a
 * single malloc block
 */
static void
traverse_dynamic(ClientInfo *cp, char *start, int *num_names, char ***names)
{
    int		sts;
    int		i;
    char	**offspring;
    int		*statuslist;
    char	*namelist[1];
    pmID	idlist[1];
    int		fake = 0;

    /*
     * if we get any errors in the setup (unexpected), simply skip
     * that name[i] entry and move on ... any client using the associated
     * name[i] will get an error later, e.g. when trying to fetch the
     * pmDesc
     *
     * process in reverse order so stitching does not disturb the ones
     * we've not processed yet
     */
    if (*num_names == 0) {
	/*
	 * special case, where starting point is _below_ the dynamic
	 * node in the PMNS known to pmcd  (or name is simply invalid) ...
	 * fake a single name in the list so far ... names[] does not hold
	 * the string value as well, but this is OK because names[0] will
	 * be rebuilt * replacing "name" (or cleaned up at the end) ...
	 * note travNL_strlen initialization so resize below is correct
	 */
	fake = 1;
	*names = (char **)malloc(sizeof((*names)[0]));
	if (*names == NULL)
	    return;
	(*names)[0] = start;
	*num_names = 1;
	travNL_strlen = strlen(start) + 1;
    }
    for (i = *num_names-1; i >= 0; i--) {
	offspring = NULL;
	namelist[0] = (*names)[i];
	sts = pmLookupName(1, namelist, idlist);
	if (sts < 1)
	    continue;
	if (pmid_domain(idlist[0]) == DYNAMIC_PMID && pmid_item(idlist[0]) == 0) {
	    int		domain = pmid_cluster(idlist[0]);
	    AgentInfo	*ap;
	    if ((ap = FindDomainAgent(domain)) == NULL)
		continue;
	    if (!ap->status.connected)
		continue;
	    if (ap->ipcType == AGENT_DSO) {
		if (ap->ipc.dso.dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		    ap->ipc.dso.dispatch.version.four.ext->e_context = cp - client;
		if (ap->ipc.dso.dispatch.comm.pmda_interface >= PMDA_INTERFACE_4) {
		    sts = ap->ipc.dso.dispatch.version.four.children(namelist[0], 1, &offspring, &statuslist,
				      ap->ipc.dso.dispatch.version.four.ext);
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_PMNS) {
			fprintf(stderr, "traverse_dynamic: DSO PMDA: expand dynamic PMNS entry %s (%s) -> ", namelist[0], pmIDStr(idlist[0]));
			if (sts < 0)
			    fprintf(stderr, "%s\n", pmErrStr(sts));
			else {
			    int		j;
			    fprintf(stderr, "%d names\n", sts);
			    for (j = 0; j < sts; j++) {
				fprintf(stderr, "    %s\n", offspring[j]);
			    }
			}
		    }
#endif
		    if (sts < 0)
			continue;
		    if (statuslist) free(statuslist);
		}
		else {
		    /* Not PMDA_INTERFACE_4 or later */
		    continue;
		}
	    }
	    else {
		/* daemon PMDA ... ship request on */
		int		fdfail = -1;
		if (ap->status.notReady)
		    continue;
		pmcd_trace(TR_XMIT_PDU, ap->inFd, PDU_PMNS_TRAVERSE, 1);
		sts = __pmSendTraversePMNSReq(ap->inFd, cp - client, namelist[0]);
		if (sts >= 0) {
		    int		numnames;
		    __pmPDU	*pb;
		    int		pinpdu;
		    pinpdu = sts = __pmGetPDU(ap->outFd, ANY_SIZE, _pmcd_timeout, &pb);
		    if (sts > 0)
			pmcd_trace(TR_RECV_PDU, ap->outFd, sts, (int)((__psint_t)pb & 0xffffffff));
		    if (sts == PDU_PMNS_NAMES) {
			sts = __pmDecodeNameList(pb, &numnames,
						       &offspring, &statuslist);
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_PMNS) {
			    fprintf(stderr, "traverse_dynamic: daemon PMDA: expand dynamic PMNS entry %s (%s) -> ", namelist[0], pmIDStr(idlist[0]));
			    if (sts < 0)
				fprintf(stderr, "%s\n", pmErrStr(sts));
			    else {
				int		j;
				fprintf(stderr, "%d names\n", sts);
				for (j = 0; j < sts; j++) {
				    fprintf(stderr, "    %s\n", offspring[j]);
				}
			    }
			}
#endif
			if (statuslist) {
			    free(statuslist);
			    statuslist = NULL;
			}
			if (sts >= 0) {
			    sts = numnames;
			}
		    }
		    else if (sts == PDU_ERROR) {
			__pmDecodeError(pb, &sts);
			pmcd_trace(TR_RECV_ERR, ap->outFd, PDU_PMNS_NAMES, sts);
		    }
		    else {
			pmcd_trace(TR_WRONG_PDU, ap->outFd, PDU_PMNS_IDS, sts);
			sts = PM_ERR_IPC;	/* Wrong PDU type */
			fdfail = ap->outFd;
		    }
		    if (pinpdu > 0)
			__pmUnpinPDUBuf(pb);
		}
		else {
		    /* __pmSendChildReq failed */
		    sts = __pmMapErrno(sts);
		    fdfail = ap->inFd;
		}
		if (ap != NULL && ap->ipcType != AGENT_DSO &&
		    (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT || sts == -EPIPE) &&
		    fdfail != -1)
		    CleanupAgent(ap, AT_COMM, fdfail);
	    }
	}
	/* Stitching ... remove names[i] and add sts names from offspring[] */
	if (offspring) {
	    int		j;
	    int		k;		/* index for copying to new[] */
	    int		ii;		/* index for copying from names[] */
	    char	**new;
	    char	*p;		/* string copy dest ptr */
	    int		new_len;

	    fake = 0;			/* don't need to undo faking */
	    new_len = travNL_strlen - strlen(namelist[0]) - 1;
	    for (j = 0; j < sts; j++)
		new_len += strlen(offspring[j]) + 1;
	    new = (char **)malloc(new_len + (*num_names - 1 + sts)*sizeof(new[0]));
	    if (new == NULL) {
		/* tough luck! */
		free(offspring);
		continue;
	    }
	    *num_names = *num_names - 1 + sts;
	    p = (char *)&new[*num_names];
	    ii = 0;
	    for (k = 0; k < *num_names; k++) {
		if (k < i || k >= i+sts) {
		    /* copy across old name */
		    if (k == i+sts)
			ii++;	/* skip name than new ones replaced */
		    strcpy(p, (*names)[ii]);
		    ii++;
		}
		else {
		    /* stitch in new name */
		    strcpy(p, offspring[k-i]);
		}
		new[k] = p;
		p += strlen(p) + 1;
	    }

	    free(offspring);
	    free(*names);
	    *names = new;
	    travNL_strlen = new_len;
	}
    }

    if (fake == 1) {
	/*
	 * need to undo initial faking as this name is simply not valid!
	 */
	*num_names = 0;
	free(*names);
	*names = NULL;
	travNL_strlen = 0;
    }

}

/*
 * This handler is for the remote version of pmTraversePMNS.
 *
 * Notes:
 *	We are building up a name-list and giving it to 
 *	__pmSendNameList.
 *	This is a bit inefficient but convenient.
 *	It would really be better to build up a PDU buffer
 *	directly and not do the extra copying !
 */
int
DoPMNSTraverse(ClientInfo *cp, __pmPDU *pb)
{
    int		sts = 0;
    char 	*name = NULL;
    int		travNL_need = 0;

    travNL = NULL;

    if ((sts = __pmDecodeTraversePMNSReq(pb, &name)) < 0)
	goto done;
  
    travNL_strlen = 0;
    travNL_num = 0;
    if ((sts = pmTraversePMNS(name, AddLengths)) < 0)
    	goto check;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMNS) {
	fprintf(stderr, "DoPMNSTraverse: %d names below %s after pmTraversePMNS\n", travNL_num, name);
    }
#endif

    /* for each ptr, string bytes, and string terminators */
    travNL_need = travNL_num * (int)sizeof(char*) + travNL_strlen;

    if ((travNL = (char**)malloc(travNL_need)) == NULL) {
      sts = -oserror();
      goto done;
    }

    travNL_i = 0;
    travNL_ptr = (char*)&travNL[travNL_num];
    sts = pmTraversePMNS(name, BuildNameList);

check:
    /*
     * sts here is last result of calling pmTraversePMNS() ... may need
     * this later
     * for dynamic PMNS entries, travNL_num will be 0 (PM_ERR_PMID from
     * pmTraversePMNS()).
     */
    traverse_dynamic(cp, name, &travNL_num, &travNL);
    if (travNL_num < 1)
	goto done;

    pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_PMNS_NAMES, travNL_num);
    if ((sts = __pmSendNameList(cp->fd, FROM_ANON, travNL_num, travNL, NULL)) < 0) {
	pmcd_trace(TR_XMIT_ERR, cp->fd, PDU_PMNS_NAMES, sts);
	CleanupClient(cp, sts);
	goto done;
    }

done:
    if (name) free(name);
    if (travNL) free(travNL);
    return sts;
}

/*************************************************************************/

static int
GetAttribute(ClientInfo *cp, int code)
{
    __pmPDU	*pb;
    char	*value;
    int		vlen, attr;
    int		sts, pinpdu;

    /* Expecting an attribute (code) PDU from the client */
    pinpdu = sts = __pmGetPDU(cp->fd, LIMIT_SIZE, _pmcd_timeout, &pb);
    if (sts > 0)
	pmcd_trace(TR_RECV_PDU, cp->fd, sts, (int)((__psint_t)pb & 0xffffffff));
    if (sts == PDU_ATTR) {
	if ((sts = __pmDecodeAttr(pb, &attr, &value, &vlen)) == 0) {
	    if (code != attr) {	/* unanticipated attribute */
		sts = PM_ERR_IPC;
	    } else if ((value = strndup(value, vlen)) == NULL) {
		sts = -ENOMEM;
	    } else {	/* stash the attribute for this client */
		sts = __pmHashAdd(attr, (void *)value, &cp->attrs);
	    }
	}
    } else if (sts > 0) {	/* unexpected PDU type */
	sts = PM_ERR_IPC;
    }
    if (pinpdu > 0)
	__pmUnpinPDUBuf(pb);
    return sts;
}

static int
ConnectionAttributes(ClientInfo *cp, int flags)
{
    int sts;

    if ((sts = __pmSecureServerHandshake(cp->fd, flags, &cp->attrs)) < 0) {
	if (pmDebug & DBG_TRACE_AUTH)
	    fprintf(stderr, "DoCreds: __pmSecureServerHandshake gave %d: %s\n",
		    sts, pmErrStr(sts));
	return sts;
    }

    if ((flags & PDU_FLAG_CONTAINER) &&
	(sts = GetAttribute(cp, PCP_ATTR_CONTAINER)) < 0) {
	if (pmDebug & DBG_TRACE_ATTR)
	    fprintf(stderr, "DoCreds: failed GetAttribute container %d: %s\n",
		    sts, pmErrStr(sts));
	return sts;
    }

    return 0;
}

int
DoCreds(ClientInfo *cp, __pmPDU *pb)
{
    int			i, sts, flags = 0, version = 0, sender = 0, credcount = 0;
    __pmCred		*credlist = NULL;
    __pmVersionCred	*vcp;

    if ((sts = __pmDecodeCreds(pb, &sender, &credcount, &credlist)) < 0)
	return sts;
    pmcd_trace(TR_RECV_PDU, cp->fd, PDU_CREDS, credcount);

    for (i = 0; i < credcount; i++) {
	switch(credlist[i].c_type) {
	    case CVERSION:
		vcp = (__pmVersionCred *)&credlist[i];
		flags = vcp->c_flags;
		version = vcp->c_version;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_ATTR) {
		    static const struct {
			int	flag;
			char	*name;
		    } flag_dbg[] = {
			{ PDU_FLAG_SECURE, 	"SECURE" },
			{ PDU_FLAG_COMPRESS,	"COMPRESS" },
			{ PDU_FLAG_AUTH,	"AUTH" },
			{ PDU_FLAG_CREDS_REQD,	"CREDS_REQD" },
			{ PDU_FLAG_SECURE_ACK,	"SECURE_ACK" },
			{ PDU_FLAG_NO_NSS_INIT,	"NO_NSS_INIT" },
			{ PDU_FLAG_CONTAINER,	"CONTAINER" },
		    };
		    int	i;
		    int	first = 1;
		    fprintf(stderr, "DoCreds: version cred (%u) flags=%x (", vcp->c_version, vcp->c_flags);
		    for (i = 0; i < sizeof(flag_dbg)/sizeof(flag_dbg[0]); i++) {
			if (flags & flag_dbg[i].flag) {
			    if (first)
				first = 0;
			    else
				fputc('|', stderr);
			    fprintf(stderr, "%s", flag_dbg[i].name);
			}
		    }
		    fprintf(stderr, ")\n");
		}
#endif
		break;

	    default:
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_AUTH)
		    fprintf(stderr, "DoCreds: Error: bogus cred type %d\n", credlist[i].c_type);
#endif
		sts = PM_ERR_IPC;
		break;
	}
    }
    if (credlist != NULL)
	free(credlist);

    if (sts >= 0 && version)
	sts = __pmSetVersionIPC(cp->fd, version);
    if (sts >= 0 && flags) {
	/*
	 * new client has arrived; may want encryption, authentication, etc
	 * complete the handshake (depends on features requested), continue
	 * on to check access is allowed for the authenticated persona, and
	 * finally notify any interested PMDAs
	 */
	if ((sts = ConnectionAttributes(cp, flags)) < 0)
	    return sts;
    }
    if ((sts = CheckAccountAccess(cp)) < 0) {	/* host access done already */
	if (pmDebug & DBG_TRACE_AUTH)
	    fprintf(stderr, "DoCreds: CheckAccountAccess returns %d: %s\n",
		    sts, pmErrStr(sts));
	return sts;
    }
    /*
     * account authentication successful (if needed) and/or other attributes
     * have been given - in these cases, we need to inform interested PMDAs.
     */
    else if (sts > 0 || (flags & PDU_FLAG_CONTAINER)) {
	sts = AgentsAttributes(cp - client);
	if (sts < 0 && (pmDebug & (DBG_TRACE_AUTH|DBG_TRACE_ATTR)))
	    fprintf(stderr, "DoCreds: AgentsAttributes returns %d: %s\n",
		    sts, pmErrStr(sts));
    }

    return sts;
}
