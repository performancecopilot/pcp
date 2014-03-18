/***********************************************************************
 * pmie.c - performance inference engine
 ***********************************************************************
 *
 * Copyright (c) 2013 Red Hat, Inc.
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
#include "pmapi.h"
#include "impl.h"
#include <sys/stat.h>
#include <assert.h>

#include "dstruct.h"
#include "stomp.h"
#include "syntax.h"
#include "pragmatics.h"
#include "eval.h"
#include "show.h"

#if HAVE_TRACE_BACK_STACK
#define MAX_PCS 30	/* max callback procedure depth */
#define MAX_SIZE 48	/* max function name length     */
#include <libexc.h>
#endif


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
static char logfile[MAXPATHLEN+1];
static char perffile[MAXPATHLEN+1];	/* /var/tmp/<pid> file name */
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

static char usage[] =
    "Usage: %s [options] [filename ...]\n\n"
    "Options:\n"
    "  -A align     align sample times on natural boundaries\n"
    "  -a archive   metrics source is a PCP log archive\n"
    "  -b           one line buffered output stream, stdout on stderr\n"
    "  -C           parse configuration and exit\n"
    "  -c filename  configuration file\n"
    "  -d           interactive debugging mode\n"
    "  -e           force timestamps to be reported when used with -V, -v or -W\n"
    "  -f           run in foreground\n"
    "  -H           do not do a name lookup on the default hostname\n"
    "  -h host      metrics source is PMCD on host\n"
    "  -j stompfile stomp protocol (JMS) file [default %s%cconfig%cpmie%cstomp]\n"
    "  -l logfile   send status and error messages to logfile\n"
    "  -n pmnsfile  use an alternative PMNS\n"
    "  -O offset    initial offset into the time window\n"
    "  -S starttime start of the time window\n"
    "  -T endtime   end of the time window\n"
    "  -t interval  sample interval [default 10 seconds]\n"
    "  -U username  in daemon mode, run as named user [default pcp]\n"
    "  -V           verbose mode, annotated expression values printed\n"
    "  -v           verbose mode, expression values printed\n"
    "  -W           verbose mode, satisfying expression values printed\n"
    "  -x           run in domain agent mode (summary PMDA)\n"
    "  -Z timezone  set reporting timezone\n"
    "  -z           set reporting timezone to local time of metrics source\n";

/***********************************************************************
 * usage message
 ***********************************************************************/

static void
usageMessage(void)
{
    int sep = __pmPathSeparator();
    fprintf(stderr, usage, pmProgname, pmGetConfig("PCP_VAR_DIR"), sep,sep,sep);
    exit(1);
}


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
    int		sep = __pmPathSeparator();
    char	config[MAXPATHLEN+1];

    /* search for configfile on configuration file path */
    if (fname && access(fname, F_OK) != 0) {
	sts = oserror();	/* always report the first error */
	if (__pmAbsolutePath(fname)) {
	    fprintf(stderr, "%s: cannot access config file %s: %s\n", pmProgname, 
		    fname, strerror(sts));
	    exit(1);
	}
#if PCP_DEBUG
	else if (pmDebug & DBG_TRACE_APPL0) {
	    fprintf(stderr, "load: cannot access config file %s: %s\n", fname, strerror(sts));
	}
#endif
	snprintf(config, sizeof(config)-1, "%s%c" "pmie" "%c%s",
		pmGetConfig("PCP_SYSCONF_DIR"), sep, sep, fname);
	if (access(config, F_OK) != 0) {
	    fprintf(stderr, "%s: cannot access config file as either %s or %s: %s\n",
		    pmProgname, fname, config, strerror(sts));
	    exit(1);
	}
#if PCP_DEBUG
	else if (pmDebug & DBG_TRACE_APPL0) {
	    fprintf(stderr, "load: using standard config file %s\n", config);
	}
#endif
	fname = config;
    }
