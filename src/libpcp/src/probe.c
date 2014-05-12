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

/*
 * Service discovery by active probing. The given subnet is probed for the requested
 * service(s).
 */
static unsigned addressesRemaining;

#if PM_MULTI_THREAD
/*
 * Multi thread support. We need to protect the updating of the url list and the
 * unfinished thread counter.
 */
#include <semaphore.h>
static __pmMutex lock;
static sem_t threadsComplete;
#define LOCK_INIT(lock) pthread_mutex_init(lock, NULL)
#define LOCK_LOCK(lock) PM_LOCK(*(lock))
#define LOCK_UNLOCK(lock) PM_UNLOCK((*lock))
#define LOCK_TERM(lock) pthread_mutex_destroy(lock)
#define SEM_INIT(sem, value) sem_init(sem, 0, value)
#define SEM_WAIT(sem) sem_wait(sem)
#define SEM_POST(sem) sem_post(sem)
#define SEM_TERM(sem) sem_destroy(sem)
#define THREAD_START(thread, func, arg) (pthread_create(thread, NULL, func, arg))
#define THREAD_DETACH() (pthread_detach(pthread_self()))
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
#define THREAD_START(thread, func, arg) (func(arg), 0)
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
cleanupThread (void)
{
    int	isFinalThread;
    /*
     * Indicate that this attempt is complete.
     * Post the threadsComplete semaphore, if this was the final thread.
     *
     * CAUTION: Posting the semaphore will release the main thread which
     * destroys the lock. Therefore make sure we're done with the lock before posting
     * the semaphore.
     */
    LOCK_LOCK(&lock);
    --addressesRemaining;
    isFinalThread = (addressesRemaining == 0);
    LOCK_UNLOCK(&lock);
    if (isFinalThread)
	SEM_POST(&threadsComplete);
}

/* Attempt a connection on the given address and port. Return 0, if successful. */
static void *
attemptConnection (void *arg)
{
    int			s;
    int			flags;
    int			sts;
    struct timeval	canwait = { 1, 000000 };
    struct timeval	stv;
    struct timeval	*pstv;
    __pmFdSet		wfds;
    __pmServiceInfo	serviceInfo;
    int			attempt;
    connectionContext *context = arg;
	
    /* If we are on our own thread, then run detached. */
    THREAD_DETACH();

    /*
     * Create a socket. There may be a limit on open fds. If we get EAGAIN, then
     * wait 0.1 seconds and try again.  We must have a limit in case something goes wrong.
     * Make it 5 seconds (50 * 100,000 usecs).
     */
    for (attempt = 0; attempt < 50; ++attempt) {
	if (__pmSockAddrIsInet(context->address))
	    s = __pmCreateSocket();
	else /* address family already checked */
	    s = __pmCreateIPv6Socket();
	if (s != -EAGAIN)
	    break;
	usleep(100000);
    }
    if (pmDebug & DBG_TRACE_DISCOVERY) {
	if (attempt > 0) {
	    __pmNotifyErr(LOG_INFO, "Waited for %d attempts for an available fd\n",
			  attempt);
	}
    }

    if (s < 0) {
	char *addrString = __pmSockAddrToString(context->address);
	__pmNotifyErr(LOG_WARNING, "__pmProbeDiscoverServices: Unable to create socket for address, %s",
		      addrString);
	free(addrString);
	goto done;
    }

    /* Attempt to connect. If flags comaes back as less than zero, then the
       socket has already been closed by __pmConnectTo(). */
    sts = -1;
    flags = __pmConnectTo(s, context->address, context->port);
    if (flags >= 0) {
	/* FNDELAY and we're in progress - wait on select */
	stv = canwait;
	pstv = (stv.tv_sec || stv.tv_usec) ? &stv : NULL;
	__pmFD_ZERO(&wfds);
	__pmFD_SET(s, &wfds);
	sts = __pmSelectWrite(s+1, &wfds, pstv);

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
	*context->numUrls = __pmAddDiscoveredService(&serviceInfo, *context->numUrls, context->urls);
	LOCK_UNLOCK(&lock);
    }

 done:
    /*
     * 
     * The context and its address were allocated by the thread dispatcher.
     * Free them now.
     */
    __pmSockAddrFree(context->address);
    free(context);
    cleanupThread ();

    return NULL;
}

