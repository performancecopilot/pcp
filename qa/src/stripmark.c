#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

/*
 * filter to copy archive.0 and strip mark records
 */

int
main(int argc, char *argv[])
{
    int		*len;
    int		buflen;
    char	*buf ;
    int		in;
    int		out;
    int		nb;
    int		sts;

    if (argc != 3) {
	fprintf(stderr, "Usage: stripmark in.0 out.0\n");
	exit(1);
    }

    if ((in = open(argv[1], O_RDONLY)) < 0) {
	fprintf(stderr, "Failed to open %s: %s\n", argv[1], strerror(errno));
	exit(1);
    }
    if ((out = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0) {
	fprintf(stderr, "Failed to create %s: %s\n", argv[2], strerror(errno));
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
	if (htonl(*len) > sizeof(__pmPDUHdr) - sizeof(*len) + sizeof(__pmTimeval) + sizeof(int)) {
	    sts = write(out, buf, htonl(*len));
	    if (sts != htonl(*len)) {
		fprintf(stderr, "Error: write %d returns %d\n", htonl(*len), sts);
		exit(1);
	    }
	}
	else
	    fprintf(stderr, "Skip mark @ byte %d into input\n", (int)(lseek(in, 0, SEEK_CUR) - sizeof(*len)));
    }
    return 0;
}
