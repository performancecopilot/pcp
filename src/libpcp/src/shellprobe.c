/*
 * Copyright (c) 2014,2018 Red Hat.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include "pmapi.h"
#include "libpcp.h"
#define SOCKET_INTERNAL
#include "internal.h"
#include "shellprobe.h"
#include <ctype.h>

#define PROBE	"__pmShellProbeDiscoverServices"

/*
 * Service discovery by indirect probing. The list of hostnames and/or IP
 * addresses to probe is determined by $PCP_BINADM_DIR/discover scripts.
 */
typedef struct targetInfo {
    char		*target;
    __pmHostEnt		*servInfo;	
} targetInfo;

typedef struct connectionOptions {
    char		path[MAXPATHLEN]; /* Discovery script path */
    char		cmd[128];	/* optional individual command */
    targetInfo		*targets;
    unsigned		numTargets;	/* Count of target addresses */
    unsigned		maxTargets;	/* High-water mark for targets */
    unsigned		maxThreads;	/* Max number of threads to use */
    struct timeval	timeout;	/* Connection timeout */
    const __pmServiceDiscoveryOptions *globalOptions; /* Global options */
} connectionOptions;

/* Context for each thread. */
typedef struct connectionContext {
    const char		*service;	/* Service spec */
    targetInfo		*nextTarget;	/* Next available target address */
    int			nports;		/* Number of ports per address */
    int			portIx;		/* Index of next available port */
    const int		*ports;		/* The actual ports */
    int			*numUrls;	/* Size of the results */
    char		***urls;	/* The results */
    const connectionOptions *options;	/* Connection options */
#if PM_MULTI_THREAD
    __pmMutex		addrLock;	/* lock for the above address/port */
    __pmMutex		urlLock;	/* lock for the above results */
#endif
} connectionContext;

/*
 * Attempt connection based on the given context until there are no more
 * addresses+ports to try.
 */
