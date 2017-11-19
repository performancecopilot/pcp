/*
 * Dumb, a PMDA which never responds to requests ... used in qa/023
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pcp/pmda.h>

#include "localconfig.h"

#if PCP_VER < 3611
#define __pmRead read
#endif

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options] controlwords\n\n", pmGetProgname());
    fputs("Options:\n"
	  "  -D debugspec set PCP debugging options\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -h helpfile  get help text from helpfile rather then default path\n"
	  "  -l logfile write log into logfile rather than using default log name\n",
	  stderr);
    exit(1);
}

/*
 * Set up the agent if running as a daemon.
 */

int
main(int argc, char **argv)
{
    int			err = 0;
    int			sts;
    pmdaInterface	desc = { 0 };
    char		c;
    int			exit_action = 0;

    pmSetProgname(argv[0]);

    pmdaDaemon(&desc, PMDA_INTERFACE_3, pmGetProgname(), desc.domain, "dumb_pmda.log", NULL);
    if (desc.status != 0) {
	fprintf(stderr, "pmdaDaemon() failed!\n");
	exit(1);
    }

    if (pmdaGetOpt(argc, argv, "D:d:h:l:", &desc, &err) != EOF)
    	err++;
    if (err)
    	usage();

    /*
     * scan cmd line args for action keywords ...
     */
    for (; optind < argc; optind++) {
	if (strcmp(argv[optind], "exit") == 0) exit_action = 1;
    }

    pmdaOpenLog(&desc);
    pmdaConnect(&desc);

    /*
     * We have connection to pmcd ... consume PDUs from pmcd,
     * ignore them, optionally execute an action and exit on end of file
     */

    while ((sts = __pmRead(desc.version.two.ext->e_infd, &c, 1)) == 1) {
	if (exit_action) exit(1);
    }

    if (sts < 0) {
	fprintf(stderr, "dumb_pmda: Error on read from pmcd?: %s\n", strerror(errno));
	exit(1);
    }

    exit(0);
}
