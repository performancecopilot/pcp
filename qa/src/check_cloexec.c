/*
 * Verify that AF_UNIX sockets created by __pmCreateUnixSocket()
 * have FD_CLOEXEC set.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <fcntl.h>

int
main(int argc, char **argv)
{
    int		fd, flags;

    pmSetProgname(argv[0]);

    fd = __pmCreateUnixSocket();
    if (fd < 0) {
	fprintf(stderr, "Error: __pmCreateUnixSocket failed: %s\n",
	    pmErrStr(fd));
	return 1;
    }

    flags = fcntl(fd, F_GETFD);
    if (flags < 0) {
	fprintf(stderr, "Error: fcntl F_GETFD failed\n");
	close(fd);
	return 1;
    }

    if (flags & FD_CLOEXEC)
	printf("FD_CLOEXEC is set\n");
    else
	printf("FAIL: FD_CLOEXEC is NOT set\n");

    close(fd);
    return 0;
}
