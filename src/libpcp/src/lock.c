/*
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
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
 */

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	lock_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t		__pmLock_extcall = PTHREAD_MUTEX_INITIALIZER;
#else /* !PM_MULTI_THREAD - symbols exposed at the shlib ABI level */
void *__pmLock_extcall;
void *__pmLock_libpcp;
void *__pmLock_extcall;
#endif /* PM_MULTI_THREAD */

#ifdef PM_MULTI_THREAD
typedef struct {
    void	*dbg_lock;
    int		count;
} lockdbg_t;

#define PM_LOCK_OP	1
#define PM_UNLOCK_OP	2

static int		multi_init[PM_SCOPE_MAX+1];
static pthread_t	multi_seen[PM_SCOPE_MAX+1];

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == lock_lock
 */
int
__pmIsLockLock(void *lock)
{
    return lock == (void *)&lock_lock;
}
#endif

/* the big libpcp lock */
pthread_mutex_t	__pmLock_libpcp = PTHREAD_MUTEX_INITIALIZER;

#ifndef HAVE___THREAD
pthread_key_t 	__pmTPDKey = 0;

static void
__pmTPD__destroy(void *addr)
{
    free(addr);
}
#endif /* HAVE___THREAD */
#endif /* PM_MULTI_THREAD */

static void
SetupDebug(void)
{
    /*
     * if $PCP_DEBUG is set in the environment then use the
     * value to set debug options (and flags)
     */
    char	*val;

    PM_LOCK(__pmLock_extcall);
    val = getenv("PCP_DEBUG");		/* THREADSAFE */
    if (val != NULL) {
	int		sts;
	sts = pmSetDebug(val);
	if (sts != 0)
	    fprintf(stderr, "Error: $PCP_DEBUG=%s is not valid, ignored\n", val);
    }
    PM_UNLOCK(__pmLock_extcall);
    setlinebuf(stderr);
}

#ifdef PM_MULTI_THREAD
#ifdef PM_MULTI_THREAD_DEBUG
static char
*lockname(void *lock)
{
    static char locknamebuf[30];
    int		ctxid;
    char	*ctxlist;

    if (__pmIsDeriveLock(lock))
	return "derived_metric";
    else if (__pmIsAuxconnectLock(lock))
	return "auxconnect";
    else if (__pmIsConfigLock(lock))
	return "config";
#ifdef PM_FAULT_INJECTION
    else if (__pmIsFaultLock(lock))
	return "fault";
#endif
    else if (__pmIsPduLock(lock))
	return "pdu";
    else if (__pmIsPdubufLock(lock))
	return "pdubuf";
    else if (__pmIsUtilLock(lock))
	return "util";
    else if (__pmIsContextsLock(lock))
	return "contexts";
    else if (__pmIsIpcLock(lock))
	return "ipc";
    else if (__pmIsOptfetchLock(lock))
	return "optfetch";
    else if (__pmIsErrLock(lock))
	return "err";
    else if (__pmIsLockLock(lock))
	return "lock";
    else if (__pmIsLogutilLock(lock))
	return "logutil";
    else if (__pmIsPmnsLock(lock))
	return "pmns";
    else if (__pmIsAFLock(lock))
	return "AF";
    else if (__pmIsSecureserverLock(lock))
	return "secureserver";
    else if (__pmIsConnectLock(lock))
	return "connect";
    else if (__pmIsExecLock(lock))
	return "exec";
    else if (__pmIsresultLock(lock))
	return "result";
    else if (lock == (void *)&__pmLock_extcall)
	return "global_extcall";
    else if ((ctxid = __pmIsContextLock(lock)) != -1) {
	pmsprintf(locknamebuf, sizeof(locknamebuf), "c_lock[slot %d]", ctxid);
	return locknamebuf;
    }
    else if ((ctxlist = __pmIsLogCtlLock(lock)) != NULL) {
	pmsprintf(locknamebuf, sizeof(locknamebuf), "lock[handle(s) %s]", ctxlist);
	free(ctxlist);
	return locknamebuf;
    }
    else {
	pmsprintf(locknamebuf, sizeof(locknamebuf), "? " PRINTF_P_PFX "%p", lock);
	return locknamebuf;
    }
}
#endif /* PM_MULTI_THREAD_DEBUG */
#endif /* PM_MULTI_THREAD */

#define REPORT_STYLE0	1
#define REPORT_STYLE1	2
#define REPORT_STYLE2	3

