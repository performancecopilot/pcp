/*
 * Copyright (c) 2020 Red Hat.  All Rights Reserved.
 *
 * Simple daemon to test service manager notifications
 * and portability, with/without forking or daemonizing.
 */

#include <signal.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

#define SERVICESPEC "test-service-notify"

void
on_signal(int sig)
{
    fprintf(stderr, "caught signal %d\n", sig);
}

int
main(int argc, char *argv[])
{
    pid_t mainpid;
    int foreground = 0;
    char *hostarg = "local:";
    char *logfile = SERVICESPEC ".log";
    int c, sts, errflag = 0;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "fl:D:?")) != EOF) {
        switch (c) {
	case 'f':
	    foreground = 1;
	    break;
        case 'D':       /* debug flag */
            if ((sts = pmSetDebug(optarg)) < 0) {
                fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
                    pmGetProgname(), optarg);
                errflag++;
            }
            break;

	case 'l':
	    logfile = optarg;
	    break;
        case '?':
        default:
            errflag++;
            break;
        }
    }

    if (errflag) {
        printf("Usage: %s %s\n", pmGetProgname(), "[-Dflag] [-f] [host]\n");
        exit(1);
    }

    if (argc - optind > 0)
    	hostarg = argv[optind];

    fprintf(stderr, "%s: foreground=%d host=%s\n", pmGetProgname(), foreground, hostarg);

    if (!foreground)
	__pmServerStart(argc, argv, 1);

    pmOpenLog(pmGetProgname(), logfile, stderr, &sts);

    mainpid = getpid();
    if (__pmServerCreatePIDFile(SERVICESPEC, PM_FATAL_ERR) < 0) {
    	fprintf(stderr, "Error: failed to create pidfile for \"%s\" for service \"%s\"\n", hostarg, SERVICESPEC);
	exit(1);
    }

    signal(SIGTERM, on_signal);

    __pmServerNotifyServiceManagerReady(mainpid);
    pause(); /* this service doesn't do a lot! */
    __pmServerNotifyServiceManagerStopping(mainpid);

    /* pidfile is removed by exit handler */
    exit(0);
}
