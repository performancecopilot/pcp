/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * Check that file descriptors from pmNewContext are closed on an exec()
 * call.  If not, every open pmcd file descriptor is carried across into the
 * new program and even if the new program uses libpcp, libpcp can't know what
 * the file descriptors are for.  In other words we leak fds across an exec().
 *
 * This was fixed by setting the close on exec flag in pmNewContext.
 */

#include <pcp/pmapi.h>
#include <sys/wait.h>

void
printNextFd(void)
{
    int fd;
    int sts;

    fd = open("/dev/null", O_RDONLY);
    if (fd < 0) {
	perror("printNextFd");
	exit(1);
    }
    printf("next free file descriptor = %d", fd);
    sts = close(fd);
    if (sts < 0) {
	fprintf(stderr, "error closing fd = %d\n", fd);
	exit(1);
    }
}

int
main(int argc, char* argv[])
{
    int		sts;
    long	run;
    char	*endp;
    int		fd;

    pmSetProgname(argv[0]);

    if (argc != 2) {
	fprintf(stderr,
		"Usage: %s count\nwhere count is the number of runs required\n",
		pmGetProgname());
	exit(1);
    }
    run = strtol(argv[1], &endp, 0);
    if (*endp || run < 0) {
	fprintf(stderr, "bad run count specified: %s\n", argv[1]);
	exit(1);
    }

    /*
     * Some stdio environments start with strange fd's open ... close
     * 'em all to give us some breathing space
     */
    for (fd = 3; fd < 100; fd++)
	close(fd);

    printf("invocation %ld: ", run);
    fputs("    at startup, ", stdout);
    printNextFd();
    sts = pmNewContext(PM_CONTEXT_HOST, "localhost");
    if (sts < 0) {
	fprintf(stderr, "_pmNewContext(localhost): %s\n", pmErrStr(sts));
	exit(1);
    }
    fputs((run > 1) ? "    at exec,    " : "    at exit,    ", stdout);
    printNextFd();
    fputs("\n", stdout);

    if (run > 1) {
	pid_t childPid;

	childPid = fork();
	if (childPid < 0) {
	    perror("fork() failed");
	    exit(1);
	}
	else if (childPid) {
	    int	sts;

	    wait(&sts);
	}
	else {
	    char*	childArgv[3];
	    int		sts, len;

	    childArgv[0] = argv[0];
	    /* numeric arguments will get shorter, not longer so this is safe */
	    len = strlen(argv[1]) + 1;
	    childArgv[1] = strdup(argv[1]);
	    if (childArgv[1] == NULL) {
		perror("can't copy argv[1]\n");
		exit(1);
	    }
	    pmsprintf(childArgv[1], len, "%ld", run - 1);
	    childArgv[2] = NULL;
	    sts = execvp(childArgv[0], childArgv);
	    if (sts < 0) {
		perror("execvp() failed");
		exit(1);
	    }
	}
    }

    return 0;
}
