/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * app2.c
 *
 * Sample program to demonstrate use of the PCP trace performance metrics
 * domain agent (PMDA(3)).  This agent needs to be installed before metrics
 * can be made available via the performance metrics namespace (pmns(5)),
 * and the Performance Metrics Collector Daemon (pmcd(1)).
 *
 * Once this program is running, the trace PMDA metrics & instances can be
 * viewed through PCP monitor tools such as pmchart(1), pmgadgets(1), and
 * pmview(1).  To view the help text associated with each of these metrics,
 * use:
 *   $ pminfo -tT trace
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <pcp/trace.h>


#define IO_UPPER_LIMIT		1000	/* I/O ops */
#define CPU_UPPER_LIMIT		0xffff	/* iterations */
#define TIME_UPPER_LIMIT	10	/* seconds */

static void io_sucker(void);
static void cpu_sucker(void);
static void time_sucker(void);
static char *prog;

int
main(int argc, char **argv)
{
    int	i, sts;

    prog = argv[0];
    srand48(time(0));
    /* uncomment this for debugging information */
    /* pmtracestate(PMTRACE_STATE_API|PMTRACE_STATE_COMMS|PMTRACE_STATE_PDU); */
    /* uncomment this to use the asynchronous protocol */
    /* pmtracestate(PMTRACE_STATE_ASYNC); */

    for (i = 0;; i++) {
	if ((sts = pmtracepoint("mainloop")) < 0) {
	    fprintf(stderr, "%s: mainloop point trace failed (%d): %s\n",
						prog, sts, pmtraceerrstr(sts));
	    exit(1);
	}
	switch(i % 3) {
	case 0:
	    time_sucker();
	    break;
	case 1:
	    io_sucker();
	    break;
	case 2:
	    cpu_sucker();
	    break;
	}
    }
}


static void
cpu_sucker(void)
{
    int		i, j, sts;
    double	array[100];
    long	iterations;

    if ((sts = pmtracebegin("cpu_sucker")) < 0) {
	fprintf(stderr, "%s: cpu_sucker begin (%d): %s\n",
		prog, sts, pmtraceerrstr(sts));
	return;
    }

    iterations = lrand48() % CPU_UPPER_LIMIT;
    memset((void *)array, 0, 100*sizeof(double));

    for (i = 0; i < iterations; i++)
	for (j = 0; j < 100; j++)
	    array[j] = (double)(j*iterations);

    if ((sts = pmtraceend("cpu_sucker")) < 0) {
	fprintf(stderr, "%s: cpu_sucker end (%d): %s\n",
		prog, sts, pmtraceerrstr(sts));
	return;
    }
}

static void
time_sucker(void)
{
    long	seconds;
    int		sts;

    if ((sts = pmtracebegin("time_sucker")) < 0) {
	fprintf(stderr, "%s: time_sucker start (%d): %s\n",
		prog, sts, pmtraceerrstr(sts));
	return;
    }

    seconds = lrand48() % TIME_UPPER_LIMIT;
    sleep((unsigned int)seconds);

    if ((sts = pmtraceend("time_sucker")) < 0) {
	fprintf(stderr, "%s: time_sucker end (%d): %s\n",
		prog, sts, pmtraceerrstr(sts));
	return;
    }
}

static void
io_sucker(void)
{
    long	characters;
    FILE	*foo;
    int		i, sts;

    if ((sts = pmtracebegin("io_sucker")) < 0) {
	fprintf(stderr, "%s: io_sucker start (%d): %s\n",
		prog, sts, pmtraceerrstr(sts));
	return;
    }

    if ((foo = fopen("/dev/null", "rw")) == NULL) {
	fprintf(stderr, "%s: io_sucker can't open /dev/null.\n", prog);
	return;
    }

    characters = lrand48() % IO_UPPER_LIMIT;
    for (i = 0; i < characters; i++) {
	fgetc(foo);
	fputc('!', foo);
    }
    fclose(foo);

    if ((sts = pmtraceend("io_sucker")) < 0) {
	fprintf(stderr, "%s: io_sucker end (%d): %s\n",
		prog, sts, pmtraceerrstr(sts));
	return;
    }
}
