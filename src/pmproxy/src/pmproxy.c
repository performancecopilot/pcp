/*
 * Copyright (c) 2012-2021 Red Hat.
 * Copyright (c) 2002 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include "pmapi.h"
#include "libpcp.h"
#include "pmproxy.h"
#include "pmwebapi.h"
#include <strings.h>
#include <sys/resource.h>
#include <sys/stat.h>

#define MAXPENDING	128	/* maximum number of pending connections */
#define STRINGIFY(s)    #s
#define TO_STRING(s)    STRINGIFY(s)

#define RUN_DAEMON	1		/* default */
#define RUN_FOREGROUND	2		/* -f */
#define RUN_SYSTEMD	3		/* -F */

static void	*info;			/* opaque server information */
static struct pmproxy	*server;	/* proxy server implementation */
struct dict	*config;		/* configuration file settings */

static char	*logfile = "pmproxy.log";	/* log file name */
static int	run_mode = RUN_DAEMON;	/* style of execution, see -f and -F */
static char	*fatalfile = "/dev/tty";/* fatal messages at startup go here */
static char	*username;
static char	*certdb;		/* certificate DB path (NSS) */
static char	*dbpassfile;		/* certificate DB password file */
static char     *cert_nickname;         /* alternate nickname for server certificate */
static char	sockpath[MAXPATHLEN];	/* local unix domain socket path */

static void
DontStart(void)
{
    FILE	*tty;
    FILE	*log;

    pmNotifyErr(LOG_ERR, "pmproxy not started due to errors!\n");

    if ((tty = fopen(fatalfile, "w")) != NULL) {
	fflush(stderr);
	fprintf(tty, "NOTE: pmproxy not started due to errors!  ");
	if ((log = fopen(logfile, "r")) != NULL) {
	    int		c;
	    fprintf(tty, "Log file \"%s\" contains ...\n", logfile);
	    while ((c = fgetc(log)) != EOF)
		fputc(c, tty);
	    fclose(log);
	}
	else
	    fprintf(tty, "Log file \"%s\" has vanished!\n", logfile);
	fclose(tty);
    }
    exit(1);
}

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_DEBUG,
    PMOPT_HELP,
    PMAPI_OPTIONS_HEADER("Service options"),
    { "", 0, 'A', 0, "disable service advertisement" },
    { "deprecated", 0, 'd', 0, "backward-compatibility mode; no REST APIs" },
    { "foreground", 0, 'f', 0, "run in the foreground" },
    { "systemd", 0, 'F', 0, "run in systemd mode" },
    { "timeseries", 0, 't', 0, "automatic, scalable timeseries; REST APIs" },
    { "username", 1, 'U', "USER", "in daemon mode, run as named user [default pcp]" },
    PMAPI_OPTIONS_HEADER("Configuration options"),
    { "config", 1, 'c', "PATH", "path to configuration file (implies --timeseries)"},
    { "certdb", 1, 'C', "PATH", "path to NSS certificate database (implies --deprecated)" },
    { "passfile", 1, 'P', "PATH", "password file for certificate database access (implies --deprecated)" },
    { "certname", 1, 'M', "NAME", "certificate name to use (implies --deprecated)" },
    { "", 0, 'L', 0, "maximum size for PDUs from clients [default 65536]" },
    PMAPI_OPTIONS_HEADER("Connection options"),
    { "interface", 1, 'i', "ADDR", "accept connections on this IP address" },
    { "port", 1, 'p', "PORT", "accept connections on this port" },
    { "socket", 1, 's', "PATH", "Unix domain socket file [default $PCP_RUN_DIR/pmproxy.socket]" },
    { "redisport", 1, 'r', "PORT", "Connect to Redis instance on this TCP/IP port (implies --timeseries)" },
    { "redishost", 1, 'h', "HOST", "Connect to Redis instance on this host name (implies --timeseries)" },
    PMAPI_OPTIONS_HEADER("Diagnostic options"),
    { "", 1, 'T', "TIME", "terminate after an elapsed time interval" },
    { "log", 1, 'l', "PATH", "redirect diagnostics and trace output" },
    { "", 1, 'x', "PATH", "fatal messages at startup sent to file [default /dev/tty]" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "Ac:C:dD:Ffh:i:l:L:M:p:P:r:s:tT:U:x:?",
    .long_options = longopts,
};

