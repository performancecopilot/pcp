#include <pcp/pmapi.h>
#include <pcp/trace.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <time.h>
#ifdef IS_SOLARIS
#include <sys/rtpriocntl.h>
#include <sys/tspriocntl.h>
#include <sys/processor.h>
#endif
#ifdef HAVE_PRCTL
#include <sys/prctl.h>
#endif
#include <sys/wait.h>

#ifndef HAVE_SPROC

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#endif /*HAVE_SPROC*/

#define MAXTAGNAMELEN    256

static int	obscount = 0;
static int	pointcount = 0;
static int	transactcount = 0;

void
gentag(char *buf)
{
    int	i, len;

    len = (int)lrand48() % 10 + 1;
    for (i=0; i < len; i++)
	buf[i] = (char)((int)lrand48() % 88 + 40);   /* some ascii */
    buf[i] = '\0';
    /*fprintf(stderr, "Generated tag \"%s\" for pid=%d\n", buf, getpid());*/
}

void
run(int sts, char *tag)
{
    char	*err;
    err = pmtraceerrstr(sts);
    if (sts < 0)
	fprintf(stderr, "failed on tag \"%s\": %s\n", tag, err);
}

/*ARGSUSED*/
void
sproc1(void *foo)
{
    int		i;
    char	buf[64];
    double	nan=0.0;

    for (i = 0; i < 5; i++)
	nan = 0.0/nan;
    for (i = 0; i < 1000; i++) {
	gentag(buf);
	run(pmtraceobs(buf, nan), buf);
	run(pmtraceobs(buf, 2.239801), buf);
	obscount++;
    }
}

/*ARGSUSED*/
void
sproc2(void *foo)
{
    int		i;
    char	buf[64];

    for (i = 0; i < 2000; i++) {
	gentag(buf);
	run(pmtracepoint(buf), buf);
	pointcount++;
    }
}

/*ARGSUSED*/
void
sproc3(void *foo)
{
    int		i;
    char	buf1[64], buf2[64], buf3[64], buf4[64];

    gentag(buf1);
    gentag(buf2);
    gentag(buf3);
    gentag(buf4);

    for (i=0; i < 500; i++) {
	run(pmtracebegin(buf1), buf1);
	run(pmtracebegin(buf2), buf2);
	run(pmtracebegin(buf3), buf3);
	run(pmtraceend(buf2), buf2);	transactcount++;
	run(pmtracebegin(buf4), buf4);
	run(pmtraceend(buf4), buf4);	transactcount++;
	run(pmtraceend(buf3), buf3);	transactcount++;
	run(pmtraceend(buf1), buf1);	transactcount++;
    }
}


int
main(int argc, char *argv[])
{
    int		i;
#ifndef HAVE_SPROC
    pthread_t threads[3];
#endif

    /* exercise the zero-observation case */
    run(pmtraceobs("zero", 0), "zero");
    obscount++;

    srand48((long)time(0));
    /*pmtracestate(PMTRACE_STATE_COMMS | PMTRACE_STATE_API);*/

    /* eat this, pmdatrace! */
#ifdef HAVE_SPROC
    sproc(sproc1, PR_SADDR, NULL);
    sproc(sproc2, PR_SADDR, NULL);
    sproc(sproc3, PR_SADDR, NULL);
#else
    pthread_create(threads, NULL, (void (*))sproc1, NULL);
    pthread_create(threads+1, NULL, (void (*))sproc2, NULL);
    pthread_create(threads+2, NULL, (void (*))sproc3, NULL);
#endif

    for (i=0; i < 3; i++) {
#ifdef HAVE_SPROC
	wait(NULL);
#else
	void * rv;

	pthread_join (threads[i], &rv);
#endif
	fprintf(stderr, "reaped sproc #%d\n", i);
    }

    fprintf(stderr, "torture_trace counters:\n");
    fprintf(stderr, "    pmtraceobs      %d\n", obscount);
    fprintf(stderr, "    pmtracepoint    %d\n", pointcount);
    fprintf(stderr, "    pmtracetransact %d\n", transactcount);

    exit(0);
}
