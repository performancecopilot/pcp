/*
 * Copyright (c) 2012-2019,2021-2022 Red Hat.
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "libpcp.h"
#include "pmcd.h"

/* Freq. histogram: pmids for each agent in current fetch request */

static int	*aFreq;

/* Routine to break a list of pmIDs up into sublists of metrics within the
 * same metric domain.  The resulting lists are returned via a pointer to an
 * array of per-domain lists as defined by the struct below.  Any metrics for
 * which no agent exists are collected into a list at the end of the list of
 * valid lists.  This list has domain = -1 and is used to indicate the end of
 * the list of pmID lists.
 */

typedef struct {
    int  domain;
    int  listSize;
    pmID *list;
} DomPmidList;

static DomPmidList *
SplitPmidList(int nPmids, pmID *pmidList)
{
    int			i, j;
    static int		*resIndex;	/* resIndex[k] = index of agent[k]'s list in result */
    static int		nDoms;	/* No. of entries in two tables above */
    int			nGood;
    static int		currentSize;
    int			resultSize;
    static DomPmidList	*result;
    pmID		*resultPmids;

    /* Allocate the frequency histogram and array for mapping from agent to
     * result list index.  Because a SIGHUP reconfiguration may have caused a
     * change in the number of agents, reallocation using a new size may be
     * necessary.
     * There are nAgents + 1 entries in the aFreq and resIndex arrays.  The
     * last entry in each is used for the pmIDs for which no agent could be
     * found.
     */
    if (nAgents > nDoms) {
	nDoms = nAgents;
	if (resIndex != NULL)
	    free(resIndex);
	if (aFreq != NULL)
	    free(aFreq);
	resIndex = (int *)malloc((nAgents + 1) * sizeof(int));
	aFreq = (int *)malloc((nAgents + 1) * sizeof(int));
	if (resIndex == NULL || aFreq == NULL) {
	    pmNoMem("SplitPmidList.resIndex", 2 * (nAgents + 1) * sizeof(int), PM_FATAL_ERR);
	    /* NOTREACHED */
	}
    }

    memset(aFreq, 0, (nAgents + 1) * sizeof(aFreq[0]));

    if (nPmids == 1) {
	/* FastTrack this case */
	for (i = 0; i < nAgents; i++)
	    resIndex[i] = 1;
	i = mapdom[((__pmID_int *)&pmidList[0])->domain];
	aFreq[i] = 1;
	resIndex[i] = 0;
	nGood = i == nAgents ? 0 : 1;
	goto doit;
    }

    /*
     * Build a frequency histogram of metric domains (use aFreq[nAgents],
     * via mapdom[] for pmids for which there is no agent).
     */
    for (i = 0; i < nPmids; i++) {
	j = mapdom[((__pmID_int *)&pmidList[i])->domain];
	aFreq[j]++;
    }

    /* Build the mapping between agent index and the position of the agent's
     * subset of the pmidList in the returned result's DomPmidList.
     */
    nGood = 0;
    for (i = 0; i < nAgents; i++)
	if (aFreq[i])
	    nGood++;

    /* nGood is the number of "valid" result pmid lists.  It is also the INDEX
     * of the BAD list in the resulting list of DomPmidLists).
     */
    j = 0;
    for (i = 0; i < nAgents; i++)
	resIndex[i] = (aFreq[i]) ? j++ : nGood;
    resIndex[nAgents] = nGood;		/* For the "bad" list */

    /* Now malloc up a single heap block for the resulting list of pmID lists.
     * First is a list of (nDoms + 1) DomPmidLists (the last is a sentinel with
     * a domain of -1), then come the pmID lists pointed to by the
     * DomPmidLists.
     */
doit:
    resultSize = (nGood + 1) * (int)sizeof(DomPmidList);
    resultSize += nPmids * sizeof(pmID);
    if (resultSize > currentSize) {
	if (currentSize > 0)
	    free(result);
	result = (DomPmidList *)malloc(resultSize);
	if (result == NULL) {
	    pmNoMem("SplitPmidList.result", resultSize, PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	currentSize = resultSize;
    }

    resultPmids = (pmID *)&result[nGood + 1];
    if (nPmids == 1) {
	/* more FastTrack */
	if (nGood) {
	    /* domain known, otherwise things fixed up below */
	    i = mapdom[((__pmID_int *)&pmidList[0])->domain];
	    j = resIndex[i];
	    result[j].domain = agent[i].pmDomainId;
	    result[j].listSize = 0;
	    result[j].list = resultPmids;
	    resultPmids++;
	}
    }
    else {
	for (i = 0; i < nAgents; i++) {
	    if (aFreq[i]) {
		j = resIndex[i];
		result[j].domain = agent[i].pmDomainId;
		result[j].listSize = 0;
		result[j].list = resultPmids;
		resultPmids += aFreq[i];
	    }
	}
    }
    result[nGood].domain = -1;		/* Set up the "bad" list */
    result[nGood].listSize = 0;
    result[nGood].list = resultPmids;

    for (i = 0; i < nPmids; i++) {
 	j = resIndex[mapdom[((__pmID_int *)&pmidList[i])->domain]];
 	result[j].list[result[j].listSize++] = pmidList[i];
    }
    return result;
}

/* Build a pmResult indicating that no values are available for the pmID list
 * supplied.
 */

static pmResult *
MakeBadResult(int npmids, pmID *list, int sts)
{
    int	       need;
    int	       i;
    pmValueSet *vSet;
    pmResult   *result;

    /*
     * Note: do not convert to __pmResult, the pmcd-pmda interfaces and
     * 	 and pmcd internally will continue to use pmResult
     */
    need = (int)sizeof(pmResult) +
	(npmids - 1) * (int)sizeof(pmValueSet *);
	/* npmids - 1 because there is already 1 pmValueSet* in a pmResult */
    result = (pmResult *)malloc(need);
    if (result == NULL) {
	pmNoMem("MakeBadResult.result", need, PM_FATAL_ERR);
    }
    result->timestamp.tv_sec = 0;
    result->timestamp.tv_usec = 0;
    result->numpmid = npmids;
    for (i = 0; i < npmids; i++) {
	vSet = (pmValueSet *)malloc(sizeof(pmValueSet));
	if (vSet == NULL) {
	    pmNoMem("MakeBadResult.vSet", sizeof(pmValueSet), PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	result->vset[i] = vSet;
	vSet->pmid = list[i];
	vSet->numval = sts;
    }
    return result;
}

static pmResult *
SendFetch(DomPmidList *dpList, AgentInfo *aPtr, ClientInfo *cPtr, int ctxnum)
{
    __pmHashCtl		*hcp;
    __pmHashNode	*hp;
    pmProfile		*profile;
    pmProfile		defprofile = {PM_PROFILE_INCLUDE, 0, NULL};
    pmResult		*result = NULL;
    int			sts = 0;
    int			bad = 0;
    int			i;

    if (pmDebugOptions.appl0) {
	fprintf(stderr, "SendFetch %d metrics to PMDA domain %d ",
	    dpList->listSize, dpList->domain);
	switch (aPtr->ipcType) {
	case AGENT_DSO:
	    fprintf(stderr, "(dso)\n");
	    break;

	case AGENT_SOCKET:
	    fprintf(stderr, "(socket)\n");
	    break;

	case AGENT_PIPE:
	    fprintf(stderr, "(pipe)\n");
	    break;

	default:
	    fprintf(stderr, "(type %d unknown!)\n", aPtr->ipcType);
	    break;
	}
	for (i = 0; i < dpList->listSize; i++)
	    fprintf(stderr, "  pmid[%d] %s\n", i, pmIDStr(dpList->list[i]));
    }

    /* status.madeDsoResult is only used for DSO agents so don't waste time by
     * checking that the agent is a DSO first.
     */
    aPtr->status.madeDsoResult = 0;

    if (aPtr->profClient != cPtr || ctxnum != aPtr->profIndex) {
	hcp = &cPtr->profile;
	hp = __pmHashSearch(ctxnum, hcp);
	if (hp != NULL)
	    profile = (pmProfile *)hp->data;
	else
	    profile = &defprofile;
	if (aPtr->ipcType == AGENT_DSO) {
	    if (aPtr->ipc.dso.dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		aPtr->ipc.dso.dispatch.version.four.ext->e_context = cPtr - client;
	    sts = aPtr->ipc.dso.dispatch.version.any.profile(profile,
				     aPtr->ipc.dso.dispatch.version.any.ext);
	}
	else {
	    if (aPtr->status.notReady == 0) {
		pmcd_trace(TR_XMIT_PDU, aPtr->inFd, PDU_PROFILE, ctxnum);
		if ((sts = __pmSendProfile(aPtr->inFd, cPtr - client,
					   ctxnum, profile)) < 0) {
		    pmcd_trace(TR_XMIT_ERR, aPtr->inFd, PDU_PROFILE, sts);
		}
	    } else {
		sts = PM_ERR_AGAIN;
	    }

	}
	if (sts >= 0) {
	    aPtr->profClient = cPtr;
	    aPtr->profIndex = ctxnum;
	}
    }

    if (sts >= 0) {
	if (aPtr->ipcType == AGENT_DSO) {
	    if (aPtr->ipc.dso.dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		aPtr->ipc.dso.dispatch.version.four.ext->e_context = cPtr - client;
	    sts = aPtr->ipc.dso.dispatch.version.any.fetch(dpList->listSize,
				   dpList->list, &result, 
				   aPtr->ipc.dso.dispatch.version.any.ext);
	    if (sts >= 0) {
		if (result == NULL) {
		    pmNotifyErr(LOG_WARNING,
				"\"%s\" agent (DSO) returned a null result\n",
				aPtr->pmDomainLabel);
		    sts = PM_ERR_PMID;
		    bad = 1;
		}
		else if (result->numpmid != dpList->listSize) {
		    pmNotifyErr(LOG_WARNING,
				"\"%s\" agent (DSO) returned %d pmIDs (%d expected)\n",
				aPtr->pmDomainLabel,
				result->numpmid,dpList->listSize);
		    sts = PM_ERR_PMID;
		    bad = 2;
		}
	    }
	}
	else {
	    if (aPtr->status.fenced) {
		/* agent is blocked from PDUs */
		sts = PM_ERR_PMDAFENCED;
	    }
	    else if (aPtr->status.notReady == 0) {
		/* agent is ready for PDUs */
		pmcd_trace(TR_XMIT_PDU, aPtr->inFd, PDU_FETCH, dpList->listSize);
		if ((sts = __pmSendFetch(aPtr->inFd, cPtr - client, ctxnum, 
				   dpList->listSize, dpList->list)) < 0)
		    pmcd_trace(TR_XMIT_ERR, aPtr->inFd, PDU_FETCH, sts);
	    }
	    else {
		/* agent is not ready for PDUs */
		sts = PM_ERR_AGAIN;
	    }
	}
    }

    if (sts < 0) {
	if (pmDebugOptions.appl0)
	    switch (bad) {
		case 0:
		    fprintf(stderr, "FETCH error: \"%s\" agent : %s\n",
			    aPtr->pmDomainLabel, pmErrStr(sts));
		    break;
		case 1:
		    fprintf(stderr, "\"%s\" agent (DSO) returned a null result\n",
			    aPtr->pmDomainLabel);
		    break;
		case 2:
		    fprintf(stderr, "\"%s\" agent (DSO) returned %d pmIDs (%d expected)\n",
			    aPtr->pmDomainLabel,
			    result->numpmid, dpList->listSize);
		    break;
	    }
	if (aPtr->ipcType == AGENT_DSO) {
	    aPtr->status.madeDsoResult = 1;
	    sts = 0;
	}
	else if (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT || sts == -EPIPE)
	    CleanupAgent(aPtr, AT_COMM, aPtr->inFd);

	result = MakeBadResult(dpList->listSize, dpList->list, sts);
    }

    return result;
}

/*
 * pmResults coming back from PMDAs have their timestamp field
 * overloaded to contain out-of-band information such as state
 * changes that may need to be communicated back to clients.
 * Extract the flags that indicate those state changes here from
 * PMDA agent[i].
 */
static int
ExtractState(int i, void *timestamp)
{
    unsigned char	byte;

    memcpy(&byte, timestamp, sizeof(unsigned char));
    /*
     * integrity checks on the state change ...
     * - only 7 bits defined in pmapi.h (so max value is 0x3f)
     * - cannot have any of the PMCD_AGENT_CHANGE bits set (these
     *   are set by pmcd, not a PMDA)
     * - ditto for the PMCD_HOSTNAME_CHANGE bit
     */
    if (byte > 0x3f ||
        (byte & PMCD_AGENT_CHANGE) ||
	(byte & PMCD_HOSTNAME_CHANGE)) {
	/*
	 * mapping to at most PMCD_LABEL_CHANGE | PMCD_NAMES_CHANGE
	 * is probably not right (the PMDA is broken), but this is a
	 * pretty harmless state change for upstream clients,
	 * e.g. won't kill pmlogger!
	 */
	unsigned char	new_byte = byte & (PMCD_LABEL_CHANGE | PMCD_NAMES_CHANGE);

	pmNotifyErr(LOG_WARNING,
	    "ExtractState: \"%s\" agent returned a bogus state change (0x%x) modified to 0x%x\n",
	    agent[i].pmDomainLabel, byte, new_byte);
	byte = new_byte;
    }

    if (byte != 0 && pmDebugOptions.appl6) {
	fprintf(stderr, "ExtractState: %s agent (dom %d) set ", agent[i].pmDomainLabel, agent[i].pmDomainId);
	__pmDumpFetchFlags(stderr, byte);
	fputc('\n', stderr);
    }

    return (int)byte;
}

/*
 * Handle both the original and high resolution fetch PDU requests.
 * The input handling and PMDA interactions are the same, difference
 * is in the output result - new uses pmHighResResult (timespec),
 * original uses pmResult (timeval).
 */
static int
HandleFetch(ClientInfo *cip, __pmPDU* pb, int pdutype)
{
    int			i, j;
    int 		sts;
    int			ctxnum;
    unsigned int	changes = 0;
    int			nPmids;
    pmID		*pmidList;
    static __pmResult	*endResult = NULL;
    static int		maxnpmids;	/* sizes endResult */
    DomPmidList		*dList;		/* NOTE: NOT indexed by agent index */
    static int		nDoms;
    static pmResult	**results;	/* array of replies from PMDAs */
    static int		*resIndex;
    __pmFdSet		waitFds;
    __pmFdSet		readyFds;
    int			nWait;
    int			maxFd;
    struct timeval	timeout;
    __pmHashCtl		*hcp;
    __pmHashNode	*hp;
    pmProfile		*profile;

    if (nAgents > nDoms) {
	if (results != NULL)
	    free(results);
	if (resIndex != NULL)
	    free(resIndex);
	results = (pmResult **)malloc((nAgents + 1) * sizeof (pmResult *));
	resIndex = (int *)malloc((nAgents + 1) * sizeof(int));
	if (results == NULL || resIndex == NULL) {
	    pmNoMem("DoFetch.results", (nAgents + 1) * sizeof (pmResult *) + (nAgents + 1) * sizeof(int), PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	nDoms = nAgents;
    }
    memset(results, 0, (nAgents + 1) * sizeof(results[0]));

    /* Both PDUs decode the same way, use variant without retired timestamp */
    sts = __pmDecodeHighResFetch(pb, &ctxnum, &nPmids, &pmidList);
    if (sts < 0)
	return sts;

    /* Check that a profile has been received from the specified context */
    profile = NULL;
    if (ctxnum >= 0) {
	hcp = &cip->profile;
	hp = __pmHashSearch(ctxnum, hcp);
	if (hp != NULL)
	    profile = (pmProfile *)hp->data;
    }
    if (ctxnum < 0 || profile == NULL) {
	__pmUnpinPDUBuf(pb);
	if (ctxnum < 0)
	    pmNotifyErr(LOG_ERR, "DoFetch: bad ctxnum=%d\n", ctxnum);
	else
	    pmNotifyErr(LOG_ERR, "DoFetch: no profile for ctxnum=%d\n", ctxnum);
	return PM_ERR_NOPROFILE;
    }

    if (nPmids > maxmetrics) {
	__pmUnpinPDUBuf(pb);
	return PM_ERR_TOOBIG;
    }

    if (nPmids > maxnpmids) {
	if (endResult != NULL) {
	    endResult->numpmid = 0;	/* don't free vset's */
	    __pmFreeResult(endResult);
	}
	if ((endResult = __pmAllocResult(nPmids)) == NULL) {
	    pmNoMem("DoFetch.endResult", sizeof(__pmResult) + (nPmids - 1) * sizeof(pmValueSet *), PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	maxnpmids = nPmids;
    }

    dList = SplitPmidList(nPmids, pmidList);

    /* For each domain in the split pmidList, dispatch the per-domain subset
     * of pmIDs to the appropriate agent.  For DSO agents, the pmResult will
     * come back immediately.  If a request cannot be sent to an agent, a
     * suitable pmResult (containing metric not available values) will be
     * returned.
     */
    __pmFD_ZERO(&waitFds);
    nWait = 0;
    maxFd = -1;
    for (i = 0; dList[i].domain != -1; i++) {
	j = mapdom[dList[i].domain];
	results[j] = SendFetch(&dList[i], &agent[j], cip, ctxnum);
	if (results[j] == NULL) { /* Wait for agent's response */
	    int fd = agent[j].outFd;
	    agent[j].status.busy = 1;
	    __pmFD_SET(fd, &waitFds);
	    if (fd > maxFd)
		maxFd = fd;
	    nWait++;
	} else {
	    changes |= ExtractState(i, &results[j]->timestamp);
	}
    }
    /* Construct pmResult for bad-pmID list */
    if (dList[i].listSize != 0)
	results[nAgents] = MakeBadResult(dList[i].listSize, dList[i].list, PM_ERR_NOAGENT);

    /* Wait for results to roll in from agents */
    while (nWait > 0) {
        __pmFD_COPY(&readyFds, &waitFds);
	if (nWait > 1) {
	    timeout.tv_sec = pmcd_timeout;
	    timeout.tv_usec = 0;

            retry:
	    setoserror(0);
	    sts = __pmSelectRead(maxFd+1, &readyFds, &timeout);

	    if (sts == 0) {
		pmNotifyErr(LOG_INFO, "DoFetch: select timeout");

		/* Timeout, terminate agents with undelivered results */
		for (i = 0; i < nAgents; i++) {
		    if (agent[i].status.busy) {
			/* Find entry in dList for this agent */
			for (j = 0; dList[j].domain != -1; j++)
			    if (dList[j].domain == agent[i].pmDomainId)
				break;
			results[i] = MakeBadResult(dList[j].listSize,
						   dList[j].list,
						   PM_ERR_NOAGENT);
			pmcd_trace(TR_RECV_TIMEOUT, agent[i].outFd, PDU_RESULT, 0);
			CleanupAgent(&agent[i], AT_COMM, agent[i].inFd);
		    }
		}
		break;
	    }
	    else if (sts < 0) {
		if (neterror() == EINTR)
		    goto retry;
		/* this is not expected to happen! */
		pmNotifyErr(LOG_ERR, "DoFetch: fatal select failure: %s\n",
			netstrerror());
		Shutdown();
		exit(1);
	    }
	}

	/* Read results from agents that have them ready */
	for (i = 0; i < nAgents; i++) {
	    AgentInfo	*ap = &agent[i];
	    int		pinpdu;
	    if (!ap->status.busy || !__pmFD_ISSET(ap->outFd, &readyFds))
		continue;
	    ap->status.busy = 0;
	    __pmFD_CLR(ap->outFd, &waitFds);
	    nWait--;
	    pinpdu = sts = __pmGetPDU(ap->outFd, ANY_SIZE, pmcd_timeout, &pb);
	    if (sts > 0)
		pmcd_trace(TR_RECV_PDU, ap->outFd, sts, (int)((__psint_t)pb & 0xffffffff));
	    if (sts == PDU_RESULT) {
		__pmResult *rp;
		if ((sts = __pmDecodeResult(pb, &rp)) >= 0) {
		    results[i] = __pmOffsetResult(rp);
		    if (results[i]->numpmid == aFreq[i]) {
			changes |= ExtractState(i, &rp->timestamp);
		    } else {
			if (pmDebugOptions.appl0)
			    pmNotifyErr(LOG_ERR, "DoFetch: \"%s\" agent given %d pmIDs, returned %d\n",
					 ap->pmDomainLabel, aFreq[i], results[i]->numpmid);
			__pmFreeResult(rp);
			results[i] = NULL;
			sts = PM_ERR_IPC;
		    }
		}
	    }
	    else {
		if (sts == PDU_ERROR) {
		    int s;
		    if ((s = __pmDecodeError(pb, &sts)) < 0)
			sts = s;
		    else if (sts >= 0)
			sts = PM_ERR_GENERIC;
		    pmcd_trace(TR_RECV_ERR, ap->outFd, PDU_RESULT, sts);
		}
		else if (sts >= 0) {
		    pmcd_trace(TR_WRONG_PDU, ap->outFd, PDU_RESULT, sts);
		    sts = PM_ERR_IPC;
		}
	    }
	    if (pinpdu > 0)
		__pmUnpinPDUBuf(pb);

	    if (sts < 0) {
		/* Find entry in dList for this agent */
		for (j = 0; dList[j].domain != -1; j++)
		    if (dList[j].domain == agent[i].pmDomainId)
			break;
		results[i] = MakeBadResult(dList[j].listSize,
					   dList[j].list, sts);

		if (sts == PM_ERR_PMDANOTREADY) {
		    /* the agent is indicating it can't handle PDUs for now */
		    int k;

		    for (k = 0; k < dList[j].listSize; k++)
			results[i]->vset[k]->numval = PM_ERR_AGAIN;
		    sts = CheckError(&agent[i], sts);
		}

		if (pmDebugOptions.appl0) {
		    fprintf(stderr, "RESULT error from \"%s\" agent : %s\n",
			    ap->pmDomainLabel, pmErrStr(sts));
		}
		if (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)
		    CleanupAgent(ap, AT_COMM, ap->outFd);
	    }
	}
    }

    if (changes)
	MarkStateChanges(changes);

    endResult->numpmid = nPmids;
    __pmGetTimestamp(&endResult->timestamp);

    /* The order of the pmIDs in the per-domain results is the same as in the
     * original request, but on a per-domain basis.  resIndex is an array of
     * indices (one per agent) of the next metric to be retrieved from each
     * per-domain result value set.
     */
    memset(resIndex, 0, (nAgents + 1) * sizeof(resIndex[0]));
    for (i = 0; i < nPmids; i++) {
	j = mapdom[((__pmID_int *)&pmidList[i])->domain];
	endResult->vset[i] = results[j]->vset[resIndex[j]++];
    }

    pmcd_trace(TR_XMIT_PDU, cip->fd, pdutype, nPmids);

    sts = 0;
    if (cip->status.changes) {
	/* notify client of PMCD state change */
	if (pmDebugOptions.appl6) {
	    fprintf(stderr, "HandleFetch: client[%d] (fd %d) sent ", (int)(cip - client), cip->fd);
	    __pmDumpFetchFlags(stderr, cip->status.changes);
	    fputc('\n', stderr);
	}
	sts = __pmSendError(cip->fd, FROM_ANON, (int)cip->status.changes);
	if (sts > 0)
	    sts = 0;
	cip->status.changes = 0;
    }
    if (sts == 0)
	sts = (pdutype == PDU_HIGHRES_FETCH) ?
		__pmSendHighResResult(cip->fd, FROM_ANON, endResult) :
		__pmSendResult(cip->fd, FROM_ANON, endResult);

    if (sts < 0) {
	pmcd_trace(TR_XMIT_ERR, cip->fd, pdutype, sts);
	CleanupClient(cip, sts);
    }

    /*
     * pmFreeResult() all the accumulated results.
     */
    for (i = 0; dList[i].domain != -1; i++) {
	j = mapdom[dList[i].domain];
	if (agent[j].ipcType == AGENT_DSO && agent[j].status.connected &&
	    !agent[j].status.madeDsoResult)
	    /* Living DSO's manage their own pmResult skeleton unless
	     * MakeBadResult was called to create the result.  The value sets
	     * within the skeleton need to be freed though!
	     */
	    __pmFreeResultValues(results[j]);
	else
	    /* For others it is dynamically allocated in __pmDecodeResult or
	     * MakeBadResult
	     */
	    pmFreeResult(results[j]);
    }
    if (results[nAgents] != NULL)
	pmFreeResult(results[nAgents]);
    __pmUnpinPDUBuf(pmidList);
    return 0;
}

int
DoFetch(ClientInfo *cip, __pmPDU *pb)
{
    return HandleFetch(cip, pb, PDU_FETCH);
}

int
DoHighResFetch(ClientInfo *cip, __pmPDU *pb)
{
    return HandleFetch(cip, pb, PDU_HIGHRES_FETCH);
}
