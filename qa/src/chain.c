/*
 *  Chained pipe throughput -- based upon MUSBUS ....
 *
 *  Context switching via synchronized unbuffered pipe i/o
 *
 *  Header: context1.c,v 3.4 87/06/22 14:22:59 kjmcdonell Beta
 *
 * Yep! code from 1987.
 *
 * This code comes from the Musbus benchmark that was written by me
 * and released into the public domain circa 1984.
 * - Ken McDonell, Oct 2017
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

int
main(int argc, char **argv)
{
    int		i;
    int		iter = 0;
    int		numiter;
    int		links;
    int		first = 1;
    int		master = 1;
    int		result;
    int		pid;
    int		debug = 0;
    int		p1[2], p2[2];
    char	pbuf[512];

    if (argc > 3 && strcmp(argv[1], "-d") == 0) {
	debug=1;
	argc--;
	argv++;
    }
    if (argc != 3) {
	fprintf(stderr, "Usage: chain links iter\n");
	exit(1);
    }

    links = atoi(argv[1]);
    if (links < 1) {
	fprintf(stderr, "chain: bogus links (%s)\n", argv[1]);
	exit(1);
    }
    numiter = atoi(argv[2]);
    if (numiter < 1) {
	fprintf(stderr, "chain: bogus iter (%s)\n", argv[2]);
	exit(1);
    }
    if (pipe(p2)) {
	perror("chain: p2 pipe failed");
	exit(1);
    }
    close(0);
    if (dup(p2[0]) < 0) {
	perror("chain: p2 dup->0 failed");
	exit(1);
    }
    close(p2[0]);
    if (links == 1) {
	close(1);
	if (dup(p2[1]) < 0) {
	    perror("chain: p2 dup->1 failed");
	    exit(1);
	}
    }
    else {
	for (i=1; i<links; i++) {
	    if (pipe(p1)) {
		perror("chain: p1 pipe failed");
		exit(1);
	    }

	    master = fork();

	    if (master == -1) {
		perror("chain: fork failed");
		exit(1);
	    }
	    else if (master) {
		close(1);
		if (dup(p1[1]) < 0) {
		    perror("chain: p1 dup->1 failed");
		    exit(1);
		}
	    }
	    else {
		iter++;
		close(0);
		if (dup(p1[0]) < 0) {
		    perror("chain: p1 dup->0 failed");
		    exit(1);
		}
		if (first) {
		    close(1);
		    if (dup(p2[1]) < 0) {
			perror("chain: p2 dup->1 failed");
			exit(1);
		    }
		    close(p2[1]);
		}
		close(p1[0]);
		close(p1[1]);
		close(p2[1]);

	    }
	    close(p1[0]);
	    close(p1[1]);
	    first = 0;
	    if (!master)
		break;
	}
    }
    close(p2[1]);

    if (master) {
	    if (debug)
		fprintf(stderr, "initial write, master pid %d\n", getpid());
	    if (write(1, pbuf, sizeof(pbuf)) != sizeof(pbuf)) {
		perror("master write failed");
		exit(1);
	    }
    }

    while (1) {
	    result = read(0, pbuf, sizeof(pbuf));
	    /* do some computation to default Pyramid 9825 race condititon */
	    { int x; for (x = 0; x < 5000; x++) ; }
	    if (result == 0)
		break;
	    if (result != sizeof(pbuf)) {
		perror("read failed");
		exit(1);
	    }
	    if (debug)
		fprintf(stderr, "read seq %d pid %d\n", iter, getpid());
	    if (iter >= numiter)
		break;
	    iter += links;
	    if (debug)
		fprintf(stderr, "write seq %d pid %d\n", iter, getpid());
	    if (write(1, pbuf, sizeof(pbuf)) != sizeof(pbuf)) {
		    perror("write failed");
		    exit(1);
	    }
    }
    close(1);

    while (master && (pid = wait(&result)) != -1) {
	if (debug)
	    fprintf(stderr, "wait -> %d and 0x%x\n", pid, result);
    }

    exit(0);
}
