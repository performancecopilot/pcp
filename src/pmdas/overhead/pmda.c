/*
 * overhead(1) PMDA 
 *
 * Copyright (c) 1995-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2022 Ken McDonell.  All Rights Reserved.
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
 * Debug flags
 *  appl0	config file parsing
 *  appl1	refresh() start-end timestamps
 *  appl3	/proc selection in refresh()
 */

#include <ctype.h>
#include "overhead.h"
#include "domain.h"

__uint32_t	refreshtime = 60;	/* default refresh time, 1 minute */
char		*proc_prefix = "";	/* allow prefix for QA */
long		hertz;			/* kernel clock rate for "ticks" */
grouptab_t	*grouptab;		/* group of interest */
int		ngroup;			/* number of grouptab[] entries */

extern void	overhead_init(pmdaInterface *);
extern int	do_config(char *);

void
usage(void)
{
    fprintf(stderr, 
"Usage: %s [options]\n\n\
Options:\n\
  -C             parse configuration file(s) and exit\n\
  -c configfile  explicit configuration file\n\
  -D debug       set debug options, see pmdbg(1)\n\
  -d domain      use domain (numeric) for metrics domain of PMDA\n\
  -l logfile     write log into logfile rather than using the default log\n\
  -R interval    refresh time in seconds [default 60]\n",
	pmGetProgname());
    exit(1);
}

int
main(int argc, char **argv)
{
    pmdaInterface	dispatch;
    int			n = 0;
    int			sts = 0;
    int                 parseonly = 0;
    char		*env;
    char		*endnum;
    char		*configfile = NULL;	/* for -c configfile */

    pmSetProgname(argv[0]);

    /*
     * get kernel clock rate, for converting ticks to msec
     */
    if ((env = getenv("KERNEL_HERTZ")) != NULL)
	hertz = strtol(env, NULL, 10);
    else
	hertz = sysconf(_SC_CLK_TCK);

    /*
     * allow alternate prefix to /proc files for QA
     */
    if ((env= getenv("PROC_PREFIX")) != NULL)
	proc_prefix = env;

    pmdaDaemon(&dispatch, PMDA_INTERFACE_4, pmGetProgname(), OVERHEAD, 
		"overhead.log", NULL);

    while ((n = pmdaGetOpt(argc, argv,"c:CD:d:l:R:?",
			&dispatch, &sts)) != EOF) {
	switch (n) {

	    case 'c':
		configfile = optarg;
		break;

	    case 'C':
		parseonly = 1;
		break;

	    case 'R':
		refreshtime = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, 
		    	    "%s: -R requires number of seconds as argument\n",
			    pmGetProgname());
		    sts++;
		}
		break;

	    case '?':
		sts++;
	}
    }

    if (sts || optind != argc) {
    	usage();
    }

    if ((sts = do_config(configfile)) < 0) {
	fprintf(stderr, "%s: Aborting due to configuration file errors\n", pmGetProgname());
	exit(1);
    }

    if (parseonly)
	exit(0);

    pmdaOpenLog(&dispatch);

    overhead_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
}
