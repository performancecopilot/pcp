/*
 * exercise multi-threaded concurrent context opens
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <strings.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h> /* for __pmPathSeparator */
#include <pthread.h>


void _pcp_warn(int sts, const char* file, int line)
{
    char message[512];
    
    if (sts == 0)
        return;

    fprintf(stderr, "warn fail %s:%d %d %s\n", file, line, sts, pmErrStr_r(sts, message, sizeof(message)));
    fflush(stderr);
}


#define pcp_warn(x) _pcp_warn(x,__FILE__,__LINE__)


struct workitem /* one per thread */
{
    const char *host_or_archive_name; /* pointer into argv[] */
    int ncpu; /* desired output */
};


void*
thread_fn(void *data)
{
    struct workitem *work = (struct workitem *)data;
    pmFG fg;
    int rc, rc2;
    pmAtomValue ncpu;

    if (rindex (work->host_or_archive_name, __pmPathSeparator()) != NULL) /* path name? */
        rc = pmCreateFetchGroup(&fg, PM_CONTEXT_ARCHIVE, work->host_or_archive_name);
    else
        rc = pmCreateFetchGroup(&fg, PM_CONTEXT_HOST, work->host_or_archive_name);
    pcp_warn (rc);
    if (rc) {
        work->ncpu = rc;
        return NULL;
    }

    rc = pmExtendFetchGroup_item(fg, "hinv.ncpu", NULL, NULL, &ncpu, PM_TYPE_32, & rc2);
    if (rc)
        work->ncpu = rc; /* metric not found perhaps */
    else {
        rc = pmFetchGroup(fg);
        /* BZ1325037 precludes us from testing for error codes reliably. */
        /* pcp_warn (rc < 0 ? rc : 0); */
        /* pcp_warn (rc2); */
        work->ncpu = rc2 ? rc2 : ncpu.l; /* might be negative */
    }

    rc = pmDestroyFetchGroup(fg);
    pcp_warn (rc);

    return NULL;
}


int
main(int argc, char **argv)
{
    struct workitem *work;
    pthread_t *tids;
    int i;
    int rc;
    
    tids = malloc(sizeof(pthread_t) * argc);
    assert (tids != NULL);
    work = malloc(sizeof(struct workitem) * argc);
    assert (work != NULL);

    alarm (30); /* somewhat longer than $PMCD_CONNECT_TIMEOUT */
    
    for (i=0; i<argc-1; i++) {
        work[i].host_or_archive_name = argv[i+1];
        rc = pthread_create(& tids[i], NULL, &thread_fn, &work[i]);
        pcp_warn (rc);
        assert (rc == 0);
    }

    for (i=0; i<argc-1; i++) {
        rc = pthread_join (tids[i], NULL);
        /* NB: pthread_cancel not appropriate or necessary. */
        pcp_warn (rc);
    }

    for (i=0; i<argc-1; i++) {
        printf("%s %d\n", work[i].host_or_archive_name, work[i].ncpu);
    }

    exit(0);
}
