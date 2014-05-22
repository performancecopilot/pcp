/*
 * Copyright (c) 2014 Red Hat.
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
#include "impl.h"
#include "internal.h"
#include "probe.h"

#define IS_MULTI_THREAD (PM_MULTI_THREAD && HAVE_SEM_T)

/*
 * Service discovery by active probing. The given subnet is probed for the
 * requested service(s).
 */
static __pmSockAddr	*addressesProcessed;
static __pmSockAddr	*netAddress;
static int		maskBits;

#if IS_MULTI_THREAD
/*
 * Multi thread support. We need to protect the updating of the url list and the
 * unfinished thread counter.
 */
#include <semaphore.h>
static __pmMutex lock;
static pthread_attr_t attr;
static sem_t threadsAvailable;
static sem_t threadsComplete;
static unsigned	maxThreads = SEM_VALUE_MAX; /* No fixed limit by default */
#define LOCK_INIT(lock) pthread_mutex_init(lock, NULL)
#define LOCK_LOCK(lock) PM_LOCK(*(lock))
#define LOCK_UNLOCK(lock) PM_UNLOCK((*lock))
#define LOCK_TERM(lock) pthread_mutex_destroy(lock)
#define SEM_INIT(sem, value) sem_init(sem, 0, value)
#define SEM_WAIT(sem) sem_wait(sem)
#define SEM_POST(sem) sem_post(sem)
#define SEM_TERM(sem) sem_destroy(sem)
#define ATTR_INIT(attr) pthread_attr_init(attr)
#define ATTR_SET_STACKSIZE(attr) pthread_attr_setstacksize(attr, PTHREAD_STACK_MIN)
#define ATTR_TERM(attr) pthread_attr_destroy(attr)
#define THREAD_START(thread, attr, func, arg) pthread_create(thread, attr, func, arg)
#define THREAD_DETACH() pthread_detach(pthread_self())
#else
/* No multi thread support. */
#define LOCK_INIT(lock) /* do nothing */
#define LOCK_LOCK(lock) /* do nothing */
#define LOCK_UNLOCK(lock) /* do nothing */
#define LOCK_TERM(lock) /* do nothing */
#define SEM_INIT(sem, value) /* do nothing */
#define SEM_WAIT(sem) /* do nothing */
#define SEM_POST(sem) /* do nothing */
#define SEM_TERM(sem) /* do nothing */
#define ATTR_INIT(attr) /* do nothing */
#define ATTR_SET_STACKSIZE(attr) /* do nothing */
#define ATTR_TERM(attr) /* do nothing */
#define THREAD_START(thread, attr, func, arg) (func(arg), 0)
#define THREAD_DETACH() /* do nothing */
#endif

/* The number of connection attempts remaining. */
typedef struct connectionContext {
    const char *service;
    __pmSockAddr *address;
    int port;
    int *numUrls;
    char ***urls;
} connectionContext;

static void
attemptComplete(void)
{
    int	isFinalThread;
    /*
     * Indicate that this attempt is complete.
     * Post the threadsComplete semaphore, if this was the final thread.
     *
     * CAUTION: Posting the semaphore will release the main thread which
     * destroys the lock. Therefore make sure we're done with the lock before
     * posting the semaphore.
     */
    LOCK_LOCK(&lock);
    addressesProcessed = __pmSockAddrNextSubnetAddr(addressesProcessed, maskBits);
    isFinalThread = (addressesProcessed == NULL);
    LOCK_UNLOCK(&lock);
    if (isFinalThread)
	SEM_POST(&threadsComplete);
}

/*
 * Attempt a connection on the given address and port. Return 0, if successful.
 */
