/*
 * Utility functions for the pipe PMDA.
 *
 * Copyright (c) 2015 Red Hat.
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
#include "util.h"
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#define PARENT_END 0			/* parent end of the pipe */
#define CHILD_END 1			/* child end of the pipe */
#define STDOUT_FD 1			/* stdout fd */

int
start_cmd(const char *cmd, const char *usr, pid_t *ppid)
{
    pid_t	child_pid;
    int		i, pipe_fds[2];
#if !defined(HAVE_PIPE2)
    int		sts;
#endif

    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_INFO, "%s: Trying to run command: %s", __FUNCTION__,
		  cmd);

    /* Create the pipes. */
#if defined(HAVE_PIPE2)
    if ((pipe2(pipe_fds, O_CLOEXEC|O_NONBLOCK)) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: pipe2() returned %s", __FUNCTION__,
		      strerror(errno));
	return -1;
    }
#else
    if ((sts = pipe(pipe_fds)) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: pipe() returned %s", __FUNCTION__,
		      strerror(errno));
	return -1;
    }

    /* Set the right flags on the pipes. */
    if (fcntl(pipe_fds[PARENT_END], F_SETFL, O_NDELAY) < 0 ||
	fcntl(pipe_fds[CHILD_END], F_SETFL, O_NDELAY) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: fcntl() returned %s", __FUNCTION__,
		      strerror(errno));
	return -1;
    }
    if (fcntl(pipe_fds[PARENT_END], F_SETFD, O_CLOEXEC) < 0
	|| fcntl(pipe_fds[CHILD_END], F_SETFD, O_CLOEXEC) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: fcntl() returned %s", __FUNCTION__,
		      strerror(errno));
	return -1;
    }
#endif

    /* Create the new process. */
    child_pid = fork();
    if (child_pid == 0) {		/* child process */
	/*
	 * Duplicate our pipe fd onto stdout of the child. Note that
	 * this clears O_CLOEXEC, so the new stdout should stay open
	 * when we call exec().
	 */
	if (pipe_fds[CHILD_END] != STDOUT_FD) {
	    if (dup2(pipe_fds[CHILD_END], STDOUT_FD) < 0) {
		__pmNotifyErr(LOG_ERR, "%s: dup2() returned %s", __FUNCTION__,
			      strerror(errno));
		_exit(127);
	    }
	}

	/* Close all other fds. */
	for (i = 0; i <= pipe_fds[CHILD_END]; i++) {
	    if (i != STDOUT_FD)
		close(i);
	}

	/* Switch to user account under which command is to run. */
	__pmSetProcessIdentity(usr);

	/* Actually run the command. */
	execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
	_exit(127);
    }
    else if (child_pid > 0) {		/* parent process */
	close(pipe_fds[CHILD_END]);
	if (ppid != NULL)
	    *ppid = child_pid;
    }
    else if (child_pid < 0) {		/* fork error */
	__pmNotifyErr(LOG_ERR, "%s: fork() returned %s", __FUNCTION__,
		      strerror(errno));
	close(pipe_fds[PARENT_END]);
	close(pipe_fds[CHILD_END]);
	return -1;
    }

    return pipe_fds[PARENT_END];
}

int
stop_cmd(pid_t pid)
{
    pid_t	wait_pid;
    int		sts, wstatus = 0;

    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_INFO, "stop_cmd: killing pid %" FMT_PID, pid);

    /* Send the TERM signal. */
    sts = kill(pid, SIGTERM);

    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_INFO, "stop_cmd: kill returned %d", sts);

    /* Wait for the process to go away. */
    do {
	wait_pid = waitpid(pid, &wstatus, 0);
	__pmNotifyErr(LOG_INFO, "stop_cmd: waitpid returned %d", (int)wait_pid);
    } while (wait_pid == -1 && errno == EINTR);

    /* Return process exit status. */
    return wstatus;
}

int
wait_cmd(pid_t pid)
{
    pid_t	wait_pid;
    int		wstatus = 0;

    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_INFO, "wait_cmd: checking pid %" FMT_PID, pid);

    /* Check whether the process has gone away. */
    wait_pid = waitpid(pid, &wstatus, WNOHANG);

    if (pmDebug & DBG_TRACE_APPL0)
	__pmNotifyErr(LOG_INFO, "wait_cmd: waitpid returned %d", (int)wait_pid);

    /* Return process status. */
    return wait_pid == pid ? wstatus : -1;
}
