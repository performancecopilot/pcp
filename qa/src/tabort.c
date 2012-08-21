#include <pcp/trace.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
    int	sts;

    if ((sts = pmtracebegin("foo")) < 0) {
	fprintf(stderr, "pmtracebegin failed: %s\n", pmtraceerrstr(sts));
	exit(1);
    }

    if ((sts = pmtraceabort("foo")) < 0) {
	fprintf(stderr, "pmtraceabort failed: %s\n", pmtraceerrstr(sts));
	exit(1);
    }

    if ((sts = pmtracebegin("foo")) < 0) {
	fprintf(stderr, "pmtraceabort isn't working properly!\n");
	exit(1);
    }

    if ((sts = pmtraceend("foo")) < 0) {
	fprintf(stderr, "pmtraceend failed: %s\n", pmtraceerrstr(sts));
	exit(1);
    }

    exit(0);
}