static int
ParseOptions(int argc, char *argv[], int *nports, int *maxpending)
{
    int		c;
    int		sts;
    int		usage = 0;
    int		timeseries = 1;
    int		redis_port = 6379;
    char	*redis_host = NULL;
    char	*endnum;
    const char	*inifile = NULL;
    sds		option;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'A':	/* disable pmproxy service advertising */
	    __pmServerClearFeature(PM_SERVER_FEATURE_DISCOVERY);
	    break;

	case 'c':	/* path to .ini configuration file */
	    inifile = opts.optarg;
	    timeseries = 1;
	    break;

	case 'C':	/* path to NSS certificate database */
	    certdb = opts.optarg;
	    break;

	case 'd':	/* run in deprecated (libpcp, select) mode */
	    timeseries = 0;
	    break;

	case 'D':	/* debug options */
	    if ((sts = pmSetDebug(opts.optarg)) < 0) {
		pmprintf("%s: unrecognized debug options specification (%s)\n",
			pmGetProgname(), opts.optarg);
		opts.errors++;
	    }
	    break;

	case 'F':	/* foreground but do pidfile and uid work */
	    if (run_mode != RUN_DAEMON) {
		pmprintf("%s: at most one of -f and -F allowed\n",
			pmGetProgname());
		opts.errors++;
	    }
	    else
		run_mode = RUN_SYSTEMD;
	    break;

	case 'f':	/* foreground, i.e. do _not_ run as a daemon */
	    if (run_mode != RUN_DAEMON) {
		pmprintf("%s: at most one of -f and -F allowed\n",
			pmGetProgname());
		opts.errors++;
	    }
	    else
		run_mode = RUN_FOREGROUND;
	    break;

	case 'h':	/* Redis host name */
	    redis_host = opts.optarg;
	    timeseries = 1;
	    break;

	case 'i':
	    /* one (of possibly several) interfaces for client requests */
	    __pmServerAddInterface(opts.optarg);
	    break;

	case 'l':	/* log file name */
	    logfile = opts.optarg;
	    break;

        case 'M':	/* nickname for server cert, use to query the nssdb */
            cert_nickname = opts.optarg;
	    timeseries = 0;
            break;

	case 'L':	/* maximum size for PDUs from clients */
	    sts = (int)strtol(opts.optarg, NULL, 0);
	    if (sts <= 0) {
		pmprintf("%s: -L requires a positive value\n", pmGetProgname());
		opts.errors++;
	    } else {
		__pmSetPDUCeiling(sts);
	    }
	    break;

	case 'p':
	    if (__pmServerAddPorts(opts.optarg) < 0) {
		pmprintf("%s: -p requires a positive numeric argument (%s)\n",
			pmGetProgname(), opts.optarg);
		opts.errors++;
	    } else {
		*nports += 1;
	    }
	    break;

	case 'P':	/* password file for certificate database access */
	    dbpassfile = opts.optarg;
	    timeseries = 0;
	    break;

	case 'Q':	/* require clients to provide a trusted cert */
	    __pmServerSetFeature(PM_SERVER_FEATURE_CERT_REQD);
	    break;

	case 'r':	/* Redis port number */
	    redis_port = (int)strtol(opts.optarg, NULL, 0);
	    if (redis_port <= 0) {
		pmprintf("%s: -r requires a positive value\n", pmGetProgname());
		opts.errors++;
	    }
	    timeseries = 1;
	    break;

	case 's':	/* path to local unix domain socket */
	    pmsprintf(sockpath, sizeof(sockpath), "%s", opts.optarg);
	    break;

	case 'S':	/* only allow authenticated clients */
	    __pmServerSetFeature(PM_SERVER_FEATURE_CREDS_REQD);
	    break;

	case 't':	/* run in timeseries mode (libuv, REST APIs) */
	    timeseries = 1;
	    break;

	case 'T':	/* terminate after given time has elapsed */
	    sts = pmParseInterval(opts.optarg, &opts.finish, &endnum);
	    if (sts < 0) {
		pmprintf("%s: bad -T interval format:\n", pmGetProgname());
		opts.errors++;
		free(endnum);
	    } else {
	        opts.finish_optarg = opts.optarg;
	    }
	    break;

	case 'U':	/* run as user username */
	    username = opts.optarg;
	    break;

	case 'x':
	    fatalfile = opts.optarg;
	    break;

	case '?':
	    usage = 1;
	    break;

	default:
	    opts.errors++;
	    break;
	}
    }

    /*
     * Parse the configuration file, extracting a dictionary of key/value
     * pairs.  Each key is "section.name" and values are always strings.
     * If no config given, default is /etc/pcp/pmproxy/pmproxy.conf (in addition,
     * local user path settings in $HOME/.pcp/pmproxy.conf are merged).
     */
    if ((config = pmIniFileSetup(inifile)) == NULL) {
	pmprintf("%s: cannot setup from configuration file %s\n",
			pmGetProgname(), inifile? inifile : "pmproxy.conf");
	opts.errors++;
    } else {
	int	fallback = 0;

	/* Extract pmproxy configuration information needed immediately */
	if ((option = pmIniFileLookup(config, "pmproxy", "maxpending")))
	    *maxpending = atoi(option);

	/*
	 * Push command line options into the configuration, and ensure
	 * we have some default for attemping Redis server connections.
	 */
	if ((option = pmIniFileLookup(config, "redis", "servers")) == NULL) {
	    if ((option = pmIniFileLookup(config, "pmseries", "servers")))
	        fallback = 1;
	}
	if (option == NULL || redis_host != NULL || redis_port != 6379) {
	    option = sdscatfmt(sdsempty(), "%s:%u",
		    redis_host? redis_host : "localhost", redis_port);
	    if (!fallback)
		pmIniFileUpdate(config, "redis", "servers", option);
	    else
		pmIniFileUpdate(config, "pmseries", "servers", option);
	}
    }