/* Dispatch a connection attempt, on its own thread, if supported. */
static void
dispatchConnection (
    const char *service,
    const __pmSockAddr *address,
    int port,
    int *numUrls,
    char ***urls
)
{
#if PM_MULTI_THREAD
    pthread_t thread;
#endif
    connectionContext *context;
    int rc;
    int attempt;

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
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscoverServices: unable to copy socket address %s",
		      addrString);
	free(addrString);
	cleanupThread();
	return;
    }

    /*
     * Attempt the connection. Since we're not passing in attributes for the (possible)
     * new thread, the only error that can occur is EAGAIN. Sleep for 0.1 seconds
     * before trying again. We must have a limit in case something goes wrong. Make it
     * 5 seconds (50 * 100,000 usecs).
     */
    for (attempt = 0; attempt < 50; ++attempt) {
        rc = THREAD_START(&thread, attemptConnection, context);
	if (rc != EAGAIN)
	    break;
	usleep(100000);
    }

    /* Check to see that the thread started. */
    if (rc != 0) {
	char *addrString = __pmSockAddrToString(address);
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscoverServices: unable to start a thread to probe address %s",
		      addrString);
	free(addrString);
	cleanupThread();
    }
    else if (pmDebug & DBG_TRACE_DISCOVERY) {
	if (attempt > 0) {
	    __pmNotifyErr(LOG_INFO, "Waited for %d attempts to create a thread\n",
			  attempt);
	}
    }
}

static unsigned
subnetSize (const __pmSockAddr *addr, int maskBits)
{
    int family;
    int netBits;
    /*
     * The address family has already been checked, so we need only consider
     * inet and ipv6. Also, the size of the mask has also been checked so that
     * the size of the subnet will be reasonable. i.e. the arithmetic below will
     * not overflow.
     */
    family = __pmSockAddrGetFamily(addr);
    if (family == AF_INET)
	netBits = 32;
    else
	netBits = 128;

    return 1 << (netBits - maskBits);
}

static int
probeForServices (
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
	    __pmNotifyErr(LOG_ERR, "__pmProbeDiscoverServices: service '%s; is not valid",
			  service);
	    return 0;
	}
    }
    __pmSockAddrSetPort(netAddress, port);

    /*
     * We have a network address, a subnet and a port. Iterate over the addresses in the subnet,
     * trying to connect.
     * Each connection attempt could be on its own thread, so use synchronization
     * controls.
     */

    LOCK_INIT(&lock);
    SEM_INIT(&threadsComplete, 0);

    addressesRemaining = subnetSize(netAddress, maskBits);
    prevNumUrls = numUrls;
    for (address = __pmSockAddrFirstSubnetAddr(netAddress, maskBits);
	 address != NULL;
	 address = __pmSockAddrNextSubnetAddr(address, maskBits)) {
	dispatchConnection (service, address, port, &numUrls, urls);
    }

    /* Wait for all the connection attempts to finish. */
    SEM_WAIT(&threadsComplete);
    SEM_TERM(&threadsComplete);

    /* The lock must not be destroyed until all of the threads have finished. */
    LOCK_TERM(&lock);

    /* Return the number of new urls. */
    return numUrls - prevNumUrls;
}

int
__pmProbeDiscoverServices(const char *service, const char *mechanism, int numUrls, char ***urls)
{
    const char		*addressString;
    const char		*maskString;
    size_t		len;
    char		*buf;
    char		*end;
    __pmSockAddr	*netAddress;
    int			maskBits;
    int			family;

    /* Nothing to probe? */
    if (mechanism == NULL)
	return 0;

    /* First extract the subnet argument and parse it. */
    addressString = strchr(mechanism, '=');
    if (addressString == NULL || addressString[1] == '\0') {
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscoverServices: No argument provided");
	return 0;
    }
    ++addressString;
    maskString = strchr(mechanism, '/');
    if (maskString == NULL || maskString[1] == '\0') {
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscoverServices: No subnet mask provided");
	return 0;
    }
    ++maskString;

    /* Convert the address string to a socket address. */
    len = maskString - addressString; /* enough space for the nul */
    buf = malloc(len);
    memcpy(buf, addressString, len - 1);
    buf[len - 1] = '\0';
    netAddress = __pmStringToSockAddr(buf);
    if (netAddress == NULL) {
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscoverServices: Address '%s' is not valid",
		      buf);
	free(buf);
	return 0;
    }
    free(buf);

    /* Convert the mask string to an integer */
    maskBits = strtol(maskString, &end, 0);
    if (*end != '\0') {
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscoverServices: Subnet mask '%s' is not valid",
		      maskString);
	return 0;
    }

    /* Check the number of bits in the mask against the address family. */
    if (maskBits < 0) {
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscoverServices: Inet subnet mask must be >= 0 bits");
	return 0;
    }
    family = __pmSockAddrGetFamily(netAddress);
    switch (family) {
    case AF_INET:
	if (maskBits > 32) {
	    __pmNotifyErr(LOG_ERR, "__pmProbeDiscoverServices: Inet subnet mask must be <= 32 bits");
	    return 0;
	}
	break;
    case AF_INET6:
	if (maskBits > 128) {
	    __pmNotifyErr(LOG_ERR, "__pmProbeDiscoverServices: Inet subnet mask must be <= 128 bits");
	    return 0;
	}
	break;
    default:
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscoverServices: Unsupported address family, %d",
		      family);
	return 0;
    }

    /* Everything checks out. Now do the actual probing. */
    numUrls = probeForServices (service, netAddress, maskBits, numUrls, urls);

    __pmSockAddrFree(netAddress);

    return numUrls;
}
