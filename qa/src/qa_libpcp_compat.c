/*
 * Copyright (c) 2017 Ken McDonell.  All Rights Reserved.
 *
 * Used in qa/1166 ... not intended to be built in the src directory.
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
extern const char *__pmGetAPIConfig(const char *);
extern FILE *__pmOpenLog(const char *, const char *, FILE *, int *);
extern void __pmNoMem(const char *, size_t, int);
extern void __pmNotifyErr(int, const char *, ...) __PM_PRINTFLIKE(2,3);
extern void __pmSyslog(int);
extern void __pmPrintDesc(FILE *, const pmDesc *);
extern void __pmtimevalNow(struct timeval *);
extern void __pmtimevalInc(struct timeval *, const struct timeval *);
extern void __pmtimevalDec(struct timeval *, const struct timeval *);
extern double __pmtimevalAdd(const struct timeval *, const struct timeval *);
extern double __pmtimevalSub(const struct timeval *, const struct timeval *);
extern double __pmtimevalToReal(const struct timeval *);
extern void __pmtimevalFromReal(double, struct timeval *);
extern void __pmPrintStamp(FILE *, const struct timeval *);
extern void __pmPrintHighResStamp(FILE *, const struct timespec *);
extern int __pmPathSeparator(void);
extern int __pmGetUsername(char **);
extern int __pmSetProcessIdentity(const char *);
extern void pmFreeHighResResult(pmHighResResult *);
extern char __pmSpecLocalPMDA(const char *);

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
    pmHighResResult	*rp;
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

    printf("__pmGetAPIConfig test: ");
    fflush(stdout);
    p = __pmGetAPIConfig("lock_asserts");
    if (p != NULL &&
        (strcmp(p, "false") == 0 || strcmp(p, "true") == 0))
	printf("OK\n");
    else
	printf("FAIL: value \"%s\" for lock_asserts is not expected\n", p);

    printf("__pmOpenLog test: expect to see standard log format\n");
    if ((f = fopen(TMP ".tmp", "w")) == NULL) {
	printf("Error: failed to create " TMP ".tmp\n");
	exit(1);
    }
    fflush(stdout);
    __pmOpenLog(SEQ, TMP ".log", f, &sts);
    if (sts != 1) {
	printf("__pmOpenLog failed: status=%d\n", sts);
	exit(1);
    }
    fprintf(f, "G'day cobber\n");
    fflush(f);
    sts = system("cat " TMP ".log");
    if (sts != 0) printf("system() returns %d?\n", sts);

    printf("__pmNoMem test: expect to see a message\n");
    fflush(stdout);
    errno = ENOMEM;
    __pmNoMem("SEQ", (size_t)123456, PM_RECOV_ERR);

    printf("__pmNotifyErr test: expect to see standard message format\n");
    fflush(stdout);
    __pmSyslog(0);
    __pmNotifyErr(LOG_NOTICE, "Hullo %s! The answer is %d\n", "world", 42);

    printf("__pmPrintDesc test:\n");
    __pmPrintDesc(stdout, &desc);
    fflush(stdout);

    printf("__pmtimeval* tests:\n");
    a.tv_sec = 233;
    a.tv_usec = 967000;
    b.tv_sec = 1000;
    b.tv_usec = 600000;
    t = __pmtimevalAdd(&a, &b);
    printf("Add (expect 1234.567000): %.6f\n", t);
    __pmtimevalInc(&a, &b);
    printf("Inc (expect 1234.567000): %d.%06d\n", (int)a.tv_sec, (int)a.tv_usec);
    __pmtimevalInc(&a, &b);
    t = __pmtimevalSub(&a, &b);
    printf("Sub (expect 1234.567000): %.6f\n", t);
    __pmtimevalDec(&a, &b);
    printf("Dec (expect 1234.567000): %d.%06d\n", (int)a.tv_sec, (int)a.tv_usec);
    __pmtimevalNow(&b);
    gettimeofday(&a, NULL);
    t = __pmtimevalSub(&a, &b);
    if (t < 0.01) printf("Now: OK\n");
    else printf("Now: expected delta < 0.01, got %.6f\n", t);
    a.tv_sec = 1*3600 + 2*60 + 3;
    a.tv_usec = 456000;
    pmNewZone("UTC");
    printf("Stamp: (expect 01:02:03.456) ");
    __pmPrintStamp(stdout, &a);
    putchar('\n');
    x.tv_sec = 2*3600 + 1*60;
    x.tv_nsec = 123456789;
    printf("HighResStamp: (expect 02:01:00.123456789) ");
    __pmPrintHighResStamp(stdout, &x);
    putchar('\n');
    fflush(stdout);

    printf("__pmPathSeparator test: ");
    c = __pmPathSeparator();
    if (c == '/' || c == '\\') printf("OK\n");
    else printf("FAIL %c (%02x) is not slash or backslash\n", 0xff & c, 0xff & c);

    printf("__pmGetUsername test: ");
    u = NULL;
    sts = __pmGetUsername(&u);
    if (u != NULL) printf("OK\n");
    else printf("FAIL sts=%d u==NULL\n", sts);

    printf("pmFreeHighResResult test:\n");
    fflush(stdout);
    rp = (pmHighResResult *)malloc(sizeof(*rp));
    rp->timestamp.tv_sec = 1;
    rp->timestamp.tv_nsec = 123456789;
    rp->numpmid = 0;
    /* nothing to test here, just diags */
    pmSetDebug("pdubuf");
    pmFreeHighResResult(rp);

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
    printf("__pmSetProcessIdentity test: (expect failure)\n");
    fflush(stdout);
    sts = __pmSetProcessIdentity("no-such-user");
    if (sts == 0) printf("OK\n");
    else printf("FAIL sts=%d\n", sts);

    return 0;
}