static void *
attemptConnection(void *arg)
{
    int			s;
    int			flags;
    int			sts;
    __pmFdSet		wfds;
    __pmServiceInfo	serviceInfo;
    int			attempt;
    struct timeval	canwait = {0, 100000 * 1000}; /* 0.1 seconds */
    connectionContext	*context = arg;
	
    /* If we are on our own thread, then run detached. */
    THREAD_DETACH();

    /*
     * Create a socket. There may be a limit on open fds. If we get EAGAIN, then
     * wait 0.1 seconds and try again.  We must have a limit in case something
     * goes wrong. Make it 5 seconds (50 * 100,000 usecs).
     */
    for (attempt = 0; attempt < 50; ++attempt) {
	if (__pmSockAddrIsInet(context->address))
	    s = __pmCreateSocket();
	else /* address family already checked */
	    s = __pmCreateIPv6Socket();
	if (s != -EAGAIN)
	    break;
	__pmtimevalSleep(canwait);
    }
    if (pmDebug & DBG_TRACE_DISCOVERY) {
	if (attempt > 0) {
	    __pmNotifyErr(LOG_INFO,
			  "Waited for %d attempts for an available fd\n",
			  attempt);
	}
    }

    if (s < 0) {
	char *addrString = __pmSockAddrToString(context->address);
	__pmNotifyErr(LOG_WARNING,
		      "__pmProbeDiscoverServices: Unable to create socket for address, %s",
		      addrString);
	free(addrString);
	goto done;
    }

    /*
     * Attempt to connect. If flags comes back as less than zero, then the
     * socket has already been closed by __pmConnectTo().
     */
    sts = -1;
    flags = __pmConnectTo(s, context->address, context->port);
    if (flags >= 0) {
	/* FNDELAY and we're in progress - wait on select */
	__pmFD_ZERO(&wfds);
	__pmFD_SET(s, &wfds);
	canwait = *__pmConnectTimeout();
	sts = __pmSelectWrite(s+1, &wfds, &canwait);

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
	serviceInfo.address = __pmSockAddrDup(context->address);
	LOCK_LOCK(&lock);
	*context->numUrls =
	    __pmAddDiscoveredService(&serviceInfo, *context->numUrls,
				     context->urls);
	LOCK_UNLOCK(&lock);
    }

 done:
    /*
     * The context and its address were allocated by the thread dispatcher.
     * Free them now.
     */
    __pmSockAddrFree(context->address);
    free(context);
    attemptComplete ();

    /* Always post this semaphore, which enforces the max number of active threads. */
    SEM_POST(&threadsAvailable);

    return NULL;
}

/* Dispatch a connection attempt, on its own thread, if supported. */
static void
dispatchConnection(
    const char *service,
    const __pmSockAddr *address,
    int port,
    int *numUrls,
    char ***urls
)
{
#if IS_MULTI_THREAD
    pthread_t thread;
#endif
    struct timeval	canwait = {0, 100000 * 1000}; /* 0.1 seconds */
    connectionContext	*context;
    int			sts;
    int			attempt;

    /* We need a separate connection context for each potential thread. */
    context = malloc(sizeof(*context));
    if (context == NULL) {
	__pmNoMem("__pmProbeDiscoverServices: unable to allocate connection context",
		  sizeof(*context), PM_FATAL_ERR);
    }
    context->service = service;
    context->port = port;
    context->numUrls = numUrls;
    context->urls = urls;
    context->address = __pmSockAddrDup(address);
    if (context->address == NULL) {
	char *addrString = __pmSockAddrToString(address);
	__pmNotifyErr(LOG_ERR,
		      "__pmProbeDiscoverServices: unable to copy socket address %s",
		      addrString);
	free(addrString);
	attemptComplete();
	return;
    }

    /*
     * Attempt the connection. Since we're not passing in attributes for the
     * (possible) new thread, the only error that can occur is EAGAIN. Sleep
     * for 0.1 seconds before trying again. We must have a limit in case
     * something goes wrong. Make it 5 seconds (50 * 100,000 usecs).
     *
     * Respect the requested maximum number of threads.
     */
    SEM_WAIT(&threadsAvailable);
    for (attempt = 0; attempt < 50; ++attempt) {
	/* Attempt the connection on a new thread. */
        sts = THREAD_START(&thread, &attr, attemptConnection, context);
	if (sts != EAGAIN)
	    break;

	/* Wait before trying again. */
	__pmtimevalSleep(canwait);
    }

    /* Check to see that the thread started. */
    if (sts != 0) {
	char *addrString = __pmSockAddrToString(address);
	__pmNotifyErr(LOG_ERR,
		      "__pmProbeDiscoverServices: unable to start a thread to probe address %s",
		      addrString);
	free(addrString);
	attemptComplete();
	SEM_POST(&threadsAvailable);
    }
    else if (pmDebug & DBG_TRACE_DISCOVERY) {
	if (attempt > 0) {
	    __pmNotifyErr(LOG_INFO,
			  "Waited for %d attempts to create a thread\n",
			  attempt);
	}
    }
}

