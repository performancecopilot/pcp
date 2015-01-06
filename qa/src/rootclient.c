/*
 * Copyright (c) 2015 Red Hat.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/pmda.h>

static int sockfd;
static const char *sockname;
static pmLongOptions longopts[] = {
    PMOPT_DEBUG,
    { "socket", 1, 's', "PATH", "Unix domain socket file [default $PCP_TMP_DIR/pmcd/root.socket]" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};
static pmOptions opts = {
    .short_options = "D:s:?",
    .long_options = longopts,
};

int
main(int argc, char **argv)
{
    int	c;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 's':	/* socket name */
	    sockname = opts.optarg;
	    break;

	case '?':
	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if ((sockfd = pmdaRootConnect(sockname)) < 0) {
	fprintf(stderr, "pmdaRootConnect: %s\n", pmErrStr(sockfd));
	exit(1);
    }

    pmdaRootShutdown(sockfd);
    return 0;
}
