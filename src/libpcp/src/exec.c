/*
 * pmProcessExec() and friends.
 *
 * Copyright (c) 2017 Ken McDonell.  All Rights Reserved.
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
 *
 * Thread-safe notes
 *	TODO
 */

#include <stdarg.h>
#include <sys/stat.h> 

#include "pmapi.h"
#include "impl.h"
#include "internal.h"
#include "fault.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

struct __pmExecCtl {
    int		argc;
    char	**argv;
};

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	exec_lock;
#else
void			*exec_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == exec_lock
 */
int
__pmIsExecLock(void *lock)
{
    return lock == (void *)&exec_lock;
}
#endif

void
init_exec_lock(void)
{
    __pmInitMutex(&exec_lock);
}

/* Cleanup after error or after __pmExecCtl_t has been used. */
static void
cleanup(__pmExecCtl_t *ep)
{
    int		i;

    if (ep == NULL)
	return;
    if (ep->argv != NULL) {
	for (i = 0; i < ep->argc; i++) {
	    if (ep->argv[i] != NULL)
		free(ep->argv[i]);
	}
	free(ep->argv);
    }
    free(ep);
}

/*
 * Built array of arguments for use in pmProcessExec() or
 * pmProcessPipe()
 */
int
__pmProcessAddArg(__pmExecCtl_t **handle, char *arg)
{
    __pmExecCtl_t	*ep;
    char		**tmp_argv;

    if (*handle == NULL) {
	PM_LOCK(exec_lock);
	/* first call in a sequence */
	if ((ep = (__pmExecCtl_t *)malloc(sizeof(__pmExecCtl_t))) == NULL) {
	    __pmNoMem("pmProcessAddArg: __pmExecCtl_t malloc", sizeof(__pmExecCtl_t), PM_RECOV_ERR);
	    PM_UNLOCK(exec_lock);
	    return -ENOMEM;
	}
	ep->argc = 0;
PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_ALLOC);
	if ((ep->argv = (char **)malloc(sizeof(ep->argv[0]))) == NULL) {
	    __pmNoMem("pmProcessAddArg: argv malloc", sizeof(ep->argv[0]), PM_RECOV_ERR);
	    cleanup(ep);
	    *handle = NULL;
	    PM_UNLOCK(exec_lock);
	    return -ENOMEM;
	}
    }
    else
	ep = *handle;

PM_FAULT_POINT("libpcp/" __FILE__ ":2", PM_FAULT_ALLOC);
    /*
     * ep->argc+2 ... +1 for this one, +2 for NULL @ end of args
     */
    if ((tmp_argv = (char **)realloc(ep->argv, sizeof(ep->argv[0])*(ep->argc+2))) == NULL) {
	__pmNoMem("pmProcessAddArg: argv realloc", sizeof(ep->argv[0])*(ep->argc+2), PM_RECOV_ERR);
	cleanup(ep);
	*handle = NULL;
	PM_UNLOCK(exec_lock);
	return -ENOMEM;
    }
    else
	ep->argv = tmp_argv;

    ep->argc++;

PM_FAULT_POINT("libpcp/" __FILE__ ":3", PM_FAULT_ALLOC);
    if ((ep->argv[ep->argc-1] = strdup(arg)) == NULL) {
	__pmNoMem("pmProcessAddArg: arg strdup", strlen(arg)+1, PM_RECOV_ERR);
	ep->argc--;
	cleanup(ep);
	PM_UNLOCK(exec_lock);
	*handle = NULL;
	return -ENOMEM;
    }

    *handle = ep;
    return 0;
}

/*
 * Like system(3), but uses execvp() and the array of args built
 * by __pmProcessAddArg().
 *
 * If toss&PM_EXEC_TOSS_STDIN, reassign stdin to /dev/null.
 * If toss&PM_EXEC_TOSS_STDOUT, reassign stdout to /dev/null.
 * If toss&PM_EXEC_TOSS_STDERR, reassign stderr to /dev/null.
 *
 * If wait == PM_EXEC_WAIT, wait for the child process to exit and
 * return 0 if exit status is 0, else return -1 and the status
 * from waitpid() is returned via status.
 * Otherwise, don't wait and return 0.
 *
 * If the fork() fails before we even get to the execvp(), return
 * -errno.
 */
int
__pmProcessExec(__pmExecCtl_t **handle, int toss, int wait, int *status)
{
    __pmExecCtl_t	*ep = *handle;
    int			i;
    pid_t		pid;
    int			sts;

    if (ep == NULL)
	/* no executable path or args */
	return PM_ERR_TOOSMALL;

    if (pmDebugOptions.exec) {
	fprintf(stderr, "pmProcessExec: argc=%d", ep->argc);
	for (i = 0; i < ep->argc; i++)
	    fprintf(stderr, " \"%s\"", ep->argv[i]);
	fputc('\n', stderr);
    }

    ep->argv[ep->argc] = NULL;

    /* fork-n-exec and (maybe) wait */
    pid = fork();

    /* begin fault-injection block */
    PM_FAULT_POINT("libpcp/" __FILE__ ":4", PM_FAULT_MISC);
    if (PM_FAULT_CHECK(PM_FAULT_MISC)) {
	PM_FAULT_CLEAR;
	if (pid > (pid_t)0)
	    kill(pid, SIGKILL);
	setoserror(EAGAIN);
	pid = -1;
    }
    /* end fault-injection block */

    if (pid == (pid_t)0) {
	/* child */
	char	*p;
	char	*path;
	path = ep->argv[0];
	p = &path[strlen(ep->argv[0])-1];
	while (p > ep->argv[0]) {
	    p--;
	    if (*p == '/') {
		path = &p[1];
		break;
	    }
	}
	if (toss & PM_EXEC_TOSS_STDIN)
	    freopen("/dev/null", "r", stdin);
	if (toss & PM_EXEC_TOSS_STDOUT)
	    freopen("/dev/null", "w", stdout);
	if (toss & PM_EXEC_TOSS_STDERR)
	    freopen("/dev/null", "w", stderr);
	sts = execvp(path, (char * const *)ep->argv);
	/* oops, not supposed to get here */
	exit(127);
    }
    else if (pid > (pid_t)0) {
	/* parent */
	if (wait == PM_EXEC_WAIT) {
	    pid_t	wait_pid;
	    wait_pid = waitpid(pid, status, 0);
	    if (wait_pid == pid) {
		if (WIFEXITED(*status)) {
		    sts = WEXITSTATUS(*status);
		    if (sts != 0)
			sts = -1;
		}
		else
		    sts = -1;
	    }
	    else
		sts = -oserror();
	}
	else
	    sts = 0;
    }
    else
	sts = -oserror();

    /* cleanup the malloc'd control structures */
    cleanup(ep);
    PM_UNLOCK(exec_lock);
    *handle = NULL;

    return sts;
}

/*
 * Like popen(3), but uses execvp() and the array of args built
 * __pmProcessAddArg().
 */
FILE *
__pmProcessPipe(__pmExecCtl_t **handle, const char *type, int *status)
{
    return NULL;	/* NYI */
}

/*
 * Like pclose(3), but pipe created by pmProcessPipe()
 */
int
__pmProcessPipeClose(FILE *stream)
{
    return PM_ERR_NYI;
}
