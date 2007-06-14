/*
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: dopdus.c,v 1.3 2002/07/23 10:47:31 kenmcd Exp $"

#include <syslog.h>
#include <errno.h>
#include "pmapi.h"
#include "impl.h"
#include "pmcd.h"

extern int	errno;
extern int	pmcdLicensed;

extern unsigned int	__pmMakeAuthCookie(unsigned int, pid_t);

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
    AgentInfo*	ap;
    char	*buffer;

    if ((sts = __pmDecodeTextReq(pb, PDU_BINARY, &ident, &type)) < 0)
	return sts;

    if ((ap = FindDomainAgent(((__pmID_int *)&ident)->domain)) == NULL)
	return PM_ERR_PMID;
    else if (!ap->status.connected)
	return PM_ERR_NOAGENT;

    if (ap->ipcType == AGENT_DSO) {
	if (ap->ipc.dso.dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
	    sts = ap->ipc.dso.dispatch.version.one.text(ident, type, &buffer);
	else
	    sts = ap->ipc.dso.dispatch.version.two.text(ident, type, &buffer,
					  ap->ipc.dso.dispatch.version.two.ext);
	if (sts < 0 &&
	    ap->ipc.dso.dispatch.comm.pmapi_version == PMAPI_VERSION_1)
		sts = XLATE_ERR_1TO2(sts);
    }
    else {
	if (ap->status.notReady)
	    return PM_ERR_AGAIN;
	if (_pmcd_trace_mask)
	    pmcd_trace(TR_XMIT_PDU, ap->inFd, PDU_TEXT_REQ, ident);
	sts = __pmSendTextReq(ap->inFd, ap->pduProtocol, ident, type);
	if (sts >= 0) {
	    sts = __pmGetPDU(ap->outFd, ap->pduProtocol, _pmcd_timeout, &pb);
	    if (sts > 0 && _pmcd_trace_mask)
		pmcd_trace(TR_RECV_PDU, ap->outFd, sts, (int)((__psint_t)pb & 0xffffffff));
	    if (sts == PDU_TEXT)
		sts = __pmDecodeText(pb, ap->pduProtocol, &ident, &buffer);
	    else if (sts == PDU_ERROR) {
		s = __pmDecodeError(pb, ap->pduProtocol, &sts);
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
	}
	else
	    pmcd_trace(TR_XMIT_ERR, ap->inFd, PDU_TEXT_REQ, sts);
    }

    if (ap->ipcType != AGENT_DSO &&
	(sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT || sts == -EPIPE))
	CleanupAgent(ap, AT_COMM, ap->inFd);	

    if (sts >= 0) {
	if (_pmcd_trace_mask)
	    pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_TEXT, ident);
	sts = __pmSendText(cp->fd, PDU_BINARY, ident, buffer);
	if (sts < 0 && ap->ipcType != AGENT_DSO) {
	    pmcd_trace(TR_XMIT_ERR, cp->fd, PDU_TEXT, sts);
	    CleanupClient(cp, sts);
	}
	if ((ap->ipcType == AGENT_DSO &&
	     ap->ipc.dso.dispatch.comm.pmda_interface == PMDA_INTERFACE_1) ||
	    ap->ipcType != AGENT_DSO) {
		/* daemons and old-style DSOs have a malloc'd buffer */
		free(buffer);
	}
    }
    return sts;
}

