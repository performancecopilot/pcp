/*
 * Copyright (c) 2015 Red Hat.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

static int rootfd;
static const char *sockname;
static pmLongOptions longopts[] = {
    PMOPT_DEBUG,
    { "container", 1, 'c', "NAME",
	"specify an individual container to be targetted" },
    { "process", 1, 'p', "PID",
	"specify the process identifier to be targetted" },
    { "socket", 1, 's', "PATH",
	"pmdaroot socket file [default $PCP_TMP_DIR/pmcd/root.socket]" },
    { "hostname", 0, 'H', NULL, "Lookup container hostname" },
    { "cgroup", 0, 'C', NULL, "Lookup container cgroup name" },
    { "pid", 0, 'P', NULL, "Lookup PID for a container" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};
static pmOptions opts = {
    .short_options = "D:c:p:s:CHP?",
    .long_options = longopts,
};

int
main(int argc, char **argv)
{
    int	c, sts;
    int	Cflag = 0;
    int	Hflag = 0;
    int	Pflag = 0;
    int	length = 0;
    char *contain = NULL;
    char buffer[BUFSIZ];

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'D':	/* enable debugging */
	    sts = pmSetDebug(opts.optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug options specification (%s)\n",
			pmProgname, opts.optarg);
		opts.errors++;
	    }
	    break;

	case 'c':	/* target container name */
	    contain = opts.optarg;
	    length = strlen(contain);
	    break;
	case 'p':	/* target process identifier */
	    contain = NULL;
	    length = atoi(opts.optarg);
	    break;

	case 's':	/* socket name */
	    sockname = opts.optarg;
	    break;

	case 'C':
	    Cflag = 1;
	    break;
	case 'H':
	    Hflag = 1;
	    break;
	case 'P':
	    Pflag = 1;
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

    if ((rootfd = pmdaRootConnect(sockname)) < 0) {
	fprintf(stderr, "pmdaRootConnect: %s\n", pmErrStr(rootfd));
	exit(1);
    }

    if (Hflag) {
	sts = pmdaRootContainerHostName(rootfd, contain, length, buffer, BUFSIZ);
	if (sts < 0) {
	    fprintf(stderr, "pmdaRootContainerHostName: %s\n", pmErrStr(sts));
	    exit(1);
	}
	printf("Hostname: %s\n", buffer);
    }
    if (Cflag) {
	sts = pmdaRootContainerCGroupName(rootfd, contain, length, buffer, BUFSIZ);
	if (sts < 0) {
	    fprintf(stderr, "pmdaRootContainerCGroupName: %s\n", pmErrStr(sts));
	    exit(1);
	}
	printf("CGroup: %s\n", buffer);
    }
    if (Pflag) {
	sts = pmdaRootContainerProcessID(rootfd, contain, length);
	if (sts < 0) {
	    fprintf(stderr, "pmdaRootContainerProcessID: %s\n", pmErrStr(sts));
	    exit(1);
	}
	printf("PID: %d\n", sts);
    }

    pmdaRootShutdown(rootfd);
    return 0;
}
