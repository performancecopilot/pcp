/*
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include <sys/stat.h>
#include "summary.h"
#include "domain.h"

extern void	summary_init(pmdaInterface *);
extern void	summary_done(void);

pid_t		clientPID;

int
main(int argc, char **argv)
{
    int			errflag = 0;
    int			sep = __pmPathSeparator();
    char		**commandArgv;
    pmdaInterface	dispatch;
    int			i;
    int			len, c;
    int			clientPipe[2];
    char		helpfile[MAXPATHLEN]; 
    int			cmdpipe;		/* metric source/cmd pipe */
    char		*command = NULL;
    char		*username;

    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);
    __pmSetInternalState(PM_STATE_PMCS);  /* we are below the PMAPI */

    snprintf(helpfile, sizeof(helpfile), "%s%c" "summary" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon (&dispatch, PMDA_INTERFACE_2, pmProgname, SYSSUMMARY,
		"summary.log", helpfile);

    while ((c = pmdaGetOpt(argc, argv, "H:h:D:d:l:U:",
			   &dispatch, &errflag)) != EOF) {
	switch (c) {

	    case 'H':		/* backwards compatibility, synonym for -h */
		dispatch.version.two.ext->e_helptext = optarg;
		break;

	    case 'U':
		username = optarg;
		break;

	    case '?':
		errflag++;
		break;
	}
    }

    for (len=0, i=optind; i < argc; i++) {
	len += strlen(argv[i]) + 1;
    }
    if (len == 0) {
	fprintf(stderr, "%s: a command must be given after the options\n",
		pmProgname);
	errflag++;
    }
    else {
	command = (char *)malloc(len+2);
	command[0] = '\0';
	for (i=optind; i < argc; i++) {
	    if (i > optind)
		strcat(command, " ");
	    strcat(command, argv[i]);
	}
    }
    commandArgv = argv + optind;

    if (errflag) {
	fprintf(stderr, "Usage: %s [options] command [arg ...]\n\n",
		pmProgname);
	fputs("Options:\n"
	      "  -h helpfile    help text file\n"
	      "  -d domain      use domain (numeric) for metrics domain of PMDA\n"
	      "  -l logfile     write log into logfile rather than using default log name\n"
	      "  -U username    user account to run under (default \"pcp\")\n",
	      stderr);		
	exit(1);
    }

    /* force errors from here on into the log */
    pmdaOpenLog(& dispatch);

    /* switch to alternate user account now */
    __pmSetProcessIdentity(username);

    /* initialize */
    summary_init(&dispatch);

    pmdaConnect(&dispatch);
    if (dispatch.status) {
	fprintf (stderr, "Cannot connect to pmcd: %s\n",
		 pmErrStr(dispatch.status));
        exit (1); 
    }

    /*
     * open a pipe to the command
     */
    if (pipe1(clientPipe) < 0) {
	perror("pipe");
	exit(oserror());
    }

    if ((clientPID = fork()) == 0) {
	/* child */
	char cmdpath[MAXPATHLEN+5];
	close(clientPipe[0]);
	if (dup2(clientPipe[1], fileno(stdout)) < 0) {
	    perror("dup");
	    exit(oserror());
	}
	close(clientPipe[1]);

        snprintf (cmdpath, sizeof(cmdpath), "exec %s", commandArgv[0]);
	execv(commandArgv[0], commandArgv);
  
	perror(cmdpath);
	exit(oserror());
    }

    fprintf(stderr, "clientPID = %" FMT_PID "\n", clientPID);

    close(clientPipe[1]);
    cmdpipe = clientPipe[0]; /* parent/agent reads from here */
    __pmSetVersionIPC(cmdpipe, PDU_VERSION2);

    summaryMainLoop(pmProgname, cmdpipe, &dispatch);

    summary_done();
    exit(0);
}
