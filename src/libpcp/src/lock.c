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
#include "impl.h"
#include "internal.h"
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#ifdef PM_MULTI_THREAD
typedef struct {
    void	*lock;
    int		count;
} lockdbg_t;

#define PM_LOCK_OP	1
#define PM_UNLOCK_OP	2

static int		multi_init[PM_SCOPE_MAX+1];
static pthread_t	multi_seen[PM_SCOPE_MAX+1];

/* the big libpcp lock */
#ifdef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
pthread_mutex_t	__pmLock_libpcp = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#else
pthread_mutex_t	__pmLock_libpcp = PTHREAD_MUTEX_INITIALIZER;

#ifndef HAVE___THREAD
pthread_key_t 	__pmTPDKey = 0;

static void
__pmTPD__destroy(void *addr)
{
    free(addr);
}
#endif
#endif

static void
SetupDebug(void)
{
    /*
     * if $PCP_DEBUG is set in the environment then use the (decimal)
     * value to set bits in pmDebug via bitwise ORing
     */
    char	*val = getenv("PCP_DEBUG");
    int		ival;

    if (val != NULL) {
	char	*end;
	ival = strtol(val, &end, 10);
	if (*end != '\0') {
	    fprintf(stderr, "Error: $PCP_DEBUG=%s is not numeric, ignored\n", val);
	}
	else
	    pmDebug |= ival;
    }
}

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
#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
	/*
	 * Unable to initialize at compile time, need to do it here in
	 * a one trip for all threads run-time initialization.
	 */
	pthread_mutexattr_t	attr;

	if ((psts = pthread_mutexattr_init(&attr)) != 0) {
	    pmErrStr_r(-psts, errmsg, sizeof(errmsg));
	    fprintf(stderr, "__pmInitLocks: pthread_mutexattr_init failed: %s", errmsg);
	    exit(4);
	}
	if ((psts = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE)) != 0) {
	    pmErrStr_r(-psts, errmsg, sizeof(errmsg));
	    fprintf(stderr, "__pmInitLocks: pthread_mutexattr_settype failed: %s", errmsg);
	    exit(4);
	}
	if ((psts = pthread_mutex_init(&__pmLock_libpcp, &attr)) != 0) {
	    pmErrStr_r(-psts, errmsg, sizeof(errmsg));
	    fprintf(stderr, "__pmInitLocks: pthread_mutex_init failed: %s", errmsg);
	    exit(4);
	}
#endif
#ifndef HAVE___THREAD
	/* first thread here creates the thread private data key */
	if ((psts = pthread_key_create(&__pmTPDKey, __pmTPD__destroy)) != 0) {
	    pmErrStr_r(-psts, errmsg, sizeof(errmsg));
	    fprintf(stderr, "__pmInitLocks: pthread_key_create failed: %s", errmsg);
	    exit(4);
	}
#endif
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
	    __pmNoMem("__pmInitLocks: __pmTPD", sizeof(__pmTPD), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	if ((psts = pthread_setspecific(__pmTPDKey, tpd)) != 0) {
	    pmErrStr_r(-psts, errmsg, sizeof(errmsg));
	    fprintf(stderr, "__pmInitLocks: pthread_setspecific failed: %s", errmsg);
	    exit(4);
	}
	tpd->curcontext = PM_CONTEXT_UNDEF;
    }
#endif
}

