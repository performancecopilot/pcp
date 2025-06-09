/*
 * Copyright (c) 2017 Ken McDonell.  All Rights Reserved.
 *
 * Used in qa/1166 ... not intended to be built in the src directory.
 *
 * Note: as of PCP 7.0 this is largely irrelevant because most of
 *       the deprecated symbols have been removed from libpcp.so.4
 */

#include <stdio.h>
#include <errno.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <sys/time.h>

#ifdef BINARY_COMPAT_TEST
/*
 * insert here declarations for old symbols that have been promoted,
 * so that we can compile the source code
 */
#else
/*
 * for source compatibility, there may be demoted symbols
 */
#include "libpcp.h"
/*
 * for source compatibility, deprecated.h should handle everything
 */
#include <pcp/deprecated.h>
#endif

#ifndef TMP
#define TMP "tmp"
#endif

#ifndef SEQ
#define SEQ "9999"
#endif

static pmOptions opts = {
    .short_options = "D:",
    .long_options = NULL,
    .short_usage = "[options]",
};

static pmDesc desc = {
    .pmid = 0,
    .type = PM_TYPE_U64,
    .indom = PM_INDOM_NULL,
    .sem = PM_SEM_COUNTER,
    .units = PMDA_PMUNITS(1, -1, 0, PM_SPACE_MBYTE, PM_TIME_SEC, 0),
};

int
main(int argc, char **argv)
{
    int			c;
    const char 		*p;
    FILE		*f;
    int			sts;
    char		*u;
    struct timeval	a;
    struct timeval	b;
    struct timespec	x;
    double		t;
    pmResult	*rp;
    char		buf[1024];
    char		*pcp_pmdas_dir;
    char		*dso_suffix;

    setlinebuf(stdout);
    setlinebuf(stderr);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    pmSetProgname("qa_libpcp_compat");

    printf("pmGetAPIConfig test: ");
    fflush(stdout);
    p = pmGetAPIConfig("lock_asserts");
    if (p != NULL &&
        (strcmp(p, "false") == 0 || strcmp(p, "true") == 0))
	printf("OK\n");
    else
	printf("FAIL: value \"%s\" for lock_asserts is not expected\n", p);

    printf("pmOpenLog test: expect to see standard log format\n");
    if ((f = fopen(TMP ".tmp", "w")) == NULL) {
	printf("Error: failed to create " TMP ".tmp\n");
	exit(1);
    }
    fflush(stdout);
    pmOpenLog(SEQ, TMP ".log", f, &sts);
    if (sts != 1) {
	printf("pmOpenLog failed: status=%d\n", sts);
	exit(1);
    }
    fprintf(f, "G'day cobber\n");
    fflush(f);
    sts = system("cat " TMP ".log");
    if (sts != 0) printf("system() returns %d?\n", sts);

    printf("pmNoMem test: expect to see a message\n");
    fflush(stdout);
    errno = ENOMEM;
    pmNoMem(SEQ, (size_t)123456, PM_RECOV_ERR);

    printf("pmNotifyErr test: expect to see standard message format\n");
    fflush(stdout);
    pmSyslog(0);
    pmNotifyErr(LOG_NOTICE, "Hullo %s! The answer is %d\n", "world", 42);

    printf("pmPrintDesc test:\n");
    pmPrintDesc(stdout, &desc);
    fflush(stdout);

    printf("pmtimeval* tests:\n");
    a.tv_sec = 233;
    a.tv_usec = 967000;
    b.tv_sec = 1000;
    b.tv_usec = 600000;
    t = pmtimevalAdd(&a, &b);
    printf("Add (expect 1234.567000): %.6f\n", t);
    pmtimevalInc(&a, &b);
    printf("Inc (expect 1234.567000): %d.%06d\n", (int)a.tv_sec, (int)a.tv_usec);
    pmtimevalInc(&a, &b);
    t = pmtimevalSub(&a, &b);
    printf("Sub (expect 1234.567000): %.6f\n", t);
    pmtimevalDec(&a, &b);
    printf("Dec (expect 1234.567000): %d.%06d\n", (int)a.tv_sec, (int)a.tv_usec);
    pmtimevalNow(&b);
    gettimeofday(&a, NULL);
    t = pmtimevalSub(&a, &b);
    if (t < 0.01) printf("Now: OK\n");
    else printf("Now: expected delta < 0.01, got %.6f\n", t);
    a.tv_sec = 1*3600 + 2*60 + 3;
    a.tv_usec = 456000;
    pmNewZone("UTC");
    printf("Stamp: (expect 01:02:03.456) ");
    pmtimevalPrint(stdout, &a);
    putchar('\n');
    x.tv_sec = 2*3600 + 1*60;
    x.tv_nsec = 123456789;
    printf("HighResStamp: (expect 02:01:00.123456789) ");
    pmtimespecPrint(stdout, &x);
    putchar('\n');
    fflush(stdout);

    printf("pmPathSeparator test: ");
    c = pmPathSeparator();
    if (c == '/' || c == '\\') printf("OK\n");
    else printf("FAIL %c (%02x) is not slash or backslash\n", 0xff & c, 0xff & c);

    printf("pmGetUsername test: ");
    u = NULL;
    sts = pmGetUsername(&u);
    if (u != NULL) printf("OK\n");
    else printf("FAIL sts=%d u==NULL\n", sts);

    printf("pmFreeResult test:\n");
    fflush(stdout);
    rp = (pmResult *)malloc(sizeof(*rp));
    rp->timestamp.tv_sec = 1;
    rp->timestamp.tv_nsec = 123456789;
    rp->numpmid = 0;
    /* nothing to test here, just diags */
    pmSetDebug("pdubuf,alloc");
    pmFreeResult(rp);

    printf("pmSpecLocalPMDA test:\n");
    fflush(stdout);
    u = pmSpecLocalPMDA("foo");
    if (u != NULL)
	printf("Expected error: pmSpecLocal(foo): %s\n", u);
    else
	printf("Error: expected error from pmSpecLocal(foo)\n");
    u = pmSpecLocalPMDA("clear");
    if (u != NULL)
	printf("Unexpected error: pmSpecLocal(clear): %s\n", u);
    else
	printf("pmSpecLocal(clear): OK\n");
    pcp_pmdas_dir = pmGetConfig("PCP_PMDAS_DIR");
    dso_suffix = pmGetConfig("DSO_SUFFIX");
    sprintf(buf, "add,30,%s/sample/pmda_sample.%s,sample_init", pcp_pmdas_dir, dso_suffix);
    u = pmSpecLocalPMDA(buf);
    if (u != NULL)
	printf("Unexpected error: pmSpecLocal(%s): %s\n", buf, u);
    else
	printf("pmSpecLocal(add,...): OK\n");

    /* need this to be last ... this cripples the process! */
    printf("pmSetProcessIdentity test: (expect failure)\n");
    fflush(stdout);
    sts = pmSetProcessIdentity("no-such-user");
    if (sts == 0) printf("OK\n");
    else printf("FAIL sts=%d\n", sts);

    return 0;
}
