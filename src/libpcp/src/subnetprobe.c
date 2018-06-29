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
#include "internal.h"
#include "subnetprobe.h"

#define PROBE	"__pmSubnetProbeDiscoverServices"

/*
 * Service discovery by active probing. The given subnet is probed for the
 * requested service(s).
 */
typedef struct connectionOptions {
    __pmSockAddr	*netAddress;	/* Address of the subnet */
    int			maskBits;	/* Number of bits in the subnet */
    unsigned		maxThreads;	/* Max number of threads to use. */
    struct timeval	timeout;	/* Connection timeout */
    const __pmServiceDiscoveryOptions *globalOptions; /* Global discover options */
} connectionOptions;

/* Context for each thread. */
typedef struct connectionContext {
    const char		*service;	/* Service spec */
    __pmSockAddr	*nextAddress;	/* Next available address */
    int			nports;		/* Number of ports per address */
    int			portIx;		/* Index of next available port */
    const int		*ports;		/* The actual ports */
    int			*numUrls;	/* Size of the results */
    char		***urls;	/* The results */
    const connectionOptions *options;	/* Connection options */
    __pmMutex		addrLock;	/* lock for the above address/port */
    __pmMutex		urlLock;	/* lock for the above results */
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
	/* Obtain the address lock while securing the next address, if any. */
	PM_LOCK(context->addrLock);
	if (context->nextAddress == NULL) {
	    /* No more addresses. */
	    PM_UNLOCK(context->addrLock);
	    break;
	}

	/*
	 * There is an address+port remaining. Secure them. If we cannot
	 * obtain our own copy of the address, then give up the lock and
	 * try again. Another thread will try this address+port.
	 */
	addr = __pmSockAddrDup(context->nextAddress);
	if (addr == NULL) {
	    PM_UNLOCK(context->addrLock);
	    continue;
	}
	port = context->ports[context->portIx];
	__pmSockAddrSetPort(addr, port);

	/*
	 * Advance the port index for the next thread. If we took the
	 * final port, then advance the address and reset the port index.
	 * The address may become NULL which is the signal for all
	 * threads to exit.
	 */
	++context->portIx;
	if (context->portIx == context->nports) {
	    context->portIx = 0;
	    context->nextAddress =
		__pmSockAddrNextSubnetAddr(context->nextAddress,
					   context->options->maskBits);
	}
	PM_UNLOCK(context->addrLock);

	/*
	 * Create a socket. There is a limit on open fds, not just from
	 * the OS, but also in the IPC table. If we get EAGAIN,
	 * then wait 0.1 seconds and try again.  We must have a limit in case
	 * something goes wrong. Make it 5 seconds (50 * 100,000 usecs).
	 */
	for (attempt = 0; attempt < 50; ++attempt) {
	    if (__pmSockAddrIsInet(addr))
		s = __pmCreateSocket();
	    else /* address family already checked */
		s = __pmCreateIPv6Socket();
	    if (s != -EAGAIN)
		break;
	    __pmtimevalSleep(againWait);
	}
	if (pmDebugOptions.discovery) {
	    if (attempt > 0) {
		pmNotifyErr(LOG_INFO, "%s: Waited %d times for available fd\n",
			      PROBE, attempt);
	    }
	}
	if (s < 0) {
	    char *addrString = __pmSockAddrToString(addr);
	    pmNotifyErr(LOG_WARNING, "%s: Cannot create socket for address %s",
			  PROBE, addrString);
	    free(addrString);
	    __pmSockAddrFree(addr);
	    continue;
	}

	/*
	 * Attempt to connect. If flags comes back as less than zero, then the
	 * socket has already been closed by __pmConnectTo().
	 */
	sts = -1;
	flags = __pmConnectTo(s, addr, port);
	if (flags >= 0) {
	    /*
	     * FNDELAY and we're in progress - wait on __pmSelectWrite.
	     * __pmSelectWrite may alter the contents of the timeout so make a
	     * copy.
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
    } /* Loop over connection attempts. */

    return NULL;
}

static int
probeForServices(const char *service,
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
	pmNotifyErr(LOG_ERR, "%s: could not find ports for service '%s'",
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

    /*
     * Initialize the first address of the subnet. This pointer will become
     * NULL and the memory freed by __pmSockAddrNextSubnetAddr() when the
     * final address+port has been probed.
     */
    context.nextAddress =
	__pmSockAddrFirstSubnetAddr(options->netAddress, options->maskBits);
    if (context.nextAddress == NULL) {
	char *addrString = __pmSockAddrToString(options->netAddress);
	pmNotifyErr(LOG_ERR, "%s: unable to determine the first address"
			     " of the subnet: %s/%d",
			PROBE, addrString, options->maskBits);
	free(addrString);
	goto done;
    }

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

 done:
    free(ports);
    if (context.nextAddress)
	__pmSockAddrFree(context.nextAddress);
#if PM_MULTI_THREAD
    if (threads)
	free(threads);
#endif

    /* Return the number of new urls. */
    return numUrls - prevNumUrls;
}

/*
 * Parse the mechanism string for options. The first option will be of the form
 *
 *   probe=<net-address>/<maskSize>
 *
 * Subsequent options, if any, will be separated by commas. Currently supported:
 *
 *   timeout=<double>      -- number of seconds before timing out an address
 *   maxThreads=<integer>  -- specifies a hard limit on the number of active
 *                            threads.
 */
static int
parseOptions(const char *mechanism, connectionOptions *options)
{
    const char		*addressString;
    const char		*maskString;
    size_t		len;
    char		*buf;
    char		*end;
    const char		*option;
    int			family;
    int			sts;
    long		longVal;
    unsigned		subnetBits;
    unsigned		subnetSize;

    /* Nothing to probe? */
    if (mechanism == NULL)
	return -1;

    /* First extract the subnet argument, parse it and check it. */
    addressString = strchr(mechanism, '=');
    if (addressString == NULL || addressString[1] == '\0') {
	pmNotifyErr(LOG_ERR, "%s: No argument provided", PROBE);
	return -1;
    }
    ++addressString;
    maskString = strchr(addressString, '/');
    if (maskString == NULL || maskString[1] == '\0') {
	pmNotifyErr(LOG_ERR, "%s: No subnet mask provided", PROBE);
	return -1;
    }
    ++maskString;

    /* Convert the address string to a socket address. */
    len = maskString - addressString; /* enough space for the nul */
    buf = malloc(len);
    memcpy(buf, addressString, len - 1);
    buf[len - 1] = '\0';
    options->netAddress = __pmStringToSockAddr(buf);
    if (options->netAddress == NULL) {
	pmNotifyErr(LOG_ERR, "%s: Address '%s' is not valid", PROBE, buf);
	free(buf);
	return -1;
    }
    free(buf);

    /* Convert the mask string to an integer */
    options->maskBits = strtol(maskString, &end, 0);
    if (*end != '\0' && *end != ',') {
	pmNotifyErr(LOG_ERR, "%s: Subnet mask '%s' is not valid",
			PROBE, maskString);
	return -1;
    }

    /* Check the number of bits in the mask against the address family. */
    if (options->maskBits < 0) {
	pmNotifyErr(LOG_ERR, "%s: Inet subnet mask must be >= 0 bits", PROBE);
	return -1;
    }
    family = __pmSockAddrGetFamily(options->netAddress);
    switch (family) {
    case AF_INET:
	if (options->maskBits > 32) {
	    pmNotifyErr(LOG_ERR, "%s: Inet subnet mask must be <= 32 bits",
			    PROBE);
	    return -1;
	}
	break;
    case AF_INET6:
	if (options->maskBits > 128) {
	    pmNotifyErr(LOG_ERR, "%s: Inet subnet mask must be <= 128 bits",
			    PROBE);
	    return -1;
	}
	break;
    default:
	pmNotifyErr(LOG_ERR, "%s: Unsupported address family, %d",
			PROBE, family);
	return -1;
    }

    /*
     * Parse any remaining options.
     * Initialize to defaults first.
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

    /* Now parse the options. */
    sts = 0;
    for (option = end; *option != '\0'; /**/) {
	/*
	 * All additional options begin with a separating comma.
	 * Make sure something has been specified.
	 */
	++option;
	if (*option == '\0') {
	    pmNotifyErr(LOG_ERR, "%s: Missing option after ','", PROBE);
	    return -1;
	}

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

    /*
     * We now have a maximum for the number of threads
     * but there's no point in creating more threads than the number of
     * addresses in the subnet (less 1 for the main thread).
     *
     * We already know that the address is inet or ipv6 and that the
     * number of mask bits is appropriate.
     *
     * Beware of overflow!!! If the calculation would have overflowed,
     * then it means that the subnet is extremely large and therefore
     * much larger than maxThreads anyway.
     */
    if (__pmSockAddrIsInet(options->netAddress))
	subnetBits = 32 - options->maskBits;
    else
	subnetBits = 128 - options->maskBits;
    if (subnetBits < sizeof(subnetSize) * 8) {
	subnetSize = 1 << subnetBits;
	if (subnetSize - 1 < options->maxThreads)
	    options->maxThreads = subnetSize - 1;
    }

    return sts;
}

int
__pmSubnetProbeDiscoverServices(const char *service,
	const char *mechanism, const __pmServiceDiscoveryOptions *globalOptions,
	int numUrls, char ***urls)
{
    connectionOptions options;
    int	sts;

    /* Interpret the mechanism string. */
    sts = parseOptions(mechanism, &options);
    if (sts != 0)
	return 0;
    options.globalOptions = globalOptions;

    /* Everything checks out. Now do the actual probing. */
    numUrls = probeForServices(service, &options, numUrls, urls);

    /* Clean up */
    __pmSockAddrFree(options.netAddress);

    return numUrls;
}
