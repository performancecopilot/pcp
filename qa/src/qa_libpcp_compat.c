/*
 * Copyright (c) 2017 Ken McDonell.  All Rights Reserved.
 *
 * Used in qa/1166 ... not intended to be built in the src directory.
 */

#include <stdio.h>
#include <errno.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>

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
#else
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

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,		/* -D */
    PMAPI_OPTIONS_END
};

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
    int		c;
    const char *p;
    FILE	*f;
    int		sts;

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
    system("cat " TMP ".log");

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

    return 0;
}
