/*
 *  Process creation -- based upon MUSBUS ....
 *
 *  fork()-n-exit() for child
 *
 *  $Header: spawn.c,v 3.5 1993/07/02 21:17:11 kenj Exp $
 *
 * Yep! code from 1993.
 *
 * This code comes from the Musbus benchmark that was written by me
 * and first released into the public domain circa 1984.
 * - Ken McDonell, Jan 2018
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
	int	iter;
	int	slave;
	int	status;

	if (argc != 2) {
		printf("Usage: %s count\n", argv[0]);
		exit(1);
	}

	iter = atoi(argv[1]);

	while (iter-- > 0) {
		if ((slave = fork()) == 0) {
			/* slave .. boring */
#if debug
			printf("fork OK\n");
#endif
			exit(0);
		} else if (slave < 0) {
			/* woops ... */
			printf("Fork failed at iteration %d\n", iter);
			perror("Reason");
			exit(2);
		} else
			wait(&status);
		if (status != 0) {
			printf("Bad wait status: 0x%x\n", status);
			exit(2);
		}
#if debug
		printf("Child %d done.\n", slave);
#endif
	}
	exit(0);
}
