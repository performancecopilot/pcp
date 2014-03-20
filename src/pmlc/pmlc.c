/*
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 1995-2005 Silicon Graphics, Inc.  All Rights Reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "pmapi.h"
#include "impl.h"
#include "pmlc.h"
#include "gram.h"

char		*configfile;
__pmLogCtl	logctl;
int		parse_done;
int		pid = PM_LOG_NO_PID;
int		port = PM_LOG_NO_PORT;
int		is_unix = 0;	/* host spec is a unix: url. */
int		is_local = 0;	/* host spec is a local: url. */
int		is_socket_path = 0; /* host spec is a url with a path. */
int		zflag;		/* for -z */
char 		*tz;		/* for -Z timezone */
int		tztype = TZ_LOCAL;	/* timezone for status cmd */

int		eflag;
int		iflag;

extern int	parse_stmt;
extern int	tzchange;

static char title[] = "Performance Co-Pilot Logger Control (pmlc), Version %s\n\n%s\n";
static char menu[] =
"pmlc commands\n\n"
"  show loggers [@<host>]             display <pid>s of running pmloggers\n"
"  connect _logger_id [@<host>]       connect to designated pmlogger\n"
"  status                             information about connected pmlogger\n"
"  query metric-list                  show logging state of metrics\n"
"  new volume                         start a new log volume\n"
"  flush                              flush the log buffers to disk\n"
"\n"
"  log { mandatory | advisory } on <interval> _metric-list\n"
"  log { mandatory | advisory } off _metric-list\n"
"  log mandatory maybe _metric-list\n"
"\n"
"  timezone local|logger|'<timezone>' change reporting timezone\n"
"  help                               print this help message\n"
"  quit                               exit from pmlc\n"
"\n"
"  _logger_id   is  primary | <pid> | port <n>\n"
"  _metric-list is  _metric-spec | { _metric-spec ... }\n"
"  _metric-spec is  <metric-name> | <metric-name> [ <instance> ... ]\n";


