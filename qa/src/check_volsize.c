/*
 * Exercise pmiSetVolumeSize() - write enough records to trigger at least
 * one volume rotation, verify the callback fires with the correct path,
 * and confirm that pmiSetVolumeSize rejects a threshold at or below the
 * archive label size.
 *
 * The test is written to produce deterministic pass/fail output regardless
 * of the exact rotation count (which depends on internal record sizes that
 * may vary between PCP versions).  pmlogcheck in the QA shell script is
 * the authoritative check for multi-volume archive integrity.
 *
 * Copyright (c) 2026 Red Hat.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/import.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int	rotate_count;
static char	first_rotated[MAXPATHLEN];

static void
on_rotate(const char *vol_path)
{
    rotate_count++;
    if (rotate_count == 1)
	strncpy(first_rotated, vol_path, sizeof(first_rotated) - 1);
    /* callbacks are not printed — rotation count is non-deterministic */
}

static void
check(int sts, const char *name)
{
    if (sts < 0)
	fprintf(stderr, "%s: Error: %s\n", name, pmiErrStr(sts));
    else {
	fprintf(stderr, "%s: OK", name);
	if (sts != 0) fprintf(stderr, " ->%d", sts);
	fputc('\n', stderr);
    }
}

int
main(int argc, char **argv)
{
    int		sts;
    int		ctx;
    int		i;
    int		handle;
    struct stat	st;
    /*
     * Threshold must exceed the archive label size (v2=124, v3=800).
     * 1500 bytes produces several rotations across 50 writes of a u32
     * counter without being so small that every write rotates.
     */
    size_t	threshold = 1500;

    pmSetProgname(argv[0]);

    /* === Phase 1: basic volume rotation === */
    ctx = pmiStart("volsizetest", 0);
    check(ctx, "pmiStart");

    sts = pmiSetHostname("testhost.example.com");
    check(sts, "pmiSetHostname");

    sts = pmiSetTimezone("UTC");
    check(sts, "pmiSetTimezone");

    sts = pmiAddMetric("qa.volsize.counter",
		       PM_ID_NULL, PM_TYPE_U32, PM_INDOM_NULL,
		       PM_SEM_COUNTER, pmiUnits(0, 0, 0, 0, 0, 0));
    check(sts, "pmiAddMetric");

    handle = pmiGetHandle("qa.volsize.counter", NULL);
    check(handle, "pmiGetHandle");

    sts = pmiSetVolumeSize(threshold, on_rotate);
    check(sts, "pmiSetVolumeSize");

    for (i = 1; i <= 50; i++) {
	pmAtomValue av;

	av.ul = (unsigned int)i;
	sts = pmiPutAtomValueHandle(handle, &av);
	if (sts < 0) { check(sts, "pmiPutAtomValueHandle"); break; }
	sts = pmiWrite((unsigned long long)i * 10, 0);
	if (sts < 0) { check(sts, "pmiWrite"); break; }
    }
    check(0, "pmiWrite loop");

    sts = pmiEnd();
    check(sts, "pmiEnd");

    if (rotate_count == 0) {
	fprintf(stderr, "FAIL: no volume rotation triggered\n");
	exit(1);
    }
    fprintf(stderr, "volume rotation: OK\n");

    if (stat("volsizetest.0", &st) != 0) {
	fprintf(stderr, "FAIL: volsizetest.0 not found\n");
	exit(1);
    }
    fprintf(stderr, "volsizetest.0: present\n");

    if (stat("volsizetest.1", &st) != 0) {
	fprintf(stderr, "FAIL: volsizetest.1 not found\n");
	exit(1);
    }
    fprintf(stderr, "volsizetest.1: present\n");

    /* Each completed volume must not grossly exceed the threshold */
    if ((size_t)st.st_size > threshold * 3) {
	fprintf(stderr, "FAIL: volume size %lld far exceeds threshold %zu\n",
		(long long)st.st_size, threshold);
	exit(1);
    }
    fprintf(stderr, "volume size bound: OK\n");

    if (strstr(first_rotated, "volsizetest.0") == NULL) {
	fprintf(stderr, "FAIL: first callback path '%s' missing 'volsizetest.0'\n",
		first_rotated);
	exit(1);
    }
    fprintf(stderr, "callback path: OK\n");

    /* === Phase 2: label-size guard — threshold at/below label must fail === */
    ctx = pmiStart("guardtest", 0);
    if (ctx < 0) { check(ctx, "pmiStart (guard)"); exit(1); }

    sts = pmiSetVolumeSize(1, on_rotate);
    if (sts >= 0) {
	fprintf(stderr, "FAIL: pmiSetVolumeSize(1) should have failed\n");
	exit(1);
    }
    fprintf(stderr, "label size guard: OK\n");

    sts = pmiSetVolumeSize(0, NULL);
    check(sts, "pmiSetVolumeSize(0) disable");

    pmiEnd();

    exit(0);
}
