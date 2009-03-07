/*
 * Quick and dirty sudo ... cmd and args all in argv[1] ... no checks
 *
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * Note: no checks ... THIS IS A GIANT SECURITY HOLE ... but it is
 *       needed to run those parts of the PCP QA that need root
 *       privileges, e.g. to install a PMDA, start or stop pmcd, etc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>

main(int argc, char *argv[])
{
    char *cmd;
    int	status;

    if (argc <= 1)
	exit(0);

    cmd = strdup(argv[1]);
    for (argv+=2, argc-=2; argc > 0; argc--, argv++) {
	cmd = realloc(cmd, strlen(cmd) + strlen(*argv) + 4);
	strcat(cmd, " ");
	strcat(cmd, *argv);
    }

    seteuid(0);
    setuid(0);
    status = system(cmd);
    if (WIFSIGNALED(status)) {
	fprintf(stderr, "sudo: command terminated with signal %d\n", WTERMSIG(status));
	exit(1);
    }
    else if (WIFEXITED(status))
	exit(WEXITSTATUS(status));
    else {
	fprintf(stderr, "sudo: command bizarre wait status 0x%x\n", status);
	exit(1);
    }
    /*NOTREACHED*/
}