static void *
attemptConnections(void *arg)
{
    int			s;
    int			flags;
    int			sts;
    __pmFdSet		wfds;
    __pmServiceInfo	serviceInfo;
    __pmSockAddr	*addr;
    targetInfo		target;
    void		*enumIx;
    const __pmAddrInfo	*ai;
    const __pmServiceDiscoveryOptions *globalOptions;
    int			port;
    int			attempt;
    struct timeval	againWait = {0, 100000}; /* 0.1 seconds */
    connectionContext	*context = arg;

    /*
     * Keep trying to secure an address+port until there are no more
     * or until we are interrupted.
     */
    globalOptions = context->options->globalOptions;
    while (! globalOptions->timedOut &&
	   (! globalOptions->flags ||
	    (*globalOptions->flags & PM_SERVICE_DISCOVERY_INTERRUPTED) == 0)) {
	/* Obtain the address lock while securing the next target, if any. */
	PM_LOCK(context->addrLock);
	if (context->nextTarget == NULL) {
	    /* No more target addresses to probe. */
	    PM_UNLOCK(context->addrLock);
	    break;
	}

	/*
	 * There is an address+port remaining. Copy them locally.
	 */
	memcpy(&target, context->nextTarget, sizeof(target));
	port = context->ports[context->portIx];

	/*
	 * Advance the port index for the next thread. If we took the
	 * final port, then advance the address and reset the port index.
	 * The next target will then be set NULL which is the signal for
	 * all threads to exit.
	 */
	++context->portIx;
	if (context->portIx == context->nports) {
	    context->portIx = 0;
	    s = context->nextTarget - context->options->targets + 1;
	    if (context->options->numTargets <= s)
		context->nextTarget = NULL;
	    else
		context->nextTarget++;
	}
	PM_UNLOCK(context->addrLock);

	enumIx = NULL;
	for (addr = __pmHostEntGetSockAddr(target.servInfo, &enumIx);
	     addr != NULL;
	     addr = __pmHostEntGetSockAddr(target.servInfo, &enumIx)) {
	    /*
	     * We should be ignoring any entry without SOCK_STREAM (= 1),
	     * since we're connecting with TCP only. The other entries
	     * (SOCK_RAW, SOCK_DGRAM) are not relevant.
	     */
	    ai = __pmHostEntGetAddrInfo(target.servInfo, enumIx);
	    if (ai->ai_socktype != SOCK_STREAM) {
		__pmSockAddrFree(addr);
		continue;
	    }
	    __pmSockAddrSetPort(addr, port);

	    /*
	     * Create a socket. There is a limit on open fds, not just from
	     * the OS, but also in the IPC table. If we get EAGAIN, then
	     * wait 0.1 seconds and try again. We must have a limit in case
	     * something goes wrong. Make it 5 seconds (50 * 100,000 usecs).
	     */
	    for (attempt = 0; attempt < 50; ++attempt) {
		if (__pmSockAddrIsInet(addr))
		    s = __pmCreateSocket();
		else if (__pmSockAddrIsIPv6(addr))
		    s = __pmCreateIPv6Socket();
		else
		    s = -EINVAL;
		if (s != -EAGAIN)
		    break;
		__pmtimevalSleep(againWait);
	    }

	    if (pmDebugOptions.discovery && attempt > 0) {
		pmNotifyErr(LOG_INFO, "%s: Waited %d times for available fd\n",
				PROBE, attempt);
	    }

	    if (s < 0) {
	        pmNotifyErr(LOG_WARNING, "%s: Cannot create socket for %s:%d",
				PROBE, target.target, port);
		__pmSockAddrFree(addr);
		continue;
	    }

	    /*
	     * Attempt to connect. If flags comes back as less than zero,
	     * then the socket has already been closed by __pmConnectTo().
	     */
	    sts = -1;
	    flags = __pmConnectTo(s, addr, port);
	    if (flags >= 0) {
		/*
		 * FNDELAY and we're in progress - wait on __pmSelectWrite.
		 * __pmSelectWrite may alter the contents of the timeout so
		 * make a copy.
		 */
		struct timeval timeout = context->options->timeout;
		__pmFD_ZERO(&wfds);
		__pmFD_SET(s, &wfds);
		sts = __pmSelectWrite(s+1, &wfds, &timeout);

		/* Was the connection successful? */
		if (sts == 0)
		    sts = -1; /* Timed out */
		else if (sts > 0)
		    sts = __pmConnectCheckError(s);
		__pmCloseSocket(s);
	    }

	    /* If connection was successful, add this service to the list.  */
	    if (sts == 0) {
		serviceInfo.spec = context->service;
		serviceInfo.address = addr;
		if (strcmp(context->service, PM_SERVER_SERVICE_SPEC) == 0)
		    serviceInfo.protocol = SERVER_PROTOCOL;
		else if (strcmp(context->service, PM_SERVER_PROXY_SPEC) == 0)
		    serviceInfo.protocol = PROXY_PROTOCOL;
		else if (strcmp(context->service, PM_SERVER_WEBD_SPEC) == 0)
		    serviceInfo.protocol = PMWEBD_PROTOCOL;

		PM_LOCK(context->urlLock);
		*context->numUrls =
			__pmAddDiscoveredService(&serviceInfo, globalOptions,
					 *context->numUrls, context->urls);
		PM_UNLOCK(context->urlLock);
	    }

	    __pmSockAddrFree(addr);
	}
    }

    return NULL;
}

