#include <stdio.h>
#include <sys/time.h>
#include <pcp/pmapi.h>

main(int argc, char *argv[])
{
    int			sts;
    char		*err;
    struct timeval	time;

    while (argc > 1) {
	printf("\"%s\"", argv[1]);
	sts = pmParseInterval(argv[1], &time, &err);
	if (sts == -1) {
	    printf(" Error:\n%s", err);
	    free(err);
	}
	else if (sts == 0) {
	    printf(" Time: %d.%06d sec\n", time.tv_sec, time.tv_usec);
	}
	else {
	    printf(" Bogus return value: %d\n", sts);
	}
	argc--;
	argv++;
    }

    exit(0);
    /*NOTREACHED*/
}
