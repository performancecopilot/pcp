#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

/*
 * filter to count mark records
 */

int
main(int argc, char *argv[])
{
    int		*len;
    int		buflen;
    char	*buf ;
    int		in;
    int		nb;

    if (argc != 2) {
	fprintf(stderr, "Usage: countmark in.0\n");
	exit(1);
    }

    if ((in = open(argv[1], O_RDONLY)) < 0) {
	fprintf(stderr, "Failed to open %s: %s\n", argv[1], strerror(errno));
	exit(1);
    }
    buflen = sizeof(*len);
    buf = (char *)malloc(buflen);
    len = (int *)buf;

    for ( ; ; ) {
	if ((nb = read(in, buf, sizeof(*len))) != sizeof(*len)) {
	    if (nb == 0) break;
	    if (nb < 0)
		fprintf(stderr, "read error: %s\n", strerror(errno));
	    else
		fprintf(stderr, "read error: expected %d bytes, got %d\n",
			(int)sizeof(*len), nb);
	    exit(1);
	}
	if (htonl(*len) > buflen) {
	    buflen = htonl(*len);
	    buf = (char *)realloc(buf, buflen);
	    len = (int *)buf;
	}
	if ((nb = read(in, &buf[sizeof(*len)], htonl(*len)-sizeof(*len))) != htonl(*len)-sizeof(*len)) {
	    if (nb == 0)
		fprintf(stderr, "read error: end of file\n");
	    else if (nb < 0)
		fprintf(stderr, "read error: %s\n", strerror(errno));
	    else
		fprintf(stderr, "read error: expected %d bytes, got %d\n",
			(int)(htonl(*len)-sizeof(*len)), nb);
	    exit(1);
	}
	if (htonl(*len) > sizeof(__pmPDUHdr) - sizeof(*len) + sizeof(pmTimeval) + sizeof(int))
	    continue;
	fprintf(stderr, "<mark> @ byte offset %d\n", (int)(lseek(in, 0, SEEK_CUR) - sizeof(*len)));
    }
    return 0;
}
