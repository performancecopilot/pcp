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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */


#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <limits.h>

/*
 * test program to calibrate system call rates ... just compile
 * and run:
 *	$ cc rate-syscalls.c
 *	$ ./a.out
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

    gettimeofday(&then, NULL);
    n = 600000 * scale;
    for (i = 0; i < n; i++)
	getpid();
    gettimeofday(&now, NULL);
    delta = now.tv_sec - then.tv_sec +
		(double)(now.tv_usec - then.tv_usec) / 1000000;
    printf("getpid()\t\t\t- %9d syscalls/sec [%.2f sec]\n",
	(int)(0.5 + n / delta), delta);

    gettimeofday(&then, NULL);
    n = 300000 * scale;
    for (i = 0; i < n; i++)
	gettimeofday(&eek, NULL);
    gettimeofday(&now, NULL);
    delta = now.tv_sec - then.tv_sec +
		(double)(now.tv_usec - then.tv_usec) / 1000000;
    printf("gettimeofday()\t\t\t- %9d syscalls/sec [%.2f sec]\n",
	(int)(0.5 + n / delta), delta);

    fd = open("/dev/null", 0);
    n = 150000 * scale;
    gettimeofday(&then, NULL);
    for (i = 0; i < n; i++) {
	/* expect EOF */
	read(fd, &c, 1);
    }
    gettimeofday(&now, NULL);
    delta = now.tv_sec - then.tv_sec +
		(double)(now.tv_usec - then.tv_usec) / 1000000;
    printf("read() at end of file\t\t- %9d syscalls/sec [%.2f sec]\n",
	(int)(0.5 + n / delta), delta);
    close(fd);

    fd = open("/dev/null", 0);
    n = 400000 * scale;
    gettimeofday(&then, NULL);
    for (i = 0; i < n; i++) {
	lseek(fd, 0L, 0);
    }
    gettimeofday(&now, NULL);
    delta = now.tv_sec - then.tv_sec +
		(double)(now.tv_usec - then.tv_usec) / 1000000;
    printf("lseek() to start of file\t- %9d syscalls/sec [%.2f sec]\n",
	(int)(0.5 + n / delta), delta);
    close(fd);

    unlink("/tmp/creat-clo");
    n = 20000 * scale;
    gettimeofday(&then, NULL);
    for (i = 0; i < n; i++) {
	if ((fd = creat("/tmp/creat-clo", 0644)) < 0) {
	    perror("creat");
	    exit(1);
	}
	close(fd);
    }
    gettimeofday(&now, NULL);
    delta = now.tv_sec - then.tv_sec +
		(double)(now.tv_usec - then.tv_usec) / 1000000;
    printf("file creat() and close()\t- %9d syscalls/sec [%.2f sec]\n",
	(int)(0.5 + 2*n / delta), delta);
    unlink("/tmp/creat-clo");

    servInfo = gethostbyname("localhost");
    memset(&myAddr, 0, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    memcpy(&myAddr.sin_addr, servInfo->h_addr, servInfo->h_length);
    myAddr.sin_port = htons(80);
    n = 4000 * scale;
    gettimeofday(&then, NULL);
    for (i = 0; i < n; i++) {

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    perror("socket");
	    exit(1);
	}

	if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &noLinger, sizeof(noLinger)) < 0) {
	    perror("setsockopt SO_LINGER");
	    exit(1);
	}

	if (connect(s, (struct sockaddr*) &myAddr, sizeof(myAddr)) < 0) {
	    perror("connect");
	    exit(1);
	}
	close(s);
    }
    gettimeofday(&now, NULL);
    delta = now.tv_sec - then.tv_sec +
		(double)(now.tv_usec - then.tv_usec) / 1000000;
    printf("socket(), connect() and close()\t- %9d syscalls/sec [%.2f sec]\n",
	(int)(0.5 + 3*n / delta), delta);

    return 0;
}