int
DoProfile(ClientInfo *cp, __pmPDU *pb)
{
    int		sts;
    __pmProfile	*newProf;
    int		ctxnum;

    sts = __pmDecodeProfile(pb, PDU_BINARY, &ctxnum, &newProf);
    if (sts >= 0) {
	int i;

	if (ctxnum < 0) {
	    __pmNotifyErr(LOG_ERR, "DoProfile: bad ctxnum = %d\n", ctxnum);
	    __pmFreeProfile(newProf);
	    return PM_ERR_NOCONTEXT;
	}

	/* Allocate more profile pointers if required */
	if (ctxnum >= cp->szProfile) {
	    int		oldSize = cp->szProfile;
	    __pmProfile	**newProfPtrs;
	    unsigned	n;

	    if (ctxnum - cp->szProfile < 4)
		cp->szProfile += 4;
	    else
		cp->szProfile = ctxnum + 1;
	    n = cp->szProfile * (int)sizeof(__pmProfile *);
	    if ((newProfPtrs = (__pmProfile **)malloc(n)) == NULL) {
		cp->szProfile = oldSize;
		__pmNoMem("DoProfile.newProfPtrs", n, PM_RECOV_ERR);
		__pmFreeProfile(newProf);
		return -errno;
	    }

	    /* Copy any old pointers and zero the newly allocated ones */
	    if ((n = oldSize * (int)sizeof(__pmProfile *))) {
		memcpy(newProfPtrs, cp->profile, n);
		free(cp->profile);	/* But not the __pmProfile ptrs! */
	    }
	    n = (cp->szProfile - oldSize) * (int)sizeof(__pmProfile *);
	    memset(&newProfPtrs[oldSize], 0, n);
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
    AgentInfo*	ap;
    pmDesc	desc;
    int		fdfail;

    if ((sts = __pmDecodeDescReq(pb, PDU_BINARY, &pmid)) < 0)
	return sts;

    if ((ap = FindDomainAgent(((__pmID_int *)&pmid)->domain)) == NULL)
	return PM_ERR_PMID;
    else if (!ap->status.connected)
	return PM_ERR_NOAGENT;

    if (ap->ipcType == AGENT_DSO) {
	if (ap->ipc.dso.dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
	    sts = ap->ipc.dso.dispatch.version.one.desc(pmid, &desc);
	else
	    sts = ap->ipc.dso.dispatch.version.two.desc(pmid, &desc,
					ap->ipc.dso.dispatch.version.two.ext);
	if (sts < 0 &&
	    ap->ipc.dso.dispatch.comm.pmapi_version == PMAPI_VERSION_1)
		sts = XLATE_ERR_1TO2(sts);
    }
    else {
	if (ap->status.notReady)
	    return PM_ERR_AGAIN;
	if (_pmcd_trace_mask)
	    pmcd_trace(TR_XMIT_PDU, ap->inFd, PDU_DESC_REQ, (int)pmid);
	sts = __pmSendDescReq(ap->inFd, ap->pduProtocol, pmid);
	if (sts >= 0) {
	    sts = __pmGetPDU(ap->outFd, ap->pduProtocol, _pmcd_timeout, &pb);
	    if (sts > 0 && _pmcd_trace_mask)
		pmcd_trace(TR_RECV_PDU, ap->outFd, sts, (int)((__psint_t)pb & 0xffffffff));
	    if (sts == PDU_DESC)
		sts = __pmDecodeDesc(pb, ap->pduProtocol, &desc);
	    else if (sts == PDU_ERROR) {
		s = __pmDecodeError(pb, ap->pduProtocol, &sts);
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
	}
	else {
	    pmcd_trace(TR_XMIT_ERR, ap->inFd, PDU_DESC_REQ, sts);
	    fdfail = ap->inFd;
	}
    }

    if (sts >= 0) {
	if (_pmcd_trace_mask)
	    pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_DESC, (int)desc.pmid);
	sts = __pmSendDesc(cp->fd, PDU_BINARY, &desc);
	if (sts < 0) {
	    pmcd_trace(TR_XMIT_ERR, cp->fd, PDU_DESC, sts);
	    CleanupClient(cp, sts);
	}
    }
    else
	if (ap->ipcType != AGENT_DSO &&
	    (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT || sts == -EPIPE))
	    CleanupAgent(ap, AT_COMM, fdfail);

    return sts;
}

int
DoInstance(ClientInfo *cp, __pmPDU* pb)
{
    int			sts = 0, s;
    __pmTimeval		when;
    pmInDom		indom;
    int			inst;
    char		*name;
    __pmInResult		*inresult = NULL;
    AgentInfo		*ap;
    int			fdfail;

    __pmDecodeInstanceReq(pb, PDU_BINARY, &when, &indom, &inst, &name);
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
	if (ap->ipc.dso.dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
	    sts = ap->ipc.dso.dispatch.version.one.instance(indom, inst, name, 
							     &inresult);
	else
	    sts = ap->ipc.dso.dispatch.version.two.instance(indom, inst, name,
					&inresult,
					ap->ipc.dso.dispatch.version.two.ext);
	if (sts < 0 &&
	    ap->ipc.dso.dispatch.comm.pmapi_version == PMAPI_VERSION_1)
		sts = XLATE_ERR_1TO2(sts);
    }
    else {
	if (ap->status.notReady)
	    return PM_ERR_AGAIN;
	if (_pmcd_trace_mask)
	    pmcd_trace(TR_XMIT_PDU, ap->inFd, PDU_INSTANCE_REQ, (int)indom);
	sts = __pmSendInstanceReq(ap->inFd, ap->pduProtocol, &when, indom, inst, name);
	if (sts >= 0) {
	    sts = __pmGetPDU(ap->outFd, ap->pduProtocol, _pmcd_timeout, &pb);
	    if (sts > 0 && _pmcd_trace_mask)
		pmcd_trace(TR_RECV_PDU, ap->outFd, sts, (int)((__psint_t)pb & 0xffffffff));
	    if (sts == PDU_INSTANCE)
		sts = __pmDecodeInstance(pb, ap->pduProtocol, &inresult);
	    else if (sts == PDU_ERROR) {
		inresult = NULL;
		s = __pmDecodeError(pb, ap->pduProtocol, &sts);
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
	}
	else {
	    pmcd_trace(TR_XMIT_ERR, ap->inFd, PDU_INSTANCE_REQ, sts);
	    fdfail = ap->inFd;
	}
    }
    if (name != NULL) free(name);

    if (sts >= 0) {
	if (_pmcd_trace_mask)
	    pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_INSTANCE, (int)(inresult->indom));
	sts = __pmSendInstance(cp->fd, PDU_BINARY, inresult);
	if (sts < 0) {
	    pmcd_trace(TR_XMIT_ERR, cp->fd, PDU_INSTANCE, sts);
	    CleanupClient(cp, sts);
	}
	if (inresult != NULL)
	    __pmFreeInResult(inresult);
    }
    else
	if (ap->ipcType != AGENT_DSO &&
	    (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT || sts == -EPIPE))
	    CleanupAgent(ap, AT_COMM, fdfail);

    return sts;
}

/*
 * This handler is for remote versions of pmNameAll or pmNameId.
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

    if ((sts = __pmDecodeIDList(pb, PDU_BINARY, 1, idlist, &op_sts)) < 0)
	goto fail;

    if ((sts = pmNameAll(idlist[0], &namelist)) < 0)
    	goto fail;

    numnames = sts;

    if (_pmcd_trace_mask)
	pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_PMNS_NAMES, numnames);
    if ((sts = __pmSendNameList(cp->fd, PDU_BINARY, numnames, namelist, NULL)) < 0){
	pmcd_trace(TR_XMIT_ERR, cp->fd, PDU_PMNS_NAMES, sts);
	CleanupClient(cp, sts);
    	goto fail;
    }

    return sts;

fail:
    if (namelist) free(namelist);
    return sts;
}

/*
 * This handler is for the remote version of pmLookupName.
 */
int
DoPMNSNames(ClientInfo *cp, __pmPDU *pb)
{
    int		sts = 0;
    int		numids = 0;
    pmID	*idlist = NULL;
    char	**namelist = NULL;

    if ((sts = __pmDecodeNameList(pb, PDU_BINARY, &numids, &namelist, NULL)) < 0)
	goto done;

    if ((idlist = (pmID*)malloc(sizeof(int)*numids)) == NULL) {
        sts = -errno;
	goto done;
    }

    if ((sts = pmLookupName(numids, namelist, idlist)) < 0) {
        /* If get an error which should be passed back along
         * with valid data to the client
         * then do NOT fail -> return status with the id-list.
         */
    	if (sts != PM_ERR_NAME && sts != PM_ERR_NONLEAF)
	  goto done;
    }

    if (_pmcd_trace_mask)
	pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_PMNS_IDS, numids);
    if ((sts = __pmSendIDList(cp->fd, PDU_BINARY, numids, idlist, sts)) < 0) {
	pmcd_trace(TR_XMIT_ERR, cp->fd, PDU_PMNS_IDS, sts);
	CleanupClient(cp, sts);
    	goto done;
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

    if ((sts = __pmDecodeChildReq(pb, PDU_BINARY, &name, &subtype)) < 0)
	goto fail;
  
    if (subtype == 0) {
	if ((sts = pmGetChildren(name, &offspring)) < 0)
	    goto fail;
    }
    else {
	if ((sts = pmGetChildrenStatus(name, &offspring, &statuslist)) < 0)
	    goto fail;
    }

    numnames = sts;
    if (_pmcd_trace_mask)
	pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_PMNS_NAMES, numnames);
    if ((sts = __pmSendNameList(cp->fd, PDU_BINARY, numnames, offspring, statuslist)) < 0) {
	pmcd_trace(TR_XMIT_ERR, cp->fd, PDU_PMNS_NAMES, sts);
	CleanupClient(cp, sts);
    	goto fail;
    }

    return sts;

fail:
    if (name) free(name);
    if (offspring) free(offspring);
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
  travNL_strlen += strlen(name);
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

    if ((sts = __pmDecodeTraversePMNSReq(pb, PDU_BINARY, &name)) < 0)
	goto done;
  
    travNL_strlen = 0;
    travNL_num = 0;
    if ((sts = pmTraversePMNS(name, AddLengths)) < 0)
    	goto done;

    /* for each ptr, string bytes, and string terminators */
    travNL_need = travNL_num * (int)sizeof(char*) + travNL_strlen +
                  travNL_num;

    if ((travNL = (char**)malloc(travNL_need)) == NULL) {
      sts = -errno;
      goto done;
    }

    travNL_i = 0;
    travNL_ptr = (char*)&travNL[travNL_num];
    if ((sts = pmTraversePMNS(name, BuildNameList)) < 0)
    	goto done;

    if (_pmcd_trace_mask)
	pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_PMNS_NAMES, travNL_num);
    if ((sts = __pmSendNameList(cp->fd, PDU_BINARY, 
                  travNL_num, travNL, NULL)) < 0) {
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

int
DoCreds(ClientInfo *cp, __pmPDU *pb)
{
    int			i, sts, credcount=0;
    int			sender = 0;
    unsigned int	cookie = 0;
    __pmCred		*credlist = NULL;
    __pmIPC		ipc = { UNKNOWN_VERSION, NULL };

    if ((sts = __pmDecodeCreds(pb, PDU_BINARY, &sender, &credcount, &credlist)) < 0)
	return sts;

    if (_pmcd_trace_mask)
	pmcd_trace(TR_RECV_PDU, cp->fd, PDU_CREDS, credcount);

    for (i = 0; i < credcount; i++) {
	switch(credlist[i].c_type) {
	    case CVERSION:
		ipc.version = credlist[i].c_vala;
		sts = __pmAddIPC(cp->fd, ipc);
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_CONTEXT)
		    fprintf(stderr, "pmcd: version cred (%u)\n", ipc.version);
#endif
		break;
	    case CAUTH:
		cookie |= credlist[i].c_valc;
		cookie <<= 8;
		cookie |= credlist[i].c_valb;
		cookie <<= 8;
		cookie |= credlist[i].c_vala;

		if ((sts = __pmMakeAuthCookie(cp->pduInfo.authorize, sender)) != cookie) {
		    sts = PM_ERR_PERMISSION;
		}
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_CONTEXT)
		    fprintf(stderr, "pmcd: my auth cred cookie=%u - client %s (pid=%d)\n",
			cookie, (sts==PM_ERR_PERMISSION)?("denied"):("accepted"), sender);
#endif
		break;
	}
    }

    if (credlist != NULL)
	free(credlist);

    return sts;
}
