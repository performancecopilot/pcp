/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -D N       set pmDebug debugging flag to N\n"
	  "  -d domain  use domain (numeric) for metrics domain of PMDA\n"
	  "  -h helpfile  get help text from helpfile rather then default path\n"
	  "  -l logfile write log into logfile rather than using default log name\n"
	  "\nExactly one of the following options may appear:\n"
	  "  -i port    expect PMCD to connect on given inet port (number or name)\n"
	  "  -p         expect PMCD to supply stdin/stdout (pipe)\n"
	  "  -u socket  expect PMCD to connect on given unix domain socket\n"
	  "  -6 port    expect PMCD to connect on given ipv6 port (number or name)\n",
	  stderr);		
    exit(1);
}

int
main(int argc, char *argv[])
{
    int			err = 0;
    pmdaInterface	desc = { 0 };

    __pmSetProgname(argv[0]);

    /*
     * Honour the pmcd connection protocol ... 
     */
    pmdaDaemon(&desc, PMDA_INTERFACE_3, pmProgname, desc.domain, "pmdashutdown.log", NULL);
    if (desc.status != 0) {
	fprintf(stderr, "pmdaDaemon() failed!\n");
	exit(1);
    }
    if (pmdaGetOpt(argc, argv, "D:d:h:i:l:pu:6:", &desc, &err) != EOF)
    	err++;
    if (err)
    	usage();

    pmdaOpenLog(&desc);
    pmdaConnect(&desc);

    /*
     * if we exit immediately the smarter pmcd agent initialization
     * will notice, and report unexpected end-of-file ... so sleep
     * for longer than pmcd is willing to wait, then exit
     */
    sleep(10);

    fprintf(stderr, "%s terminated\n", pmProgname);
    exit(0);
}
