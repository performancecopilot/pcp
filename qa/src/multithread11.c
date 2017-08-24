/*
 * exercise multi-threaded concurrent context error paths
 */

#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

static void *
liveworker(void *x)
{
    int         i, c;
    char        *host = x;

    for (i=0; i < 5000; i++) {
	if ((c = pmNewContext(PM_CONTEXT_HOST, host)) >= 0)
	    pmDestroyContext(c);
    }
    return NULL;
}

static void *
archworker(void *x)
{
    int         i, c;
    char        *archive = x;

    for (i=0; i < 50000; i++) {
	if ((c = pmNewContext(PM_CONTEXT_ARCHIVE, archive)) >= 0)
	    pmDestroyContext(c);
    }
    return NULL;
}

int
main(int argc, char *argv[])
{
    pthread_t p1, p2;

    if (argc != 3) {
    	fprintf(stderr, "Usage: %s hostname archivename\n", argv[0]);
	exit(1);
    }

    pthread_create(&p1, NULL, &liveworker, argv[1]);
    pthread_create(&p2, NULL, &archworker, argv[2]);
    pthread_join(p2, NULL);
    pthread_join(p1, NULL);

    /* success, if we don't abort first! */
    printf("SUCCESS\n");
    exit(0);
}

