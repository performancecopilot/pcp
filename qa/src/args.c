/*
 * Copyright (c) 2024 Red Hat.
 *
 * QA helper that waits for stdin input then exits, useful for
 * shell escape testing as it allows arbitrary args (ignored).
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
    while (1)
	sleep(1);
    exit(EXIT_SUCCESS);
}
