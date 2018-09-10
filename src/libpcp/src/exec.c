/*
 * __pmProcessExec() and friends.
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
 *	- map and nmap protected by exec_lock
 *	- we're super conservative and do not allow nesting or
 *	  concurrent execution of __pmProcessAddArg...__pmProcessExec
 *	  or __pmProcessAddArg...__pmProcessPipe sequences ... this
 *	  protects the __pmExecCtl structure behind the opaque handle
 *	  from accidental damage
 */

#include <stdarg.h>
#include <sys/stat.h> 

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"
#include "fault.h"
#include <sys/types.h>
#if defined(HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif
#include <signal.h>
#include <ctype.h>
#include <strings.h>

struct __pmExecCtl {
    int		argc;
    char	**argv;
};

static struct map_s {
    pid_t	pid;
    FILE	*fp;
} *map = NULL;

static int		nmap = 0;

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
#ifdef PM_MULTI_THREAD
    __pmInitMutex(&exec_lock);
#endif
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
 * our encoding of wait status into PMAPI return value >= 0
 */
#if !defined(IS_MINGW)
static int
decode_status(int status)
{
    if (WIFEXITED(status))
	return WEXITSTATUS(status);
    else {
	if (WIFSIGNALED(status))
	    return 1000 + WTERMSIG(status);
	else {
	    if (WIFSTOPPED(status))
		return 1000 + WSTOPSIG(status);
	    else
		return 2000;
	}
    }
}
#endif

/*
 * Built array of arguments for use in __pmProcessExec() or
 * __pmProcessPipe()
 */
int
__pmProcessAddArg(__pmExecCtl_t **handle, const char *arg)
{
    __pmExecCtl_t	*ep;
    char		**tmp_argv;

    __pmInitLocks();

    if (*handle == NULL) {
	PM_LOCK(exec_lock);
	/* first call in a sequence */
	if ((ep = (__pmExecCtl_t *)malloc(sizeof(__pmExecCtl_t))) == NULL) {
	    pmNoMem("__pmProcessAddArg: __pmExecCtl_t malloc", sizeof(__pmExecCtl_t), PM_RECOV_ERR);
	    PM_UNLOCK(exec_lock);
	    return -ENOMEM;
	}
	ep->argc = 0;
PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_ALLOC);
	if ((ep->argv = (char **)malloc(sizeof(ep->argv[0]))) == NULL) {
	    pmNoMem("__pmProcessAddArg: argv malloc", sizeof(ep->argv[0]), PM_RECOV_ERR);
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
	pmNoMem("__pmProcessAddArg: argv realloc", sizeof(ep->argv[0])*(ep->argc+2), PM_RECOV_ERR);
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
	pmNoMem("__pmProcessAddArg: arg strdup", strlen(arg)+1, PM_RECOV_ERR);
	ep->argc--;
	cleanup(ep);
	PM_UNLOCK(exec_lock);
	*handle = NULL;
	return -ENOMEM;
    }

    *handle = ep;
    return 0;
}

#if !defined(IS_MINGW)
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
 * If the fork() fails before we even get to the execvp(), or 
 * something bad happens in signal land, return -errno.
 *
 * According to POSIX ...
 *	The system() function shall ignore the SIGINT and SIGQUIT
 *	signals, and shall block the SIGCHLD signal, while waiting
 *	for the command to terminate
 * so we do the same.
 */
