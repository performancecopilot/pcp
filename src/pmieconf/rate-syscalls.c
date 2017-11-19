/*
 * Copyright (c) 2010 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include "pmapi.h"

/*
 * test program to calibrate system call rates ... just compile
 * and run:
 *	$ make rate-syscalls
 *	$ ./rate-syscalls
 *
 * useful for *cpu/syscall rules
 */

int
main()
{
    int			fd;
    int			i;
    int			n;
    int			c;
    struct timeval	now;
    struct timeval	then;
    struct timeval	eek;
    double		delta;
    struct hostent	*servInfo;
    int			s;
    struct sockaddr_in	myAddr;
    struct linger	noLinger = {1, 0};
    int			scale = 2;

    pmtimevalNow(&then);
    n = 600000 * scale;
    for (i = 0; i < n; i++)
	getpid();
    pmtimevalNow(&now);
    delta = pmtimevalSub(&now, &then);
    printf("getpid()\t\t\t- %9d syscalls/sec [%.2f sec]\n",
	(int)(0.5 + n / delta), delta);

    pmtimevalNow(&then);
    n = 300000 * scale;
    for (i = 0; i < n; i++)
	pmtimevalNow(&eek);
    pmtimevalNow(&now);
    delta = pmtimevalSub(&now, &then);
    printf("gettimeofday()\t\t\t- %9d syscalls/sec [%.2f sec]\n",
	(int)(0.5 + n / delta), delta);

    fd = open("/dev/null", 0);
    n = 150000 * scale;
    pmtimevalNow(&then);
    for (i = 0; i < n; i++) {
	/* expect EOF */
	read(fd, &c, 1);
    }
    pmtimevalNow(&now);
    delta = pmtimevalSub(&now, &then);
    printf("read() at end of file\t\t- %9d syscalls/sec [%.2f sec]\n",
	(int)(0.5 + n / delta), delta);
    close(fd);

    fd = open("/dev/null", 0);
    n = 400000 * scale;
    pmtimevalNow(&then);
    for (i = 0; i < n; i++) {
	lseek(fd, 0L, 0);
    }
    pmtimevalNow(&now);
    delta = pmtimevalSub(&now, &then);
    printf("lseek() to start of file\t- %9d syscalls/sec [%.2f sec]\n",
	(int)(0.5 + n / delta), delta);
    close(fd);

    unlink("/tmp/creat-clo");
    n = 20000 * scale;
    pmtimevalNow(&then);
    for (i = 0; i < n; i++) {
	if ((fd = creat("/tmp/creat-clo", 0644)) < 0) {
	    fprintf(stderr, "creat: %s\n", osstrerror());
	    exit(1);
	}
	close(fd);
    }
    pmtimevalNow(&now);
    delta = pmtimevalSub(&now, &then);
    printf("file creat() and close()\t- %9d syscalls/sec [%.2f sec]\n",
	(int)(0.5 + 2*n / delta), delta);
    unlink("/tmp/creat-clo");

    servInfo = gethostbyname("localhost");
    memset(&myAddr, 0, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    memcpy(&myAddr.sin_addr, servInfo->h_addr, servInfo->h_length);
    myAddr.sin_port = htons(80);
    n = 4000 * scale;
    pmtimevalNow(&then);
    for (i = 0; i < n; i++) {

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    fprintf(stderr, "socket: %s\n", netstrerror());
	    exit(1);
	}

	if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &noLinger, sizeof(noLinger)) < 0) {
	    fprintf(stderr, "setsockopt(SO_LINGER): %s\n", netstrerror());
	    exit(1);
	}

	if (connect(s, (struct sockaddr*) &myAddr, sizeof(myAddr)) < 0) {
	    fprintf(stderr, "connect: %s\n", netstrerror());
	    exit(1);
	}
	close(s);
    }
    pmtimevalNow(&now);
    delta = pmtimevalSub(&now, &then);
    printf("socket(), connect() and close()\t- %9d syscalls/sec [%.2f sec]\n",
	(int)(0.5 + 3*n / delta), delta);

    return 0;
}