#if PCP_DEBUG
    else if (pmDebug & DBG_TRACE_APPL0) {
	fprintf(stderr, "load: using config file %s\n",
		fname == NULL? "<stdin>":fname);
    }
#endif

    if (perf->config[0] == '\0') {	/* keep record of first config */
	if (fname == NULL)
	    strcpy(perf->config, "<stdin>");
	else if (realpath(fname, perf->config) == NULL) {
	    fprintf(stderr, "%s: failed to resolve realpath for %s: %s\n",
		    pmProgname, fname, osstrerror());
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
	    printf("%s: error - rule \"%s\" not defined\n", pmProgname, name);
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
	    printf("%s: error - rule '%s' not defined\n", pmProgname, name);
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
    snprintf(pmie_dir, sizeof(pmie_dir), "%s%c%s",
	     pmGetConfig("PCP_TMP_DIR"), __pmPathSeparator(), PMIE_SUBDIR);
    if (mkdir2(pmie_dir, S_IRWXU | S_IRWXG | S_IRWXO) < 0) {
	if (oserror() != EEXIST) {
	    fprintf(stderr, "%s: warning cannot create stats file dir %s: %s\n",
		    pmProgname, pmie_dir, osstrerror());
	}
    }
    atexit(stopmonitor);

    /* create and initialize memory mapped performance data file */
    sprintf(perffile, "%s%c%" FMT_PID, pmie_dir, __pmPathSeparator(), getpid());
    unlink(perffile);
    if ((fd = open(perffile, O_RDWR | O_CREAT | O_EXCL | O_TRUNC,
			     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
	/* cannot create stats file; too bad, so sad, continue on without it */
	perf = &instrument;
	return;
    }
    /* seek to struct size and write one zero */
    lseek(fd, sizeof(pmiestats_t)-1, SEEK_SET);
    if (write(fd, &zero, 1) != 1) {
	fprintf(stderr, "%s: Warning: write failed for stats file %s: %s\n",
		pmProgname, perffile, osstrerror());
    }

    /* map perffile & associate the instrumentation struct with it */
    if ((ptr = __pmMemoryMap(fd, sizeof(pmiestats_t), 1)) == NULL) {
	fprintf(stderr, "%s: memory map failed for stats file %s: %s\n",
		pmProgname, perffile, osstrerror());
	perf = &instrument;
    } else {
	perf = (pmiestats_t *)ptr;
    }
    close(fd);

    path = (logfile[0] == '\0') ? "<none>" : logfile;
    strncpy(perf->logfile, path, sizeof(perf->logfile));
    perf->logfile[sizeof(perf->logfile)-1] = '\0';
    strncpy(perf->defaultfqdn, dfltHostName, sizeof(perf->defaultfqdn));
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
    __pmNotifyErr(LOG_INFO, "%s caught SIGINT or SIGTERM\n", pmProgname);
    exit(1);
}

static void
sigbye(int sig)
{
    exit(0);
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
    if ((j = dup(fileno(stderr))) != i)
	fprintf(stderr, "%s: Warning: failed to link stdout ... "
			"dup() returns %d, expected %d (stderr=%d)\n",
			pmProgname, j, i, fileno(stderr));
}

static void
sighupproc(int sig)
{
    FILE *fp;
    int sts;

    fp = __pmRotateLog(pmProgname, logfile, logfp, &sts);
    if (sts != 0) {
	fprintf(stderr, "pmie: PID = %" FMT_PID ", default host = %s via %s\n\n",
                getpid(), dfltHostName, dfltHostConn);
	remap_stdout_stderr();
	logfp = fp;
    } else {
	__pmNotifyErr(LOG_ERR, "pmie: log rotation failed\n");
    }
}


static void
dotraceback(void)
{
#if HAVE_TRACE_BACK_STACK
    __uint64_t	call_addr[MAX_PCS];
    char	*call_fn[MAX_PCS];
    char	names[MAX_PCS][MAX_SIZE];
    int		res;
    int		i;

    fprintf(stderr, "\nProcedure call traceback ...\n");
    for (i = 0; i < MAX_PCS; i++)
        call_fn[i] = names[i];
    res = trace_back_stack(MAX_PCS, call_addr, call_fn, MAX_PCS, MAX_SIZE);
    for (i = 1; i < res; i++)
    fprintf(stderr, "  " PRINTF_P_PFX "%p [%s]\n", (void *)call_addr[i], call_fn[i]);
#endif
    return;
}

static void
sigbadproc(int sig)
{
    __pmNotifyErr(LOG_ERR, "Unexpected signal %d ...\n", sig);
    dotraceback();
    fprintf(stderr, "\nDumping to core ...\n");
    fflush(stderr);
    stopmonitor();
    abort();
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
    char		*msg;
    int			checkFlag = 0;
    int			foreground = 0;
    int			err = 0;
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

    while ((c=getopt(argc, argv, "a:A:bc:CdD:efHh:j:l:n:O:S:t:T:U:vVWXxzZ:?")) != EOF) {
        switch (c) {

	case 'a':			/* archives */
	    if (dfltConn && dfltConn != PM_CONTEXT_ARCHIVE) {
                /* (technically, multiple -a's are allowed.) */
		fprintf(stderr, "%s: at most one of -a or -h allowed\n",
			pmProgname);
		err++;
		break;
	    }
	    dfltConn = PM_CONTEXT_ARCHIVE;
	    subopts = optarg;
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

	case 'A': 			/* sample alignment */
	    alignFlag = optarg;
	    break;

	case 'b':			/* line buffered, stdout on stderr */
	    bflag++;
	    break;

	case 'c': 			/* configuration file */
	    if (interactive) {
		fprintf(stderr, "%s: at most one of -c and -d allowed\n",
			pmProgname);
		err++;
		break;
	    }
	    configfile = optarg;
	    break;

	case 'C': 			/* check config and exit */
	    checkFlag = 1;
	    break;

	case 'd': 			/* interactive mode */
	    if (configfile) {
		fprintf(stderr, "%s: at most one of -c and -d allowed\n",
			pmProgname);
		err++;
		break;
	    }
	    interactive = 1;
	    break;

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification "
			"(%s)\n", pmProgname, optarg);
		err++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'e':	/* force timestamps */
	    showTimeFlag = 1;
	    break;

	case 'f':			/* in foreground, not as daemon */
	    foreground = 1;
	    break;

	case 'H': 			/* no name lookup on exported host */
	    noDnsFlag = 1;
	    break;

	case 'h': 			/* default host name */
	    if (dfltConn) {
		fprintf(stderr, "%s: at most one of -a or -h allowed\n",
			pmProgname);
		err++;
		break;
	    }
	    dfltConn = PM_CONTEXT_HOST;
	    dfltHostConn = optarg;
            dfltHostName = ""; /* unknown until newContext */
	    break;

        case 'j':			/* stomp protocol (JMS) config */
	    stompfile = optarg;
	    break;

	case 'l':			/* alternate log file */
	    if (commandlog != NULL) {
		fprintf(stderr, "%s: at most one -l option is allowed\n",
			pmProgname);
		err++;
		break;
	    }
	    commandlog = optarg;
	    isdaemon = 1;
	    break;

        case 'n':			/* alternate namespace file */
	    pmnsfile = optarg;
	    break;

	case 'O':			/* position within time window */
	    offsetFlag = optarg;
	    break;

	case 'S':			/* start run time */
	    startFlag = optarg;
	    break;

	case 't':			/* sample interval */
	    if (pmParseInterval(optarg, &tv1, &msg) == 0)
		dfltDelta = realize(tv1);
	    else {
		fprintf(stderr, "%s: could not parse -t argument (%s)\n", pmProgname, optarg);
		fputs(msg, stderr);
		free(msg);
		err++;
	    }
	    break;

	case 'T':			/* evaluation period */
	    stopFlag = optarg;
	    break;

	case 'U': 			/* run as named user */
	    username = optarg;
	    isdaemon = 1;
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

	case 'z':			/* timezone from host */
	    hostZone = 1;
	    if (timeZone) {
		fprintf(stderr, "%s: only one of -Z and -z allowed\n",
			pmProgname);
		err++;
	    }
	    break;

	case 'Z':			/* explicit TZ string */
	    timeZone = optarg;
	    if (hostZone) {
		fprintf(stderr, "%s: only one of -Z and -z allowed\n",
			pmProgname);
		err++;
	    }
	    break;

	case '?':
            err++;
	}
    }

    if (configfile && optind != argc) {
	fprintf(stderr, "%s: extra filenames cannot be given after using -c\n",
		pmProgname);
	err++;
    }
    if (bflag && agent) {
	fprintf(stderr, "%s: the -b and -x options are incompatible\n",
		pmProgname);
	err++;
    }
    if (err)
    	usageMessage();

    if (foreground)
	isdaemon = 0;

    if (archives || interactive)
	perf = &instrument;

    if (isdaemon) {			/* daemon mode */
	/* done before opening log to get permissions right */
	__pmSetProcessIdentity(username);

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
	__pmSetSignalHandler(SIGINT, sigbye);
	__pmSetSignalHandler(SIGTERM, sigbye);
    }

    if (commandlog != NULL) {
	logfp = __pmOpenLog(pmProgname, commandlog, stderr, &sts);
	if (realpath(commandlog, logfile) == NULL) {
	    fprintf(stderr, "%s: cannot find realpath for log %s: %s\n",
		    pmProgname, commandlog, osstrerror());
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

    /* default host from leftmost archive on command line, or from
       discovery after a brief connection */
    if (archives) {
	a = archives;
	while (a->next)
	    a = a->next;
	dfltHostName = a->hname; /* already filled in during initArchive() */
    } else if (!dfltConn || dfltConn == PM_CONTEXT_HOST) {
	if (dfltConn == 0)	/* default case, no -a or -h */
	    dfltHostConn = "local:";
	sts = pmNewContext(PM_CONTEXT_HOST, dfltHostConn);
	/* pmcd down locally, try to extract hostname manually */
	if (sts < 0 && (!dfltConn ||
			!strcmp(dfltHostConn, "localhost") ||
			!strcmp(dfltHostConn, "local:") ||
			!strcmp(dfltHostConn, "unix:")))
	    sts = pmNewContext(PM_CONTEXT_LOCAL, NULL);
	if (sts < 0) {
	    __pmNotifyErr(LOG_ERR, "%s: cannot find host name for %s\n"
			"pmNewContext failed: %s\n",
			pmProgname, dfltHostConn, pmErrStr(sts));
	    dfltHostName = "?";
	} else {
	    const char	*tmp;
	    tmp = pmGetContextHostName(sts);
	    if (strlen(tmp) == 0) {
		fprintf(stderr, "%s: pmGetContextHostName(%d) failed\n",
		    pmProgname, sts);
		exit(EXIT_FAILURE);
	    }
	    if ((dfltHostName = strdup(tmp)) == NULL)
		__pmNoMem("host name copy", strlen(tmp)+1, PM_FATAL_ERR);
	    pmDestroyContext(sts);
        }
    }
    assert (dfltHostName != NULL);

    if (!archives && !interactive) {
	if (commandlog != NULL)
            fprintf(stderr, "pmie: PID = %" FMT_PID ", default host = %s via %s\n\n",
                    getpid(), dfltHostName, dfltHostConn);
	startmonitor();
    }

    /* initialize time */
    now = archives ? first : getReal() + 1.0;
    zoneInit();
    reflectTime(dfltDelta);

    /* parse time window - just to check argument syntax */
    unrealize(now, &tv1);
    if (archives) {
	unrealize(last, &tv2);
    } else {
	tv2.tv_sec = INT_MAX;		/* sizeof(time_t) == sizeof(int) */
	tv2.tv_usec = 0;
    }
    if (pmParseTimeWindow(startFlag, stopFlag, alignFlag, offsetFlag,
                          &tv1, &tv2,
                          &tv, &tv2, &tv1,
		          &msg) < 0) {
	fputs(msg, stderr);
        exit(1);
    }
    start = realize(tv1);
    stop = realize(tv2);
    runTime = stop - start;

    /* initialize PMAPI */
    if (pmnsfile != PM_NS_DEFAULT && (sts = pmLoadNameSpace(pmnsfile)) < 0) {
	fprintf(stderr, "%s: pmLoadNameSpace failed: %s\n", pmProgname,
		pmErrStr(sts));
	exit(1);
    }

    /* when not in secret agent mode, register client id with pmcd */
    if (!agent)
	clientid = __pmGetClientId(argc, argv);

    if (!interactive && optind == argc) {	/* stdin or config file */
	load(configfile);
    }
    else {					/* list of 1/more filenames */
	while (optind < argc) {
	    load(argv[optind]);
	    optind++;
	}
    }

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	dumpRules();
#endif

    if (checkFlag)
	exit(errs == 0 ? 0 : 1);	/* exit 1 for syntax errors ...
					 * suggestion from 
					 * Kevin Wang <kjw@rightsock.com>
					 */

    if (isdaemon) {			/* daemon mode */
	/* Note: we can no longer unilaterally close stdin here, as it
	 * can really confuse remap_stdout_stderr() during log rotation!
	 */
	if (agent)
	    close(fileno(stdin));
#ifndef IS_MINGW
	setsid();	/* not process group leader, lose controlling tty */
#endif
    }

    if (stomping)
	stompInit();			/* connect to our message server */

    if (agent)
	agentInit();			/* initialize secret agent stuff */

    /* really parse time window */
    if (!archives) {
	now = getReal() + 1.0;
	reflectTime(dfltDelta);
    }
    unrealize(now, &tv1);
    if (archives) {
	unrealize(last, &tv2);
    } else {
	tv2.tv_sec = INT_MAX;
	tv2.tv_usec = 0;
    }
    if (pmParseTimeWindow(startFlag, stopFlag, alignFlag, offsetFlag,
		          &tv1, &tv2,
                          &tv, &tv2, &tv1,
		          &msg) < 0) {
	fputs(msg, stderr);
	exit(1);
    }

    /* set run timing window */
    start = realize(tv1);
    stop = realize(tv2);
    runTime = stop - start;
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
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL1)
		    dumpRules();
#endif
		break;

	    case 'l':
		token = scanArg(finger);
		list(token);
		break;

	    case 'r':
		token = scanArg(finger);
		if (token) {
		    if (pmParseInterval(token, &tv1, &msg) == 0)
			runTime = realize(tv1);
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
		    fprintf(stderr, "%s: error - argument required\n", pmProgname);
		    break;
		}
		unrealize(start, &tv1);
		if (archives) {
		    unrealize(last, &tv2);
		} else {
		    tv2.tv_sec = INT_MAX;
		    tv2.tv_usec = 0;
		}
		if (__pmParseTime(token, &tv1, &tv2, &tv1, &msg) < 0) {
		    fputs(msg, stderr);
		    free(msg);
		    break;
		}
		start = realize(tv1);
		if (archives)
		    invalidate();
		break;

	    case 'T':
		token = scanArg(finger);
		if (token == NULL) {
		    fprintf(stderr, "%s: error - argument required\n", pmProgname);
		    break;
		}
		if (pmParseInterval(token, &tv1, &msg) < 0) {
		    fputs(msg, stderr);
		    free(msg);
		    break;
		}
		runTime = realize(tv1);
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
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);
    setlinebuf(stdout);

    /* PCP_COUNTER_WRAP in environment enables "counter wrap" logic */
    if (getenv("PCP_COUNTER_WRAP") != NULL)
	dowrap = 1;

    getargs(argc, argv);

    if (interactive)
	interact();
    else
	run();
    exit(0);
}