int
__pmProcessExec(__pmExecCtl_t **handle, int toss, int wait)
{
    __pmExecCtl_t	*ep = *handle;
    int			i;
    pid_t		pid;
    int			sts;
    int			status;
    struct sigaction	ignore;
    struct sigaction	save_intr;
    struct sigaction	save_quit;
    sigset_t		mask;
    sigset_t		save_mask;

    if (ep == NULL)
	/* no executable path or args */
	return PM_ERR_TOOSMALL;

    if (pmDebugOptions.exec) {
	fprintf(stderr, "__pmProcessExec: argc=%d toss=%d", ep->argc, toss);
	for (i = 0; i < ep->argc; i++)
	    fprintf(stderr, " \"%s\"", ep->argv[i]);
	fputc('\n', stderr);
    }

    /* ignore SIGINT and SIGQUIT, block SIGCHLD */
    ignore.sa_handler = SIG_IGN;
    ignore.sa_flags = 0;
    sigemptyset(&ignore.sa_mask);
    sigemptyset(&save_intr.sa_mask);
    if (sigaction(SIGINT, &ignore, &save_intr) < 0)
	return -oserror();
    sigemptyset(&save_quit.sa_mask);
    if (sigaction(SIGQUIT, &ignore, &save_quit) < 0)
	return -oserror();
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &mask, &save_mask) < 0)
	return -oserror();

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
	char	*name;
	int	namelen;

	/*
	 * restore SIGINT and SIGQUIT actions and signal mask
	 * -- handling possible errors here is not much help
	 */
	sigaction(SIGINT, &save_intr, NULL);
	sigaction(SIGQUIT, &save_quit, NULL);
	sigprocmask(SIG_SETMASK, &save_mask, NULL);

	name = path = ep->argv[0];
	p = &path[strlen(ep->argv[0])-1];
	/* strip leading part path from argv[0] */
	while (p > ep->argv[0]) {
	    p--;
	    if (*p == '/') {
		name = &p[1];
		break;
	    }
	}
	/* be careful, argv[0] is malloc'd ... don't just clobber argv[0] */
	namelen = strlen(name)+1;
	if ((name = strdup(name)) == NULL) {
	    pmNoMem("__pmProcessExec: name strdup", namelen, PM_RECOV_ERR);
	    cleanup(ep);
	    PM_UNLOCK(exec_lock);
	    *handle = NULL;
	    return -ENOMEM;
	}
	/* still hold ref to argv[0] via path */
	ep->argv[0] = name;
	if (toss & PM_EXEC_TOSS_STDIN) {
	    if (freopen("/dev/null", "r", stdin) == NULL)
		fprintf(stderr, "__pmProcessExec: freopen stdin failed\n");
	}
	if (toss & PM_EXEC_TOSS_STDOUT) {
	    if (freopen("/dev/null", "w", stdout) == NULL)
		fprintf(stderr, "__pmProcessExec: freopen stdout failed\n");
	}
	if (toss & PM_EXEC_TOSS_STDERR) {
	    if ((freopen("/dev/null", "w", stderr)) == NULL)
		/* cannot always safely write to stderr if freopen fails; ignore */
		;
	}

	execvp(path, (char * const *)ep->argv);
	/* oops, not supposed to get here */
	cleanup(ep);	/* for valgrind */
	exit(127);
    }

    /* cleanup on the parent (caller) side */
    cleanup(ep);
    PM_UNLOCK(exec_lock);
    *handle = NULL;

    if (pid > (pid_t)0) {
	/* parent */

	if (wait == PM_EXEC_WAIT) {
	    pid_t	wait_pid;
	    while ((wait_pid = waitpid(pid, &status, 0)) < 0) {
		if (oserror() != EINTR)
		    break;
	    }
	    if (pmDebugOptions.exec) {
		fprintf(stderr, "__pmProcessExec: pid=%" FMT_PID " wait_pid=%" FMT_PID , pid, wait_pid);
		if (wait_pid < 0) fprintf(stderr, " errno=%d", oserror());
		if (WIFEXITED(status)) fprintf(stderr, " exit=%d", WEXITSTATUS(status));
		if (WIFSIGNALED(status)) fprintf(stderr, " signal=%d", WTERMSIG(status));
		if (WIFSTOPPED(status)) fprintf(stderr, " stop signal=%d", WSTOPSIG(status));
#ifdef WIFCONTINUED
		if (WIFCONTINUED(status)) fprintf(stderr, " continued");
#endif
		if (WCOREDUMP(status)) fprintf(stderr, " core dumped");
		fputc('\n', stderr);
	    }
	    if (wait_pid == pid)
		sts = decode_status(status);
	    else
		sts = -oserror();
	}
	else
	    sts = 0;

	/*
	 * restore SIGINT and SIGQUIT actions and signal mask
	 */
	if (sigaction(SIGINT, &save_intr, NULL) < 0)
	    return -oserror();
	if (sigaction(SIGQUIT, &save_quit, NULL) < 0)
	    return -oserror();
	if (sigprocmask(SIG_SETMASK, &save_mask, NULL) < 0)
	    return -oserror();
    }
    else
	sts = -oserror();

    return sts;
}
#else
/*
 * MinGW version
 */
