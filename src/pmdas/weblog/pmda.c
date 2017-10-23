/*
 * Web PMDA, based on generic driver for a daemon-based PMDA
 *
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "weblog.h"
#include "domain.h"
#if defined(HAVE_REGEX_H)
#include <regex.h>
#endif
#if defined(HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif
#if defined(HAVE_SCHED_H)
#include <sched.h>
#endif

#ifdef IS_SOLARIS
#define CLONE_VM        0x00000100 
#elif !defined(CLONE_VM)
#define CLONE_VM        0x0
#endif

#ifndef HAVE_SPROC
int sproc (void (*entry) (void *), int flags, void *arg);
#endif

/* path to the configuration file */
static char	*configFileName = (char*)0;

/* line number of configuration file */
static int	line = 0;

/* number of errors in configuration file */
static int	err = 0;

/* configured for num of servers */
__uint32_t	wl_numServers = 0;

/* number of active servers */
__uint32_t	wl_numActive = 0;

/* check logs every 15 seconds by default */
__uint32_t	wl_refreshDelay = 15;

/* re-open logs if unchanged in this number of seconds */
__uint32_t	wl_chkDelay = 20;

/* max servers per sproc */
__uint32_t	wl_sprocThresh = 80;

/* number of sprocs spawned */
__uint32_t	wl_numSprocs = 0;

/* number of regex parsed */
__uint32_t	wl_numRegex = 0;

/* list of web servers */
WebServer	*wl_servers = (WebServer*)0;

/* list of regular expressions */
WebRegex	*wl_regexTable = (WebRegex*)0;

/* instance table of web servers */
pmdaInstid	*wl_serverInst = (pmdaInstid*)0;

/* list of sprocs spawned from the main process */
WebSproc	*wl_sproc;

/* default name for log file */
char		*wl_logFile = "weblog.log";

/* default path to help file */
char		wl_helpFile[MAXPATHLEN];

/* default user name for PMDA */
char		*wl_username;

/*
 * Usage Information
 */

void
usage(void)
{
    fprintf(stderr, 
	    "Usage: %s [options] configfile\n\
\n\
Options\n\
  -C            check configuration and exit\n\
  -d domain	PMDA domain number\n\
  -h helpfile   get help text from helpfile rather than default path\n\
  -i port	expect PMCD to connect on given inet port (number or name)\n\
  -l logfile	redirect diagnostics and trace output (default weblog.log)\n\
  -n idlesec	number of seconds of weblog inactivity before checking for\n\
		log rotation\n\
  -p		expect PMCD to supply stdin/stdout (pipe)\n\
  -S num	number of web servers per sproc\n\
  -t delay	maximum number of seconds between reading weblog files\n\
  -u socket	expect PMCD to connect on given unix domain socket\n\
  -U username   user account to run under (default \"pcp\")\n\
  -6 port	expect PMCD to connect on given ipv6 port (number or name)\n\
\n\
If none of the -i, -p or -u options are given, the configuration file is\n\
checked and then %s terminates.\n", pmProgname, pmProgname);
    exit(1);
}

void
logmessage(int priority, const char *format, ...)
{
    va_list     arglist;
    char	buffer[2048];
    char	*level;
    char	*p;
    time_t	now;
    int		bytes;

    buffer[0] = '\0';
    time(&now);

    switch (priority) {
        case LOG_EMERG :
            level = "Emergency";
            break;
        case LOG_ALERT :
            level = "Alert";
            break;
        case LOG_CRIT :
            level = "Critical";
            break;
        case LOG_ERR :
            level = "Error";
            break;
        case LOG_WARNING :
            level = "Warning";
            break;
        case LOG_NOTICE :
            level = "Notice";
            break;
        case LOG_INFO :
            level = "Info";
            break;
        case LOG_DEBUG :
            level = "Debug";
            break;
        default:
            level = "???";
            break;
    }

    va_start(arglist, format);
    bytes = vsnprintf(buffer, sizeof(buffer), format, arglist);
    va_end(arglist);
    if (bytes >= sizeof(buffer))
	buffer[sizeof(buffer)-1] = '\0';
    if (bytes < 0)
	buffer[0] = '\0';
    for (p = buffer; *p; p++);
    if (*(--p) == '\n') *p = '\0';

    fprintf(stderr, "[%.19s] %s(%" FMT_PID ") %s: %s\n", ctime(&now), pmProgname, (pid_t)getpid(), level, buffer) ;
}