int
main(int argc, char **argv)
{
    int			c;
    int			sts = 0;	/* initialize to pander to gcc */
    int			errflag = 0;
    char		*host = NULL;
    char		*pmnsfile = PM_NS_DEFAULT;
    char		*endnum;
    int			primary;
    char		*prefix_end;
    size_t		prefix_len;

    __pmSetProgname(argv[0]);

    iflag = isatty(0);

    while ((c = getopt(argc, argv, "D:eh:in:Pp:zZ:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'e':		/* echo input */
	    eflag++;
	    break;

	case 'h':		/* hostspec */
	    /*
	     * We need to know if a socket path has been specified.
	     */
	    host = optarg;
	    prefix_end = strchr(host, ':');
	    if (prefix_end != NULL) {
		prefix_len = prefix_end - host + 1;
		if (prefix_len == 6 && strncmp(host, "local:", prefix_len) == 0)
		    is_local = 1;
		else if (prefix_len == 5 && strncmp(host, "unix:", prefix_len) == 0)
		    is_unix = 1;
		if (is_local || is_unix) {
		    const char *p;
		    /*
		     * Find out is a path was specified.
		     * Skip any initial path separators.
		     */
		    for (p = host + prefix_len; *p == __pmPathSeparator(); ++p)
			;
		    if (*p != '\0')
			is_socket_path = 1;
		}
	    }
	    break;

	case 'i':		/* be interactive */
	    iflag++;
	    break;

	case 'n':		/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 'P':		/* connect to primary logger */
	    if (port != PM_LOG_NO_PORT || (is_unix && is_socket_path)) {
		fprintf(stderr, "%s: at most one of -P and/or -p and/or a unix socket and/or a pid may be specified\n", pmProgname);
		errflag++;
	    }
	    else {
		port = PM_LOG_PRIMARY_PORT;
	    }
	    break;

	case 'p':		/* connect via port */
	    if (port != PM_LOG_NO_PORT || is_unix) {
		fprintf(stderr, "%s: at most one of -P and/or -p and/or a unix socket url and/or a pid may be specified\n", pmProgname);
		errflag++;
	    }
	    else {
		port = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0' || port <= PM_LOG_PRIMARY_PORT) {
		    fprintf(stderr, "%s: port must be numeric and greater than %d\n", pmProgname, PM_LOG_PRIMARY_PORT);
		    errflag++;
		}
	    }
	    break;

	case 'z':	/* timezone from host */
	    if (tz != NULL) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    zflag++;
	    tztype = TZ_LOGGER;
	    tzchange = 1;
	    break;

	case 'Z':	/* $TZ timezone */
	    if (zflag) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    if ((tz = strdup(optarg)) == NULL) {
		__pmNoMem("initialising timezone", strlen(optarg), PM_FATAL_ERR);
	    }
	    tztype  = TZ_OTHER;
	    tzchange = 1;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (optind < argc-1)
	errflag++;
    else if (optind == argc-1) {
	/* pid was specified */
	if (port != PM_LOG_NO_PORT || (is_unix && is_socket_path)) {
	    fprintf(stderr, "%s: at most one of -P and/or -p and/or a unix socket path and/or a pid may be specified\n", pmProgname);
	    errflag++;
	}
	else {
	    pid = (int)strtol(argv[optind], &endnum, 10);
	    if (*endnum != '\0' || pid <= PM_LOG_PRIMARY_PID) {
		fprintf(stderr, "%s: pid must be a numeric process id and greater than %d\n", pmProgname, PM_LOG_PRIMARY_PID);
		errflag++;
	    }
	}
    }

    if (errflag == 0 && host != NULL && pid == PM_LOG_NO_PID &&
	port == PM_LOG_NO_PORT && ! is_socket_path) {
	fprintf(stderr, "%s: -h may not be used without -P or -p or a socket path or a pid\n", pmProgname);
	errflag++;
    }

    if (errflag) {
	fprintf(stderr,
		"Usage: %s [options] [pid]\n\n"
		"Options:\n"
		"  -e           echo input\n"
		"  -h hostspec  connect to pmlogger using hostspec\n"
		"  -i           be interactive and prompt\n"
		"  -n pmnsfile  use an alternative PMNS\n"
		"  -P           connect to primary pmlogger\n"
		"  -p port      connect to pmlogger on this TCP/IP port\n"
		"  -Z timezone  set reporting timezone\n"
		"  -z           set reporting timezone to local time for pmlogger\n",
		pmProgname);
	exit(1);
    }

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((sts = pmLoadNameSpace(pmnsfile)) < 0) {
	    fprintf(stderr, "%s: Cannot load namespace from \"%s\": %s\n",
		    pmProgname, pmnsfile, pmErrStr(sts));
	    exit(1);
	}
    }

    if (host == NULL)
	host = "local:";

    primary = 0;
    if (port == PM_LOG_PRIMARY_PORT || pid == PM_LOG_PRIMARY_PID)
	primary = 1;

    if (pid != PM_LOG_NO_PID || port != PM_LOG_NO_PORT || is_socket_path)
	sts = ConnectLogger(host, &pid, &port);

    if (iflag)
	printf(title, PCP_VERSION, menu);

    if (pid != PM_LOG_NO_PID || port != PM_LOG_NO_PORT || is_socket_path) {
	if (sts < 0) {
	    if (primary) {
		fprintf(stderr, "Unable to connect to primary pmlogger at %s: ",
			host);
		if (still_connected(sts))
		    fprintf(stderr, "%s\n", pmErrStr(sts));
	    }
	    else if (is_socket_path) {
		fprintf(stderr, "Unable to connect to pmlogger via the local socket at %s: ",
			host);
		if (still_connected(sts))
		    fprintf(stderr, "%s\n", pmErrStr(sts));
	    }
	    else if (port != PM_LOG_NO_PORT) {
		fprintf(stderr, "Unable to connect to pmlogger on port %d at %s: ",
			port, host);
		if (still_connected(sts))
		    fprintf(stderr, "%s\n", pmErrStr(sts));
	    }
	    else {
		fprintf(stderr, "Unable to connect to pmlogger pid %d at %s: ",
			pid, host);
		if (still_connected(sts))
		    fprintf(stderr, "%s\n", pmErrStr(sts));
	    }
	}
	else {
	    if (primary)
		printf("Connected to primary pmlogger at %s\n", host);
	    else if (is_socket_path)
		printf("Connected to pmlogger via local socket at %s\n", host);
	    else if (port != PM_LOG_NO_PORT)
		printf("Connected to pmlogger on port %d at %s\n", port, host);
	    else
		printf("Connected to pmlogger pid %d at %s\n", pid, host);
	}
    }

    for ( ; ; ) {
	char		*realhost;
	extern int	logfreq;
	extern void	ShowLoggers(char *);
	extern void	Query(void);
	extern void	LogCtl(int, int, int);
	extern void	Status(int, int);
	extern void	NewVolume(void);
	extern void	Sync(void);
	extern void	Qa(void);
	extern int	yywrap(void);

	is_local = 0;
	is_unix = 0;
	is_socket_path = 0;
	parse_stmt = -1;
	metric_cnt = 0;
	yyparse();
	if (yywrap()) {
	    if (iflag)
		putchar('\n');
	    break;
	}
	if (metric_cnt < 0)
	    continue;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    printf("stmt=%d, state=%d, control=%d, hostspec=%s, pid=%d, port=%d\n",
		   parse_stmt, state, control, hostname, pid, port);
#endif

	realhost = (hostname == NULL) ? host : hostname;
	switch (parse_stmt) {

	    case SHOW:
		ShowLoggers(realhost);
		break;

	    case CONNECT:
		/* The unix: url requres either 'primary', a pid or a socket path. */
		if (is_unix && pid == PM_LOG_NO_PID && ! is_socket_path) {
		    fprintf(stderr, "The 'unix:' url requires either 'primary', a pid or a socket path");
		    if (still_connected(sts))
			fprintf(stderr, "\n");
		    break;
		}
		/* The local: url requres either 'primary', a pid a port or a socket path. */
		if (is_local && pid == PM_LOG_NO_PID && port == PM_LOG_NO_PORT && ! is_socket_path) {
		    fprintf(stderr, "The 'local:' url requires either 'primary', a pid, a port or a socket path");
		    if (still_connected(sts))
			fprintf(stderr, "\n");
		    break;
		}
		primary = 0;
		if (port == PM_LOG_PRIMARY_PORT || pid == PM_LOG_PRIMARY_PID)
		    primary = 1;
		if ((sts = ConnectLogger(realhost, &pid, &port)) < 0) {
		    if (primary) {
			fprintf(stderr, "Unable to connect to primary pmlogger at %s: ",
				realhost);
		    }
		    else if (is_socket_path) {
			fprintf(stderr, "Unable to connect to pmlogger via local socket at %s: ",
				realhost);
		    }
		    else if (port != PM_LOG_NO_PORT) {
			fprintf(stderr, "Unable to connect to pmlogger on port %d at %s: ",
				port, realhost);
		    }
		    else {
			fprintf(stderr, "Unable to connect to pmlogger pid %d at %s: ",
				pid, realhost);
		    }
		    if (still_connected(sts))
			fprintf(stderr, "%s\n", pmErrStr(sts));
		}
		else
		    /* if the timezone is "logger time", it has changed
		     * because the logger may be in a different zone
		     * (note that tzchange may already be set (e.g. -Z and
		     * this connect is the first).
		     */
		    tzchange |= (tztype == TZ_LOGGER);
		break;		

	    case HELP:
		puts(menu);
		break;

	    case LOG:
		if (state == PM_LOG_ENQUIRE) {
		    Query();
		    break;
		}
		if (logfreq == -1)
		    logfreq = 0;
		if (state == PM_LOG_ON) {
		    if (logfreq < 0) {
fprintf(stderr, "Logging delta (%d msec) must be positive\n", logfreq);
			break;
		    }
		    else if (logfreq > PMLC_MAX_DELTA) {
fprintf(stderr, "Logging delta (%d msec) cannot be bigger than %d msec\n", logfreq, PMLC_MAX_DELTA);
			break;
		    }
		}
		LogCtl(control, state, logfreq);
		break;

	    case QUIT:
		printf("Goodbye\n");
		DisconnectLogger();
		exit(0);
		break;

	    case STATUS:
		Status(pid, primary);
		break;

	    case NEW:
		NewVolume();
		break;

	    case TIMEZONE:
		tzchange = 1;
		break;

	    case SYNC:
		Sync();
		break;

	    case QA:
		if (qa_case == 0)
		    fprintf(stderr, "QA Test Case deactivated\n");
		else
		    fprintf(stderr, "QA Test Case #%d activated\n", qa_case);
		Qa();
	}

	if (hostname != NULL) {
	    free(hostname);
	    hostname = NULL;
	}

    }

    DisconnectLogger();
    exit(0);
}
