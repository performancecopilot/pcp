/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 *
 * exercise __pmMultiThreaded()
 */

#include <stdio.h>
#include <stdlib.h>
#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pthread.h>

#ifndef HAVE_PTHREAD_BARRIER_T
#include "pthread_barrier.h"
#endif

static void *
func(void *arg)
{
    if (__pmMultiThreaded(PM_SCOPE_DSO_PMDA))
	printf("func: is multithreaded\n");
    else
	printf("func: is NOT multithreaded\n");
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

    if (__pmMultiThreaded(PM_SCOPE_DSO_PMDA))
	printf("main0: is multithreaded\n");
    else
	printf("main0: is NOT multithreaded\n");

    sts = pthread_create(&tid1, NULL, func, NULL);
    if (sts != 0) {
	printf("thread_create: tid1: sts=%d\n", sts);
	exit(1);
    }

    if (__pmMultiThreaded(PM_SCOPE_DSO_PMDA))
	printf("main1: is multithreaded\n");
    else
	printf("main1: is NOT multithreaded\n");

    sts = pthread_create(&tid2, NULL, func, NULL);
    if (sts != 0) {
	printf("thread_create: tid2: sts=%d\n", sts);
	exit(1);
    }

    if (__pmMultiThreaded(PM_SCOPE_DSO_PMDA))
	printf("main2: is multithreaded\n");
    else
	printf("main2: is NOT multithreaded\n");

    wait_for_thread("tid1", tid1);
    wait_for_thread("tid2", tid2);

    exit(0);
}