static int
shellProbeForServices(const char *script, const char *service,
    const connectionOptions *options, int numUrls, char ***urls)
{
    int			*ports = NULL;
    int			nports;
    int			prevNumUrls = numUrls;
    connectionContext	context;
#if PM_MULTI_THREAD
    int			sts;
    pthread_t		*threads = NULL;
    unsigned		threadIx;
    unsigned		nThreads;
    pthread_attr_t	threadAttr;
#endif

    /* Determine the port numbers for this service. */
    ports = NULL;
    nports = 0;
    nports = __pmServiceAddPorts(service, &ports, nports);
    if (nports <= 0) {
	if (pmDebugOptions.discovery)
	    pmNotifyErr(LOG_INFO, "%s: no recognised ports for service '%s'",
			PROBE, service);
	return 0;
    }

    /*
     * Initialize the shared probing context. This will be shared among all of
     * the worker threads.
     */
    context.service = service;
    context.ports = ports;
    context.nports = nports;
    context.numUrls = &numUrls;
    context.urls = urls;
    context.portIx = 0;
    context.options = options;
    context.nextTarget = &options->targets[0];

#if PM_MULTI_THREAD
    /*
     * Set up the concurrrency controls. These locks will be tested
     * even if we fail to allocate/use the thread table below.
     */
    pthread_mutex_init(&context.addrLock, NULL);
    pthread_mutex_init(&context.urlLock, NULL);

    if (options->maxThreads > 0) {
	/*
	 * Allocate the thread table. We have a maximum for the number of
	 * threads, so that will be the size.
	 */
	threads = malloc(options->maxThreads * sizeof(*threads));
	if (threads == NULL) {
	    /*
	     * Unable to allocate the thread table, however, We can still do the
	     * probing on the main thread.
	     */
	    pmNotifyErr(LOG_ERR, "%s: unable to allocate %u threads",
			  PROBE, options->maxThreads);
	}
	else {
	    /* We want our worker threads to be joinable. */
	    pthread_attr_init(&threadAttr);
	    pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_JOINABLE);

	    /*
	     * Our worker threads don't need much stack. PTHREAD_STACK_MIN is
	     * enough except when resolving addresses, where twice that much is
	     * sufficient.  Or it would be, except for error messages.  Those
	     * call pmNotifyErr -> pmprintf -> pmGetConfig, which are
	     * stack-intensive due to multiple large local variables
	     * (char[MAXPATHLEN]).
	     */
	    if (options->globalOptions->resolve ||
		(options->globalOptions->flags &&
		 (*options->globalOptions->flags & PM_SERVICE_DISCOVERY_RESOLVE)
		 != 0))
		pthread_attr_setstacksize(&threadAttr, 2*PTHREAD_STACK_MIN + MAXPATHLEN*4);
	    else
		pthread_attr_setstacksize(&threadAttr, PTHREAD_STACK_MIN + MAXPATHLEN*4);

	    /* Dispatch the threads. */
	    for (nThreads = 0; nThreads < options->maxThreads; ++nThreads) {
		sts = pthread_create(&threads[nThreads], &threadAttr,
				     attemptConnections, &context);
		/*
		 * If we failed to create a thread, then we've reached the OS
		 * limit.
		 */
		if (sts != 0)
		    break;
	    }

	    /* We no longer need this. */
	    pthread_attr_destroy(&threadAttr);
	}
    }
#endif

    /*
     * In addition to any threads we've dispatched, this thread can also
     * participate in the probing.
     */
    attemptConnections(&context);

#if PM_MULTI_THREAD
    if (threads) {
	/* Wait for all the connection attempts to finish. */
	for (threadIx = 0; threadIx < nThreads; ++threadIx)
	    pthread_join(threads[threadIx], NULL);
    }

    /* These must not be destroyed until all of the threads have finished. */
    pthread_mutex_destroy(&context.addrLock);
    pthread_mutex_destroy(&context.urlLock);
#endif

    free(ports);
#if PM_MULTI_THREAD
    if (threads)
	free(threads);
#endif

    /* Return the number of new urls. */
    return numUrls - prevNumUrls;
}

static void
addTarget(char *target, connectionOptions *options)
{
    __pmHostEnt	*servInfo;
    targetInfo	*tp;
    size_t	bytes;
    char	*name;
    int		index;

    while (options->numTargets >= options->maxTargets) {
	if (options->maxTargets == 0)
	    options->maxTargets = 16;
	else
	    options->maxTargets *= 2;
	bytes = options->maxTargets * sizeof(struct targetInfo);
	if ((tp = realloc(options->targets, bytes)) == NULL) {
	    if (pmDebugOptions.discovery)
		pmNotifyErr(LOG_ERR, "%s: failed targets realloc: %" FMT_INT64 " bytes",
				PROBE, (__int64_t)bytes);
	    return;
	}
	options->targets = tp;
    }

    /* Remove any leading or trailing whitespace we've been handed */
    while (*target != '\0' && isspace((int)(*target)))
	target++;
    bytes = strlen(target);
    name = target + bytes - 1;
    while (name < target && isspace((int)(*name)))
	name--;
    if (name > target) {
	bytes = name - target;
	*name = '\0';
    }

    /* Lookup address and stash target info into the array to scan */
    if ((servInfo = __pmGetAddrInfo(target)) == NULL) {
	if (pmDebugOptions.discovery) {
	    const char  *errmsg;
	    PM_LOCK(__pmLock_extcall);
	    errmsg = hoststrerror();            /* THREADSAFE */
	    fprintf(stderr, "%s:(%s) : hosterror=%d, ``%s''",
		    PROBE, target, hosterror(), errmsg);
	    PM_UNLOCK(__pmLock_extcall);
	}
    } else if ((name = strndup(target, bytes)) == NULL) {
	if (pmDebugOptions.discovery)
	    pmNotifyErr(LOG_ERR, "%s: failed target %s dup", PROBE, target);
	__pmHostEntFree(servInfo);
    } else {
	index = options->numTargets++;
	options->targets[index].target = name;
	options->targets[index].servInfo = servInfo;
    }
}