#if !defined(HAVE_LIBUV)
    if (timeseries) {
	timeseries = 0;
	pmprintf("%s: disabled time series, requires libuv support (missing)\n",
			pmGetProgname());
	pmflush();
    }
    server = &libpcp_pmproxy;
#else
    server = timeseries ? &libuv_pmproxy: &libpcp_pmproxy;
#endif

    if (opts.optind < argc)
	opts.errors++;
    if (opts.flags & PM_OPTFLAG_EXIT)
	usage++;

    if (usage || opts.errors) {
	pmUsageMessage(&opts);
	if (usage)
	    exit(0);
	DontStart();
    }

    return timeseries;
}

/* Called to shutdown pmproxy in an orderly manner */
void
Shutdown(void)
{
    server->shutdown(info);
    __pmSecureServerShutdown();
    pmNotifyErr(LOG_INFO, "pmproxy Shutdown\n");
    fflush(stderr);
}

void *
GetServerInfo(void)
{
    return info;	/* deprecated access mode for server information */
}

static void
set_rlimit_maxfiles(void)
{
    struct rlimit limit;

    if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
	pmNotifyErr(LOG_ERR, "Cannot get open file limits\n");
    else {
	limit.rlim_cur = limit.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
	    pmNotifyErr(LOG_ERR, "Cannot adjust open file limits\n");
    }
}

#define ENV_WARN_PORT		1
#define ENV_WARN_LOCAL		2
#define ENV_WARN_MAXPENDING	4

