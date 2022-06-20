/***********************************************************************
 * pmie.c - performance inference engine
 ***********************************************************************
 *
 * Copyright (c) 2013-2015,2017,2020,2022 Red Hat.
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

/*
 * pmie debug flags:
 *	APPL0	- lexical scanning
 *	APPL1	- parse/expression tree construction
 *	APPL2	- expression execution
 */

#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "libpcp.h"

#include "dstruct.h"
#include "stomp.h"
#include "syntax.h"
#include "pragmatics.h"
#include "eval.h"
#include "show.h"

/***********************************************************************
 * constants
 ***********************************************************************/

#define LINE_LENGTH	255		/* max length of command token */
#define PROC_FNAMESIZE  20              /* from proc pmda - proc.h */

static char *prompt = "pmie> ";
static char *intro  = "Performance Co-Pilot Inference Engine (pmie), "
		      "Version %s\n\n%s%s";
char	*clientid;

static FILE *logfp;
static char logfile[MAXPATHLEN];
static char perffile[MAXPATHLEN];	/* /var/tmp/<pid> file name */
static char *username;

static char menu[] =
"pmie debugger commands\n\n"
"  f [file-name]      - load expressions from given file or stdin\n"
"  l [expr-name]      - list named expression or all expressions\n"
"  r [interval]       - run for given or default interval\n"
"  S time-spec        - set start time for run\n"
"  T time-spec        - set default interval for run command\n"
"  v [expr-name]      - print subexpression used for %h, %i and\n"
"                       %v bindings\n"
"  h or ?             - print this menu of commands\n"
"  q                  - quit\n\n";

/***********************************************************************
 * command line usage
 ***********************************************************************/

static int
override(int opt, pmOptions *opts)
{
    if (opt == 'a' || opt == 'h' || opt == 'H' || opt == 'V')
	return 1;
    return 0;
}

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_ALIGN,
    PMOPT_ARCHIVE,
    PMOPT_DEBUG,
    PMOPT_HOST,
    PMOPT_NAMESPACE,
    PMOPT_ORIGIN,
    PMOPT_START,
    PMOPT_FINISH,
    PMOPT_INTERVAL,
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_HELP,
    PMAPI_OPTIONS_HEADER("Runtime options"),
    { "check", 0, 'C', 0, "parse configuration and exit" },
    { "config", 1, 'c', "FILE", "configuration file" },
    { "interact", 0, 'd', 0, "interactive debugging mode" },
    { "primary", 0, 'P', 0, "execute as primary inference engine" },
    { "foreground", 0, 'f', 0, "run in the foreground, not as a daemon" },
    { "systemd", 0, 'F', 0, "systemd mode - notify service manager (if any) when started and ready" },
    { "", 0, 'H', NULL }, /* was: no DNS lookup on the default hostname */
    { "", 1, 'j', "FILE", "stomp protocol (JMS) file" },
    { "logfile", 1, 'l', "FILE", "send status and error messages to FILE" },
    { "username", 1, 'U', "USER", "run as named USER in daemon mode [default pcp]" },
    PMAPI_OPTIONS_HEADER("Reporting options"),
    { "buffer", 0, 'b', 0, "one line buffered output stream, stdout on stderr" },
    { "timestamp", 0, 'e', 0, "force timestamps to be reported with -V, -v or -W" },
    { "quiet", 0, 'q', 0, "quiet mode, default diagnostics suppressed" },
    { "", 0, 'v', 0, "verbose mode, expression values printed" },
    { "verbose", 0, 'V', 0, "verbose mode, annotated expression values printed" },
    { "", 0, 'W', 0, "verbose mode, satisfying expression values printed" },
    { "secret-applet", 0, 'X', 0, "run in secret applet mode (thin client)" },
    { "secret-agent", 0, 'x', 0, "run in secret agent mode (summary PMDA)" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_STDOUT_TZ,
    .short_options = "a:A:bc:CdD:efFHh:j:l:n:O:PqS:t:T:U:vVWXxzZ:?",
    .long_options = longopts,
    .short_usage = "[options] [filename ...]",
    .override = override,
};


