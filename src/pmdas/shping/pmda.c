/*
 * sh(1) ping PMDA 
 *
 * Copyright (c) 1995-2003 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <ctype.h>
#include "shping.h"
#include "domain.h"

__uint32_t	cycletime = 120;	/* default cycle time, 2 minutes */
__uint32_t	timeout = 20;		/* response timeout in seconds */
static char	*username;		/* user account to run under */
cmd_t		*cmdlist;

#ifdef HAVE_SPROC

/* Signals are only used with sproc, threads will never generate SIGGHLD */
void
onchld(int s)
{
    int done;
    int waitStatus;
    int	die = 0;

    while ((done = waitpid(-1, &waitStatus, WNOHANG)) > 0) {

	if (done != sprocpid)
	    continue;
	die = 1;

	if (WIFEXITED(waitStatus)) {

	    if (WEXITSTATUS(waitStatus) == 0)
		logmessage(LOG_INFO, 
			   "Sproc (pid=%d) exited normally\n",
			   done);
	    else
		logmessage(LOG_CRIT, 
			   "Sproc (pid=%d) exited with status = %d\n",
			   done, WEXITSTATUS(waitStatus));
	}
	else if (WIFSIGNALED(waitStatus)) {
	    
	    if (WCOREDUMP(waitStatus))
		logmessage(LOG_CRIT, 
			   "Sproc (pid=%d) received signal = %d and dumped core\n",
			   done, WTERMSIG(waitStatus));
	    else
		logmessage(LOG_CRIT, 
			   "Sproc (pid=%d) received signal = %d\n",
			   done, WTERMSIG(waitStatus));
	}
	else {
	    logmessage(LOG_CRIT, 
		       "Sproc (pid=%d) died, reason unknown\n", done);
	}
    }

    if (die) {
	logmessage(LOG_INFO, "Main process exiting\n");
	exit(0);
    }
}
#endif

void
usage(void)
{
    fprintf(stderr, 
"Usage: %s [options] configfile\n\n\
Options:\n\
  -C           parse configuration file and exit\n\
  -d domain    use domain (numeric) for metrics domain of PMDA\n\
  -I interval  cycle time in seconds between subsequent executions of each\n\
               command [default 120 seconds]\n\
  -l logfile   write log into logfile rather than using the default log\n\
  -t timeout   time in seconds before aborting the wait for individual\n\
               commands to complete [default 20 seconds]\n\
  -U username  run the agent and commands as alternate user [default \"pcp\"]\n",
	pmProgname);
    exit(1);
}

int
main(int argc, char **argv)
{
    pmdaInterface	dispatch;
    int			n = 0;
    int			i;
    int			err = 0;
    int			sep = __pmPathSeparator();
    int			line;
    int                 numcmd = 0;
    int                 parseonly = 0;
    char		*configfile;
    FILE		*conf;
    char		*endnum;
    char		*p;
    char		*tag;
    char		lbuf[256];
    char		mypath[MAXPATHLEN];

    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    snprintf(mypath, sizeof(mypath), "%s%c" "shping" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_2, pmProgname, SHPING, 
		"shping.log", mypath);

    while ((n = pmdaGetOpt(argc, argv,"CD:d:I:l:t:U:?",
			&dispatch, &err)) != EOF) {
	switch (n) {

	    case 'C':
		parseonly = 1;
		break;

	    case 'I':
		cycletime = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, 
		    	    "%s: -I requires number of seconds as argument\n",
			    pmProgname);
		    err++;
		}
		break;

	    case 't':
		timeout = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0') {
		    fprintf(stderr, 
		    	    "%s: -t requires number of seconds as argument\n",
			    pmProgname);
		    err++;
		}
		break;

	    case 'U':
		username = optarg;
		break;

	    case '?':
		err++;
	}
    }

    if (err || optind != argc -1) {
    	usage();
    }

    configfile = argv[optind];
    if ((conf = fopen(configfile, "r")) == NULL) {
	fprintf(stderr, "%s: Unable to open config file \"%s\": %s\n",
	    pmProgname, configfile, osstrerror());
	exit(1);
    }
    line = 0;

    for ( ; ; ) {
	if (fgets(lbuf, sizeof(lbuf), conf) == NULL)
	    break;

	line++;
	p = lbuf;
	while (*p && isspace((int)*p))
	    p++;
	if (*p == '\0' || *p == '\n' || *p == '#')
	    continue;
	tag = p++;
	while (*p && !isspace((int)*p))
	    p++;
	if (*p)
	    *p++ = '\0';
	while (*p && isspace((int)*p))
	    p++;
	if (*p == '\0' || *p == '\n') {
	    fprintf(stderr, "[%s:%d] missing command after tag \"%s\"\n",
		configfile, line, tag);
	    exit(1);
	}

	numcmd++;
	if (parseonly)
	    continue;
	if ((cmdlist = (cmd_t *)realloc(cmdlist, numcmd * sizeof(cmd_t))) == NULL) {
	    __pmNoMem("main:cmdlist", numcmd * sizeof(cmd_t), 
		     PM_FATAL_ERR);
	}

	cmdlist[numcmd-1].tag = strdup(tag);
	cmdlist[numcmd-1].cmd = strdup(p);
	cmdlist[numcmd-1].status = STATUS_NOTYET;
	cmdlist[numcmd-1].error = 0;
	cmdlist[numcmd-1].real = cmdlist[numcmd-1].usr = cmdlist[numcmd-1].sys = -1;

	/* trim trailing newline */
	p = cmdlist[numcmd-1].cmd;
	while (*p && *p != '\n')
	    p++;
	*p = '\0';
    }

    fclose(conf);

    if (numcmd == 0) {
	fprintf(stderr, "%s: No commands in config file \"%s\"?\n",
	    pmProgname, configfile);
	exit(1);
    }
    else if (parseonly)
	exit(0);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	fprintf(stderr, "Parsed %d commands\n", numcmd);
	fprintf(stderr, "Tag\tCommand\n");
	for (i = 0; i < numcmd; i++) {
	    fprintf(stderr, "[%s]\t%s\n", cmdlist[i].tag, cmdlist[i].cmd);
	}
    }
#endif

    /* set up indom description */
    indomtab.it_numinst = numcmd;
    if ((indomtab.it_set = (pmdaInstid *)malloc(numcmd*sizeof(pmdaInstid))) == NULL) {
	__pmNoMem("main.indomtab", numcmd * sizeof(pmdaInstid), PM_FATAL_ERR);
    }
    for (i = 0; i < numcmd; i++) {
	indomtab.it_set[i].i_inst = i;
	indomtab.it_set[i].i_name = cmdlist[i].tag;
    }

#ifdef HAVE_SPROC
    signal(SIGCHLD, onchld);
#endif

    pmdaOpenLog(&dispatch);
    __pmSetProcessIdentity(username);

    shping_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
}