/*
 * Errors message during parsing of config file
 */

static void
yyerror(char *s)
{
    fprintf(stderr, "[%s:%d] Error: %s\n", configFileName, line, s);
    err++;
}

/*
 * Warning message during parsing of config file
 */

static void
yywarn(char *s)
{
    fprintf(stderr, "[%s:%d] Warning: %s\n", configFileName, line, s);
}

/*
 * skip remaining characters on this line
 */

static void
skip_to_eol(FILE *f)
{
    int		c;

    while ((c = fgetc(f)) != EOF) {
	if (c == '\n')
	    return;
    }
    return;
}

/*
 * Are we at the end of the line (sucks up spaces and tabs which may preceed 
 * EOL)
 */

static void
check_to_eol(FILE *f)
{
    int		c;
    int		i = 0;

    while ((c = fgetc(f)) != EOF) {
	if (c == '\n')
	    break;
	if (c == ' ' || c == '\t')
	    continue;
	i++;
    }
    if (i)
	yywarn("additional words in line, ignored");

    return;
}

/*
 * Get a word. A word if any text until a whitespace
 */

static int
getword(FILE *f, char *buf, int len)
{
    int		c;
    char	*bend = &buf[len-1];

    while ((c = fgetc(f)) != EOF) {
	if (c == ' ' || c == '\t')
	    continue;
	ungetc(c, f);
	break;
    }

    while ((c = fgetc(f)) != EOF) {
	if (c == ' ' || c == '\t')
	    break;
	if (c == '\n') {
	    ungetc(c, f);
	    break;
	}
	if (buf < bend) {
	    *buf++ = c;
	    continue;
	}
	else {
	    yyerror("word too long, remainder of line ignored");
	    return -1;
	}
    }
    *buf = '\0';

    return c == EOF ? 0 : 1;
}

/*
 * Get the next line from buffer
 */

static int
get_to_eol(FILE *f, char *buf, int len)
{
    int		c;
    char	*bend = &buf[len-1];

    while ((c = fgetc(f)) != EOF) {
	if (c == ' ' || c == '\t')
	    continue;
	ungetc(c, f);
	break;
    }

    while ((c = fgetc(f)) != EOF) {
	if (c == '\n')
	    break;
	if (buf < bend) {
	    *buf++ = c;
	    continue;
	}
	else {
	    yyerror("list of words too long, remainder of line ignored");
	    return -1;
	}
    }
    *buf = '\0';
    return c == EOF ? 0 : 1;
}

/*
 * Replacement for pmdaMainLoop
 * Has a select loop on pipe from PMCD, reads in PDUs and acts on them 
 * appropriately.
 */

static void
receivePDUs(pmdaInterface *dispatch)
{
    int			nfds = 0;
    time_t		interval = 0;
    int			sts = 0;
    struct timeval	timeout;
    fd_set		rfds;


    FD_ZERO(&rfds);
    nfds = fileno(stdin)+1;

    for (;;) {

	FD_SET(fileno(stdin), &rfds);
	__pmtimevalNow(&timeout);
	timeout.tv_usec = 0;
	interval = (time_t)wl_refreshDelay - (timeout.tv_sec % (time_t)wl_refreshDelay);
	timeout.tv_sec = interval;

	if (pmDebugOptions.appl1)
	    logmessage(LOG_DEBUG, "Select set for %d seconds\n", 
			 interval);

	sts = select(nfds, &rfds, (fd_set*)0, (fd_set*)0, &timeout);
	if (sts < 0) {
	    logmessage(LOG_ERR, "Error on fetch select: %s", netstrerror());
	    exit(1);
	}  

	if (sts == 0) {

	    if (pmDebugOptions.appl1)
	    	logmessage(LOG_DEBUG, "Select timed out\n");

	    refreshAll();
	    continue;
	}

	if (__pmdaMainPDU(dispatch) < 0){
	    exit(1);
	}

	if (interval == 0) {
	    refreshAll();
	}

    }
}