#ifdef PM_MULTI_THREAD
void
__pmDebugLock(int op, void *lock, const char *file, int line)
{
    int			report = 0;
#ifdef PM_MULTI_THREAD_DEBUG
    int			ctx;
#endif
    static __pmHashCtl	hashctl = { 0, 0, NULL };
    __pmHashNode	*hp = NULL;
    lockdbg_t		*ldp;
    int			try;
    int			sts;

    if (lock == (void *)&__pmLock_libpcp) {
	if (pmDebugOptions.appl0 ||
	    (!pmDebugOptions.appl0 && !pmDebugOptions.appl1 && !pmDebugOptions.appl2))
	    report = REPORT_STYLE0;
    }
#ifdef PM_MULTI_THREAD_DEBUG
    else if ((ctx = __pmIsContextLock(lock)) >= 0) {
	if (pmDebugOptions.appl1 ||
	    (!pmDebugOptions.appl0 && !pmDebugOptions.appl1 && !pmDebugOptions.appl2))
	    report = REPORT_STYLE1;
    }
#endif
    else {
	if (pmDebugOptions.appl2 ||
	    (!pmDebugOptions.appl0 && !pmDebugOptions.appl1 && !pmDebugOptions.appl2))
	    report = REPORT_STYLE2;
    }

    if (report) {
	__psint_t		key = (__psint_t)lock;
	fprintf(stderr, "%s:%d %s", file, line, op == PM_LOCK_OP ? "lock" : "unlock");
	try = 0;
again:
	hp = __pmHashSearch((unsigned int)key, &hashctl);
	while (hp != NULL) {
	    ldp = (lockdbg_t *)hp->data;
	    if (ldp->dbg_lock == lock)
		break;
	    hp = hp->next;
	}
	if (hp == NULL) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    ldp = (lockdbg_t *)malloc(sizeof(lockdbg_t));
	    ldp->dbg_lock = lock;
	    ldp->count = 0;
	    sts = __pmHashAdd((unsigned int)key, (void *)ldp, &hashctl);
	    if (sts == 1) {
		try++;
		if (try == 1)
		    goto again;
	    }
	    hp = NULL;
	    fprintf(stderr, " hash control failure: %s\n", pmErrStr_r(-sts, errmsg, sizeof(errmsg)));
	}
    }

    if (report == REPORT_STYLE0) {
	fprintf(stderr, "(global_libpcp)");
    }
#ifdef PM_MULTI_THREAD_DEBUG
    else if (report == REPORT_STYLE1) {
	fprintf(stderr, "(ctx %d)", ctx);
    }
#endif
    else if (report == REPORT_STYLE2) {
#ifdef PM_MULTI_THREAD_DEBUG
	fprintf(stderr, "(%s)", lockname(lock));
#else
	fprintf(stderr, "(%p)", lock);
#endif
    }
    if (report) {
	if (hp != NULL) {
	    ldp = (lockdbg_t *)hp->data;
	    if (op == PM_LOCK_OP) {
		ldp->count++;
		if (ldp->count != 1)
		    fprintf(stderr, " [count=%d]", ldp->count);
	    }
	    else {
		ldp->count--;
		if (ldp->count != 0)
		    fprintf(stderr, " [count=%d]", ldp->count);
	    }
	}
	fputc('\n', stderr);
	if (pmDebugOptions.desperate)
	    __pmDumpStack();
    }
}
#else
#define __pmDebugLock(op, lock, file, line) do { } while (0)
#endif /* PM_MULTI_THREAD */

/*
 * Initialize a single mutex to our preferred flavour ...
 * PTHREAD_MUTEX_ERRORCHECK today.
 */
#ifdef PM_MULTI_THREAD
void
__pmInitMutex(pthread_mutex_t *lock)
{
    int			sts;
    char		errmsg[PM_MAXERRMSGLEN];
    pthread_mutexattr_t	attr;

    if ((sts = pthread_mutexattr_init(&attr)) != 0) {
	pmErrStr_r(-sts, errmsg, sizeof(errmsg));
	fprintf(stderr, "__pmInitMutex(");
#ifdef PM_MULTI_THREAD_DEBUG
	fprintf(stderr, "%s)", lockname(lock));
#else
	fprintf(stderr, "%p)", lock);
#endif
	fprintf(stderr, ": pthread_mutexattr_init failed: %s\n", errmsg);
	exit(4);
    }
    if ((sts = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK)) != 0) {
	pmErrStr_r(-sts, errmsg, sizeof(errmsg));
	fprintf(stderr, "__pmInitMutex(");
#ifdef PM_MULTI_THREAD_DEBUG
	fprintf(stderr, "%s)", lockname(lock));
#else
	fprintf(stderr, "%p)", lock);
#endif
	fprintf(stderr, ": pthread_mutexattr_settype failed: %s\n", errmsg);
	exit(4);
    }
    if ((sts = pthread_mutex_init(lock, &attr)) != 0) {
	pmErrStr_r(-sts, errmsg, sizeof(errmsg));
	fprintf(stderr, "__pmInitMutex(");
#ifdef PM_MULTI_THREAD_DEBUG
	fprintf(stderr, "%s)", lockname(lock));
#else
	fprintf(stderr, "%p)", lock);
#endif
	fprintf(stderr, ": pthread_mutex_init failed: %s\n", errmsg);
	exit(4);
    }
    pthread_mutexattr_destroy(&attr);

    if (pmDebugOptions.lock) {
	fprintf(stderr, "__pmInitMutex(");
#ifdef PM_MULTI_THREAD_DEBUG
	fprintf(stderr, "%s", lockname(lock));
#else
	fprintf(stderr, "%p", lock);
#endif
	fprintf(stderr, ")\n");
    }
}
#else
#define __pmInitMutex(lock) do { } while (0)
#endif /* PM_MULTI_THREAD */

