/*
 * Copyright (c) 2007 Aconex, Inc.  All Rights Reserved.
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
#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>

char command[32];
char *progname;
char *service;
char *timenow;
time_t now;

void stop(int unused)
{
    (void)unused;
    now = time(NULL);
    timenow = ctime(&now);
    timenow[strlen(timenow)-1] = '\0';
    snprintf(command, sizeof(command), "/etc/%s stop", service);
    fprintf(stdout, "[%s] %s issuing \"%s\"\n", timenow, progname, command);
    fflush(stdout);

    if (system(command) == 0)
	exit(0);
}

int main(int argc, char **argv)
{
    progname = basename(argv[0]);
    if (argc != 2) {
	fprintf(stderr, "%s: bad argument count (expected only 1, not %d).\n",
		progname, argc - 1);
	return 1;
    }
    service = argv[1];

    now = time(NULL);
    timenow = ctime(&now);
    timenow[strlen(timenow)-1] = '\0';
    snprintf(command, sizeof(command), "/etc/%s start", service);
    fprintf(stdout, "[%s] %s issuing \"%s\"\n", timenow, progname, command);
    fflush(stdout);

    signal(SIGHUP, stop);
    if (system(command) == 0)
	pause();
    exit(1);
}