/*
 * Catch an SPROC dying, report what we know, and exit
 * -- when main exits, other sprocs will get SIGHUP and exit quietly
 */
static void
onchld(int dummy)
{
    int done;
    int waitStatus;
    int sprocNum;

    while ((done = waitpid(-1, &waitStatus, WNOHANG)) > 0) {
	for (sprocNum = 1; 
	     wl_sproc[sprocNum].pid != done && sprocNum <= wl_numSprocs; 
	     sprocNum++);

	if (sprocNum > wl_numSprocs)
	    {
		logmessage(LOG_INFO, 
			"Unexpected child process (pid=%d) died!\n",
			done);
		continue;
	    }

	if (WIFEXITED(waitStatus)) {

	    if (WEXITSTATUS(waitStatus) == 0)
		logmessage(LOG_INFO, 
			     "Sproc %d (pid=%d) exited normally\n",
			     sprocNum, done);
	    else
		logmessage(LOG_INFO, 
			     "Sproc %d (pid=%d) exited with status = %d\n",
			     sprocNum, done, WEXITSTATUS(waitStatus));
	}
	else if (WIFSIGNALED(waitStatus)) {
	    
#ifdef WCOREDUMP
	    if (WCOREDUMP(waitStatus))
		logmessage(LOG_INFO, 
			     "Sproc %d (pid=%d) received signal = %d and dumped core\n",
			     sprocNum, done, WTERMSIG(waitStatus));
#endif
	    logmessage(LOG_INFO, 
			 "Sproc %d (pid=%d) received signal = %d\n",
			 sprocNum, done, WTERMSIG(waitStatus));
	}
	else {
	    logmessage(LOG_INFO, 
			 "Sproc %d (pid=%d) died, reason unknown\n",
			 sprocNum, done);
	}

	logmessage(LOG_INFO, 
		     "Sproc %d managed servers %d to %d\n",
		     sprocNum,
		     wl_sproc[sprocNum].firstServer,
		     wl_sproc[sprocNum].lastServer);
    }

    logmessage(LOG_INFO, "Main process exiting\n");
    exit(0);
}

/*
 * Parse command line args and the configuration file. Also sets up and fires 
 * off the required sprocs
 */

