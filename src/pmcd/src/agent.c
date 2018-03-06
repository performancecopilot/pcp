/*
 * Copyright (c) 2012-2013,2015-2017 Red Hat.
 * Copyright (c) 1995-2005 Silicon Graphics, Inc.  All Rights Reserved.
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
#if defined(HAVE_DLFCN_H)
#include <dlfcn.h>
#elif defined(HAVE_DL_H)
#include <dl.h>
#endif
#if defined(HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif
#if defined(HAVE_SYS_RESOURCE_H)
#include <sys/resource.h>
#endif

static pid_t
waitpid_pmcd(int *status)
{
#if defined(HAVE_WAIT3)
    return wait3(status, WNOHANG, NULL);
#elif defined(HAVE_WAITPID)
    return waitpid((pid_t)-1, status, WNOHANG);
#else
    return (pid_t)-1;
#endif
}

static pid_t
waitpid_pmdaroot(int *status)
{
    if (pmdarootfd <= 0)
	return (pid_t)-1;
    return pmdaRootProcessWait(pmdarootfd, (pid_t)-1, status);
}

static void
short_delay(int milliseconds)
{
    struct timespec delay;

    delay.tv_sec = 0;
    delay.tv_nsec = milliseconds * 1000000;
    (void)nanosleep(&delay, NULL);
}

/* Return a pointer to the agent that is reposible for a given domain.
 * Note that the agent may not be in a connected state!
 */
AgentInfo *
FindDomainAgent(int domain)
{
    int i;
    for (i = 0; i < nAgents; i++)
	if (agent[i].pmDomainId == domain)
	    return &agent[i];
    return NULL;
}

void
CleanupAgent(AgentInfo* aPtr, int why, int status)
{
    int		exit_status = status;
    int		reason = 0;

    if (aPtr->ipcType == AGENT_DSO) {
	if (aPtr->ipc.dso.dlHandle != NULL) {
#ifdef HAVE_DLOPEN
	    dlclose(aPtr->ipc.dso.dlHandle);
#endif
	}
	pmcd_trace(TR_DEL_AGENT, aPtr->pmDomainId, -1, -1);
    }
    else {
	pmcd_trace(TR_DEL_AGENT, aPtr->pmDomainId, aPtr->inFd, aPtr->outFd);
	if (aPtr->inFd != -1) {
	    if (aPtr->ipcType == AGENT_SOCKET)
	      __pmCloseSocket(aPtr->inFd);
	    else {
	      close(aPtr->inFd);
	      __pmResetIPC(aPtr->inFd);
	    }
	    aPtr->inFd = -1;
	}
	if (aPtr->outFd != -1) {
	    if (aPtr->ipcType == AGENT_SOCKET)
	      __pmCloseSocket(aPtr->outFd);
	    else {
	      close(aPtr->outFd);
	      __pmResetIPC(aPtr->outFd);
	    }
	    aPtr->outFd = -1;
	}
	if (aPtr->ipcType == AGENT_SOCKET &&
	    aPtr->ipc.socket.addrDomain == AF_UNIX) {
	    /* remove the Unix domain socket */
	    unlink(aPtr->ipc.socket.name);
	}
    }

    pmNotifyErr(LOG_INFO, "CleanupAgent ...\n");
    fprintf(stderr, "Cleanup \"%s\" agent (dom %d):", aPtr->pmDomainLabel, aPtr->pmDomainId);

    if (why == AT_EXIT) {
	/* waitpid has already been done */
	fprintf(stderr, " terminated");
	reason = (status << 8) | REASON_EXIT;
    }
    else {
	if (why == AT_CONFIG) {
	    fprintf(stderr, " unconfigured");
	} else {
	    reason = REASON_PROTOCOL;
	    fprintf(stderr, " protocol failure for fd=%d", status);
	    exit_status = -1;
	}
	if (aPtr->status.isChild || aPtr->status.isRootChild) {
	    pid_t	pid = (pid_t)-1;
	    pid_t	done;
	    int 	wait_status;
	    int 	slept = 0;

	    if (aPtr->ipcType == AGENT_PIPE)
		pid = aPtr->ipc.pipe.agentPid;
	    else if (aPtr->ipcType == AGENT_SOCKET)
		pid = aPtr->ipc.socket.agentPid;
	    for ( ; ; ) {
		done = (aPtr->status.isRootChild) ?
			waitpid_pmdaroot(&wait_status):
			waitpid_pmcd(&wait_status);
		if (done < (pid_t)0)
		    wait_status = 0;
		else if (done == pid) {
		    exit_status = wait_status;
		    break;
		}
		else if (done > 0)
		    continue;
		if (slept)
		    break;
		/* give PMDA a chance to notice the close() and exit */
		short_delay(10);
		slept = 1;
	    }
	}
    }
    if (exit_status != -1) {
#ifndef IS_MINGW
	if (WIFEXITED(exit_status)) {
	    fprintf(stderr, ", exit(%d)", WEXITSTATUS(exit_status));
	    reason = (WEXITSTATUS(exit_status) << 8) | reason;
	}
	else if (WIFSIGNALED(exit_status)) {
	    fprintf(stderr, ", signal(%d)", WTERMSIG(exit_status));
#ifdef WCOREDUMP
	    if (WCOREDUMP(exit_status))
		fprintf(stderr, ", dumped core");
#endif
	    reason = (WTERMSIG(exit_status) << 16) | reason;
	}
#else
	; /* no more information for Windows ... */
#endif
    }
    fputc('\n', stderr);
    aPtr->reason = reason;
    aPtr->status.connected = 0;
    aPtr->status.busy = 0;
    aPtr->status.notReady = 0;
    aPtr->status.flags = 0;
    AgentDied = 1;

    if (pmcd_trace_mask)
	pmcd_dump_trace(stderr);

    MarkStateChanges(PMCD_DROP_AGENT);
}

static int
HarvestAgentByParent(unsigned int *total, int root)
{
    int		i;
    int		sts;
    int		found;
    pid_t	pid;
    AgentInfo	*ap;

    /* Check for child process termination.  Be careful, and ignore any
     * non-agent processes found.
     */
    do {
	found = 0;
	pid = root ? waitpid_pmdaroot(&sts) : waitpid_pmcd(&sts);
	for (i = 0; i < nAgents; i++) {
	    ap = &agent[i];
	    if (!ap->status.connected || ap->ipcType == AGENT_DSO)
		continue;
	    if (root && !ap->status.isRootChild)
		continue;
	    if (!root && !ap->status.isChild)
		continue;
	    found = 1;
	    if (pid <= (pid_t)0) {
		if (total && (*total = (*total - 1))) {
		    short_delay(10);
		    break;
		} else {
		    return -1;
		}
	    }
	    if (pid == ((ap->ipcType == AGENT_SOCKET) 
			? ap->ipc.socket.agentPid 
			: ap->ipc.pipe.agentPid)) {
		CleanupAgent(ap, AT_EXIT, sts);
		break;
	    }
	}
    } while (found);

    return 0;
}

/* Wait up to total secs for agents to terminate.
 * Return 0 if all terminate, else -1
 */
int
HarvestAgents(unsigned int total)
{
    unsigned	*tp = total ? &total : NULL;
    int		sts = 0;

    /*
     * Check for pmdaroot child process termination first because
     * pmdaroot itself (a direct child of pmcd) is involved there.
     * If we harvest it before any others, we'd struggle.
     */
    sts |= HarvestAgentByParent(tp, 1);
    sts |= HarvestAgentByParent(tp, 0);
    return sts;
}
