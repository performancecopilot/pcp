/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 *
 * exercise multi-threaded checks for PM_CONTEXT_LOCAL
 */

#include <stdio.h>
#include <stdlib.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pthread.h>
#if darwin_hack
#include <dlfcn.h>
#endif

#ifndef HAVE_PTHREAD_BARRIER_T
#include "pthread_barrier.h"
#endif

static pthread_barrier_t barrier;

static int		ctx = -1;
static char		*namelist[] = { "sampledso.colour" };
static pmID		pmidlist[] = { 0 };
static pmDesc		desc;
static char		**instname;
static int		*instance;
static pmResult		*rp;

static void
func(void)
{
    int			sts;
    char		**children;
    char		*p;

    if ((sts = pmUseContext(ctx)) < 0) {
	printf("pmUseContext(%d): %s\n", ctx, pmErrStr(sts));
	pthread_exit(NULL);
    }

    /*
     * expect this to fail for the second thread through when
     * using PM_CONTEXT_LOCAL
     */
    if ((sts = pmDupContext()) < 0)
	printf("pmDupContext: %s\n", pmErrStr(sts));
    else
	printf("pmDupContext: -> %d\n", sts);

    if ((sts = pmUseContext(ctx)) < 0) {
	printf("pmUseContext(%d): %s\n", ctx, pmErrStr(sts));
	pthread_exit(NULL);
    }
    else
	printf("pmUseContext(%d) -> %d\n", ctx, sts);

    if ((sts = pmLookupName(1, namelist, pmidlist)) < 0)
	printf("pmLookupName: %s\n", pmErrStr(sts));
    else
	printf("pmLookupName: -> %s\n", pmIDStr(pmidlist[0]));

    pthread_barrier_wait(&barrier);

    if (pmidlist[0] == 0)
	pthread_exit("Loser failed to get pmid!");

    if ((sts = pmGetPMNSLocation()) < 0)
	printf("pmGetPMNSLocation: %s\n", pmErrStr(sts));
    else
	printf("pmGetPMNSLocation: -> %d\n", sts);

    /* leaf node, expect no children */
    if ((sts = pmGetChildrenStatus(namelist[0], &children, NULL)) < 0)
	printf("pmGetChildrenStatus: %s\n", pmErrStr(sts));
    else
	printf("pmGetChildrenStatus: -> %d\n", sts);

    if ((sts = pmLookupDesc(pmidlist[0], &desc)) < 0)
	printf("pmLookupDesc: %s\n", pmErrStr(sts));
    else
	printf("pmLookupDesc: -> %s type=%s indom=%s\n", pmIDStr(desc.pmid), pmTypeStr(desc.type), pmInDomStr(desc.indom));

    pthread_barrier_wait(&barrier);

    if (desc.pmid == 0)
	pthread_exit("Loser failed to get pmDesc!");

    if ((sts = pmLookupText(pmidlist[0], PM_TEXT_ONELINE, &p)) < 0)
	printf("pmLookupText: %s\n", pmErrStr(sts));
    else
	printf("pmLookupText: -> %s\n", p);

    if ((sts = pmGetInDom(desc.indom, &instance, &instname)) < 0)
	printf("pmGetInDom: %s: %s\n", pmInDomStr(desc.indom), pmErrStr(sts));
    else
	printf("pmGetInDom: -> %d\n", sts);

    pthread_barrier_wait(&barrier);

    if (instance == NULL)
	pthread_exit("Loser failed to get indom!");

    if ((sts = pmNameInDom(desc.indom, instance[0], &p)) < 0)
	printf("pmNameInDom: %s\n", pmErrStr(sts));
    else
	printf("pmNameInDom: %d -> %s\n", instance[0], p);

    if ((sts = pmLookupInDom(desc.indom, instname[0])) < 0)
	printf("pmLookupInDom: %s\n", pmErrStr(sts));
    else
	printf("pmLookupInDom: %s -> %d\n", instname[0], sts);

    if ((sts = pmFetch(1, pmidlist, &rp)) < 0)
	printf("pmFetch: %s\n", pmErrStr(sts));
    else
	printf("pmFetch: -> OK\n");

    pthread_barrier_wait(&barrier);

    if (rp == NULL)
	pthread_exit("Loser failed to get pmResult!");

    if ((sts = pmStore(rp)) < 0)
	printf("pmStore: %s\n", pmErrStr(sts));
    else
	printf("pmStore: -> OK\n");

    pthread_exit(NULL);
}

static void *
func1(void *arg)
{
    int			sts;

    if ((sts = pmNewContext(PM_CONTEXT_LOCAL, NULL)) < 0)
	printf("pmNewContext: %s\n", pmErrStr(sts));
    else {
	ctx = sts;
	printf("pmNewContext: -> %d\n", ctx);
    }

    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(&barrier);

    func();
    /*NOTREACHED*/
    return NULL;
}

static void *
func2(void *arg)
{
    int			sts;

    pthread_barrier_wait(&barrier);

    /*
     * expect this to fail for the second thread through when
     * using PM_CONTEXT_LOCAL
     */
    if ((sts = pmNewContext(PM_CONTEXT_LOCAL, NULL)) < 0)
	printf("pmNewContext: %s\n", pmErrStr(sts));
    else {
	ctx = sts;
	printf("pmNewContext: -> %d\n", ctx);
    }

    pthread_barrier_wait(&barrier);

    func();
    /*NOTREACHED*/
    return NULL;
}

int
main()
{
    pthread_t	tid1;
    pthread_t	tid2;
    int		sts;
    char	*msg;

#if darwin_hack
    /*
     * This is just too bizarre!
     * On Mac OS X, if one falls into the "CoreFoundation" libraries
     * in a thread, without first having run the initialization code
     * from the mainline, you die with a SIGTRAP!  Calling dlopen() with
     * doomed to fail parameters suffices.
     *
     * See also the -framework CoreFoundation glue needed in
     * GNUmakefile.
     *
     * I am not making this up, check out
     * http://openradar.appspot.com/7209349
     */
    dlopen("/no/such/dso", RTLD_LAZY);
#endif

    sts = pthread_barrier_init(&barrier, NULL, 2);
    if (sts != 0) {
	printf("pthread_barrier_init: sts=%d\n", sts);
	exit(1);
    }

    sts = pthread_create(&tid1, NULL, func1, NULL);
    if (sts != 0) {
	printf("pthread_create: tid1: sts=%d\n", sts);
	exit(1);
    }
    sts = pthread_create(&tid2, NULL, func2, NULL);
    if (sts != 0) {
	printf("pthread_create: tid2: sts=%d\n", sts);
	exit(1);
    }

    pthread_join(tid1, (void *)&msg);
    if (msg != NULL) printf("tid1: %s\n", msg);
    pthread_join(tid2, (void *)&msg); 
    if (msg != NULL) printf("tid2: %s\n", msg);

    if (rp != NULL)
	pmFreeResult(rp);

    exit(0);
}
