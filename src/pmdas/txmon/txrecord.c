/*
 * Copyright (c) 1995-2000 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <pcp/pmapi.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "txmon.h"

/*
 * Update the shm segment the is used to export metrics via the txmon PMDA
 */
int
main(int argc, char **argv)
{
    int			shmid;
    int			n;
    char		*p;
    stat_t		*sp = NULL;	/* initialize to pander to gcc */

    if (argc == 1 || (argc == 2 && strcmp(argv[1], "-?") == 0)) {
	fprintf(stderr, "Usage: txrecord tx_type servtime [tx_type servtime ...]\n"
			"       txrecord -l\n");
	exit(1);
    }

    /*
     * attach to the txmon PMDA shm segment ...
     */
    if ((shmid = shmget(KEY, 0, 0)) < 0) {
	fprintf(stderr, "Cannot attach to shm segment, shmid: %s\n", osstrerror());
	fprintf(stderr, "Is the txmon PMDA configured and running?\n");
	exit(1);
    }
    if ((control = (control_t *)shmat(shmid, NULL, 0)) == (control_t *)-1) {
	fprintf(stderr, "Cannot attach to shm segment, shmat: %s\n", osstrerror());
	fprintf(stderr, "Is the txmon PMDA configured and running?\n");
	exit(1);
    }

    if (control->level == 0) {
	fprintf(stderr, "Stats collection disabled\n");
	exit(1);
    }
    else if (control->level == 1)
	fprintf(stderr, "Warning: stats time collection disabled\n");

    if (argc == 2 && strcmp(argv[1], "-l") == 0) {
	printf("txmon shared memory segment summary\n");
	printf("                 tx  reset     total maximum\n");
	printf("index offset  count  count      time    time name\n");
	for (n = 0; n < control->n_tx; n++) {
	    sp = (stat_t *)((__psint_t)control + control->index[n]);
	    printf("%5d %6d %6d %6d %9.3f %7.3f %s\n",
		n, control->index[n], sp->count, sp->reset_count,
		(float)sp->sum_time, sp->max_time, sp->type);
	}
	exit(0);
    }

    while (argc > 1) {

	for (n = 0; n < control->n_tx; n++) {
	    sp = (stat_t *)((__psint_t)control + control->index[n]);
	    if (strcmp(argv[1], sp->type) == 0)
		break;
	}
	argc--;
	argv++;
	if (argc == 1) {
	    fprintf(stderr, "Missing time for tx type \"%s\"?\n", argv[0]);
	    exit(1);
	}

	if (n == control->n_tx)
	    fprintf(stderr, "Unknown tx type \"%s\" ... skipped\n", argv[0]);

	else {
	    double	e;
	    e = strtod(argv[1], &p);
	    if (*p != '\0')
		fprintf(stderr, "Time value (%s) for tx type \"%s\" is bogus ... skipped\n", argv[1], argv[0]);
	    else {
		sp->count++;
		if (control->level == 2) {
		    sp->sum_time += (float)e;
		    if ((float)e > sp->max_time)
			sp->max_time = e;
		}
	    }
	}
	argc--;
	argv++;
    }

    exit(0);
}