static void
runScript(const char *path, connectionOptions *options)
{
    __pmExecCtl_t	*argp = NULL;
    FILE		*fp;
    char		linebuf[256];
    char		msg[PM_MAXERRMSGLEN];
    const int		toss = PM_EXEC_TOSS_STDIN | PM_EXEC_TOSS_STDERR;
    int			sts;

    if ((sts = __pmProcessAddArg(&argp, path)) < 0) {
	if (pmDebugOptions.discovery)
	    pmNotifyErr(LOG_ERR, "%s: failed to add process path: %s",
			      PROBE, pmErrStr_r(sts, msg, sizeof(msg)));
	return;
    }
    if ((sts = __pmProcessPipe(&argp, "r", toss, &fp)) < 0) {
	if (pmDebugOptions.discovery)
	    pmNotifyErr(LOG_WARNING, "%s: failed to open process pipe: %s",
			      PROBE, pmErrStr_r(sts, msg, sizeof(msg)));
	return;
    }
    while (fgets(linebuf, sizeof(linebuf), fp) != NULL) {
	if (pmDebugOptions.discovery)
	    pmNotifyErr(LOG_INFO, "%s: line from script %s: %s",
			      PROBE, path, linebuf);
	addTarget(linebuf, options);
    }
    if ((sts = __pmProcessPipeClose(fp)) != 0) {
	if (pmDebugOptions.discovery)
	    pmNotifyErr(LOG_ERR, "%s: failed pipe close on script %s: %s",
			      PROBE, path, pmErrStr_r(sts, msg, sizeof(msg)));
    }
}

static int
probeForServices(const char *service, connectionOptions *options,
		int numUrls, char ***urls)
{
    DIR			*dir;
    int			sep = pmPathSeparator();
    char		path[MAXPATHLEN];
    char		*command = options->cmd[0] ? options->cmd : NULL;
    struct dirent	*dp;

    if ((dir = opendir(options->path)) == NULL)
	return 0;

    /* Scan directory for service probe scripts */
    while ((dp = readdir(dir)) != NULL) {
	if (dp->d_name[0] == '.')
	    continue;
	if (command && strcmp(command, dp->d_name) != 0)
	    continue;
	pmsprintf(path, sizeof(path), "%s%c%s", options->path, sep, dp->d_name);
	/* Run this script, add its output line-by-line to options->targets */
	runScript(path, options);
    }
    closedir(dir);

    if (options->numTargets == 0)
	return 0;

    return shellProbeForServices(path, service, options, numUrls, urls);
}

static const char *
parseFile(const char *option, char *buffer, int buflen)
{
    char		*end;

    strncpy(buffer, option, buflen);
    buffer[buflen-1] = '\0';

    /* Remove back-slash escaped comma characters, if any. */
    for (end = buffer; *end != '\0' && *end != ','; end++) {
	if (*end == '\\' && *(end+1) == ',')
	    memmove(end, end + 1, strlen(end) + 1);
    }
    end[0] = '\0';

    return option + (end - buffer);
}

/*
 * Parse the mechanism string for any options, separated by commas.
 *
 *   command=<file>        -- specifies an individual shell command basename
 *   path=<directory>      -- specifies location of the probe shell commands
 *   timeout=<double>      -- number of seconds before timing out an address
 *   maxThreads=<integer>  -- specifies a hard limit on the number of active
 *                            threads.
 */
