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
int		is_unix;	/* host spec is a unix: url. */
int		is_local;	/* host spec is a local: url. */
int		is_socket_path; /* host spec is a url with a path. */
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

static int overrides(int, pmOptions *);

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "echo", 0, 'e', 0, "echo input" },
    { "host", 1, 'h', "HOST", "connect to pmlogger using host specification" },
    { "interactive", 0, 'i', 0, "be interactive and prompt" },
    PMOPT_NAMESPACE,
    { "primary", 0, 'P', 0, "connect to primary pmlogger" },
    { "port", 1, 'p', "N", "connect to pmlogger on this TCP/IP port" },
    PMOPT_TIMEZONE,
    { "logzone", 0, 'z', 0, "set reporting timezone to local time for pmlogger" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:eh:in:Pp:zZ:?",
    .long_options = longopts,
    .short_usage = "[options] [pid]",
    .override = overrides,
};

static int
overrides(int opt, pmOptions *opts)
{
    if (opt == 'h' || opt == 'p')
	return 1;
    return 0;
}

int
main(int argc, char **argv)
{
    int			c;
    int			sts = 0;	/* initialize to pander to gcc */
    char		*host = NULL;
    char		*endnum;
    int			primary;
    size_t		prefix_len;
    char		*prefix_end;

    iflag = isatty(0);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'e':		/* echo input */
	    eflag++;
	    break;

	case 'h':		/* hostspec */
	    /*
	     * We need to know if a socket path has been specified.
	     */
	    host = opts.optarg;
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

	case 'P':		/* connect to primary logger */
	    if (port != PM_LOG_NO_PORT || (is_unix && is_socket_path)) {
		pmprintf("%s: at most one of -P, -p, unix socket, or PID may be specified\n",
			pmProgname);
		opts.errors++;
	    } else {
		port = PM_LOG_PRIMARY_PORT;
	    }
	    break;

	case 'p':		/* connect via port */
	    if (port != PM_LOG_NO_PORT || is_unix) {
		pmprintf("%s: at most one of -P, -p, unix socket, or PID may be specified\n",
			pmProgname);
		opts.errors++;
	    } else {
		port = (int)strtol(opts.optarg, &endnum, 10);
		if (*endnum != '\0' || port <= PM_LOG_PRIMARY_PORT) {
		    pmprintf("%s: port must be numeric and greater than %d\n",
			    pmProgname, PM_LOG_PRIMARY_PORT);
		    opts.errors++;
		}
	    }
	    break;
	}
    }

    if (opts.optind < argc - 1)
	opts.errors++;
    else if (opts.optind == argc - 1) {
	/* pid was specified */
	if (port != PM_LOG_NO_PORT || (is_unix && is_socket_path)) {
	    pmprintf("%s: at most one of -P, -p, unix socket, or PID may be specified\n",
		    pmProgname);
	    opts.errors++;
	}
	else {
	    pid = (int)strtol(argv[opts.optind], &endnum, 10);
	    if (*endnum != '\0' || pid <= PM_LOG_PRIMARY_PID) {
		pmprintf("%s: PID must be a numeric process ID and greater than %d\n",
			pmProgname, PM_LOG_PRIMARY_PID);
		opts.errors++;
	    }
	}
    }

    if (!opts.errors && host && pid == PM_LOG_NO_PID &&
	port == PM_LOG_NO_PORT && !is_socket_path) {
	pmprintf("%s: -h may not be used without -P or -p or a socket path or a PID\n",
		pmProgname);
	opts.errors++;
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if (opts.tzflag) {
	tztype = TZ_LOGGER;
	tzchange = 1;
    } else if (opts.timezone) {
	tz = opts.timezone;
	tztype = TZ_OTHER;
	tzchange = 1;
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
	if (pmDebugOptions.appl1)
	    printf("stmt=%d, state=%d, control=%d, hostspec=%s, pid=%d, port=%d\n",
		   parse_stmt, state, control, hostname, pid, port);

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