int
__pmProcessExec(__pmExecCtl_t **handle, int toss, int wait)
{
    __pmExecCtl_t	*ep = *handle;
    int			i;
    int			status;
    int			sig;
    int			sts = 0;
    pid_t		pid;
    pid_t		wait_pid;

    if (ep == NULL)
	/* no executable path or args */
	return PM_ERR_TOOSMALL;

    if (pmDebugOptions.exec) {
	fprintf(stderr, "__pmProcessExec: argc=%d toss=%d", ep->argc, toss);
	for (i = 0; i < ep->argc; i++)
	    fprintf(stderr, " \"%s\"", ep->argv[i]);
	fputc('\n', stderr);
    }

    ep->argv[ep->argc] = NULL;

    pid = __pmProcessCreate(ep->argv, NULL, NULL);

    /* cleanup */
    cleanup(ep);
    PM_UNLOCK(exec_lock);
    *handle = NULL;

    if (pid > (pid_t)0) {
	/* not in the child process, so can't do too much here */
	if (toss & PM_EXEC_TOSS_STDIN)
	    ;
	if (toss & PM_EXEC_TOSS_STDOUT)
	    ;
	if (wait == PM_EXEC_WAIT) {
	    wait_pid = __pmProcessWait(pid, 0, &status, &sig);
	    if (pmDebugOptions.exec) {
		fprintf(stderr, "__pmProcessExec: pid=%" FMT_PID " wait_pid=%" FMT_PID , pid, wait_pid);
		if (wait_pid < 0) fprintf(stderr, " errno=%d", oserror());
		fprintf(stderr, " status=%d signal=%d\n", status, sig);
	    }
	    if (wait_pid != pid)
		sts = -oserror();
	    else {
		if (sig != -1)
		    sts = 1000 + sig; 
		else
		    sts = status;
	    }
	}
    }
    else
	sts = -oserror();

    return sts;
}
#endif

#if !defined(IS_MINGW)
/*
 * Like popen(3), but uses execvp() and the array of args built
 * __pmProcessAddArg().
 */
