/*
 * Create privileged PMDAs on behalf of PMCD.
 *
 * Copyright (c) 2015-2016 Red Hat.
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
#include "root.h"
#if defined(HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif

int
root_agent_wait(int *status)
{
#if defined(HAVE_WAIT3)
    return wait3(status, WNOHANG, NULL);
#elif defined(HAVE_WAITPID)
    return waitpid((pid_t)-1, status, WNOHANG);
#else
    status = 0;
    return 0;
#endif
}

#ifndef IS_MINGW
int
root_create_agent(int ipctype, char *argv, char *label, int *infd, int *outfd)
{
    int		i = 0, j;
    int		inPipe[2];	/* Pipe for input to child */
    int		outPipe[2];	/* For output to child */
    pid_t	childPid = (pid_t)-1;
    char	*transfer_argv;
    char	*transfer_final[MAXPATHLEN] = { "" };
    const char	*delim = " ";

    *infd = -1;
    *outfd = -1;
    __pmNotifyErr(LOG_INFO, "Starting %s agent: %s", label, argv);

    transfer_argv = strtok(argv, delim);
    if (transfer_argv == NULL)
	return (pid_t)-1;

    transfer_final[i++] = transfer_argv;
    do {
	transfer_argv = strtok(NULL, delim);
	if (transfer_argv == NULL)
	    break;
	transfer_final[i++] = transfer_argv;
    } while (transfer_argv != NULL);

    if (pmDebugOptions.appl1) {
	for (j = 0; j < i; j++)
	    __pmNotifyErr(LOG_DEBUG, "arg[%d] %s", j, transfer_final[j]);
    }

    if (ipctype == ROOT_AGENT_PIPE) {
	if (pipe1(inPipe) < 0) {
	    __pmNotifyErr(LOG_ERR,
		    "%s: input pipe create failed for \"%s\" agent: %s\n",
		    pmProgname, label, osstrerror());
	    return (pid_t)-1;
	}
	if (pipe1(outPipe) < 0) {
	    __pmNotifyErr(LOG_ERR,
		    "%s: output pipe create failed for \"%s\" agent: %s\n",
		    pmProgname, label, osstrerror());
	    close(inPipe[0]);
	    close(inPipe[1]);
	    return (pid_t)-1;
	}
	if (outPipe[1] > root_maximum_fd)
	    root_maximum_fd = outPipe[1];
    }
    if (transfer_final != NULL) { /* Start a new agent if required */
	childPid = fork();
	if (childPid == (pid_t)-1) {
	    if (ipctype == ROOT_AGENT_PIPE) {
		close(inPipe[0]);
		close(inPipe[1]);
		close(outPipe[0]);
		close(outPipe[1]);
	    }
	    return (pid_t)-1;
	}
	if (childPid) {
	    /* This is the parent (pmdaroot) */
	    if (ipctype == ROOT_AGENT_PIPE) {
		close(inPipe[0]);
		close(outPipe[1]);
		*infd = inPipe[1];
		*outfd = outPipe[0];
	    }
	}
	else {
	    /*
	     * This is the child (new agent)
	     * make sure stderr is fd 2
	     */
	    dup2(fileno(stderr), STDERR_FILENO); 
	    if (ipctype == ROOT_AGENT_PIPE) {
		/* make pipe stdin for PMDA */
		dup2(inPipe[0], STDIN_FILENO);
		/* make pipe stdout for PMDA */
		dup2(outPipe[1], STDOUT_FILENO);
	    }
	    else {
		/*
		 * not a pipe, close stdin and attach stdout to stderr
		 */
		close(STDIN_FILENO);
		dup2(STDERR_FILENO, STDOUT_FILENO);
	    }
	    for (i = 0; i <= root_maximum_fd; i++) {
		/* Close all except std{in,out,err} */
		if (i == STDIN_FILENO ||
		    i == STDOUT_FILENO ||
		    i == STDERR_FILENO)
		    continue;
		close(i);
	    }

	    execvp(transfer_final[0], transfer_final);
	    /* botch if reach here */
	    fprintf(stderr, "%s: error starting %s: %s\n",
		    pmProgname, transfer_final[0], osstrerror());
	    /* avoid atexit() processing, so _exit not exit */
	    _exit(1);
	}
    }
    if (pmDebugOptions.appl0) {
	__pmNotifyErr(LOG_DEBUG, "Started %s agent pid=%d infd=%d outfd=%d\n",
			label, childPid, *infd, *outfd);
    }
    return childPid;
}
#else
int
root_create_agent(int ipctype, char *argv, char *label, int *infd, int *outfd)
{
    (void)ipctype;
    (void)argv;
    (void)label;
    (void)infd;
    (void)outfd;
    return (pid_t)-EOPNOTSUPP;
}
#endif