int
__pmMultiThreaded(int scope)
{
    int		sts = 0;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (!multi_init[scope]) {
	multi_init[scope] = 1;
	multi_seen[scope] = pthread_self();
    }
    else {
	if (!pthread_equal(multi_seen[scope], pthread_self()))
	    sts = 1;
    }
    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

#ifdef PM_MULTI_THREAD_DEBUG
void
__pmDebugLock(int op, void *lock, const char *file, int line)
{
    int			report = 0;
    int			ctx;
    static __pmHashCtl	hashctl = { 0, 0, NULL };
    __pmHashNode	*hp = NULL;
    lockdbg_t		*ldp;
    int			try;
    int			sts;

    if (lock == (void *)&__pmLock_libpcp) {
	if ((pmDebug & DBG_TRACE_APPL0) | ((pmDebug & (DBG_TRACE_APPL0 | DBG_TRACE_APPL1 | DBG_TRACE_APPL2)) == 0))
	    report = DBG_TRACE_APPL0;
    }
    else if ((ctx = __pmIsContextLock(lock)) >= 0) {
	if ((pmDebug & DBG_TRACE_APPL1) | ((pmDebug & (DBG_TRACE_APPL0 | DBG_TRACE_APPL1 | DBG_TRACE_APPL2)) == 0))
	    report = DBG_TRACE_APPL1;
    }
    else {
	if ((pmDebug & DBG_TRACE_APPL2) | ((pmDebug & (DBG_TRACE_APPL0 | DBG_TRACE_APPL1 | DBG_TRACE_APPL2)) == 0))
	    report = DBG_TRACE_APPL2;
    }

    if (report) {
	__psint_t		key = (__psint_t)lock;
	fprintf(stderr, "%s:%d %s", file, line, op == PM_LOCK_OP ? "lock" : "unlock");
	try = 0;
again:
	hp = __pmHashSearch((unsigned int)key, &hashctl);
	while (hp != NULL) {
	    ldp = (lockdbg_t *)hp->data;
	    if (ldp->lock == lock)
		break;
	    hp = hp->next;
	}
	if (hp == NULL) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    ldp = (lockdbg_t *)malloc(sizeof(lockdbg_t));
	    ldp->lock = lock;
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

    if (report == DBG_TRACE_APPL0) {
	fprintf(stderr, "(global_libpcp)");
    }
    else if (report == DBG_TRACE_APPL1) {
	fprintf(stderr, "(ctx %d)", ctx);
    }
    else if (report == DBG_TRACE_APPL2) {
	if ((ctx = __pmIsChannelLock(lock)) >= 0)
	    fprintf(stderr, "(ctx %d ipc channel)", ctx);
	else if (__pmIsDeriveLock(lock))
	    fprintf(stderr, "(derived_metric)");
	else
	    fprintf(stderr, "(" PRINTF_P_PFX "%p)", lock);
    }
    if (report) {
	if (hp != NULL) {
	    ldp = (lockdbg_t *)hp->data;
	    if (op == PM_LOCK_OP) {
		if (ldp->count != 0)
		    fprintf(stderr, " [count=%d]", ldp->count);
		ldp->count++;
	    }
	    else {
		if (ldp->count != 1)
		    fprintf(stderr, " [count=%d]", ldp->count);
		ldp->count--;
	    }
	}
	fputc('\n', stderr);
#ifdef HAVE_BACKTRACE
#define MAX_TRACE_DEPTH 32
	{
	    void	*backaddr[MAX_TRACE_DEPTH];
	    sts = backtrace(backaddr, MAX_TRACE_DEPTH);
	    if (sts > 0) {
		char	**symbols;
		symbols = backtrace_symbols(backaddr, MAX_TRACE_DEPTH);
		if (symbols != NULL) {
		    int		i;
		    fprintf(stderr, "backtrace:\n");
		    for (i = 0; i < sts; i++)
			fprintf(stderr, "  %s\n", symbols[i]);
		    free(symbols);
		}
	    }
	}

#endif
    }
}
#else
#define __pmDebugLock(op, lock, file, line) do { } while (0)
#endif

int
__pmLock(void *lock, const char *file, int line)
{
    int		sts;

    if (pmDebug & DBG_TRACE_LOCK)
	__pmDebugLock(PM_LOCK_OP, lock, file, line);

    if ((sts = pthread_mutex_lock(lock)) != 0) {
	sts = -sts;
	if (pmDebug & DBG_TRACE_DESPERATE)
	    fprintf(stderr, "%s:%d: lock failed: %s\n", file, line, pmErrStr(sts));
    }
    return sts;
}

int
__pmUnlock(void *lock, const char *file, int line)
{
    int		sts; 

    if (pmDebug & DBG_TRACE_LOCK)
	__pmDebugLock(PM_UNLOCK_OP, lock, file, line);

    if ((sts = pthread_mutex_unlock(lock)) != 0) {
	sts = -sts;
	if (pmDebug & DBG_TRACE_DESPERATE)
	    fprintf(stderr, "%s:%d: unlock failed: %s\n", file, line, pmErrStr(sts));
    }
    return sts;
}

#else /* !PM_MULTI_THREAD - symbols exposed at the shlib ABI level */
void *__pmLock_libpcp;
void __pmInitLocks(void)
{
    static int		done = 0;
    if (!done) {
	SetupDebug();
	done = 1;
    }
}
int __pmMultiThreaded(int scope) { (void)scope; return 0; }
int __pmLock(void *l, const char *f, int n) { (void)l, (void)f, (void)n; return 0; }
int __pmUnlock(void *l, const char *f, int n) { (void)l, (void)f, (void)n; return 0; }
#endif
