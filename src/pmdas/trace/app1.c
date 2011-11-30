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
 * app1.c
 *
 * Simple program to demonstrate use of the PCP trace performance metrics
 * domain agent (PMDA(3)).  This agent needs to be installed before metrics
 * can be made available via the performance metrics namespace (PMNS(4)),
 * and the Performance Metrics Collector Daemon (PMCD(1)).
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
#include <pcp/trace.h>


int
main(int argc, char **argv)
{
    int		sts;
    char	*prog;

    prog = argv[0];
    sts = pmtracestate(PMTRACE_STATE_API|PMTRACE_STATE_COMMS|PMTRACE_STATE_PDU);
    fprintf(stderr, "%s: start: %s (state=0x%x)\n", prog,
	pmtraceerrstr(0), sts);	/* force call to all library symbols */

    if ((sts = pmtracebegin("simple")) < 0) {
	fprintf(stderr, "%s: pmtracebegin error: %s\n",
		prog, pmtraceerrstr(sts));
	exit(1);
    }
    if (sleep(2) != 0) {
	fprintf(stderr, "%s: sleep prematurely awaken\n", prog);
	pmtraceabort("simple");
    }
    if ((sts = pmtraceend("simple")) < 0) {
	fprintf(stderr, "%s: pmtraceend error: %s\n",
		prog, pmtraceerrstr(sts));
	exit(1);
    }

    if ((sts = pmtracebegin("ascanbe")) < 0) {
	fprintf(stderr, "%s: pmtracebegin error: %s\n",
		prog, pmtraceerrstr(sts));
	exit(1);
    }
    sleep(1);
    if ((sts = pmtraceend("ascanbe")) < 0) {
	fprintf(stderr, "%s: pmtraceend error: %s\n",
		prog, pmtraceerrstr(sts));
	exit(1);
    }

    if ((sts = pmtraceobs("observe", 101.0)) < 0) {
	fprintf(stderr, "%s: pmtraceobs error: %s\n",
		prog, pmtraceerrstr(sts));
	exit(1);
    }

    if ((sts = pmtracecounter("counter", 101.1)) < 0) {
	fprintf(stderr, "%s: pmtracecounter error: %s\n",
		prog, pmtraceerrstr(sts));
	exit(1);
    }

    if ((sts = pmtracepoint("imouttahere")) < 0) {
	fprintf(stderr, "%s: pmtracepoint error: %s\n",
		prog, pmtraceerrstr(sts));
	exit(1);
    }

    exit(0);
}
