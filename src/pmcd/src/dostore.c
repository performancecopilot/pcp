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

#include "pmapi.h"
#include "impl.h"
#include "pmcd.h"
#include <assert.h>

/* Routine to split a result into a list of results, each containing metrics
 * from a single domain.  The end of the list is marked by a pmResult with a
 * numpmid of zero.  Any pmids for which there is no agent will be in the
 * second to last pmResult which will have a negated numpmid value.
 */

pmResult **
SplitResult(pmResult *res)
{
    int		i, j;
    static int	*aFreq = NULL;	/* Freq. histogram: pmids for each agent */
    static int	*resIndex = NULL;	/* resIndex[k] = index of agent[k]'s list in result */
    static int	nDoms = 0;		/* No. of entries in two tables above */
    int		nGood;
    int		need;
    pmResult	**results;

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
	if (aFreq != NULL)
	    free(aFreq);
	if (resIndex != NULL)
	    free(resIndex);
	aFreq = (int *)malloc((nAgents + 1) * sizeof(int));
	resIndex = (int *)malloc((nAgents + 1) * sizeof(int));
	if (aFreq == NULL || resIndex == NULL) {
	    __pmNoMem("SplitResult.freq", 2 * (nAgents + 1) * sizeof(int), PM_FATAL_ERR);
	}
    }

    /* Build a frequency histogram of metric domains (use aFreq[nAgents] for
     * pmids for which there is no agent).
     */
    for (i = 0; i <= nAgents; i++)
	aFreq[i] = 0;
    for (i = 0; i < res->numpmid; i++) {
	int dom = ((__pmID_int *)&res->vset[i]->pmid)->domain;
	for (j = 0; j < nAgents; j++)
	    if (agent[j].pmDomainId == dom && agent[j].status.connected)
		break;
	aFreq[j]++;
    }

    /* Initialise resIndex and allocate the results structures */
    nGood = 0;
    for (i = 0; i < nAgents; i++)
	if (aFreq[i]) {
	    resIndex[i] = nGood;
	    nGood++;
	}
    resIndex[nAgents] = nGood;

    need = nGood + 1 + ((aFreq[nAgents]) ? 1 : 0);
    need *= sizeof(pmResult *);
    if ((results = (pmResult **) malloc(need)) == NULL) {
	__pmNoMem("SplitResult.results", need, PM_FATAL_ERR);
    }
    j = 0;
    for (i = 0; i <= nAgents; i++)
	if (aFreq[i]) {
	    need = (int)sizeof(pmResult) + (aFreq[i] - 1) * (int)sizeof(pmValueSet *);
	    results[j] = (pmResult *) malloc(need);
	    if (results[j] == NULL) {
		__pmNoMem("SplitResult.domain", need, PM_FATAL_ERR);
	    }
	    results[j]->numpmid = aFreq[i];
	    j++;
	}

    /* Make the "end of list" pmResult */
    if ((results[j] = (pmResult *) malloc(sizeof(pmResult))) == NULL) {
	__pmNoMem("SplitResult.domain", sizeof(pmResult), PM_FATAL_ERR);
    }
    results[j]->numpmid = 0;

    /* Foreach vset in res, find it's pmResult in the per domain results array
     * and copy a pointer to the vset to the next available position in the per
     * domain result.
     */
    for (i = 0; i <= nAgents; i++)
	aFreq[i] = 0;
    for (i = 0; i < res->numpmid; i++) {
	int dom = ((__pmID_int *)&res->vset[i]->pmid)->domain;
	for (j = 0; j < nAgents; j++)
	    if (dom == agent[j].pmDomainId && agent[j].status.connected)
		break;
	results[resIndex[j]]->vset[aFreq[j]] = res->vset[i];
	aFreq[j]++;
    }

    /* Flip the sign of numpmids in the "bad list" */
    if (aFreq[nAgents]) {
	int bad = resIndex[nAgents];
	results[bad]->numpmid = -results[bad]->numpmid;
    }

    return results;
}

