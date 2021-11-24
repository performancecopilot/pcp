/*
 * Copyright (c) 2021 Ken McDonell.  All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

typedef struct {
    int			step;
    struct timeval	time;
} stamp_t;


int
main(int argc, char **argv)
{
    int		fd;
    int		sts;
    int		epoch = 0;
    stamp_t	prior;
    stamp_t	now;

    if (argc != 2 && argc != 3) {
	fprintf(stderr, "Usage: time_stamp file [tag]\n");
	exit(1);
    }

    if ((fd = open(argv[1], O_RDWR)) < 0) {
	if ((fd = open(argv[1], O_CREAT|O_RDWR, 0644)) < 0) {
	    fprintf(stderr, "time_stamp: cannot create \"%s\": %s\n", argv[1], strerror(errno));
	    exit(1);
	}
	epoch = 1;
    }
    else {
	if ((sts = read(fd, &prior, sizeof(stamp_t))) != sizeof(stamp_t)) {
	    fprintf(stderr, "time_stamp: bad read: len=%d, not %zd as expected\n", sts, sizeof(stamp_t));
	    exit(1);
	}
    }

    gettimeofday(&now.time, NULL);

    if (epoch)
	now.step = 1;
    else
	now.step = prior.step + 1;

    lseek(fd, 0, SEEK_SET);
    if ((sts = write(fd, &now, sizeof(stamp_t))) != sizeof(stamp_t)) {
	fprintf(stderr, "time_stamp: bad write: len=%d, not %zd as expected\n", sts, sizeof(stamp_t));
	exit(1);
    }

    if (!epoch) {
	now.time.tv_sec -= prior.time.tv_sec;
	now.time.tv_usec -= prior.time.tv_usec;
	if (now.time.tv_usec < 0) {
	    now.time.tv_sec--;
	    now.time.tv_usec += 1000000;

	}
	printf("time_stamp: step %d", now.step);
	printf(" delta %ld.%06d secs", (long)now.time.tv_sec, (int)now.time.tv_usec);
	if (argc == 3)
	    printf(" tag %s", argv[2]);
	putchar('\n');
    }

    return 0;
}
