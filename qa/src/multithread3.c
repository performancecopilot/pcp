/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 *
 * exercise multi-threaded support for local PMNS functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pthread.h>

#ifndef HAVE_PTHREAD_BARRIER_T
#include "pthread_barrier.h"
#endif

static pthread_barrier_t barrier;

static char *pmnsfile;
static char pmns[] = "#undef  EXTRA\n\
root {\n\
        a\n\
	b       1:0:0\n\
	c       2:0:0\n\
#ifdef EXTRA\n\
	d\n\
	e	4:0:0\n\
#endif\n\
}\n\
a {\n\
	one     0:0:0\n\
	two     0:1:0\n\
	three\n\
}\n\
a.three {\n\
	foo     0:2:0\n\
	bar     0:2:1\n\
}\n\
#ifdef EXTRA\n\
d {\n\
	mumble	3:0:0\n\
	f\n\
}\n\
d.f {\n\
	fumble	3:0:1\n\
}\n\
#endif\n\
";


static void *
func1(void *arg)
{
    int		sts;
    char	*fn = "func1";
    char	**offspring;
    int		*status;
    int		i;
    int		j;
    char	*namelist[] = { "b", "a.three.foo", "d.f.fumble" };
    pmID	pmidlist[3];

    pthread_barrier_wait(&barrier);

    __pmDumpNameSpace(stdout, 1);

    sts = pmGetChildrenStatus("", &offspring, &status);
    if (sts >= 0) {
	printf("%s: pmGetChildrenStatus -> %d\n", fn, sts);
	for (i = 0; i < sts; i++) {
	    printf("[%d] %s %s\n", i, offspring[i], status[i] == PMNS_LEAF_STATUS ? "leaf" : "non-leaf");
	}
	free(offspring);
	free(status);
    }
    else
	printf("%s: pmGetChildrenStatus -> %s\n", fn, pmErrStr(sts));

    for (i = 0; i < 5; i++) {
	pthread_barrier_wait(&barrier);
	sts = pmLookupName(sizeof(namelist)/sizeof(namelist[0]), namelist, pmidlist);
	if (sts < 0)
	    printf("%s: pmGetChildrenStatus[%d] -> %s\n", fn, i, pmErrStr(sts));
	else {
	    for (j = 0; j < sizeof(namelist)/sizeof(namelist[0]); j++) {
		printf("%s: [%d] %s -> %s", fn, i, namelist[j], pmIDStr(pmidlist[j]));
		if (pmidlist[j] == PM_ID_NULL)
		    printf("\n");
		else {
		    char	*name;
		    sts = pmNameID(pmidlist[j], &name);
		    if (sts < 0)
			printf(": pmNameID -> %s\n", pmErrStr(sts));
		    else {
			printf(" -> %s\n", name);
			free(name);
		    }
		}
	    }
	}
	pthread_barrier_wait(&barrier);
    }

    pthread_exit(NULL);
}

static void *
func2(void *arg)
{
    int		sts;
    char	*fn = "func2";
    char	*p;
    FILE	*f;

    pthread_barrier_wait(&barrier);

    /* iter 0 */
    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(&barrier);

    /* iter 1 */
    pmUnloadNameSpace();
    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(&barrier);

    /* iter 2 */
    if ((sts = pmLoadASCIINameSpace(pmnsfile, 1)) < 0) {
	printf("%s: pmLoadASCIINameSpace: [reload] %s\n", fn, pmErrStr(sts));
	exit(1);
    }
    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(&barrier);

    /* iter 3 */
    pmUnloadNameSpace();
    for (p = pmns; *p; p++) {
	if (*p == 'u') {
	    *p++ = 'd';
	    *p++ = 'e';
	    *p++ = 'f';
	    *p++ = 'i';
	    *p++ = 'n';
	    *p++ = 'e';
	    break;
	}
    }
    if ((f = fopen(pmnsfile, "w")) == NULL) {
	printf("fopen: %s: failed: %s\n", pmnsfile, pmErrStr(-oserror()));
	exit(1);
    }
    if (fwrite(pmns, strlen(pmns), 1, f) != 1) {
	printf("fwrite: %s: failed: %s\n", pmnsfile, pmErrStr(-oserror()));
	exit(1);
    }
    fclose(f);
    if ((sts = pmLoadASCIINameSpace(pmnsfile, 1)) < 0) {
	printf("%s: pmLoadASCIINameSpace: [extra reload] %s\n", fn, pmErrStr(sts));
	exit(1);
    }
    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(&barrier);

    /* iter 4 */
    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(&barrier);

    pthread_exit(NULL);
}

int
main(int argc, char **argv)
{
    pthread_t	tid1;
    pthread_t	tid2;
    int		sts;
    char	*msg;
    FILE	*f;

    if (argc != 2) {
	printf("Usage: multithread3 tmpfile\n");
	exit(1);
    }
    pmnsfile = argv[1];
    if ((f = fopen(pmnsfile, "w")) == NULL) {
	printf("fopen: %s: failed: %s\n", pmnsfile, pmErrStr(-oserror()));
	exit(1);
    }
    if (fwrite(pmns, strlen(pmns), 1, f) != 1) {
	printf("fwrite: %s: failed: %s\n", pmnsfile, pmErrStr(-oserror()));
	exit(1);
    }
    fclose(f);

    sts = pthread_barrier_init(&barrier, NULL, 2);
    if (sts != 0) {
	printf("pthread_barrier_init: sts=%d\n", sts);
	exit(1);
    }

    if ((sts = pmLoadASCIINameSpace(pmnsfile, 1)) < 0) {
	printf("pmLoadASCIINameSpace: %s\n", pmErrStr(sts));
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

    pthread_join(tid1, (void *)&msg);
    if (msg != NULL) printf("tid1: %s\n", msg);
    pthread_join(tid2, (void *)&msg); 
    if (msg != NULL) printf("tid2: %s\n", msg);

    exit(0);
}