int
DoStore(ClientInfo *cp, __pmPDU* pb)
{
    int		sts;
    int		s = 0;
    AgentInfo	*ap;
    pmResult	*result;
    pmResult	**dResult;
    int		i;
    __pmFdSet	readyFds;
    __pmFdSet	waitFds;
    int		nWait = 0;
    int		maxFd = -1;
    int		badStore;		/* != 0 => store to nonexistent agent */
    int		notReady = 0;		/* != 0 => store to agent that's not ready */
    struct timeval	timeout;


    if ((sts = __pmDecodeResult(pb, &result)) < 0)
	return sts;

    dResult = SplitResult(result);

    /* Send the per-domain results to their respective agents */

    __pmFD_ZERO(&waitFds);
    for (i = 0; dResult[i]->numpmid > 0; i++) {
	int fd;
	ap = FindDomainAgent(((__pmID_int *)&dResult[i]->vset[0]->pmid)->domain);
	/* If it's in a "good" list, pmID has agent that is connected */
	assert(ap != NULL);

	if (ap->ipcType == AGENT_DSO) {
	    if (ap->ipc.dso.dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		ap->ipc.dso.dispatch.version.four.ext->e_context = cp - client;
	    s = ap->ipc.dso.dispatch.version.any.store(dResult[i],
				       ap->ipc.dso.dispatch.version.any.ext);
	}
	else {
	    if (ap->status.notReady == 0) {
		/* agent is ready for PDUs */
		pmcd_trace(TR_XMIT_PDU, ap->inFd, PDU_RESULT, dResult[i]->numpmid);
		s = __pmSendResult(ap->inFd, cp - client, dResult[i]);
		if (s >= 0) {
		    ap->status.busy = 1;
		    fd = ap->outFd;
		    __pmFD_SET(fd, &waitFds);
		    if (fd > maxFd)
			maxFd = fd;
		    nWait++;
		}
		else if (s == PM_ERR_IPC || sts == PM_ERR_TIMEOUT || s == -EPIPE) {
		    pmcd_trace(TR_XMIT_ERR, ap->inFd, PDU_RESULT, sts);
		    CleanupAgent(ap, AT_COMM, ap->inFd);
		}
	    }
	    else
		/* agent is not ready for PDUs */
		notReady = 1;
	}
	if (s < 0) {
	    sts = s;
	    continue;
	}
    }

    /* If there was no agent for one or more pmIDs, there will be a "bad list"
     * with a negated numpmid value.  Store as many "good" pmIDs as possible
     * but remember that there were homeless ones.
     */
    badStore = dResult[i]->numpmid < 0;

    /* Collect error PDUs containing store status from each active agent */

    while (nWait > 0) {
        __pmFD_COPY(&readyFds, &waitFds);
	if (nWait > 1) {
	    timeout.tv_sec = _pmcd_timeout;
	    timeout.tv_usec = 0;

	    s = __pmSelectRead(maxFd+1, &readyFds, &timeout);

	    if (s == 0) {
		__pmNotifyErr(LOG_INFO, "DoStore: select timeout");

		/* Timeout, terminate agents that haven't responded */
		for (i = 0; i < nAgents; i++) {
		    if (agent[i].status.busy) {
			pmcd_trace(TR_RECV_TIMEOUT, agent[i].outFd, PDU_ERROR, 0);
			CleanupAgent(&agent[i], AT_COMM, agent[i].inFd);
		    }
		}
		sts = PM_ERR_IPC;
		break;
	    }
	    else if (sts < 0) {
		/* this is not expected to happen! */
		__pmNotifyErr(LOG_ERR, "DoStore: fatal select failure: %s\n",
			netstrerror());
		Shutdown();
		exit(1);
	    }
	}

	for (i = 0; i < nAgents; i++) {
	    int		pinpdu;
	    ap = &agent[i];
	    if (!ap->status.busy || !__pmFD_ISSET(ap->outFd, &readyFds))
		continue;
	    ap->status.busy = 0;
	    __pmFD_CLR(ap->outFd, &waitFds);
	    nWait--;
	    pinpdu = s = __pmGetPDU(ap->outFd, ANY_SIZE, _pmcd_timeout, &pb);
	    if (s > 0)
		pmcd_trace(TR_RECV_PDU, ap->outFd, s, (int)((__psint_t)pb & 0xffffffff));
	    if (s == PDU_ERROR) {
		int ss;
		if ((ss = __pmDecodeError(pb, &s)) < 0)
		    sts = ss;
		else {
		    if (s < 0) {
			extern int CheckError(AgentInfo *, int);

			sts = CheckError(ap, s);
			pmcd_trace(TR_RECV_ERR, ap->outFd, PDU_RESULT, sts);
		    }
		}
	    }
	    else {
		/* Agent protocol error */
		if (s < 0)
		    pmcd_trace(TR_RECV_ERR, ap->outFd, PDU_RESULT, s);
		else
		    pmcd_trace(TR_WRONG_PDU, ap->outFd, PDU_ERROR, s);
		sts = PM_ERR_IPC;
	    }

	    if (pinpdu > 0)
		__pmUnpinPDUBuf(pb);

	    if (ap->ipcType != AGENT_DSO &&
		(sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT))
		CleanupAgent(ap, AT_COMM, ap->outFd);
	}
    }

    /* Only one error code can be returned, so "no agent" or "not
     * ready" errors have precedence over all except IPC and TIMEOUT
     * protocol failures.
     * Note that we make only a weak effort to return the most
     * appropriate error status because clients interested in the
     * outcome should be using pmStore on individual metric/instances
     * if the outcome is important.  In particular, in multi-agent
     * stores, an earlier PM_ERR_IPC error can be "overwritten" by a
     * subsequent less serious error.
     */
    if (sts != PM_ERR_IPC && sts != PM_ERR_TIMEOUT) {
	if (badStore) {
	    sts = PM_ERR_NOAGENT;
	}
	else if (notReady) {
	    sts = PM_ERR_AGAIN;
	}
    }

    if (sts >= 0) {
	/* send PDU_ERROR, even if result was 0 */
	int s;
	pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_ERROR, 0);
	s = __pmSendError(cp->fd, FROM_ANON, 0);
	if (s < 0)
	    CleanupClient(cp, s);
    }

    pmFreeResult(result);
    i = 0;
    do {
	s = dResult[i]->numpmid;
	free(dResult[i]);
	i++;
    } while (s);			/* numpmid == 0 terminates list */
    free(dResult);

    return sts;
}
