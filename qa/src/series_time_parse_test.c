/*
 * Copyright (c) 2024 Red Hat.
 *
 * Unit tests for pmproxy series API time parsing, covering the bugs
 * reported in issue #2503:
 *
 *  - Tier 1/2: negative relative offsets ("-30s", "-2m", "-1h", "-7d")
 *    fail when end sentinel is PM_MAX_TIME_T (the old broken behaviour).
 *    They MUST succeed when end is the current wall clock time.
 *
 *  - Tier 3: __pmtimespecParse() with an ISO-8601+Z string must not
 *    crash; it may return success or failure, but the process must live.
 *
 * These are UNIT tests — no Redis, no pmcd, no network required.
 * Exit 0 on success, non-zero if any assertion fails.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
static int passes = 0;

#define CHECK(expr, msg) \
    do { \
	if (!(expr)) { \
	    fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (msg)); \
	    failures++; \
	} else { \
	    printf("PASS: %s\n", (msg)); \
	    passes++; \
	} \
    } while (0)

/*
 * Attempt to parse a relative-time string using the given end boundary.
 * Returns the parsed timespec, or {-1, 0} on parse failure.
 */
static struct timespec
parse_with_end(const char *str, struct timespec *end)
{
    struct timespec	start = { 0, 0 };
    struct timespec	result = { -1, 0 };
    char		*errmsg = NULL;

    if (__pmtimespecParse(str, &start, end, &result, &errmsg) < 0) {
	if (errmsg) free(errmsg);
	result.tv_sec = -1;
    }
    return result;
}

/*
 * Confirm that parsing fails with the PM_MAX_TIME_T sentinel — this
 * documents the pre-fix broken behaviour and ensures the test itself
 * is actually testing something meaningful.
 */
static void
test_broken_sentinel_rejects_negative_offsets(void)
{
    struct timespec	broken_end = { PM_MAX_TIME_T, 0 };
    struct timespec	result;

    printf("\n--- Bug demonstration: PM_MAX_TIME_T sentinel breaks negative offsets ---\n");

    result = parse_with_end("-30s", &broken_end);
    CHECK(result.tv_sec < 0,
	  "-30s fails with PM_MAX_TIME_T end sentinel (pre-fix behaviour)");

    result = parse_with_end("-7d", &broken_end);
    CHECK(result.tv_sec < 0,
	  "-7d fails with PM_MAX_TIME_T end sentinel (pre-fix behaviour)");
}

/*
 * Confirm that negative relative offsets succeed when end=now — this is
 * the correct behaviour the fix to parsetime() in query.c enables.
 */
static void
test_negative_offsets_succeed_with_now_end(struct timespec *now)
{
    struct timespec	result;
    time_t		expected;

    printf("\n--- Fix verification: negative offsets work when end=now ---\n");

    /* -30s: result should be now minus 30 seconds */
    result = parse_with_end("-30s", now);
    CHECK(result.tv_sec > 0, "-30s parses without error when end=now");
    expected = now->tv_sec - 30;
    CHECK(result.tv_sec >= expected - 1 && result.tv_sec <= expected + 1,
	  "-30s result is approximately 30 seconds before now");

    /* -2m: result should be now minus 120 seconds */
    result = parse_with_end("-2m", now);
    CHECK(result.tv_sec > 0, "-2m parses without error when end=now");
    expected = now->tv_sec - 120;
    CHECK(result.tv_sec >= expected - 1 && result.tv_sec <= expected + 1,
	  "-2m result is approximately 2 minutes before now");

    /* -1h: result should be now minus 3600 seconds */
    result = parse_with_end("-1h", now);
    CHECK(result.tv_sec > 0, "-1h parses without error when end=now");
    expected = now->tv_sec - 3600;
    CHECK(result.tv_sec >= expected - 1 && result.tv_sec <= expected + 1,
	  "-1h result is approximately 1 hour before now");

    /* -7d: result should be now minus 7 * 86400 seconds */
    result = parse_with_end("-7d", now);
    CHECK(result.tv_sec > 0, "-7d parses without error when end=now");
    expected = now->tv_sec - (7 * 86400);
    CHECK(result.tv_sec >= expected - 1 && result.tv_sec <= expected + 1,
	  "-7d result is approximately 7 days before now");
}

/*
 * Positive relative offsets should still work — they're relative to start
 * regardless of the end sentinel.
 */
static void
test_positive_offsets_still_work(struct timespec *now)
{
    struct timespec	start = { 0, 0 };
    struct timespec	result;
    char		*errmsg = NULL;

    printf("\n--- Positive relative offsets (regression) ---\n");

    /* +30s relative to epoch zero = exactly 30 seconds */
    if (__pmtimespecParse("+30s", &start, now, &result, &errmsg) == 0) {
	CHECK(result.tv_sec == 30, "+30s from epoch yields tv_sec == 30");
    } else {
	CHECK(0, "+30s should parse successfully");
	if (errmsg) free(errmsg);
    }

    /* bare "30s" is also a positive offset */
    if (__pmtimespecParse("30s", &start, now, &result, &errmsg) == 0) {
	CHECK(result.tv_sec == 30, "30s (no sign) from epoch yields tv_sec == 30");
    } else {
	CHECK(0, "30s should parse successfully");
	if (errmsg) free(errmsg);
    }
}

/*
 * ISO-8601 with Z suffix — must not crash.  The exact return value
 * depends on getdate grammar support, which is fine; survival is mandatory.
 */
static void
test_iso8601_z_suffix_does_not_crash(struct timespec *now)
{
    struct timespec	start = { 0, 0 };
    struct timespec	result = { -1, 0 };
    char		*errmsg = NULL;
    int			sts;

    printf("\n--- ISO-8601 with Z suffix (crash safety) ---\n");

    sts = __pmtimespecParse("2024-01-15T10:30:00Z", &start, now, &result, &errmsg);
    printf("INFO: __pmtimespecParse(\"2024-01-15T10:30:00Z\") returned %d\n", sts);
    if (errmsg) { free(errmsg); errmsg = NULL; }

    /* If we reach this line, no crash — that's the pass condition */
    CHECK(1, "2024-01-15T10:30:00Z did not crash __pmtimespecParse");

    /* Malformed input must return error without crashing */
    sts = __pmtimespecParse("not-a-date", &start, now, &result, &errmsg);
    CHECK(sts < 0, "\"not-a-date\" returns error from __pmtimespecParse");
    if (errmsg) free(errmsg);
}

int
main(int argc, char *argv[])
{
    struct timespec	now;
    int			c, errflag = 0;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {
	case 'D':
	    if (pmSetDebug(optarg) < 0) {
		fprintf(stderr, "%s: unrecognized debug options (%s)\n",
			pmGetProgname(), optarg);
		errflag++;
	    }
	    break;
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr, "Usage: %s [-D debug]\n", pmGetProgname());
	return 1;
    }

    pmtimespecNow(&now);
    printf("series_time_parse_test: now = %ld\n", (long)now.tv_sec);

    test_broken_sentinel_rejects_negative_offsets();
    test_negative_offsets_succeed_with_now_end(&now);
    test_positive_offsets_still_work(&now);
    test_iso8601_z_suffix_does_not_crash(&now);

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return failures ? 1 : 0;
}
