/*
 * Copyright (c) 2012-2013 Red Hat.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 *
 * exercise multi-threaded checks for PM_SCOPE_AF and PM_SCOPE_ACL
 */

#include <stdio.h>
#include <stdlib.h>
#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pthread.h>

#ifndef HAVE_PTHREAD_BARRIER_T
#include "pthread_barrier.h"
#endif

#include "localconfig.h"

#if PCP_VER >= 3611
__pmSockAddr *addr;
#else
__pmIPAddr addr;
#endif

static pthread_barrier_t barrier;

static void
wakeup(int tag, void *data)
{
    printf("Woops, wakeup(%d, %p) called?\n", tag, data);
}

static void *
func1(void *arg)
{
    int			sts;
    int			afid;
    struct timeval	when = { 1000, 0 };
    char		*fn = "func1";
    unsigned int	op;

    afid = __pmAFregister(&when, NULL, wakeup);
    if (afid >= 0)
	printf("%s: __pmAFregister -> OK\n", fn);
    else
	printf("%s: __pmAFregister -> %s\n", fn, pmErrStr(afid));
    sts = __pmAFunregister(afid);
    if (sts >= 0)
	printf("%s: __pmAFunregister -> OK\n", fn);
    else
	printf("%s: __pmAFunregister -> %s\n", fn, pmErrStr(sts));

    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(&barrier);

    sts = __pmAccAddOp(1);
    if (sts == 0)
	printf("%s: __pmAccAddOp(1) -> OK\n", fn);
    else
	printf("%s: __pmAccAddOp(1) -> %s\n", fn, pmErrStr(sts));
    sts = __pmAccAddOp(2);
    if (sts == 0)
	printf("%s: __pmAccAddOp(2) -> OK\n", fn);
    else
	printf("%s: __pmAccAddOp(2) -> %s\n", fn, pmErrStr(sts));
    sts = __pmAccAddHost("localhost", 1, 2, 3);
    if (sts == 0)
	printf("%s: __pmAccAddHost -> OK\n", fn);
    else
	printf("%s: __pmAccAddHost -> %s\n", fn, pmErrStr(sts));
#if PCP_VER >= 3801
    putc('\n', stdout);
#endif
    __pmAccDumpHosts(stdout);
#if PCP_VER >= 3801
    putc('\n', stdout);
#endif
    sts = __pmAccSaveHosts();
    if (sts == 0)
	printf("%s: __pmAccSaveHosts -> OK\n", fn);
    else
	printf("%s: __pmAccSaveHosts -> %s\n", fn, pmErrStr(sts));
    sts = __pmAccRestoreHosts();
    if (sts == 0)
	printf("%s: __pmAccRestoreHosts -> OK\n", fn);
    else
	printf("%s: __pmAccRestoreHosts -> %s\n", fn, pmErrStr(sts));
    sts = __pmAccAddClient(addr, &op);
    if (sts == 0)
	printf("%s: __pmAccAddClient -> %d\n", fn, op);
    else
	printf("%s: __pmAccAddClient -> %s\n", fn, pmErrStr(sts));

    pthread_barrier_wait(&barrier);

    return(NULL);	/* pthread done */
}

static void *
func2(void *arg)
{
    int			sts;
    int			afid;
    struct timeval	when = { 1000, 0 };
    char		*fn = "func2";
    unsigned int	op;

    pthread_barrier_wait(&barrier);

    afid = __pmAFregister(&when, NULL, wakeup);
    if (afid >= 0)
	printf("%s: __pmAFregister -> OK\n", fn);
    else
	printf("%s: __pmAFregister -> %s\n", fn, pmErrStr(afid));
    sts = __pmAFunregister(afid);
    if (sts >= 0)
	printf("%s: __pmAFunregister -> OK\n", fn);
    else
	printf("%s: __pmAFunregister -> %s\n", fn, pmErrStr(sts));

    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(&barrier);

    sts = __pmAccAddOp(1);
    if (sts == 0)
	printf("%s: __pmAccAddOp(1) -> OK\n", fn);
    else
	printf("%s: __pmAccAddOp(1) -> %s\n", fn, pmErrStr(sts));
    sts = __pmAccAddOp(2);
    if (sts == 0)
	printf("%s: __pmAccAddOp(2) -> OK\n", fn);
    else
	printf("%s: __pmAccAddOp(2) -> %s\n", fn, pmErrStr(sts));
    sts = __pmAccAddHost("localhost", 1, 2, 3);
    if (sts == 0)
	printf("%s: __pmAccAddHost -> OK\n", fn);
    else
	printf("%s: __pmAccAddHost -> %s\n", fn, pmErrStr(sts));
    /* expect an error here - so no need for version-specific EOLs */
    __pmAccDumpHosts(stdout);
    sts = __pmAccSaveHosts();
    if (sts == 0)
	printf("%s: __pmAccSaveHosts -> OK\n", fn);
    else
	printf("%s: __pmAccSaveHosts -> %s\n", fn, pmErrStr(sts));
    sts = __pmAccRestoreHosts();
    if (sts == 0)
	printf("%s: __pmAccRestoreHosts -> OK\n", fn);
    else
	printf("%s: __pmAccRestoreHosts -> %s\n", fn, pmErrStr(sts));
    sts = __pmAccAddClient(addr, &op);
    if (sts == 0)
	printf("%s: __pmAccAddClient -> %d\n", fn, op);
    else
	printf("%s: __pmAccAddClient -> %s\n", fn, pmErrStr(sts));

    return(NULL);	/* pthread done */
}

static void
wait_for_thread(char *name, pthread_t tid)
{
    int		sts;
    char	*msg;

    sts = pthread_join(tid, (void *)&msg);
    if (sts == 0) {
	if (msg == PTHREAD_CANCELED)
	    printf("thread %s: pthread_join: cancelled?\n", name);
	else if (msg != NULL)
	    printf("thread %s: pthread_join: %s\n", name, msg);
    }
    else
	printf("thread %s: pthread_join: error: %s\n", name, strerror(sts));
}

int
main()
{
    pthread_t	tid1;
    pthread_t	tid2;
    int		sts;

#if PCP_VER >= 3702
    addr = __pmLoopBackAddress(AF_INET);
#elif PCP_VER >= 3611
    addr = __pmLoopBackAddress();
#else
    addr = __pmLoopbackAddress();
#endif

    sts = pthread_barrier_init(&barrier, NULL, 2);
    if (sts != 0) {
	printf("pthread_barrier_init: sts=%d\n", sts);
	exit(1);
    }

    /* only need this to initialize library mutexes */
    if ((sts = pmNewContext(PM_CONTEXT_LOCAL, NULL)) < 0) {
	printf("pmNewContext: %s\n", pmErrStr(sts));
	exit(1);
    }

    sts = pthread_create(&tid1, NULL, func1, NULL);
    if (sts != 0) {
	printf("thread_create: tid1: sts=%d\n", sts);
	exit(1);
    }
    sts = pthread_create(&tid2, NULL, func2, NULL);
    if (sts != 0) {
	printf("thread_create: tid2: sts=%d\n", sts);
	exit(1);
    }

    wait_for_thread("tid1", tid1);
    wait_for_thread("tid2", tid2);

#if PCP_VER >= 3611
    __pmSockAddrFree(addr);
#endif
    exit(0);
}