#ifdef PM_MULTI_THREAD
void
__pmDestroyMutex(pthread_mutex_t *lock)
{
    int			sts;
    char		errmsg[PM_MAXERRMSGLEN];

    if ((sts = pthread_mutex_destroy(lock)) != 0) {
	pmErrStr_r(-sts, errmsg, sizeof(errmsg));
	fprintf(stderr, "__pmDestroyMutex(");
#ifdef PM_MULTI_THREAD_DEBUG
	fprintf(stderr, "%s", lockname(lock));
#else
	fprintf(stderr, "%p", lock);
#endif
	fprintf(stderr, ": pthread_mutex_destroy failed: %s\n", errmsg);
	exit(4);
    }
    return;
}
#else
#define __pmDestroyMutex(lock) do { } while (0)
#endif /* PM_MULTI_THREAD */

#ifdef PM_MULTI_THREAD
/*
 * Do one-trip mutex initializations ... PM_INIT_LOCKS() comes here
 */
void
__pmInitLocks(void)
{
    static pthread_mutex_t	init = PTHREAD_MUTEX_INITIALIZER;
    static int			done = 0;
    int				psts;
    char			errmsg[PM_MAXERRMSGLEN];
    if ((psts = pthread_mutex_lock(&init)) != 0) {
	pmErrStr_r(-psts, errmsg, sizeof(errmsg));
	fprintf(stderr, "__pmInitLocks: pthread_mutex_lock failed: %s", errmsg);
	exit(4);
    }
    if (!done) {
	SetupDebug();
#ifndef HAVE___THREAD
	/* first thread here creates the thread private data key */
	if ((psts = pthread_key_create(&__pmTPDKey, __pmTPD__destroy)) != 0) {
	    pmErrStr_r(-psts, errmsg, sizeof(errmsg));
	    fprintf(stderr, "__pmInitLocks: pthread_key_create failed: %s", errmsg);
	    exit(4);
	}
#endif
	/*
	 * Global locks
	 */
	__pmInitMutex(&__pmLock_libpcp);

	/*
	 * Now initialize the local mutexes.
	 */
	init_pmns_lock();
	init_AF_lock();
	init_result_lock();
	init_secureserver_lock();
	init_connect_lock();
	init_exec_lock();

	done = 1;
    }
    if ((psts = pthread_mutex_unlock(&init)) != 0) {
	pmErrStr_r(-psts, errmsg, sizeof(errmsg));
	fprintf(stderr, "__pmInitLocks: pthread_mutex_unlock failed: %s", errmsg);
	exit(4);
    }
#ifndef HAVE___THREAD
    if (pthread_getspecific(__pmTPDKey) == NULL) {
	__pmTPD	*tpd = (__pmTPD *)malloc(sizeof(__pmTPD));
	if (tpd == NULL) {
	    pmNoMem("__pmInitLocks: __pmTPD", sizeof(__pmTPD), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	if ((psts = pthread_setspecific(__pmTPDKey, tpd)) != 0) {
	    pmErrStr_r(-psts, errmsg, sizeof(errmsg));
	    fprintf(stderr, "__pmInitLocks: pthread_setspecific failed: %s", errmsg);
	    exit(4);
	}
	memset((void *)tpd, 0, sizeof(*tpd));
	tpd->curr_handle = PM_CONTEXT_UNDEF;
    }
#endif
}
#else
void __pmInitLocks(void)
{
    static int		done = 0;
    if (!done) {
	SetupDebug();
	done = 1;
    }
}
#endif /* PM_MULTI_THREAD */

#ifdef PM_MULTI_THREAD
int
__pmMultiThreaded(int scope)
{
    int		sts = 0;

    PM_LOCK(lock_lock);
    if (!multi_init[scope]) {
	multi_init[scope] = 1;
	multi_seen[scope] = pthread_self();
    }
    else {
	if (!pthread_equal(multi_seen[scope], pthread_self()))
	    sts = 1;
    }
    PM_UNLOCK(lock_lock);
    return sts;
}
#else
int __pmMultiThreaded(int scope) { (void)scope; return 0; }
#endif /* PM_MULTI_THREAD */

#ifdef PM_MULTI_THREAD
int
__pmLock(void *lock, const char *file, int line)
{
    int		sts;

    if ((sts = pthread_mutex_lock(lock)) != 0) {
	sts = -sts;
#ifdef PM_MULTI_THREAD_DEBUG
	fprintf(stderr, "%s:%d: __pmLock(%s) failed: %s\n", file, line, lockname(lock), pmErrStr(sts));
#else
	fprintf(stderr, "%s:%d: __pmLock(%p) failed: %s\n", file, line, lock, pmErrStr(sts));
#endif
#ifdef BUILD_WITH_LOCK_ASSERTS
	__pmDumpStack();
	abort();
#endif
    }

    if (pmDebugOptions.lock)
	__pmDebugLock(PM_LOCK_OP, lock, file, line);

    return sts;
}
#else
int __pmLock(void *l, const char *f, int n) { (void)l, (void)f, (void)n; return 0; }
#endif /* PM_MULTI_THREAD */

#ifdef PM_MULTI_THREAD
int
__pmIsLocked(void *lock)
{
    int		sts;

    if ((sts = pthread_mutex_trylock(lock)) != 0) {
	if (sts == EBUSY)
	    return 1;
	sts = -sts;
	if (pmDebugOptions.desperate)
	    fprintf(stderr, "islocked: trylock %p failed: %s\n", lock, pmErrStr(sts));
	return 0;
    }
    if ((sts = pthread_mutex_unlock(lock)) != 0) {
	sts = -sts;
	if (pmDebugOptions.desperate)
	    fprintf(stderr, "islocked: unlock %p failed: %s\n", lock, pmErrStr(sts));
    }
    return 0;
}
#else
int __pmIsLocked(void *l) { (void)l; return 0; }
#endif /* PM_MULTI_THREAD */

#ifdef PM_MULTI_THREAD
#ifdef BUILD_WITH_LOCK_ASSERTS
/*
 * Assumes lock is a pthread mutex and not recursive.
 */
void
__pmCheckIsUnlocked(void *lock, char *file, int line)
{
    pthread_mutex_t	*lockp = (pthread_mutex_t *)lock;
    int	sts;

    if ((sts = pthread_mutex_trylock(lockp)) != 0) {
	if (sts == EBUSY) {
#ifdef __GLIBC__
#ifdef PM_MULTI_THREAD_DEBUG
	   fprintf(stderr, "__pmCheckIsUnlocked(%s): [%s:%d] __lock=%d __count=%d\n", lockname(lockp), file, line, lockp->__data.__lock, lockp->__data.__count);
#else
	   fprintf(stderr, "__pmCheckIsUnlocked(%p): [%s:%d] __lock=%d __count=%d\n", lockp, file, line, lockp->__data.__lock, lockp->__data.__count);
#endif
#else
	   fprintf(stderr, "__pmCheckIsUnlocked(%s): [%s:%d] locked\n", lockname(lockp), file, line);
#endif
	}
	else
	    fprintf(stderr, "__pmCheckIsUnlocked: trylock(%s) failed: %s\n", lockname(lockp), pmErrStr(-sts));
	return;
    }
    else {
	if ((sts = pthread_mutex_unlock(lockp)) != 0) {
	    fprintf(stderr, "__pmCheckIsUnlocked: unlock(%s) failed: %s\n", lockname(lockp), pmErrStr(-sts));
	}
    }
    return;
}
#endif /* BUILD_WITH_LOCK_ASSERTS */
#endif /* PM_MULTI_THREAD */

#ifdef PM_MULTI_THREAD
int
__pmUnlock(void *lock, const char *file, int line)
{
    int		sts; 

    if (pmDebugOptions.lock)
	__pmDebugLock(PM_UNLOCK_OP, lock, file, line);

    if ((sts = pthread_mutex_unlock(lock)) != 0) {
	sts = -sts;
#ifdef PM_MULTI_THREAD_DEBUG
	fprintf(stderr, "%s:%d: __pmUnlock(%s) failed: %s\n", file, line, lockname(lock), pmErrStr(sts));
#else
	fprintf(stderr, "%s:%d: __pmUnlock(%p) failed: %s\n", file, line, lock, pmErrStr(sts));
#endif
#ifdef BUILD_WITH_LOCK_ASSERTS
	__pmDumpStack();
	abort();
#endif
    }

    return sts;
}
#else
int __pmUnlock(void *l, const char *f, int n) { (void)l, (void)f, (void)n; return 0; }
#endif /* PM_MULTI_THREAD */
