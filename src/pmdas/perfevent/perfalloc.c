/*
 * Copyright (C) 2013  Joe White
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h> /* exit() */
#include <sys/prctl.h>
#include <errno.h>

#include "perflock.h" /* For lock filename */

sig_atomic_t running = 1;

void handler(int data)
{
	running = 0;
}

void usage(const char *name) 
{
    printf("Usage: %s [OPTION]... [command [arg ...]]\n"
           "Request use of the hardware performance counters. If the reservation\n"
           "request fails the process exits immediately with exit code %d.\n"
           "If successful, the process will run until a kill signal is received. The\n"
           "reservation request persists while the process is running.\n"
           "\n"
           "  -D       run in the foreground (the default)\n"
           "  -d       run in the background\n"
           "  -f FILE  Use FILE as the lock file (default $PCP_PMDA_DIR/perfevent/perflock)\n"
           "  -h       display this help message and exit\n"
           "  -v       output version number and exit\n"
           "\n"
           "If a commandline is given, this is executed as a subprocess of the agent."
           "When the command dies, so does the agent."
           , name, EXIT_FAILURE);
}

#define RUN_AS_DAEMON 0
#define RUN_FOREGROUND 1
#define RUN_USERCOMMAND 2

int main(int argc, char **argv)
{
	int fp, fd;
	int res;
	struct flock fl;
	sigset_t set, oldset;
    pid_t pid;

    int runmode, opt;
    const char *lockfile;

    lockfile = get_perf_alloc_lockfile();
    runmode = RUN_FOREGROUND;
    while ((opt = getopt(argc, argv, "dDf:hv")) != -1) {
        switch (opt) {
            case 'd':
                runmode = RUN_AS_DAEMON;
                break;
            case 'D':
                runmode = RUN_FOREGROUND;
                break;
            case 'f':
                lockfile = optarg;
                break;
            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            case 'v':
                printf("perfalloc " VERSION "\n" );
                exit(EXIT_SUCCESS);
                break;
            default: /* '?' */
                usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if( optind < argc )
    {
        runmode = RUN_USERCOMMAND;
    }

    if(RUN_AS_DAEMON == runmode)
	{
		if( -1 == daemon(0,0) ) {
			perror("fork");
			exit(EXIT_FAILURE);
		}
	}
    else if (RUN_USERCOMMAND == runmode)
    {
        pid = fork();
        if (pid == -1) 
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (pid != 0) 
        {     /* Parent - execute the given command. */
            execvp(argv[optind], &argv[optind]);
            perror(argv[optind]);
            exit(EXIT_FAILURE);
        }

        /* Child - make sure that kernel signals when parent dies */
        prctl(PR_SET_PDEATHSIG, SIGHUP);

        if ( -1 == chdir("/") ) {
            perror("chdir");
            /* carry on even if chdir fails ..*/
        }
             
        if ((fd = open("/dev/null", O_RDWR, 0)) != -1) 
        {
            (void) dup2(fd, STDIN_FILENO);
            (void) dup2(fd, STDOUT_FILENO);
            (void) dup2(fd, STDERR_FILENO);
            if (fd > 2)
                close(fd);
        }
    }

	fp = open(lockfile, O_RDONLY);

	if( fp < 0 ) {
		fprintf(stderr, "open %s: %s\n", lockfile, strerror(errno) );
		exit(EXIT_FAILURE);  
	}

	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;

	res = fcntl(fp, F_SETLK, &fl);

	if( res == -1 )
	{
		perror("fcntl");
		close(fp);
		exit(EXIT_FAILURE);  
	}

	signal (SIGINT, handler);
	signal (SIGHUP, handler);

	sigemptyset (&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGHUP);

	(void) sigprocmask(SIG_BLOCK, &set, &oldset);

	while(running)
	{
		sigsuspend(&oldset);
	}

	(void) sigprocmask(SIG_UNBLOCK, &set, NULL);

	fl.l_type = F_UNLCK;
	(void) fcntl(fp, F_SETLK, &fl);
	(void) close(fp);

	free_perf_alloc_lockfile();

	return 0;
}
