/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char *argv[])
{
    int		sts;
    FILE	*f;
    FILE	*fout;
    int		fd;
    int		nextfd;

    close(3);		/* some stdio versions start with this open */
    close(4);		/* some stdio versions start with this open */
    fd = atoi(argv[1]);
    if (fd == 1) fout = stdout;
    else if (fd == 2) fout = stderr;
    else fout = fopen("/tmp/chk.fout", "w");
    if (fout == NULL) {
	fprintf(stderr, "chkopenlog: botched open ... fd=%d\n", fd);
	sts = system("ls -l /tmp/chk.fout");
	exit(sts == 0 ? 1 : sts);
    }

    fprintf(fout, "This message on oldstream before __pmOpenLog() called\n");


#define whatis(f) (f == (stderr) ? " (stderr)" : (f == (stdout) ? " (stdout)" : (f == NULL ? " (NULL)" : "")))

    nextfd = open("/dev/null", 0);
    if (nextfd < 0) {
	fprintf(stderr, "chkopenlog: failed /dev/null open\n");
	exit(2);
    }
    fprintf(stderr, "Starting with oldstream%s fd=%d, nextfd=%d\n", whatis(fout), fileno(fout), nextfd);
    close(nextfd);

    nextfd = open("/dev/null", 0);
    f = __pmOpenLog("chkopenlog", argv[2], fout, &sts);
    fprintf(stderr, "__pmOpenLog -> sts=%d, log %s newstream%s fd=%d, nextfd=%d\n",
	sts, argv[2], whatis(f), f != NULL ? fileno(f) : -1, nextfd);
    if (f != NULL)
	fprintf(f, "[a helpful little message]\n");

    exit(0);
}
