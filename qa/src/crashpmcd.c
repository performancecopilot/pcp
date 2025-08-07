/*
 * Crashes pmcd on IRIX. Linux seems to be OK. PV 935490.
 */
#include <pcp/pmapi.h>
#include "libpcp.h"

static __pmPDUHdr hdr;
static char *target;
static uint16_t port = SERVER_PORT;

void
try(int len)
{
    int fd;
    int sts;
    static int first = 1;
    static struct sockaddr_in  myAddr;
    static struct hostent*     servInfo;
    char buf[256];

    if (first) {
	first = 0;
	if ((servInfo = gethostbyname(target)) == NULL) {
	    fprintf(stderr, "host \"%s\" unknown\n", target);
	    exit(1);
	}
	memset(&myAddr, 0, sizeof(myAddr));
	myAddr.sin_family = AF_INET;
	memcpy(&myAddr.sin_addr, servInfo->h_addr, servInfo->h_length);
	myAddr.sin_port = htons(port);
    }

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	fprintf(stderr, "socket failed: %s\n", pmErrStr(errno));
	return;
    }

    if ((sts = connect(fd, (struct sockaddr*) &myAddr, sizeof(myAddr))) < 0) {
	fprintf(stderr, "connect failed: %s\n", pmErrStr(sts));
	close(fd);
	return;
    }

    if ((sts = write(fd, &hdr, len)) != len) {
	fprintf(stderr, "write failed: %s\n", pmErrStr(sts));
	close(fd);
	return;
    }
    sts = read(fd, buf, sizeof(buf));
    if (sts < 0) {
	/* in this case don't really care about the return code from read() */
	;
    }
    close(fd);
}

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,	/* -D */
    { "port", 1, 'p', "PORT", "pmcd port [default 44321]" },
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_BOUNDARIES | PM_OPTFLAG_STDOUT_TZ,
    .short_options = "D:p:?",
    .long_options = longopts,
    .short_usage = "[options] [hostname]",
};

int
main(int argc, char *argv[])
{
    int		j;
    int		k;
    char	c;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.errors || opts.optind > argc) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    if (opts.guiport) {
	/* we're overloading "guiport" to mean pmcd port here */
	port = atoi(opts.guiport_optarg);
    }

    /* non-flag args are argv[opts.optind] ... argv[argc-1] */
    if (opts.optind < argc)
	target = argv[opts.optind];
    else
	target = "localhost";

    hdr.from = htonl(12345);

    for (k = -1; k <= 12; k++) {
	hdr.len = htonl(k);
	hdr.type = htonl(0x55aa0000);
	for (j = 0; j <= 12; j++) {
	    try(j);
	}
    }

    for (k = 0; k <= 12; k++) {
	hdr.len = htonl(k<<24);
	hdr.type = htonl(0x000055aa);
	for (j = 0; j <= 12; j++) {
	    try(j);
	}
    }

    return 0;
}
