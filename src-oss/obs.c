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

    if ((sts = pmtraceobs("simple", 6.09)) < 0) {
	fprintf(stderr, "%s: (1) pmtraceobs error: %s\n",
		prog, pmtraceerrstr(sts));
	exit(1);
    }
    if ((sts = pmtraceobs("test", 888888.8888)) < 0) {
	fprintf(stderr, "%s: (2) pmtraceobs error: %s\n",
		prog, pmtraceerrstr(sts));
	exit(1);
    }

    if ((sts = pmtraceobs("simple", 76)) < 0) {
	fprintf(stderr, "%s: (3) pmtraceobs error: %s\n",
		prog, pmtraceerrstr(sts));
	exit(1);
    }

    exit(0);
}
