/*
 * Copyright (c) 2012-2013 Red Hat.
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
    extern int	AgentDied;
#ifndef IS_MINGW
    int		exit_status = status;
#endif
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

    __pmNotifyErr(LOG_INFO, "CleanupAgent ...\n");
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
#ifndef IS_MINGW
	    exit_status = -1;
#endif
	}
	if (aPtr->status.isChild == 1) {
	    pid_t	pid = (pid_t)-1;
	    pid_t	done;
	    int 	wait_status;
	    int 	slept = 0;

	    if (aPtr->ipcType == AGENT_PIPE)
		pid = aPtr->ipc.pipe.agentPid;
	    else if (aPtr->ipcType == AGENT_SOCKET)
		pid = aPtr->ipc.socket.agentPid;
	    for ( ; ; ) {

#if defined(HAVE_WAIT3)
		done = wait3(&wait_status, WNOHANG, NULL);
#elif defined(HAVE_WAITPID)
		done = waitpid((pid_t)-1, &wait_status, WNOHANG);
#else
		wait_status = 0;
		done = 0;
#endif
		if (done == pid) {
#ifndef IS_MINGW
		    exit_status = wait_status;
#endif
		    break;
		}
		if (done > 0) {
		    continue;
		}
		if (slept) {
		    break;
		}
		/* give PMDA a chance to notice the close() and exit */
		sleep(1);
		slept = 1;
	    }
	}
    }
#ifndef IS_MINGW
    if (exit_status != -1) {
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
    }
#endif
    fputc('\n', stderr);
    aPtr->reason = reason;
    aPtr->status.connected = 0;
    aPtr->status.busy = 0;
    aPtr->status.notReady = 0;
    aPtr->status.flags = 0;
    AgentDied = 1;

    if (_pmcd_trace_mask)
	pmcd_dump_trace(stderr);

    MarkStateChanges(PMCD_DROP_AGENT);
}

/* Wait up to total secs for agents to terminate.
 * Return 0 if all terminate, else -1
 */
int 
HarvestAgents(unsigned int total)
{
    int		i;
    int		sts;
    int		found;
    pid_t	pid;
    AgentInfo	*ap;

    /*
     * Check for child process termination.  Be careful, and ignore any
     * non-agent processes found.
     */
    do {
#if defined(HAVE_WAIT3)
	pid = wait3(&sts, WNOHANG, NULL);
#elif defined(HAVE_WAITPID)
	pid = waitpid((pid_t)-1, &sts, WNOHANG);
#else
	break;
#endif
	found = 0;
	for ( i = 0; i < nAgents; i++) {
	    ap = &agent[i];
	    if (!ap->status.connected || ap->ipcType == AGENT_DSO)
		continue;

	    found = 1;
	    if (pid <= (pid_t)0) {
		if (total--) {
		    sleep(1);
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