int
main(int argc, char *argv[])
{
    int		sts;
    int		nport = 0;
    int		localhost = 0;
    int		maxpending = MAXPENDING;
    int		env_warn = 0;
    int		timeseries;
    char	*envstr;
    pid_t	mainpid;

    umask(022);
    set_rlimit_maxfiles();
    pmGetUsername(&username);
    __pmServerSetFeature(PM_SERVER_FEATURE_DISCOVERY);

    if ((envstr = getenv("PMPROXY_PORT")) != NULL) {
	nport = __pmServerAddPorts(envstr);
	env_warn |= ENV_WARN_PORT;
    }
    if ((envstr = getenv("PMPROXY_LOCAL")) != NULL) {
	if ((localhost = atoi(envstr)) != 0) {
	    __pmServerSetFeature(PM_SERVER_FEATURE_LOCAL);
	    env_warn |= ENV_WARN_LOCAL;
	}
    }
    if ((envstr = getenv("PMPROXY_MAXPENDING")) != NULL) {
	maxpending = atoi(envstr);
	env_warn |= ENV_WARN_MAXPENDING;
    }
    timeseries = ParseOptions(argc, argv, &nport, &maxpending);

    if (pmDebugOptions.appl1) {
	/*
	 * -Dappl1 is desperate logging mode ... insert .<pid> into
	 * the logfile name, just before the last ., so pmproxy.log
	 * will become pmproxy.<pid>.log
	 */
	char	newlogfile[MAXPATHLEN];
	char	pbuf[11];	/* enough for a 32-bit pid */
	char	*pend = NULL;

	snprintf(pbuf, sizeof(pbuf), ".%" FMT_PID, (pid_t)getpid());
	pend = rindex(logfile, '.');
	if (pend == NULL) {
	    /* no '.', so append .<pid> */
	    strncpy(newlogfile, logfile, MAXPATHLEN-1);
	    strcat(newlogfile, pbuf);
	}
	else {
	    /* stitch name together ... <pre>.<post> -> <pre>.<pid>.<post> */
	    char	*q = newlogfile;
	    char	*r = pbuf;
	    char	*p;

	    for (p = logfile; p < pend; ) 	/* <pre> */
		*q++ = *p++;
	    while (*r)				/* .<pid> */
		*q++ = *r++;
	    while (*p)				/* .<post> */
		*q++ = *p++;
	    *q = '\0';
	}
	pmOpenLog(pmGetProgname(), newlogfile, stderr, &sts);
    }
    else
	pmOpenLog(pmGetProgname(), logfile, stderr, &sts);

    /* close old stdout, and force stdout into same stream as stderr */
    fflush(stdout);
    close(fileno(stdout));
    if (dup(fileno(stderr)) == -1) {
	fprintf(stderr, "Warning: dup() failed: %s\n", pmErrStr(-oserror()));
    }

    if (localhost)
	__pmServerAddInterface("INADDR_LOOPBACK");
    if (nport == 0) {
	nport = __pmServerAddPorts(TO_STRING(PROXY_PORT));
	if (timeseries)
	    nport = __pmServerAddPorts(TO_STRING(WEBAPI_PORT));
    }

    /* Advertise the service on the network if that is supported */
    __pmServerSetServiceSpec(PM_SERVER_PROXY_SPEC);

    if (run_mode == RUN_DAEMON) {
	/* daemonize - fork and parent exits, setsid */
	__pmServerStart(argc, argv, 1);
    }
    mainpid = getpid();

    /* Open non-blocking request ports for client connections */
    if ((info = server->openports(sockpath, sizeof(sockpath), maxpending)) == NULL)
	DontStart();

    if (env_warn & ENV_WARN_PORT)
        fprintf(stderr, "%s: nports=%d from PMPROXY_PORT=%s in environment\n",
			"Warning", nport, getenv("PMPROXY_PORT"));
    if (env_warn & ENV_WARN_LOCAL)
	fprintf(stderr, "%s: localhost only from PMPROXY_LOCAL=%s in environment\n",
			"Warning", getenv("PMPROXY_LOCAL"));
    if (env_warn & ENV_WARN_MAXPENDING)
	fprintf(stderr, "%s: maxpending=%d from PMPROXY_MAXPENDING=%s in environment\n",
			"Warning", maxpending, getenv("PMPROXY_MAXPENDING"));

    if (run_mode == RUN_DAEMON || run_mode == RUN_SYSTEMD) {
	/* notify service manager, if any, we are ready */
	__pmServerNotifyServiceManagerReady(mainpid);
	if (__pmServerCreatePIDFile(PM_SERVER_PROXY_SPEC, PM_FATAL_ERR) < 0)
	    DontStart();
	if (pmSetProcessIdentity(username) < 0)
	    DontStart();
    }

    if (!timeseries &&
        __pmSecureServerCertificateSetup(certdb, dbpassfile, cert_nickname) < 0)
	DontStart();

    fprintf(stderr, "pmproxy: PID = %" FMT_PID, mainpid);
    fprintf(stderr, ", PDU version = %u", PDU_VERSION);
#ifdef HAVE_GETUID
    fprintf(stderr, ", user = %s (%d)\n", username, getuid());
#endif
    server->dumpports(stderr, info);
    fflush(stderr);

    /* Loop processing client connections and server responses */
    server->loop(info, opts.finish_optarg? &opts.finish : NULL);

    /* inform service manager and shutdown cleanly */
    __pmServerNotifyServiceManagerStopping(mainpid);
    Shutdown();
    exit(0);
}