int
__pmProcessPipe(__pmExecCtl_t **handle, const char *type, int toss, FILE **fp)
{
    __pmExecCtl_t	*ep = *handle;
    int			i;
    int			sts = 0;
    pid_t		pid;
    int			mypipe[2];
    struct sigaction	ignore;
    struct sigaction	save_intr;
    struct sigaction	save_quit;
    sigset_t		mask;
    sigset_t		save_mask;

    *fp = NULL;

    if (ep == NULL)
	/* no executable path or args */
	return PM_ERR_TOOSMALL;

    if (pmDebugOptions.exec) {
	fprintf(stderr, "__pmProcessPipe: argc=%d type=\"%s\" toss=%d", ep->argc, type, toss);
	for (i = 0; i < ep->argc; i++)
	    fprintf(stderr, " \"%s\"", ep->argv[i]);
	fputc('\n', stderr);
    }

    if (strlen(type) != 1 || (type[0] != 'r' && type[0] != 'w' )) {
	PM_UNLOCK(exec_lock);
	return -EINVAL;
    }

    /* ignore SIGINT and SIGQUIT, block SIGCHLD */
    ignore.sa_handler = SIG_IGN;
    ignore.sa_flags = 0;
    sigemptyset(&ignore.sa_mask);
    sigemptyset(&save_intr.sa_mask);
    if (sigaction(SIGINT, &ignore, &save_intr) < 0) {
	PM_UNLOCK(exec_lock);
	return -oserror();
    }
    sigemptyset(&save_quit.sa_mask);
    if (sigaction(SIGQUIT, &ignore, &save_quit) < 0) {
	PM_UNLOCK(exec_lock);
	return -oserror();
    }
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &mask, &save_mask) < 0) {
	PM_UNLOCK(exec_lock);
	return -oserror();
    }

    ep->argv[ep->argc] = NULL;

    sts = pipe(mypipe);

    /* begin fault-injection block */
    PM_FAULT_POINT("libpcp/" __FILE__ ":5", PM_FAULT_MISC);
    if (PM_FAULT_CHECK(PM_FAULT_MISC)) {
	PM_FAULT_CLEAR;
	if (sts >= 0) {
	    close(mypipe[0]);
	    close(mypipe[1]);
	}
	sts = -1;
	setoserror(ENFILE);
    }
    /* end fault-injection block */

    if (sts < 0) {
	/* pipe failed ... */
	cleanup(ep);
	*handle = NULL;
	PM_UNLOCK(exec_lock);
	return -oserror();
    }

    /* fork-n-exec and (maybe) wait */
    pid = fork();

    /* begin fault-injection block */
    PM_FAULT_POINT("libpcp/" __FILE__ ":6", PM_FAULT_MISC);
    if (PM_FAULT_CHECK(PM_FAULT_MISC)) {
	PM_FAULT_CLEAR;
	if (pid > (pid_t)0)
	    kill(pid, SIGKILL);
	setoserror(ENOSYS);
	pid = -1;
    }
    /* end fault-injection block */

    if (pid == (pid_t)0) {
	/* child */
	char	*p;
	char	*path;
	char	*name;
	int	namelen;

	/*
	 * restore SIGINT and SIGQUIT actions and signal mask
	 * -- handling possible errors here is not much help
	 */
	sigaction(SIGINT, &save_intr, NULL);
	sigaction(SIGQUIT, &save_quit, NULL);
	sigprocmask(SIG_SETMASK, &save_mask, NULL);

	if (type[0] == 'r') {
	    close(mypipe[0]);
	    dup2(mypipe[1], fileno(stdout));
	    close(mypipe[1]);
	    if (toss & PM_EXEC_TOSS_STDIN) {
		if (freopen("/dev/null", "r", stdin) == NULL)
		    fprintf(stderr, "__pmProcessPipe: freopen stdin failed\n");
	    }
	}
	else {	/* can safely assume 'w' */
	    close(mypipe[1]);
	    dup2(mypipe[0], fileno(stdin));
	    close(mypipe[0]);
	    if (toss & PM_EXEC_TOSS_STDOUT) {
		if (freopen("/dev/null", "w", stdout) == NULL)
		    fprintf(stderr, "__pmProcessPipe: freopen stdout failed\n");
	    }
	}

	name = path = ep->argv[0];
	p = &path[strlen(ep->argv[0])-1];
	/* strip leading part path from argv[0] */
	while (p > ep->argv[0]) {
	    p--;
	    if (*p == '/') {
		name = &p[1];
		break;
	    }
	}
	/* be careful, argv[0] is malloc'd ... don't just clobber argv[0] */
	namelen = strlen(name)+1;
	if ((name = strdup(name)) == NULL) {
	    pmNoMem("__pmProcessPipe: name strdup", namelen, PM_RECOV_ERR);
	    cleanup(ep);
	    PM_UNLOCK(exec_lock);
	    *handle = NULL;
	    return -ENOMEM;
	}
	/* still hold ref to argv[0] via path */
	ep->argv[0] = name;
	if (toss & PM_EXEC_TOSS_STDERR) {
	    if ((freopen("/dev/null", "w", stderr)) == NULL)
		/* cannot always safely write to stderr if freopen fails; ignore */
		;
	}

	execvp(path, (char * const *)ep->argv);
	/* oops, not supposed to get here */
	cleanup(ep);	/* for valgrind */
	exit(127);
    }

    /* cleanup on the parent (caller) side */
    cleanup(ep);
    *handle = NULL;

    if (pid > (pid_t)0) {
	/* parent */

	for (i = 0; i < nmap; i++) {
	    if (map[i].pid == 0)
		break;
	}
	if (i == nmap) {
	    struct map_s	*tmp_map;
PM_FAULT_POINT("libpcp/" __FILE__ ":7", PM_FAULT_ALLOC);
	    if ((tmp_map = (struct map_s *)realloc(map, sizeof(map[0])*(nmap+1))) == NULL) {
		/*
		 * we have nowhere to stash the fp and pid, which will
		 * cause problems for __pmProcessPipeClose(), but it will
		 * just return early with an error without waiting
		 */
		pmNoMem("__pmProcessPipe: argv realloc", sizeof(map[0])*(nmap+1), PM_RECOV_ERR);
		PM_UNLOCK(exec_lock);
		return -ENOMEM;
	    }
	    map = tmp_map;
	    map[i].pid = 0;
	    map[i].fp = NULL;
	    nmap++;
	}

	if (type[0] == 'r') {
	    close(mypipe[1]);
	    if ((*fp = fdopen(mypipe[0], "r")) == NULL) {
		PM_UNLOCK(exec_lock);
		return -oserror();
	    }
	}
	else {	/* can safely assume 'w' */
	    close(mypipe[0]);
	    if ((*fp = fdopen(mypipe[1], "w")) == NULL) {
		PM_UNLOCK(exec_lock);
		return -oserror();
	    }
	}

	map[i].pid = pid;
	map[i].fp = *fp;
	PM_UNLOCK(exec_lock);

	/*
	 * restore SIGINT and SIGQUIT actions and signal mask
	 */
	if (sigaction(SIGINT, &save_intr, NULL) < 0)
	    return -oserror();
	if (sigaction(SIGQUIT, &save_quit, NULL) < 0)
	    return -oserror();
	if (sigprocmask(SIG_SETMASK, &save_mask, NULL) < 0)
	    return -oserror();

	return 0;
    }
    else {
	PM_UNLOCK(exec_lock);
	return -oserror();
    }
}
#else
/*
 * MinGW version
 */