static int
probeForServices(
    const char *service,
    __pmSockAddr *netAddress,
    int maskBits,
    int numUrls,
    char ***urls
)
{
    char		*end;
    int			port;
    const char		*env;
    __pmSockAddr	*address;
    int			prevNumUrls;

    /* The service is either a service name (e.g. pmcd) or a port number. */
    if (strcmp(service, "pmcd") == 0) {
	if ((env = getenv("PMCD_PORT")) != NULL) {
	    port = strtol(env, &end, 0);
	    if (*end != '\0' || port < 0) {
		__pmNotifyErr(LOG_WARNING,
			      "__pmProbeDiscoverServices: ignored bad PMCD_PORT = '%s'\n",
			      env);
		port = SERVER_PORT;
	    }
	}
	else
	    port = SERVER_PORT;
    }
    else {
	port = strtol(service, &end, 0);
	if (*end != '\0') {
	    __pmNotifyErr(LOG_ERR,
			  "__pmProbeDiscoverServices: service '%s; is not valid",
			  service);
	    return 0;
	}
    }
    __pmSockAddrSetPort(netAddress, port);

    /*
     * We have a network address, a subnet mask size and a port. Iterate over
     * the addresses in the subnet, trying to connect.
     * Each connection attempt could be on its own thread, so use synchronization
     * controls.
     */
    LOCK_INIT(&lock);
    SEM_INIT(&threadsAvailable, maxThreads);
    SEM_INIT(&threadsComplete, 0);

    /* We don't need much stack for our worker threads. */
    ATTR_INIT(&attr);
    ATTR_SET_STACKSIZE(&attr);

    /*
     * Since the connection attempts may be on separate threads,
     * we need a counter to be incremented by each thread as they end so that
     * we know when all of the threads have ended.
     * IPv6 has 128 bits, so it is possible that the number of addresses could
     * exceed the range of the native integer types or of a semaphore. However,
     * a __pmSockAddr can be used for this purpose, since it can represent all
     * of the addresses to be probed. Use the subnet iteration API to initialize
     * and increment this counter. When the last thread has been processed, this
     * pointer will be freed by the API and become NULL.
     *
     * Note that this is separate from the iteration over addresses which is done
     * in the loop below for the purpose of launching connection attempts.
     */
    addressesProcessed = __pmSockAddrFirstSubnetAddr(netAddress, maskBits);

    /* Dispatch the connection attempts. */
    prevNumUrls = numUrls;
    for (address = __pmSockAddrFirstSubnetAddr(netAddress, maskBits);
	 address != NULL;
	 address = __pmSockAddrNextSubnetAddr(address, maskBits)) {
	dispatchConnection(service, address, port, &numUrls, urls);
    }

    /* Wait for all the connection attempts to finish. */
    SEM_WAIT(&threadsComplete);

    /* These must not be destroyed until all of the threads have finished. */
    ATTR_TERM(&attr);
    SEM_TERM(&threadsAvailable);
    SEM_TERM(&threadsComplete);
    LOCK_TERM(&lock);

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
 *   maxThreads=<integer>  -- specifies a hard limit on the number of active
 *                            threads.
 */
static int
parseOptions(const char *mechanism)
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
    long		maxThreads;

    /* Nothing to probe? */
    if (mechanism == NULL)
	return -1;

    /* First extract the subnet argument, parse it and check it. */
    addressString = strchr(mechanism, '=');
    if (addressString == NULL || addressString[1] == '\0') {
	__pmNotifyErr(LOG_ERR,
		      "__pmProbeDiscoverServices: No argument provided");
	return -1;
    }
    ++addressString;
    maskString = strchr(addressString, '/');
    if (maskString == NULL || maskString[1] == '\0') {
	__pmNotifyErr(LOG_ERR,
		      "__pmProbeDiscoverServices: No subnet mask provided");
	return -1;
    }
    ++maskString;

    /* Convert the address string to a socket address. */
    len = maskString - addressString; /* enough space for the nul */
    buf = malloc(len);
    memcpy(buf, addressString, len - 1);
    buf[len - 1] = '\0';
    netAddress = __pmStringToSockAddr(buf);
    if (netAddress == NULL) {
	__pmNotifyErr(LOG_ERR,
		      "__pmProbeDiscoverServices: Address '%s' is not valid",
		      buf);
	free(buf);
	return -1;
    }
    free(buf);

    /* Convert the mask string to an integer */
    maskBits = strtol(maskString, &end, 0);
    if (*end != '\0' && *end != ',') {
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscoverServices: Subnet mask '%s' is not valid",
		      maskString);
	return -1;
    }

    /* Check the number of bits in the mask against the address family. */
    if (maskBits < 0) {
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscoverServices: Inet subnet mask must be >= 0 bits");
	return -1;
    }
    family = __pmSockAddrGetFamily(netAddress);
    switch (family) {
    case AF_INET:
	if (maskBits > 32) {
	    __pmNotifyErr(LOG_ERR,
			  "__pmProbeDiscoverServices: Inet subnet mask must be <= 32 bits");
	    return -1;
	}
	break;
    case AF_INET6:
	if (maskBits > 128) {
	    __pmNotifyErr(LOG_ERR,
			  "__pmProbeDiscoverServices: Inet subnet mask must be <= 128 bits");
	    return -1;
	}
	break;
    default:
	__pmNotifyErr(LOG_ERR,
		      "__pmProbeDiscoverServices: Unsupported address family, %d",
		      family);
	return -1;
    }

    /* Parse any remaining options. */
    option = end;
    if (*option == '\0')
	return 0; /* no options */

    sts = 0;
    do {
	/*
	 * All additional options begin with a separating comma.
	 * Make sure something has been specified.
	 */
	++option;
	if (*option == '\0') {
	    __pmNotifyErr(LOG_ERR,
			  "__pmProbeDiscoverServices: Missing option after ','");
	    return -1;
	}
	
	/* Examine the option. */
	if (strncmp(option, "maxThreads=", 11) == 0) {
	    option += sizeof("maxThreads");
	    longVal = strtol(option, &end, 0);
	    if (*end != '\0' && *end != ',') {
		__pmNotifyErr(LOG_ERR,
			      "__pmProbeDiscoverServices: maxThreads value '%s' is not valid",
			      option);
		sts = -1;
	    }
	    else {
		option = end;
		/*
		 * Make sure the value is positive. We will set
		 * a hard limit of FD_SETSIZE, since that's the maximum number
		 * of open fds that __pmFD_*() can cope with.
		 */
		maxThreads = SEM_VALUE_MAX;
		if (maxThreads > FD_SETSIZE)
		    maxThreads = FD_SETSIZE;
		if (longVal > maxThreads) {
		    __pmNotifyErr(LOG_ERR,
				  "__pmProbeDiscoverServices: maxThreads value %ld must not exceed %ld",
				  longVal, maxThreads);
		    sts = -1;
		}
		else if (longVal <= 0) {
		    __pmNotifyErr(LOG_ERR,
				  "__pmProbeDiscoverServices: maxThreads value %ld must be positive",
				  longVal);
		    sts = -1;
		}
		else {
#if IS_MULTI_THREAD
		    maxThreads = longVal;
#else
		    __pmNotifyErr(LOG_WARNING,
				  "__pmProbeDiscoverServices: no thread support. Ignoring maxThreads value %ld",
				  longVal);
#endif
		}
	    }
	}
	else {
	    /* An invalid option. Skip it. */
	    __pmNotifyErr(LOG_ERR,
			  "__pmProbeDiscoverServices: option '%s' is not valid",
			  option);
	    sts = -1;
	    for (++option; *option != '\0' && *option != ','; ++option)
		;
	}
    } while (*option != '\0');

    return sts;
}

int
__pmProbeDiscoverServices(const char *service, const char *mechanism, int numUrls, char ***urls)
{
    int	sts;

    /* Interpret the mechanism string. */
    sts = parseOptions(mechanism);
    if (sts != 0)
	return 0;

    /* Everything checks out. Now do the actual probing. */
    numUrls = probeForServices(service, netAddress, maskBits, numUrls, urls);

    /* Clean up */
    __pmSockAddrFree(netAddress);

    return numUrls;
}
