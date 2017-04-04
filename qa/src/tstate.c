#include <stdio.h>
#include <stdlib.h>
#include <pcp/trace.h>


int
tracestate(int flag)
{
    int	tmp = pmtracestate(0);
    int	var;

    tmp |= flag;
    var = pmtracestate(tmp);
    fprintf(stderr, "tstate: state now is 0x%x\n", tmp);
    return var;
}

int
main(int argc, char **argv)
{
    int	sts;

    if (argc != 2) {
	fprintf(stderr, "tstate: takes one argument.\n");
	exit(1);
    }

    /*
     * this exercises the state setting routine in libpcp_trace,
     * particularly the async protocol flag setting, since this
     * is a little more involved.
     */

    if (argv[1][0] == '1') {
	fprintf(stderr, "--- Test 1 ---\n");
	tracestate(PMTRACE_STATE_API);
	tracestate(PMTRACE_STATE_PDU | PMTRACE_STATE_PDUBUF);
	sts = pmtracepoint("tstate1");	/* no longer able to change protocol */
	if (sts < 0) {
	    fprintf(stderr, "tstate pmtracepoint: %s\n", pmtraceerrstr(sts));
	    exit(1);
	}
	fprintf(stderr, "state should be only ASYNC 0x%x ...\n", PMTRACE_STATE_ASYNC);
	tracestate(PMTRACE_STATE_ASYNC);
    }
    else if (argv[1][0] == '2') {
	fprintf(stderr, "--- Test 2 ---\n");
	tracestate(PMTRACE_STATE_ASYNC);
	fprintf(stderr, "state should be same as previous ...\n");
	tracestate(PMTRACE_STATE_ASYNC);
    }
    else if (argv[1][0] == '3') {
	fprintf(stderr, "--- Test 3 ---\n");
	pmtracepoint("tstate3");
	fprintf(stderr, "state change should fail ... ");
	if ((sts = pmtracestate(PMTRACE_STATE_ASYNC)) < 0)
	    fprintf(stderr, "OK\n");
	else
	    fprintf(stderr, "urk - non-negative returned (%d)!\n", sts);
    }
    else if (argv[1][0] == '4') {
	fprintf(stderr, "--- Test 4 ---\n");
	tracestate(PMTRACE_STATE_ASYNC|PMTRACE_STATE_API);
	sts = pmtracepoint("tstate4");
	if (sts < 0) {
	    fprintf(stderr, "tstate pmtracepoint: %s\n", pmtraceerrstr(sts));
	    exit(1);
	}
	fprintf(stderr, "change to async only (0x%x)...\n", PMTRACE_STATE_ASYNC);
	tracestate(PMTRACE_STATE_ASYNC);
    }
    else {
	fprintf(stderr, "Don't know what to do with \"%s\"\n", argv[1]);
	exit(1);
    }

    exit(0);
}