static int
parseOptions(const char *mechanism, connectionOptions *options)
{
    char		*end;
    const char		*option = mechanism;
    long		longVal;
    int			sts;

    /*
     * Initialize the defaults first.
     *
     * FD_SETSIZE is the most open fds that __pmFD*()
     * and __pmSelect() can deal with, so it's a decent default for maxThreads.
     * The main thread also participates, so subtract 1.
     */
    options->maxThreads = FD_SETSIZE - 1;

    /*
     * Set a default for the connection timeout. 20ms allows us to scan 50
     * addresses per second per thread.
     */
    options->timeout.tv_sec = 0;
    options->timeout.tv_usec = 20 * 1000;

    /*
     * Default location for service discovery scripts
     */
    pmsprintf(options->path, sizeof(options->path), "%s%cdiscover",
		pmGetOptionalConfig("PCP_BINADM_DIR"), pmPathSeparator());

    /* Now parse the options. */
    sts = sizeof("shell,") - 1;
    if (mechanism == NULL || strncmp(mechanism, "shell,", sts) != 0)
	return 0;
    mechanism += sts - 1;
    sts = 0;
    for (option = mechanism; *option != '\0'; /**/) {
	/*
	 * All additional options begin with a separating comma.
	 * Make sure something has been specified.
	 */
	++option;

	/* Examine the option. */
	if (strncmp(option, "maxThreads=", sizeof("maxThreads=") - 1) == 0) {
	    option += sizeof("maxThreads=") - 1;
	    longVal = strtol(option, &end, 0);
	    if (*end != '\0' && *end != ',') {
		pmNotifyErr(LOG_ERR, "%s: maxThreads value '%s' is not valid",
			      PROBE, option);
		sts = -1;
	    }
	    else {
		option = end;
		/*
		 * Make sure the value is positive. Make sure that the given
		 * value does not exceed the existing value which is also the
		 * hard limit.
		 */
		if (longVal > options->maxThreads) {
		    pmNotifyErr(LOG_ERR,
				  "%s: maxThreads value %ld must not exceed %u",
				  PROBE, longVal, options->maxThreads);
		    sts = -1;
		}
		else if (longVal <= 0) {
		    pmNotifyErr(LOG_ERR,
				  "%s: maxThreads value %ld must be positive",
				  PROBE, longVal);
		    sts = -1;
		}
		else {
#if PM_MULTI_THREAD
		    /* The main thread participates, so reduce this by one. */
		    options->maxThreads = longVal - 1;
#else
		    pmNotifyErr(LOG_WARNING, "%s: no thread support."
				    " Ignoring maxThreads value %ld",
				    PROBE, longVal);
#endif
		}
	    }
	}
	else if (strncmp(option, "timeout=", sizeof("timeout=") - 1) == 0) {
	    option += sizeof("timeout=") - 1;
	    option = __pmServiceDiscoveryParseTimeout(option, &options->timeout);
	}
	else if (strncmp(option, "path=", sizeof("path=") - 1) == 0) {
	    option += sizeof("path=") - 1;
	    option = parseFile(option, options->path, sizeof(options->path));
	}
	else if (strncmp(option, "command=", sizeof("command=") - 1) == 0) {
	    option += sizeof("command=") - 1;
	    option = parseFile(option, options->cmd, sizeof(options->cmd));
	}
	else {
	    /* An invalid option. Skip it. */
	    pmNotifyErr(LOG_ERR, "%s: option '%s' is not valid", PROBE, option);
	    sts = -1;
	    ++option;
	}
	/* Locate the next option, if any. */
	for (/**/; *option != '\0' && *option != ','; ++option)
	    ;
    }

    return sts;
}

int
__pmShellProbeDiscoverServices(const char *service, const char *mechanism,
	const __pmServiceDiscoveryOptions *globalOptions,
	int numUrls, char ***urls)
{
    connectionOptions	options = { { 0 }, { 0 }, 0 };
    targetInfo		*targetInfo;
    int			sts, i;

    /* Interpret the mechanism string. */
    sts = parseOptions(mechanism, &options);
    if (sts != 0)
	return 0;
    options.globalOptions = globalOptions;

    numUrls = probeForServices(service, &options, numUrls, urls);

    /* Release all discovered target addresses. */
    if (options.numTargets) {
	for (i = 0; i < options.numTargets; i++) {
	    targetInfo = &options.targets[i];
	    __pmHostEntFree(targetInfo->servInfo);
	    free(targetInfo->target);
	}
	free(options.targets);
    }

    return numUrls;
}