int
main(int argc, char **argv)
{
    WebServer		*server = (WebServer *)0;
    WebSproc		*proc = (WebSproc *)0;

    char		*endnum = (char*)0;
    char		buf1[FILENAME_MAX];
    char		buf2[FILENAME_MAX];
    char		emess[120];
    char		*pstart, *pend;
    char		argsDone, argFound;
    char		*err_msg;

    int			i = 0;
    int			argCount = 0;
    int			checkOnly = 0;
    int			sts = 0;
    int			sep = __pmPathSeparator();
    int			n = 0;
    int			serverTableSize = 0;
    int			regexTableSize = 0;

    FILE		*configFile = (FILE*)0;
    FILE		*tmpFp = (FILE*)0;

    pmdaInterface	desc;
    struct timeval	delta;

    struct {
	int		*argPos;
	char		*argString;
    } regexargs[2];

    struct timeval	start;
    struct timeval	end;
    double		startTime;

    __pmSetProgname(argv[0]);
    __pmGetUsername(&wl_username);

    __pmtimevalNow(&start);

    wl_isDSO = 0;

    pmsprintf(wl_helpFile, sizeof(wl_helpFile), "%s%c" "weblog" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&desc, PMDA_INTERFACE_2, pmProgname, WEBSERVER,
		wl_logFile, wl_helpFile);

    while ((n = pmdaGetOpt(argc, argv, "CD:d:h:i:l:n:pS:t:u:U:6:?", 
			   &desc, &err)) != EOF) {
	switch (n) {

	case 'C':
	    checkOnly = 1;
	    break;

	case 'S':
	    wl_sprocThresh = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -S requires numeric argument\n",
			pmProgname);
		err++;
	    }
	    break;
	    
	case 'n':
	    if (pmParseInterval(optarg, &delta, &err_msg) < 0) {
                    (void)fprintf(stderr,
                            "%s: -n requires a time interval: %s\n",
                            err_msg, pmProgname);
                    free(err_msg);
                    err++;
                }
	    else {
		wl_chkDelay = delta.tv_sec;
	    }
	    break;
	    
	case 't':
	    if (pmParseInterval(optarg, &delta, &err_msg) < 0) {
                    (void)fprintf(stderr,
                            "%s: -t requires a time interval: %s\n",
                            err_msg, pmProgname);
                    free(err_msg);
                    err++;
                }
	    else {
		wl_refreshDelay = delta.tv_sec;
	    }
	    break;

	case 'U':
	    wl_username = optarg;
	    break;

	default:
	    fprintf(stderr, "%s: Unknown option \"-%c\"", pmProgname, (char)n);
	    err++;
	    break;
	}
    }

    if (err || optind != argc-1) {
    	usage();
    }

    line = 0;
    configFileName = argv[optind];
    configFile = fopen(configFileName, "r");
    
    if (configFile == (FILE*)0) {
	fprintf(stderr, "Unable to open config file %s\n", configFileName);
    	usage();
    }

    if (checkOnly == 0) {
	/*
	 * if doing more than just parsing, force errors from here
	 * on into the logfile
	 */
	pmdaOpenLog(&desc);
	__pmSetProcessIdentity(wl_username);
    }

    /*
     * Parse the configuration file
     */

    /* These settings should be reflected below */
    regexargs[0].argString = strdup("method");
    regexargs[1].argString = strdup("size");

    while(!feof(configFile)) {

	sts = getword(configFile, buf1, sizeof(buf1));

	if (sts == 0) {
	    /* End of File */
	    break;
	}

	line++;

	if (sts < 0) {
	    /* error, reported in getword() */
	    skip_to_eol(configFile);
	    continue;
	}
	
	if (buf1[0] == '\0' || buf1[0] == '#') {
	    /* comment, or nothing in the line, next line please */
	    skip_to_eol(configFile);
	    continue;
	}

	if (strcasecmp(buf1, "regex_posix") == 0) {
	    /*
	     * Parse a regex specification
	     */

	    if (wl_numRegex == regexTableSize) {
		regexTableSize += 2;
		wl_regexTable = (WebRegex*)realloc(wl_regexTable,
					   regexTableSize * sizeof(WebRegex));
		if (wl_regexTable == (WebRegex*)0) {
		    __pmNoMem("main.wl_regexInst", 
			     (wl_numRegex + 1) * sizeof(WebRegex),
			     PM_FATAL_ERR);
		}
	    }

	    sts = getword(configFile, buf1, sizeof(buf1));

	    if (sts <= 0 || buf1[0] == '\0') {
		if (sts >= 0)
		    yyerror("unable to extract regex name");
		skip_to_eol(configFile);
		continue;
	    }

	    wl_regexTable[wl_numRegex].name = strdup(buf1);

  	    if (wl_numRegex) {
		for (n = 0; n < wl_numRegex; n++) {
		    if (strcmp(wl_regexTable[n].name, 
			       wl_regexTable[wl_numRegex].name) == 0) {

			pmsprintf(emess, sizeof(emess), "duplicate regex name (%s)",
				wl_regexTable[wl_numRegex].name);
			yyerror(emess);
			break;
		    }
		}
		if (n < wl_numRegex) {
		    skip_to_eol(configFile);
		    continue;
		}
	    }

	    sts = getword(configFile, buf1, sizeof(buf1));

	    if (sts <= 0 || buf1[0] == '\0') {
		if (sts >= 0)
		    yyerror("unable to extract regex match parameters");
		skip_to_eol(configFile);
		continue;
	    }

	    regexargs[0].argPos = &(wl_regexTable[wl_numRegex].methodPos);
	    regexargs[1].argPos = &(wl_regexTable[wl_numRegex].sizePos);
	    wl_regexTable[wl_numRegex].methodPos = 0;
	    wl_regexTable[wl_numRegex].sizePos = 0;
	    wl_regexTable[wl_numRegex].sizePos = 0;
	    wl_regexTable[wl_numRegex].s_statusPos = 0;

	    pstart = buf1;
	    argCount = 0;
	    do {
		argFound = 0;
		argsDone = 1;
		argCount++;
		for(pend = pstart; *pend; pend++) {
		    if(*pend == ',') {
			*pend = '\0';
			argsDone = 0;
			break;
		    }
		}
		for(i = 0; i < sizeof(regexargs) / sizeof(regexargs[0]); i++) {
		    if(strcmp(pstart, regexargs[i].argString) == 0) {
			*regexargs[i].argPos = argCount;
			argFound = 1;
			break;
		    }
		}
		if(!argFound) {
		    /* not the old method,size style */
		    switch(pstart[0]) {
		    case '1':
			wl_regexTable[wl_numRegex].methodPos = argCount;
			argFound = 1;
			break;
		    case '2':
			wl_regexTable[wl_numRegex].sizePos = argCount;
			argFound = 1;
			break;
		    case '3':
			wl_regexTable[wl_numRegex].c_statusPos = argCount;
			argFound = 1;
			break;
		    case '4':
			wl_regexTable[wl_numRegex].s_statusPos = argCount;
			argFound = 1;
			break;
		    case '-':
			wl_regexTable[wl_numRegex].methodPos = argCount++;
			wl_regexTable[wl_numRegex].sizePos = argCount;
			argFound = 1;
			argsDone = 1;
			break;
		    default:
			break;
		    }
		}
		pstart = pend + 1;
	    } while(argsDone == 0 && argFound != 0);

	    if(argFound == 0) {
		yyerror("invalid keyword in regex match parameters");
		skip_to_eol(configFile);
		continue;
		}

	    sts = get_to_eol(configFile, buf1, sizeof(buf1));

	    if (sts <= 0 || buf1[0] == '\0') {
		if (sts >= 0)
		    yyerror("unable to extract regex");
		else
		    skip_to_eol(configFile);
		continue;
	    }

	    wl_regexTable[wl_numRegex].regex = malloc(sizeof(*wl_regexTable[wl_numRegex].regex));
	    if(wl_regexTable[wl_numRegex].regex == NULL) {
		__pmNoMem("main.wl_regex", 
			  sizeof(*wl_regexTable[wl_numRegex].regex),
			  PM_FATAL_ERR);
	    }

	    if (regcomp(wl_regexTable[wl_numRegex].regex, buf1, REG_EXTENDED) != 0) {
		yyerror("unable to compile regex");
		continue;
	    }

	    if (pmDebugOptions.appl0)
	    	logmessage(LOG_DEBUG, "%d regex %s: %s\n", 
			wl_numRegex, wl_regexTable[wl_numRegex].name, buf1);

	    wl_regexTable[wl_numRegex].posix_regexp = 1;
	    wl_numRegex++;
	}
#ifdef NON_POSIX_REGEX
	else if (strcasecmp(buf1, "regex") == 0) {
	    /*
	     * Parse a regex specification
	     */

	    if (wl_numRegex == regexTableSize) {
		regexTableSize += 2;
		wl_regexTable = (WebRegex*)realloc(wl_regexTable,
					   regexTableSize * sizeof(WebRegex));
		if (wl_regexTable == (WebRegex*)0) {
		    __pmNoMem("main.wl_regexInst", 
			     (wl_numRegex + 1) * sizeof(WebRegex),
			     PM_FATAL_ERR);
		}
	    }

	    sts = getword(configFile, buf1, sizeof(buf1));

	    if (sts <= 0 || buf1[0] == '\0') {
		if (sts >= 0)
		    yyerror("unable to extract regex name");
		skip_to_eol(configFile);
		continue;
	    }

	    wl_regexTable[wl_numRegex].name = strdup(buf1);

  	    if (wl_numRegex) {
		for (n = 0; n < wl_numRegex; n++) {
		    if (strcmp(wl_regexTable[n].name, 
			       wl_regexTable[wl_numRegex].name) == 0) {

			pmsprintf(emess, sizeof(emess), "duplicate regex name (%s)",
				wl_regexTable[wl_numRegex].name);
			yyerror(emess);
			break;
		    }
		}
		if (n < wl_numRegex) {
		    skip_to_eol(configFile);
		    continue;
		}
	    }

	    sts = get_to_eol(configFile, buf1, sizeof(buf1));

	    if (sts <= 0 || buf1[0] == '\0') {
		if (sts >= 0)
		    yyerror("unable to extract regex");
		else
		    skip_to_eol(configFile);
		continue;
	    }

	    if(strstr(buf1, "$2") != NULL && strstr(buf1, "$3") != NULL ) {
		/* 
		 * extended caching server format 
		 *
		 * although these aren't used in the non-regex code, they
		 * are a good enough placeholder until server->counts.extendedp
		 * is set below
		 */
		wl_regexTable[wl_numRegex].c_statusPos = 1;
		wl_regexTable[wl_numRegex].s_statusPos = 1;

            }
	    wl_regexTable[wl_numRegex].np_regex = regcmp(buf1, (char*)0);

	    if (wl_regexTable[wl_numRegex].np_regex == (char*)0) {
		yyerror("unable to compile regex");
		continue;
	    }

	    if (pmDebugOptions.appl0)
	    	logmessage(LOG_DEBUG, "%d NON POSIX regex %s: %s\n", 
			wl_numRegex, wl_regexTable[wl_numRegex].name, buf1);

	    wl_regexTable[wl_numRegex].posix_regexp = 0;
	    wl_numRegex++;
	}
#endif
	else if (strcasecmp(buf1, "server") == 0) {
	    /*
	     * Parse a server specification
	     */

	    if (wl_numServers == serverTableSize) {
		serverTableSize += 4;
		wl_serverInst = (pmdaInstid*)realloc(wl_serverInst,
					 serverTableSize * sizeof(pmdaInstid));
		if (wl_serverInst == (pmdaInstid*)0) {
		    __pmNoMem("main.wl_serverInst", 
			     (wl_numServers + 1) * sizeof(pmdaInstid),
			     PM_FATAL_ERR);
		}

		wl_servers = (WebServer*)realloc(wl_servers,
					 serverTableSize * sizeof(WebServer));
		if (wl_servers == (WebServer*)0) {
		    __pmNoMem("main.wl_servers", 
			     (wl_numServers + 1) * sizeof(WebServer),
			     PM_FATAL_ERR);
		}
	    }

	    /* Get server name */

	    sts = getword(configFile, buf1, sizeof(buf1));

	    if (sts <= 0 || buf1[0] == '\0') {
		if (sts >= 0)
		    yyerror("unable to extract server name");
		skip_to_eol(configFile);
		continue;
	    }

	    if (wl_numServers) {
		for (n = 0; n < wl_numServers; n++) {
		    if (strcmp(buf1, wl_serverInst[n].i_name) == 0) {
			pmsprintf(emess, sizeof(emess), "duplicate server name (%s)", buf1);
			yyerror(emess);
			break;
		    }
		}
		if (n < wl_numServers) {
		    skip_to_eol(configFile);
		    continue;
		}
	    }

	    wl_serverInst[wl_numServers].i_name = strdup(buf1);
	    wl_serverInst[wl_numServers].i_inst = wl_numServers;

	    server = &(wl_servers[wl_numServers]);
	    memset(server, 0, sizeof(*server));
	    server->access.filePtr = -1;
	    server->error.filePtr = -1;

	    /* Get server active flag */

	    sts = getword(configFile, buf1, sizeof(buf1));

	    if (sts <= 0 || buf1[0] == '\0') {
		if (sts >= 0)
		    yyerror("unable to extract active flag");
		skip_to_eol(configFile);
		continue;
	    }

	    if (strcasecmp(buf1, "on") == 0) {
		server->counts.active = 1;
	    }
	    else if (strcasecmp(buf1, "off") == 0) {
		server->counts.active = 0;
	    }
	    else {
	    	yyerror("illegal active flag");
		skip_to_eol(configFile);
		continue;
	    }

	    /* Get access log regex and file name */


	    sts = getword(configFile, buf1, sizeof(buf1));

	    if (sts <= 0 || buf1[0] == '\0') {
		if (sts >= 0)
		    yyerror("unable to extract access log regex");
		skip_to_eol(configFile);
		continue;
	    }

	    sts = getword(configFile, buf2, sizeof(buf2));

	    if (sts <= 0 || buf2[0] == '\0') {
		if (sts >= 0)
		    yyerror("unable to extract access log name");
		skip_to_eol(configFile);
		continue;
	    }

	    for (n = 0; n < wl_numRegex; n++)
	    	if (strcmp(buf1, wl_regexTable[n].name) == 0)
		    break;

	    if (n == wl_numRegex) {
	    	pmsprintf(emess, sizeof(emess), "access log regex \"%s\" not defined", buf1);
		yyerror(emess);
		skip_to_eol(configFile);
		continue;
	    } else if(wl_regexTable[n].c_statusPos > 0 &&
	              wl_regexTable[n].s_statusPos > 0) {
		/* common extended format or one that uses the same codes */
		server->counts.extendedp = 1;   
	        if(strcmp(wl_regexTable[n].name, "SQUID") == 0) {
		    /* 
		     * default squid format - uses text codes not numerics
		     * so it *has* to be a special case
		     */
		    server->counts.extendedp = 2;   
		}
	    }

	    server->access.format = n;
	    server->access.fileName = strdup(buf2);
	    
	    if (server->counts.active) {
		tmpFp = fopen(server->access.fileName, "r");
		if (tmpFp == (FILE*)0) {
		    pmsprintf(emess, sizeof(emess), "cannot open access log \"%s\"", buf2);
		    yywarn(emess);
		    server->access.filePtr = -1;
		}
		else
		    fclose(tmpFp);
	    }

	    /* Get error log regex and file name */

	    sts = getword(configFile, buf1, sizeof(buf1));

	    if (sts <= 0 || buf1[0] == '\0') {
		if (sts >= 0)
		    yyerror("unable to extract error log regex");
		skip_to_eol(configFile);
		continue;
	    }

	    sts = getword(configFile, buf2, sizeof(buf2));

	    if (sts <= 0 || buf2[0] == '\0') {
		if (sts >= 0)
		    yyerror("unable to extract error log name");
		skip_to_eol(configFile);
		continue;
	    }

	    for (n = 0; n < wl_numRegex; n++)
	    	if (strcmp(buf1, wl_regexTable[n].name) == 0)
		    break;

	    if (n == wl_numRegex) {
	    	pmsprintf(emess, sizeof(emess), "error log regex \"%s\" not defined", buf1);
		yyerror(emess);
		skip_to_eol(configFile);
		continue;
	    }

	    server->error.format = n;
	    server->error.fileName = strdup(buf2);
	    
	    if (server->counts.active) {
		tmpFp = fopen(server->error.fileName, "r");
		if (tmpFp == (FILE*)0) {
		    pmsprintf(emess, sizeof(emess), "cannot open error log \"%s\"", buf2);
		    yywarn(emess);
		    server->error.filePtr = -1;
		}
		else
		    fclose(tmpFp);
	    }

	    check_to_eol(configFile);

	    if (pmDebugOptions.appl0) {
		logmessage(LOG_DEBUG, "%d Server %s, %d, %d, %s, %d, %s\n",
			wl_numServers,
			wl_serverInst[wl_numServers].i_name,
			server->counts.active,
			server->access.format,
			server->access.fileName,
			server->error.format,
			server->error.fileName);
	    }

	    if (server->counts.active)
		wl_numActive++;

	    wl_numServers++;
	}
	else {
	    pmsprintf(emess, sizeof(emess), "illegal keyword \"%s\"", buf1);
	    yyerror(emess);
	    skip_to_eol(configFile);
	    continue;
	}
    }

    if (wl_numServers == 0) {
	yyerror("no servers were specified in the configuration file!");
    }

    fclose(configFile);

    if (checkOnly || err) {
	/* errors, or parse only, no PMCD communication option */
	exit(err);
    }

    wl_indomTable[0].it_numinst = wl_numServers;
    wl_indomTable[0].it_set = wl_serverInst;

    web_init(&desc);
    pmdaConnect(&desc);

    /* catch any sprocs dying */

    signal(SIGCHLD, onchld);

    /* fire off all the sprocs that we need */

    wl_numSprocs = (wl_numServers-1) / wl_sprocThresh;
    wl_sproc = (WebSproc*)malloc((wl_numSprocs+1) * sizeof(WebSproc));
    if (wl_sproc == NULL) {
	logmessage(LOG_ERR,
		   "wl_numServers = %d, wl_sprocThresh = %d",
		   wl_numServers,
		   wl_sprocThresh);
	__pmNoMem("main.wl_sproc", 
		  (wl_numSprocs+1) * sizeof(WebSproc),
		  PM_FATAL_ERR);
    }


    for (n = 0; n <= wl_numSprocs; n++)
	{
	    proc = &wl_sproc[n];
	    proc->pid = -1;
	    proc->methodStr = (char *)0;
	    proc->sizeStr = (char *)0;
	    proc->c_statusStr = (char *)0;
	    proc->s_statusStr = (char *)0;
	    proc->strLength = 0;
	}

    if (wl_numSprocs) {

	for (n=1; n<=wl_numSprocs; n++) {
	    proc = &wl_sproc[n];

	    sts = pipe1(proc->inFD);
	    if (sts) {
	    	logmessage(LOG_ERR, 
			     "Cannot allocate fileDes 1 for sproc[%d]", 
			     n);
		exit(1);
	    }

	    if (pmDebugOptions.appl2)
		logmessage(LOG_DEBUG,
			      "Creating in pipe (in=%d, out=%d) for sproc %d\n",
			      proc->inFD[0],
			      proc->inFD[1],
			      n);

	    sts = pipe1(proc->outFD);
	    if (sts) {
	    	logmessage(LOG_ERR, 
			     "Cannot allocate fileDes 2 for sproc[%d]", 
			     n);
		exit(1);
	    }

	    if (pmDebugOptions.appl2)
		logmessage(LOG_DEBUG,
			      "Creating out pipe (in=%d, out=%d) for sproc %d\n",
			      proc->outFD[0],
			      proc->outFD[1],
			      n);

	    proc->firstServer = (n)*wl_sprocThresh;
	    if (n != wl_numSprocs)
	    	proc->lastServer = proc->firstServer +
		    wl_sprocThresh - 1;
	    else
	    	proc->lastServer = wl_numServers - 1;

	    logmessage(LOG_INFO, 
			 "Creating sproc [%d] for servers %d to %d\n",
			 n, proc->firstServer, proc->lastServer);

	    proc->id = n;

#ifndef HAVE_SPROC
	    proc->pid = sproc(sprocMain, CLONE_VM, (void*)(&proc->id));
#else
	    proc->pid = sproc(sprocMain, PR_SADDR, (void*)(&proc->id));
#endif

	    if (proc->pid < 0) {
	    	logmessage(LOG_ERR, "main: error creating sproc %d: %s\n",
			     n, osstrerror());
		exit(1);
	    }

	    if(pmDebugOptions.appl0) {
		logmessage(LOG_INFO,
			   "main: created sproc %d: pid %" FMT_PID "\n",
			   n,
			   proc->pid);
	    }

	    /* close off unwanted pipes */

	    if(close(proc->inFD[0]) < 0) {
		logmessage(LOG_WARNING,
			   "main: pipe close(fd=%d) failed: %s\n",
			   proc->inFD[0], osstrerror());
	    }
	    if(close(proc->outFD[1]) < 0) {
		logmessage(LOG_WARNING,
			   "main: pipe close(fd=%d) failed: %s\n",
			   proc->outFD[1], osstrerror());
	    }
	}
    }

    wl_sproc[0].firstServer = 0;
    wl_sproc[0].lastServer  = (wl_numServers <= wl_sprocThresh) ?
	wl_numServers - 1 : wl_sprocThresh - 1;

    if (pmDebugOptions.appl0)
	logmessage(LOG_DEBUG,
		      "Main process will monitor servers 0 to %d\n",
		      wl_sproc[0].lastServer);

    for (n=0; n <= wl_sproc[0].lastServer; n++) {
    	if (wl_servers[n].counts.active) {
	    openLogFile(&(wl_servers[n].access));
	    openLogFile(&(wl_servers[n].error));
	}
    }

    __pmtimevalNow(&end);
    startTime = __pmtimevalSub(&end, &start);
    if (pmDebugOptions.appl0)
	logmessage(LOG_DEBUG, "Agent started in %f seconds", startTime);

    receivePDUs(&desc);

    logmessage(LOG_INFO, "Connection to PMCD closed by PMCD\n");
    logmessage(LOG_INFO, "Last fetch took %d msec\n", wl_catchupTime);
    logmessage(LOG_INFO, "Exiting...\n");

    return 0;
}