int
__pmProcessPipe(__pmExecCtl_t **handle, const char *type, int toss, FILE **fp)
{
    __pmExecCtl_t	*ep = *handle;
    int			i;
    int			fromChild;
    int			toChild;
    int			sts = 0;
    pid_t		pid;

    *fp = NULL;

    if (ep == NULL)
	/* no executable path or args */
	return PM_ERR_TOOSMALL;

    if (pmDebugOptions.exec) {
	fprintf(stderr, "__pmProcessPipe: argc=%d type=\"%s\" toss=%d", ep->argc, type, toss);
	for (i = 0; i < ep->argc; i++)
	    fprintf(stderr, " \"%s\"", ep->argv[i]);
	fputc('\n', stderr);
    }

    if (strlen(type) != 1 || (type[0] != 'r' && type[0] != 'w' )) {
	PM_UNLOCK(exec_lock);
	return -EINVAL;
    }

    ep->argv[ep->argc] = NULL;

    pid = __pmProcessCreate(ep->argv, &fromChild, &toChild);

    /* cleanup */
    cleanup(ep);
    *handle = NULL;

    if (pid > (pid_t)0) {
	for (i = 0; i < nmap; i++) {
	    if (map[i].pid == 0)
		break;
	}
	if (i == nmap) {
	    struct map_s	*tmp_map;
	    if ((tmp_map = (struct map_s *)realloc(map, sizeof(map[0])*(nmap+1))) == NULL) {
		/*
		 * we have nowhere to stash the fp and pid, which will
		 * cause problems for __pmProcessPipeClose(), but it will
		 * just return early with an error without waiting
		 */
		pmNoMem("__pmProcessPipe: argv realloc", sizeof(map[0])*(nmap+1), PM_RECOV_ERR);
		PM_UNLOCK(exec_lock);
		return -ENOMEM;
	    }
	    map = tmp_map;
	    map[i].pid = 0;
	    map[i].fp = NULL;
	    nmap++;
	}

	/*
	 * Note:
	 * 	The C fd's are over Windows pipes which have shared state
	 * 	... you cannot close the fd you're no using or the process
	 * 	at the other end will not be able to use their end of the
	 * 	pipe.  So no close()'s here.
	 */
	if (type[0] == 'r') {
	    if ((*fp = fdopen(fromChild, "r")) == NULL) {
		PM_UNLOCK(exec_lock);
		return -oserror();
	    }
	}
	else {	/* can safely assume 'w' */
	    if ((*fp = fdopen(toChild, "w")) == NULL) {
		PM_UNLOCK(exec_lock);
		return -oserror();
	    }
	}

	map[i].pid = pid;
	map[i].fp = *fp;
    }
    else
	sts = -EPIPE;

    PM_UNLOCK(exec_lock);

    return sts;
}
#endif

/*
 * Like pclose(3), but pipe created by __pmProcessPipe()
 */
