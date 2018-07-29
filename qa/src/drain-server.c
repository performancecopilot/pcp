/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 *
 * Drain server ...
 *	accepts a connection and then reads until end of input
 *
 * Based on pdu-server.c
 */

#include <pcp/pmapi.h>

int
main(int argc, char *argv[])
{
    int		fd;
    int		port = 1214;
		    /* default port assigned to kazaa what ever that is! */
    int		hang = 0;
    int		i, sts;
    int		c;
    int		newfd;
    struct sockaddr_in	myAddr;
    struct linger	noLinger = {1, 0};
    char	*endnum;
    int		errflag = 0;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:hp:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'h':	/* hang after accept */
	    hang = 1;
	    break;

	case 'p':
	    port = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: port argument must be a numeric internet port number\n", pmGetProgname());
		exit(1);
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc) {
	fprintf(stderr, "Usage: %s [-D debugspec] [-h] [-p port]\n", pmGetProgname());
	exit(1);
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	perror("socket");
	exit(1);
    }
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &i,
		   sizeof(i)) < 0) {
	perror("setsockopt(nodelay)");
	exit(1);
    }
    /* Don't linger on close */
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &noLinger, sizeof(noLinger)) < 0) {
	perror("setsockopt(nolinger)");
	exit(1);
    }

    memset(&myAddr, 0, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myAddr.sin_port = htons(port);
    sts = bind(fd, (struct sockaddr*)&myAddr, sizeof(myAddr));
    if (sts < 0) {
	fprintf(stderr, "bind(%d): %s\n", port, strerror(errno));
	exit(1);
    }

    sts = listen(fd, 5);	/* Max. of 5 pending connection requests */
    if (sts == -1) {
	perror("listen");
	exit(1);
    }

    newfd = accept(fd, (struct sockaddr *)0, 0);
    if (newfd < 0) {
	fprintf(stderr, "%s: accept: %s\n", pmGetProgname(), strerror(errno));
	exit(1);
    }

    if (hang) {
	/* wait for a signal ... */
#ifndef IS_MINGW
	pause();
#else
	/*
	 * punt ... looks like a signal will get us back from here
	 * (at least it works in one test case with SIGINT)
	 */
	SleepEx(INFINITE, TRUE);
#endif
	exit(0);
    }

    /* drain input */
    while ((sts = read(newfd, &c, 1)) == 1)
	;
    
    if (sts < 0) {
	/*
	 * ECONNRESET is expected when client exits w/out closing
	 * socket.
	 */
	if (errno != ECONNRESET)
	    fprintf(stderr, "%s: read error: %s\n", pmGetProgname(), pmErrStr(-errno));
    }

    exit(0);
}
