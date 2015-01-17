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
    { "ipc", 0, 'i', NULL, "Enter the IPC namespace of the target" },
    { "uts", 0, 'u', NULL, "Enter the UTS namespace of the target" },
    { "net", 0, 'n', NULL, "Enter the NET namespace of the target" },
    { "mnt", 0, 'm', NULL, "Enter the MNT namespace of the target" },
    { "user", 0, 'U', NULL, "Enter the USER namespace of the target" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};
static pmOptions opts = {
    .short_options = "D:c:p:s:iunmU?",
    .long_options = longopts,
};

static char *
namespacestr(int nsflags)
{
    static char buffer[256];
    int length;

    memset(buffer, 0, sizeof(buffer));
    if (nsflags & PMDA_NAMESPACE_IPC)
	strcat(buffer, "ipc,");
    if (nsflags & PMDA_NAMESPACE_UTS)
	strcat(buffer, "uts,");
    if (nsflags & PMDA_NAMESPACE_NET)
	strcat(buffer, "net,");
    if (nsflags & PMDA_NAMESPACE_MNT)
	strcat(buffer, "mnt,");
    if (nsflags & PMDA_NAMESPACE_USER)
	strcat(buffer, "user,");
    
    /* overwrite the final comma */
    if ((length = strlen(buffer)) > 0)
	buffer[length-1] = '\0';

    return buffer;
}

int
main(int argc, char **argv)
{
    int	c, sts;
    int	process = 0;
    int	nsflags = 0;
    char *contain = NULL;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'D':	/* enable debugging */
	    sts = __pmParseDebug(opts.optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
			pmProgname, opts.optarg);
		opts.errors++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'c':	/* target container name */
	    contain = opts.optarg;
	    break;
	case 'p':	/* target process identifier */
	    process = atoi(opts.optarg);
	    break;

	case 's':	/* socket name */
	    sockname = opts.optarg;
	    break;

	case 'i':
	    nsflags |= PMDA_NAMESPACE_IPC;
	    break;
	case 'u':
	    nsflags |= PMDA_NAMESPACE_UTS;
	    break;
	case 'n':
	    nsflags |= PMDA_NAMESPACE_NET;
	    break;
	case 'm':
	    nsflags |= PMDA_NAMESPACE_MNT;
	    break;
	case 'U':
	    nsflags |= PMDA_NAMESPACE_USER;
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

    if (!contain && !process) {
	fprintf(stderr, "Must specify target --container or --process\n");
	exit(1);
    }
    if (contain && process) {
	fprintf(stderr, "Cannot specify both --container or --process\n");
	exit(1);
    }

    if ((rootfd = pmdaRootConnect(sockname)) < 0) {
	fprintf(stderr, "pmdaRootConnect: %s\n", pmErrStr(rootfd));
	exit(1);
    }

    if (contain && nsflags) {
	if ((sts = pmdaEnterContainerNameSpaces(rootfd, contain, nsflags)) < 0) {
	    fprintf(stderr, "pmdaEnterContainerNameSpaces: %s\n", pmErrStr(sts));
	    exit(1);
	}
	printf("Success entering container \"%s\" namespaces: %s", contain,
		namespacestr(nsflags));
	if ((sts = pmdaLeaveNameSpaces(rootfd, nsflags)) < 0) {
	    fprintf(stderr, "pmdaLeaveNameSpaces: %s\n", pmErrStr(sts));
	    exit(1);
	}
	printf("Success leaving container \"%s\" namespaces: %s", contain,
		namespacestr(nsflags));
    }
    else if (process && nsflags) {
	if ((sts = pmdaEnterProcessNameSpaces(rootfd, process, nsflags)) < 0) {
	    fprintf(stderr, "pmdaEnterProcessNameSpaces: %s\n", pmErrStr(sts));
	    exit(1);
	}
	printf("Successfully entered PID %d namespaces: %s", process,
		namespacestr(nsflags));
	if ((sts = pmdaLeaveNameSpaces(rootfd, nsflags)) < 0) {
	    fprintf(stderr, "pmdaLeaveNameSpace: %s\n", pmErrStr(sts));
	    exit(1);
	}
	printf("Success leaving PID %d namespaces: %s", process,
		namespacestr(nsflags));
    }

    pmdaRootShutdown(rootfd);
    return 0;
}
