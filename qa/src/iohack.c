/*
 * simple I/O checker
 * - write on fd i, 0 <= i <= 5
 * - read on fd i, 0 <= i <= 5
 *
 * Used with exectest when debugging pipe+fork+exec on Windows.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

main(int argc, char **argv)
{
    int 	len;
    char 	*str = "foobar\n";
    FILE	*ferr = fopen("iohack.err", "w");
    int		fd;
    int		c;

    setlinebuf(ferr);

    for (fd = 0; fd <= 5; fd++) {
	if (fd == fileno(ferr))
	    continue;
	len = write(fd, str, (int)strlen(str));
	if (len != strlen(str)) {
	    fprintf(ferr, "write(%d, ...): botch, len=%d not %d\n", fd, len, (int)strlen(str));
	    fprintf(ferr, "errno=%d %s\n", errno, strerror(errno));
	}
	else
	    fprintf(ferr, "write(%d, ...): OK\n", fd);
    }

    fputc('\n', ferr);

    for (fd = 0; fd <= 5; fd++) {
	if (fd == fileno(ferr))
	    continue;
	len = read(fd, &c, 1);
	if (len != 1) {
	    fprintf(ferr, "read(%d, ...): botch, len=%d not %d\n", fd, len, 1);
	    fprintf(ferr, "errno=%d %s\n", errno, strerror(errno));
	}
	else
	    fprintf(ferr, "read(%d, ...): OK c=\'%c\'\n", fd, c);
    }

    exit(0);
}
