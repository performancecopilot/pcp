/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <stdlib.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char *argv[])
{
    int			port, fd;
    int			i, sts;
    struct sockaddr_in	myAddr;
    struct linger	noLinger = {1, 0};
    char		*endp;
    char		*p;

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    if (argc != 2) {
	fprintf(stderr, "Usage: %s port\n", pmProgname);
	exit(1);
    }

    port = (int)strtol(argv[1], &endp, 10);
    if (*endp != '\0') {
	fprintf(stderr, "%s: port argument must be a numeric internet port number\n", pmProgname);
	exit(1);
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	perror("socket");
	exit(1);
	/*NOTREACHED*/
    }
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &i,
		   sizeof(i)) < 0) {
	perror("setsockopt(nodelay)");
	exit(1);
	/*NOTREACHED*/
    }
    /* Don't linger on close */
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &noLinger, sizeof(noLinger)) < 0) {
	perror("setsockopt(nolinger)");
	exit(1);
	/*NOTREACHED*/
    }

    memset(&myAddr, 0, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myAddr.sin_port = htons(port);
    sts = bind(fd, (struct sockaddr*)&myAddr, sizeof(myAddr));
    if (sts < 0) {
	fprintf(stderr, "bind(%d): %s\n", port, strerror(errno));
	exit(1);
	/*NOTREACHED*/
    }

    sts = listen(fd, 5);	/* Max. of 5 pending connection requests */
    if (sts == -1) {
	perror("listen");
	exit(1);
	/*NOTREACHED*/
    }

    sts = accept(fd, (struct sockaddr *)0, 0);
    if (sts < 0) {
	perror("accept");
	exit(1);
	/*NOTREACHED*/
    }

    /*
     * if we exit immediately the smarter pmcd agent initialization
     * will notice, and report unexpected end-of-file ... so sleep
     * for longer than pmcd is willing to wait, then exit
     */
    sleep(10);

    fprintf(stderr, "%s terminated\n", pmProgname);
    exit(0);
    /*NOTREACHED*/
}