/***********************************************************************
 * interactive commands
 ***********************************************************************/

/* read command input line */
static int
readLine(char *bfr, int max)
{
    int		c, i;

    /* skip blanks */
    do
	c = getchar();
    while (isspace(c));

    /* scan till end of line */
    i = 0;
    while ((c != '\n') && (c != EOF) && (i < max)) {
	bfr[i++] = c;
	c = getchar();
    }
    bfr[i] = '\0';
    return (c != EOF);
}


/* scan interactive command token */
static char *
scanCmd(char **pp)
{
    char	*p = *pp;

    /* skip blanks */
    while (isspace((int)*p))
	p++;

    /* single char token */
    if (isgraph((int)*p)) {
	*pp = p + 1;
	return p;
    }

    return NULL;
}


/* scan interactive command argument */
static char *
scanArg(char *p)
{
    char	*q;

    /* strip leading blanks */
    while (isspace((int)*p))
	p++;
    if (*p == '\0')
	return NULL;
    q = p;

    /* strip trailing blanks */
    while (*q != '\0')
	q++;
    q--;
    while (isspace((int)*q))
	q--;
    *(q + 1) = '\0';

    /* return result */
    return p;
}


/* load rules from given file or stdin */
static void
load(char *fname)
{
    Symbol	s;
    Expr	*d;
    int		sts = 0;
    int		sep = pmPathSeparator();
    char	config[MAXPATHLEN+1];

    /* search for configfile on configuration file path */
    if (fname && access(fname, F_OK) != 0) {
	sts = oserror();	/* always report the first error */
	if (__pmAbsolutePath(fname)) {
	    fprintf(stderr, "%s: cannot access config file %s: %s\n", pmGetProgname(), 
		    fname, strerror(sts));
	    exit(1);
	}
	else if (pmDebugOptions.appl0) {
	    fprintf(stderr, "load: cannot access config file %s: %s\n", fname, strerror(sts));
	}
	pmsprintf(config, sizeof(config)-1, "%s%c" "config%c" "pmie%c" "%s",
		pmGetConfig("PCP_VAR_DIR"), sep, sep, sep, fname);
	if (access(config, F_OK) != 0) {
	    fprintf(stderr, "%s: cannot access config file as either %s or %s: %s\n",
		    pmGetProgname(), fname, config, strerror(sts));
	    exit(1);
	}
	else if (pmDebugOptions.appl0) {
	    fprintf(stderr, "load: using standard config file %s\n", config);
	}
	fname = config;
    }
    else if (pmDebugOptions.appl0) {
	fprintf(stderr, "load: using config file %s\n",
		fname == NULL? "<stdin>":fname);
    }

    if (perf->config[0] == '\0') {	/* keep record of first config */
	if (fname == NULL)
	    strcpy(perf->config, "<stdin>");
	else if (realpath(fname, perf->config) == NULL) {
	    fprintf(stderr, "%s: failed to resolve realpath for %s: %s\n",
		    pmGetProgname(), fname, osstrerror());
	    exit(1);
	}
    }

    if (synInit(fname)) {
	while ((s = syntax()) != NULL) {
	    d = (Expr *) symValue(symDelta);
	    pragmatics(s, *(RealTime *)d->smpls[0].ptr);
	}
    }
}


/* list given expression or all expressions */
static void
list(char *name)
{
    Task	*t;
    Symbol	*r;
    Symbol	s;
    int		i;

    if (name) {	/* single named rule */
	if ( (s = symLookup(&rules, name)) )
	    showSyntax(stdout, s);
	else
	    printf("%s: error - rule \"%s\" not defined\n", pmGetProgname(), name);
    }
    else {	/* all rules */
	t = taskq;
	while (t) {
	    r = t->rules;
	    for (i = 0; i < t->nrules; i++) {
		showSyntax(stdout, *r);
		r++;
	    }
	    t = t->next;
	}
    }
}


