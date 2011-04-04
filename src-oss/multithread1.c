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

static int		ctx = -1;
static struct timeval	delay = { 0, 1000 };
static char		*namelist[] = { "sampledso.colour" };
static pmID		pmidlist[] = { 0 };
static pmDesc		desc;
static char		**instname;
static int		*instance;
static pmResult		*rp;

#define NTRY 100

static void *
func(void *arg)
{
    int			sts;
    int			try;
    char		**children;
    char		*p;

    if ((sts = pmNewContext(PM_CONTEXT_LOCAL, NULL)) < 0) {
	printf("pmNewContext: %s\n", pmErrStr(sts));
	for (try = 0; try < NTRY; try++) {
	    __pmtimevalSleep(delay);
	    if (ctx >= 0)
		break;
	}
	if (ctx == -1)
	    pthread_exit("Loser failed to get context!");
	if ((sts = pmUseContext(ctx)) < 0) {
	    printf("pmUseContext: %s\n", pmErrStr(sts));
	    pthread_exit(NULL);
	}
    }
    else {
	ctx = sts;
	printf("pmNewContext: -> %d\n", ctx);
    }

    if ((sts = pmDupContext()) < 0)
	printf("pmDupContext: %s\n", pmErrStr(sts));
    else {
	printf("pmDupContext: -> %d\n", sts);
	pmUseContext(ctx);
    }

    if ((sts = pmLookupName(1, namelist, pmidlist)) < 0) {
	printf("pmLookupName: %s\n", pmErrStr(sts));
	for (try = 0; try < NTRY; try++) {
	    __pmtimevalSleep(delay);
	    if (pmidlist[0] != 0)
		break;
	}
	if (pmidlist[0] == 0)
	    pthread_exit("Loser failed to get pmid!");
    }
    else
	printf("pmLookupName: -> %s\n", pmIDStr(pmidlist[0]));

    if ((sts = pmGetPMNSLocation()) < 0)
	printf("pmGetPMNSLocation: %s\n", pmErrStr(sts));
    else
	printf("pmGetPMNSLocation: -> %d\n", sts);

    /* leaf node, expect no children */
    if ((sts = pmGetChildrenStatus(namelist[0], &children, NULL)) < 0)
	printf("pmGetChildrenStatus: %s\n", pmErrStr(sts));
    else
	printf("pmGetChildrenStatus: -> %d\n", sts);

    if ((sts = pmLookupDesc(pmidlist[0], &desc)) < 0) {
	printf("pmLookupDesc: %s\n", pmErrStr(sts));
	for (try = 0; try < NTRY; try++) {
	    __pmtimevalSleep(delay);
	    if (desc.pmid != 0)
		break;
	}
	if (desc.pmid == 0)
	    pthread_exit("Loser failed to get pmDesc!");
    }
    else
	printf("pmLookupDesc: -> %s type=%s indom=%s\n", pmIDStr(desc.pmid), pmTypeStr(desc.type), pmInDomStr(desc.indom));

    if ((sts = pmLookupText(pmidlist[0], PM_TEXT_ONELINE, &p)) < 0)
	printf("pmLookupText: %s\n", pmErrStr(sts));
    else
	printf("pmLookupText: -> %s\n", p);

    if ((sts = pmGetInDom(desc.indom, &instance, &instname)) < 0) {
	printf("pmGetInDom: %s: %s\n", pmInDomStr(desc.indom), pmErrStr(sts));
	for (try = 0; try < NTRY; try++) {
	    __pmtimevalSleep(delay);
	    if (instance != NULL)
		break;
	}
	if (instance == NULL)
	    pthread_exit("Loser failed to get indom!");
    }
    else
	printf("pmGetInDom: -> %d\n", sts);

    if ((sts = pmNameInDom(desc.indom, instance[0], &p)) < 0)
	printf("pmNameInDom: %s\n", pmErrStr(sts));
    else
	printf("pmNameInDom: %d -> %s\n", instance[0], p);

    if ((sts = pmLookupInDom(desc.indom, instname[0])) < 0)
	printf("pmLookupInDom: %s\n", pmErrStr(sts));
    else
	printf("pmLookupInDom: %s -> %d\n", instname[0], sts);

    if ((sts = pmFetch(1, pmidlist, &rp)) < 0) {
	printf("pmFetch: %s\n", pmErrStr(sts));
	for (try = 0; try < NTRY; try++) {
	    __pmtimevalSleep(delay);
	    if (rp != NULL)
		break;
	}
	if (rp == NULL)
	    pthread_exit("Loser failed to get pmResult!");
    }
    else
	printf("pmFetch: -> OK\n");

    if ((sts = pmStore(rp)) < 0)
	printf("pmStore: %s\n", pmErrStr(sts));
    else
	printf("pmStore: -> OK\n");

    pthread_exit(NULL);
}

int
main()
{
    pthread_t	tid1;
    pthread_t	tid2;
    int		sts;
    char	*msg;

    sts = pthread_create(&tid1, NULL, func, NULL);
    if (sts != 0) {
	printf("thread_create: tid1: sts=%d\n", sts);
	exit(1);
    }
    sts = pthread_create(&tid2, NULL, func, NULL);
    if (sts != 0) {
	printf("thread_create: tid2: sts=%d\n", sts);
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
