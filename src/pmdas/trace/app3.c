/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
 * app3.c
 *
 * Parallel program to demonstrate use of the PCP trace performance metrics
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
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>
#include <pcp/trace.h>


#define IO_UPPER_LIMIT		1000	/* I/O ops */
#define CPU_UPPER_LIMIT		0xffff	/* iterations */
#define TIME_UPPER_LIMIT	10	/* seconds */

static void * pio_sucker(void *);
static void * pcpu_sucker(void *);
static void * ptime_sucker(void *);
static char *prog;

int
main(int argc, char **argv)
{
    int		i;
    pthread_t p[3];

    prog = argv[0];

    pthread_create(p,   NULL, pio_sucker, NULL);
    pthread_create(p+1, NULL, pcpu_sucker, NULL);
    pthread_create(p+2, NULL, ptime_sucker, NULL);

    for (i=0; i < 3; i++) {
	wait(NULL);
	fprintf(stderr, "%s: reaped sproc #%d\n", prog, i);
    }

    exit(0);
}


static void *
pcpu_sucker(void *dummy)
{
    int		i, j, loops, sts;
    double	array[100];
    long	iterations;

    for (loops = 0; loops < 10; loops++) {
	if ((sts = pmtracebegin("pcpu_sucker")) < 0) {
	    fprintf(stderr, "%s: pcpu_sucker begin (%d): %s\n",
		    prog, sts, pmtraceerrstr(sts));
	    return NULL;
	}

	iterations = lrand48() % CPU_UPPER_LIMIT;
	memset((void *)array, 0, 100*sizeof(double));

	for (i = 0; i < iterations; i++)
	    for (j = 0; j < 100; j++)
		array[j] = (double)(j*iterations);

	if ((sts = pmtraceend("pcpu_sucker")) < 0) {
	    fprintf(stderr, "%s: pcpu_sucker end (%d): %s\n",
		    prog, sts, pmtraceerrstr(sts));
	    return NULL;
	}
    }
    fprintf(stderr, "%s: finished %d cpu-bound iterations.\n", prog, loops);
    return NULL;
}

static void *
ptime_sucker(void *dummy)
{
    long	seconds;
    int		loops, sts;

    for (loops = 0; loops < 10; loops++) {
	if ((sts = pmtracebegin("ptime_sucker")) < 0) {
	    fprintf(stderr, "%s: ptime_sucker start (%d): %s\n",
		    prog, sts, pmtraceerrstr(sts));
	    return NULL;
	}

	seconds = lrand48() % TIME_UPPER_LIMIT;
	sleep((unsigned int)seconds);

	if ((sts = pmtraceend("ptime_sucker")) < 0) {
	    fprintf(stderr, "%s: ptime_sucker end (%d): %s\n",
		    prog, sts, pmtraceerrstr(sts));
	    return NULL;
	}
    }
    fprintf(stderr, "%s: finished %d timer iterations.\n", prog, loops);
    return NULL;
}

static void *
pio_sucker(void *dummy)
{
    long	characters;
    FILE	*foo;
    int		i, loops, sts;

    for (loops = 0; loops < 10; loops++) {
	if ((sts = pmtracebegin("pio_sucker")) < 0) {
	    fprintf(stderr, "%s: pio_sucker start (%d): %s\n",
		    prog, sts, pmtraceerrstr(sts));
	    return NULL;
	}

	if ((foo = fopen("/dev/null", "rw")) == NULL) {
	    fprintf(stderr, "%s: pio_sucker can't open /dev/null.\n", prog);
	    return NULL;
	}

	characters = lrand48() % IO_UPPER_LIMIT;
	for (i = 0; i < characters; i++) {
	    fgetc(foo);
	    fputc('!', foo);
	}
	fclose(foo);

	if ((sts = pmtraceend("pio_sucker")) < 0) {
	    fprintf(stderr, "%s: pio_sucker end (%d): %s\n",
		    prog, sts, pmtraceerrstr(sts));
	    return NULL;
	}
    }
    fprintf(stderr, "%s: finished %d io-bound iterations.\n", prog, loops);
    return NULL;
}