/* list binding subexpression of given expression or all expressions */
static void
sublist(char *name)
{
    Task	*t;
    Symbol	*r;
    Symbol	s;
    int		i;

    if (name) {	/* single named rule */
	if ( (s = symLookup(&rules, name)) )
	    showSubsyntax(stdout, s);
	else
	    printf("%s: error - rule '%s' not defined\n", pmGetProgname(), name);
    }
    else {	/* all rules */
	t = taskq;
	while (t) {
	    r = t->rules;
	    for (i = 0; i < t->nrules; i++) {
		showSubsyntax(stdout, *r);
		r++;
	    }
	    t = t->next;
	}
    }
}


/***********************************************************************
 * manipulate the performance instrumentation data structure
 ***********************************************************************/

static void
stopmonitor(void)
{
    if (*perffile)
	unlink(perffile);
}

static void
startmonitor(void)
{
    void		*ptr;
    char		*path;
    int			fd;
    char		zero = '\0';
    char		pmie_dir[MAXPATHLEN];

    /* try to create the port file directory. OK if it already exists */
    pmsprintf(pmie_dir, sizeof(pmie_dir), "%s%c%s",
	     pmGetConfig("PCP_TMP_DIR"), pmPathSeparator(), PMIE_SUBDIR);
    if (mkdir2(pmie_dir, S_IRWXU | S_IRWXG | S_IRWXO) < 0) {
	if (oserror() != EEXIST) {
	    fprintf(stderr, "%s: warning cannot create stats file dir %s: %s\n",
		    pmGetProgname(), pmie_dir, osstrerror());
	}
    }
    atexit(stopmonitor);

    /* create and initialize memory mapped performance data file */
    pmsprintf(perffile, sizeof(perffile),
		"%s%c%" FMT_PID, pmie_dir, pmPathSeparator(), (pid_t)getpid());
    unlink(perffile);
    if ((fd = open(perffile, O_RDWR | O_CREAT | O_EXCL | O_TRUNC,
			     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
	/* cannot create stats file; too bad, so sad, continue on without it */
	perf = &instrument;
	return;
    }
    /* seek to struct size and write one zero */
    if (lseek(fd, sizeof(pmiestats_t)-1, SEEK_SET) < 0) {
	fprintf(stderr, "%s: Warning: lseek failed for stats file %s: %s\n",
		pmGetProgname(), perffile, osstrerror());
    }
    if (write(fd, &zero, 1) != 1) {
	fprintf(stderr, "%s: Warning: write failed for stats file %s: %s\n",
		pmGetProgname(), perffile, osstrerror());
    }

    /* map perffile & associate the instrumentation struct with it */
    if ((ptr = __pmMemoryMap(fd, sizeof(pmiestats_t), 1)) == NULL) {
	fprintf(stderr, "%s: memory map failed for stats file %s: %s\n",
		pmGetProgname(), perffile, osstrerror());
	perf = &instrument;
    } else {
	perf = (pmiestats_t *)ptr;
    }
    close(fd);

    path = (logfile[0] == '\0') ? "<none>" : logfile;
    strncpy(perf->logfile, path, sizeof(perf->logfile));
    perf->logfile[sizeof(perf->logfile)-1] = '\0';
    /* Don't try to improvise a current fdqn for "the" pmcd. 
       It'll be filled in periodically by newContext(). */
    strncpy(perf->defaultfqdn, "(uninitialized)", sizeof(perf->defaultfqdn));
    perf->defaultfqdn[sizeof(perf->defaultfqdn)-1] = '\0';

    perf->version = 1;
}


/***********************************************************************
 * signal handling
 ***********************************************************************/

static void
sigintproc(int sig)
{
    __pmSetSignalHandler(SIGINT, SIG_IGN);
    __pmSetSignalHandler(SIGTERM, SIG_IGN);
    if (pmDebugOptions.desperate)
	pmNotifyErr(LOG_INFO, "%s caught SIGINT or SIGTERM\n", pmGetProgname());
    if (inrun)
	doexit = sig;
    else {
	/* for RH BZ 1327226 */
	fprintf(stderr, "\nInterrupted! signal=%d\n", sig);
	exit(1);
    }
}

static void
remap_stdout_stderr(void)
{
    int	i, j;

    fflush(stderr);
    fflush(stdout);
    setlinebuf(stderr);
    setlinebuf(stdout);
    i = fileno(stdout);
    close(i);
    if ((j = dup(fileno(stderr))) != i) {
	fprintf(stderr, "%s: Warning: failed to link stdout ... "
			"dup() returns %d, expected %d (stderr=%d)\n",
			pmGetProgname(), j, i, fileno(stderr));
	if (j >= 0)
	    close(j);
    }
}

void
logRotate(void)
{
    FILE *fp;
    int sts;

    fp = __pmRotateLog(pmGetProgname(), logfile, logfp, &sts);
    if (sts != 0) {
	fprintf(stderr, "pmie: PID = %" FMT_PID ", via %s\n\n",
                (pid_t)getpid(), dfltHostConn);
	remap_stdout_stderr();
	logfp = fp;
    } else {
	pmNotifyErr(LOG_ERR, "pmie: log rotation failed\n");
    }
}

static void
sighupproc(int sig)
{
    __pmSetSignalHandler(SIGHUP, sighupproc);
   dorotate = 1;
}

static void
sigbadproc(int sig)
{
    if (pmDebugOptions.desperate) {
	pmNotifyErr(LOG_ERR, "Unexpected signal %d ...\n", sig);
	fprintf(stderr, "\n");
	__pmDumpStack();
	fflush(stderr);
    }
    stopmonitor();
    _exit(sig);
}


/***********************************************************************
 * command line processing - extract command line arguments & initialize
 ***********************************************************************/

static void
getargs(int argc, char *argv[])
{
    char		*configfile = NULL;
    char		*commandlog = NULL;
    char		*subopts;
    char		*subopt;
    char		*msg = NULL;
    int			checkFlag = 0;
    int			foreground = 0;
    int			primary = 0;
    int			sts;
    int			c;
    int			bflag = 0;
    int			dfltConn = 0;	/* default context type */
    Archive		*a;
    struct timeval	tv, tv1, tv2;

    extern int		showTimeFlag;
    extern int		errs;		/* syntax errors from syntax.c */

    memset(&tv, 0, sizeof(tv));
    memset(&tv1, 0, sizeof(tv1));
    memset(&tv2, 0, sizeof(tv2));
    dstructInit();

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
        switch (c) {

	case 'a':			/* archives */
	    if (dfltConn && dfltConn != PM_CONTEXT_ARCHIVE) {
                /* (technically, multiple -a's are allowed.) */
		pmprintf("%s: at most one of -a or -h allowed\n", pmGetProgname());
		opts.errors++;
		break;
	    }
	    dfltConn = opts.context = PM_CONTEXT_ARCHIVE;
	    subopts = opts.optarg;
	    for ( ; ; ) {
		subopt = subopts;
		subopts = strchr(subopts, ',');
		if (subopts != NULL) {
		    *subopts++ = '\0';
		}
		a = (Archive *)zalloc(sizeof(Archive));
		a->fname = subopt;
		if (!initArchive(a)) {
		    exit(1);
		}
		if (subopts == NULL) break;
	    }
	    foreground = 1;
	    break;

	case 'b':			/* line buffered, stdout on stderr */
	    bflag++;
	    break;

	case 'c': 			/* configuration file */
	    if (interactive) {
		pmprintf("%s: at most one of -c and -d allowed\n", pmGetProgname());
		opts.errors++;
		break;
	    }
	    configfile = opts.optarg;
	    break;

	case 'C': 			/* check config and exit */
	    checkFlag = 1;
	    break;

	case 'd': 			/* interactive mode */
	    if (configfile) {
		pmprintf("%s: at most one of -c and -d allowed\n", pmGetProgname());
		opts.errors++;
		break;
	    }
	    interactive = 1;
	    break;

	case 'e':	/* force timestamps */
	    showTimeFlag = 1;
	    break;

	case 'f':			/* in foreground, not as daemon */
	    foreground = 1;
	    break;

	case 'P':			/* primary (local) pmie process */
	    primary = 1;
	    isdaemon = 1;
	    break;

	case 'F':			/* systemd mode */
	    systemd = 1;
	    isdaemon = 1;
	    break;

	case 'H': 			/* deprecated: no DNS lookups */
	    break;

	case 'h': 			/* default host name */
	    if (dfltConn) {
		pmprintf("%s: at most one of -a or -h allowed\n", pmGetProgname());
		opts.errors++;
		break;
	    }
	    dfltConn = opts.context = PM_CONTEXT_HOST;
	    dfltHostConn = opts.optarg;
	    break;

        case 'j':			/* stomp protocol (JMS) config */
	    stompfile = opts.optarg;
	    break;

	case 'l':			/* alternate log file */
	    if (commandlog != NULL) {
		pmprintf("%s: at most one -l option is allowed\n", pmGetProgname());
		opts.errors++;
		break;
	    }
	    commandlog = opts.optarg;
	    isdaemon = 1;
	    break;

	case 'U': 			/* run as named user */
	    username = opts.optarg;
	    isdaemon = 1;
	    break;

	case 'q': 			/* suppress default diagnostics */
	    quiet = 1;
	    break;

	case 'v': 			/* print values */
	    verbose = 1;
	    break;

	case 'V': 			/* print annotated values */
	    verbose = 2;
	    break;

	case 'W': 			/* print satisfying values */
	    verbose = 3;
	    break;

	case 'X': 			/* secret applet flag */
	    applet = 1;
	    verbose = 1;
	    setlinebuf(stdout);
	    break;

	case 'x': 			/* summary PMDA flag */
	    agent = 1;
	    verbose = 1;
	    isdaemon = 1;
	    break;
	}
    }

    if (!opts.errors && configfile && opts.optind != argc) {
	pmprintf("%s: extra filenames cannot be given after using -c\n",
		pmGetProgname());
	opts.errors++;
    }
    if (!opts.errors && foreground && systemd) {
	pmprintf("%s: the -f and -F options are mutually exclusive\n",
		pmGetProgname());
	opts.errors++;
    }
    if (!opts.errors && bflag && agent) {
	pmprintf("%s: the -b and -x options are incompatible\n",
		pmGetProgname());
	opts.errors++;
    }
    if (opts.errors) {
    	pmUsageMessage(&opts);
	exit(1);
    }
    /* check if archives/hosts available in the environment */
    if (!dfltConn && opts.narchives) {
	dfltConn = opts.context = PM_CONTEXT_ARCHIVE;
	for (c = 0; c < opts.narchives; c++) {
	    a = (Archive *)zalloc(sizeof(Archive));
	    a->fname = opts.archives[c];
	    if (!initArchive(a))
		exit(1);
	}
	foreground = 1;
    }
    if (!dfltConn && opts.nhosts) {
	dfltConn = opts.context = PM_CONTEXT_HOST;
	dfltHostConn = opts.hosts[c];
    }

    if (foreground)
	isdaemon = 0;

    hostZone = opts.tzflag;
    timeZone = opts.timezone;
    if (opts.interval.tv_sec || opts.interval.tv_usec)
	dfltDelta = pmtimevalToReal(&opts.interval);

    if (archives || interactive)
	perf = &instrument;

    if (isdaemon) {			/* daemon mode */
	/* done before opening log to get permissions right */
	pmSetProcessIdentity(username);

#if defined(HAVE_TERMIO_SIGNALS)
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
#endif
	__pmSetSignalHandler(SIGINT, sigintproc);
	__pmSetSignalHandler(SIGTERM, sigintproc);
	__pmSetSignalHandler(SIGBUS, sigbadproc);
	__pmSetSignalHandler(SIGSEGV, sigbadproc);
    }
    else {
	/* need to catch these so the atexit() processing is done */
	__pmSetSignalHandler(SIGINT, sigintproc);
	__pmSetSignalHandler(SIGTERM, sigintproc);
    }

    if (commandlog != NULL) {
	logfp = pmOpenLog(pmGetProgname(), commandlog, stderr, &sts);
	if (strcmp(commandlog, "-") != 0 && realpath(commandlog, logfile) == NULL) {
	    fprintf(stderr, "%s: cannot find realpath for log %s: %s\n",
		    pmGetProgname(), commandlog, osstrerror());
	    exit(1);
	}
	__pmSetSignalHandler(SIGHUP, (isdaemon && !agent) ? sighupproc : SIG_IGN);
    } else {
	__pmSetSignalHandler(SIGHUP, SIG_IGN);
    }

    /*
     * -b ... force line buffering and stdout onto stderr
     */
    if ((bflag || isdaemon) && !agent)
	remap_stdout_stderr();

    if (isdaemon) {			/* daemon mode */
	/* Note: we can no longer unilaterally close stdin here, as it
	 * can really confuse remap_stdout_stderr() during log rotation!
	 */
	if (agent)
	    close(fileno(stdin));
#ifndef IS_MINGW
	if (!systemd)
	    setsid();	/* not process group leader, lose controlling tty */
#endif
	if (primary) {
	    if (systemd) {
		__pmServerNotifyServiceManagerReady(getpid());
	    }
	    __pmServerCreatePIDFile(pmGetProgname(), 0);
	}
    }

    /* default host from leftmost archive on command line, or from
       discovery after a brief connection */
    if (archives) {
	a = archives;
	while (a->next)
	    a = a->next;
        dfltHostConn = a->fname;
    } else if (!dfltConn || dfltConn == PM_CONTEXT_HOST) {
	if (dfltConn == 0)	/* default case, no -a or -h */
	    dfltHostConn = "local:";
    }

    if (!archives && !interactive) {
	if (commandlog != NULL) {
            fprintf(stderr, "pmie: PID = %" FMT_PID ", via %s",
                    (pid_t)getpid(), dfltHostConn);
	    if (primary)
		fprintf(stderr, " [primary]");
	    fprintf(stderr, "\n\n");
	}
	startmonitor();
    }

    /* initialize time */
    now = archives ? first : getReal();
    zoneInit();
    reflectTime(dfltDelta);

    /* parse time window - just to check argument syntax */
    pmtimevalFromReal(now, &tv1);
    if (archives) {
	pmtimevalFromReal(last, &tv2);
    } else {
	tv2.tv_sec = PM_MAX_TIME_T;
	tv2.tv_usec = 0;
    }
    if (pmParseTimeWindow(opts.start_optarg, opts.finish_optarg,
			  opts.align_optarg, opts.origin_optarg,
                          &tv1, &tv2,
                          &tv, &tv2, &tv1,
		          &msg) < 0) {
	fputs(msg, stderr);
	free(msg);
        exit(1);
    }
    start = pmtimevalToReal(&tv1);
    stop = pmtimevalToReal(&tv2);
    runTime = stop - start;

    /* when not in secret agent mode, register client id with pmcd */
    if (!agent)
	clientid = __pmGetClientId(argc, argv);

    if (!interactive && opts.optind == argc) {	/* stdin or config file */
	load(configfile);
    }
    else {					/* list of 1/more filenames */
	while (opts.optind < argc) {
	    load(argv[opts.optind]);
	    opts.optind++;
	}
    }

    if (pmDebugOptions.appl1)
	dumpRules();

    /* really parse time window */
    if (!archives) {
	now = getReal();
	reflectTime(dfltDelta);
    }

    if (checkFlag)
	exit(errs == 0 ? 0 : 1);	/* exit 1 for syntax errors ...
					 * suggestion from 
					 * Kevin Wang <kjw@rightsock.com>
					 */

    if (stomping)
	stompInit();			/* connect to our message server */

    if (agent)
	agentInit();			/* initialize secret agent stuff */

    pmtimevalFromReal(now, &tv1);
    if (archives) {
	pmtimevalFromReal(last, &tv2);
    } else {
	tv2.tv_sec = PM_MAX_TIME_T;
	tv2.tv_usec = 0;
    }
    if (pmParseTimeWindow(opts.start_optarg, opts.finish_optarg,
			  opts.align_optarg, opts.origin_optarg,
		          &tv1, &tv2,
                          &tv, &tv2, &tv1,
		          &msg) < 0) {
	fputs(msg, stderr);
	free(msg);
	exit(1);
    }

    /* set run timing window */
    start = pmtimevalToReal(&tv1);
    stop = pmtimevalToReal(&tv2);
    runTime = stop - start;

    if (msg != NULL)
	free(msg);
}

/***********************************************************************
 * interactive (debugging) mode
 ***********************************************************************/

static void
interact(void)
{
    int			quit = 0;
    char		*line = (char *)zalloc(LINE_LENGTH + 2);
    char		*finger;
    char		*token;
    char		*msg;
    RealTime		rt;
    struct timeval	tv1, tv2;

    printf(intro, PCP_VERSION, menu, prompt);
    fflush(stdout);
    while (!quit && readLine(line, LINE_LENGTH)) {
	finger = line;

	if ( (token = scanCmd(&finger)) ) { 
	    switch (*token) {

	    case 'f':
		token = scanArg(finger);
		load(token);
		if (pmDebugOptions.appl1)
		    dumpRules();
		break;

	    case 'l':
		token = scanArg(finger);
		list(token);
		break;

	    case 'r':
		token = scanArg(finger);
		if (token) {
		    if (pmParseInterval(token, &tv1, &msg) == 0)
			runTime = pmtimevalToReal(&tv1);
		    else {
			fputs(msg, stderr);
			free(msg);
			break;
		    }
		}
		if (!archives) {
		    invalidate();
		    rt = getReal();
		    if (now < rt)
			now = rt;
		    start = now;
		}
		stop = start + runTime;
		run();
		break;

	    case 'S':
		token = scanArg(finger);
		if (token == NULL) {
		    fprintf(stderr, "%s: error - argument required\n", pmGetProgname());
		    break;
		}
		pmtimevalFromReal(start, &tv1);
		if (archives) {
		    pmtimevalFromReal(last, &tv2);
		} else {
		    tv2.tv_sec = PM_MAX_TIME_T;
		    tv2.tv_usec = 0;
		}
		if (__pmParseTime(token, &tv1, &tv2, &tv1, &msg) < 0) {
		    fputs(msg, stderr);
		    free(msg);
		    break;
		}
		start = pmtimevalToReal(&tv1);
		if (archives)
		    invalidate();
		break;

	    case 'T':
		token = scanArg(finger);
		if (token == NULL) {
		    fprintf(stderr, "%s: error - argument required\n", pmGetProgname());
		    break;
		}
		if (pmParseInterval(token, &tv1, &msg) < 0) {
		    fputs(msg, stderr);
		    free(msg);
		    break;
		}
		runTime = pmtimevalToReal(&tv1);
		break;
	    case 'q':
		quit = 1;
		break;

	    case 'v':
		token = scanArg(finger);
		sublist(token);
		break;

	    case '?':
	    default:
		printf("%s", menu);
	    }
	}
	if (!quit) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
    }
    free(line);
}


/***********************************************************************
 * main
 ***********************************************************************/

int
main(int argc, char **argv)
{
#ifdef HAVE___EXECUTABLE_START
    extern char		__executable_start;

    /*
     * optionally set address for start of my text segment, to be used
     * in __pmDumpStack() if it is called later
     */
    __pmDumpStackInit((void *)&__executable_start);
#endif

    pmGetUsername(&username);
    setlinebuf(stdout);

    /* PCP_COUNTER_WRAP in environment enables "counter wrap" logic */
    if (getenv("PCP_COUNTER_WRAP") != NULL)
	dowrap = 1;

    getargs(argc, argv);

    if (interactive)
	interact();
    else {
	run();
	if (systemd)
	    __pmServerNotifyServiceManagerStopping(getpid());
    }
    exit(0);
}