int
__pmProcessPipeClose(FILE *fp)
{
    int			i;
    int			sts = 0;
    int			status;
    pid_t		pid;
    pid_t		wait_pid;
#if defined(IS_MINGW)
    int			sig;
#endif

    PM_LOCK(exec_lock);

    for (i = 0; i < nmap; i++) {
	if (map[i].fp == fp)
	    break;
    }

    if (i == nmap) {
	/* not one of our fp's or pipe never got started */
	PM_UNLOCK(exec_lock);
	return PM_ERR_NOTCONN;
    }

    pid = map[i].pid;
    map[i].pid = 0;
    map[i].fp = NULL;
    PM_UNLOCK(exec_lock);

    if (fclose(fp) != 0)
	return -oserror();

#if !defined(IS_MINGW)
    while ((wait_pid = waitpid(pid, &status, 0)) < 0) {
	if (oserror() != EINTR)
	    break;
    }

    if (pmDebugOptions.exec) {
	fprintf(stderr, "__pmProcessPipeClose: pid=%" FMT_PID " wait_pid=%" FMT_PID , pid, wait_pid);
	if (wait_pid < 0) fprintf(stderr, " errno=%d", oserror());
	if (WIFEXITED(status)) fprintf(stderr, " exit=%d", WEXITSTATUS(status));
	if (WIFSIGNALED(status)) fprintf(stderr, " signal=%d", WTERMSIG(status));
	if (WIFSTOPPED(status)) fprintf(stderr, " stop signal=%d", WSTOPSIG(status));
#ifdef WIFCONTINUED
	if (WIFCONTINUED(status)) fprintf(stderr, " continued");
#endif
	if (WCOREDUMP(status)) fprintf(stderr, " core dumped");
	fputc('\n', stderr);
    }

    if (wait_pid == pid)
	sts = decode_status(status);
    else
	sts = -oserror();
#else
    /* MinGW version */
    wait_pid = __pmProcessWait(pid, 0, &status, &sig);
    if (pmDebugOptions.exec)
	fprintf(stderr, "__pmProcessPipeClose: pid=%ld wait_pid=%ld status=0x%x sig=%d\n", (long)pid, (long)wait_pid, status, sig);
    if (wait_pid != pid)
	sts = -oserror();
    else {
	if (sig != -1)
	    sts = 1000 + sig; 
	else
	    sts = status;
    }
#endif

    return sts;
}

/*
 * Helper routine ... if a command line has already been built for
 * popen() or system(), then this routine may be used to construct
 * the equivalent __pmExecCtl_t structure.
 *
 * It is a sort of simple shell parser:
 * - args are separated by one or more spaces (not tabs)
 * - embedded spaces (or quotes) may be enclosed in '...' or "..."
 * - no \ escapes
 * - shell meta characters are not recognized and will be eaten as
 *   arguments, e.g. ; & < > |
 */
int
__pmProcessUnpickArgs(__pmExecCtl_t **argp, const char *command)
{
    char	*str = strdup(command);		/* in case command[] is const */
    int		sts = 0;
    int		done = 0;
    char	*p;
    char	*pend;
    int		endch = ' ';

    __pmInitLocks();

    if (str == NULL) {
	pmNoMem("__pmProcessUnpickArgs", strlen(command)+1, PM_RECOV_ERR);
	return -ENOMEM;
    }

    p = str;

    while (*p != '\0') {
	if (isspace((int)*p)) {
	    p++;
	    continue;
	}
	if (*p == '"' || *p == '\'') {
	    /* quote as first character, scan to matching quote */
	    endch = *p;
	    p++;
	}

	pend = index(p, endch);
	if (pend == NULL) {
	    done = 1;
	    if (endch != ' ') {
		/* not a great error, but probably the best we can do */
		pmNotifyErr(LOG_WARNING,
			"__pmProcessUnpickArgs: unterminated quote (%c) in command: %s\n",
			endch & 0xff, command);
		sts = PM_ERR_GENERIC;
		break;
	    }
	}
	else
	    *pend = '\0';

	if ((sts = __pmProcessAddArg(argp, p)) < 0)
	    break;
	if (done)
	    break;

	p = pend + 1;
	endch = ' ';
    }
    free(str);

    return sts;
}
