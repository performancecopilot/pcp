/*
 * Copyright (c) 2013-2014 Red Hat.
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
#include "avahi.h"
#include "probe.h"

/*
 * Advertise the given service using all available means. The implementation
 * must support adding and removing individual service specs on the fly.
 * e.g. "pmcd" on port 1234
 */
__pmServerPresence *
__pmServerAdvertisePresence(const char *serviceSpec, int port)
{
    __pmServerPresence *s;

    /* Allocate a server presence and copy the given data. */
    if ((s = malloc(sizeof(*s))) == NULL) {
	__pmNoMem("__pmServerAdvertisePresence: can't allocate __pmServerPresence",
		  sizeof(*s), PM_FATAL_ERR);
    }
    s->serviceSpec = strdup(serviceSpec);
    if (s->serviceSpec == NULL) {
	__pmNoMem("__pmServerAdvertisePresence: can't allocate service spec",
		  strlen(serviceSpec) + 1, PM_FATAL_ERR);
    }
    s->port = port;

    /* Now advertise our presence using all available means. If a particular
     * method is not available or not configured, then the respective call
     * will have no effect. Currently, only Avahi is supported.
     */
    __pmServerAvahiAdvertisePresence(s);
    return s;
}

/*
 * Unadvertise the given service using all available means. The implementation
 * must support removing individual service specs on the fly.
 * e.g. "pmcd" on port 1234
 */
void
__pmServerUnadvertisePresence(__pmServerPresence *s)
{
    /* Unadvertise our presence for all available means. If a particular
     * method is not active, then the respective call will have no effect.
     */
    __pmServerAvahiUnadvertisePresence(s);
    free(s->serviceSpec);
    free(s);
}

/*
 * Service discovery API entry points.
 */
char *
__pmServiceDiscoveryParseTimeout (const char *s, struct timeval *timeout)
{
    double seconds;
    char *end;

    /*
     * The string is a floating point number representing the number of seconds
     * to wait. Possibly followed by a comma, to separate the next option.
     */
    seconds = strtod(s, &end);
    if (*end != '\0' && *end != ',') {
	__pmNotifyErr(LOG_ERR, "the timeout argument '%s' is not valid", s);
	return strchrnul(s, ',');
    }

    /* Set the specified timeout. */
    __pmtimevalFromReal(seconds, timeout);

    return end;
}

static int
parseOptions(const char *optionsString, __pmServiceDiscoveryOptions *options)
{
    if (optionsString == NULL)
	return 0; /* no options to parse */

    /* Now interpret the options string. */
    while (*optionsString != '\0') {
	if (strncmp(optionsString, "resolve", sizeof("resolve") - 1) == 0)
	    options->resolve = 1;
	else if (strncmp(optionsString, "timeout=", sizeof("timeout=") - 1) == 0) {
#if ! PM_MULTI_THREAD
	    __pmNotifyErr(LOG_ERR, "__pmDiscoverServicesWithOptions: Service discovery global timeout is not supported");
	    return -EOPNOTSUPP;
#else
	    optionsString += sizeof("timeout=") - 1;
	    optionsString = __pmServiceDiscoveryParseTimeout(optionsString,
							     &options->timeout);
#endif
	}
	else {
	    __pmNotifyErr(LOG_ERR, "__pmDiscoverServicesWithOptions: unrecognized option at '%s'", optionsString);
	    return -EINVAL;
	}
	/* Locate the start of the next option. */
	optionsString = strchrnul(optionsString, ',');
    }

    return 0; /* ok */
}

#if PM_MULTI_THREAD
static void *
timeoutSleep(void *arg)
{
    __pmServiceDiscoveryOptions *options = arg;
    int old;

    /*
     * Make sure that this thread is cancellable.
     * We don't need the previous state, but pthread_setcancelstate(3) says that
     * passing in NULL as the second argument is not portable.
     */
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old);    

    /*
     * Sleep for the specified amount of time. Our thread will either be
     * cancelled by the calling thread or we will wake up on our own.
     */
    __pmtimevalSleep(options->timeout);

    /*
     * Service discovery has timed out. It's ok to set this unconditionally
     * since the object exists in the calling thread's memory space and it
     * waits to join with our thread before finishing.
     */
    options->timedOut = 1;
    return NULL;
}
#endif

int
pmDiscoverServices(const char *service,
		   const char *mechanism,
		   char ***urls)
{
    return __pmDiscoverServicesWithOptions(service, mechanism, NULL, NULL, urls);
}

int
__pmDiscoverServicesWithOptions(const char *service,
				const char *mechanism,
				const char *optionsString,
				const volatile sig_atomic_t *flags,
				char ***urls)
{
    __pmServiceDiscoveryOptions	options;
    int				numUrls;
    int				sts;
#if PM_MULTI_THREAD
    pthread_t			timeoutThread;
    pthread_attr_t		threadAttr;
    int				timeoutSet = 0;
#endif

    /* Interpret the options string. Initialize first. */
    memset(&options, 0, sizeof(options));
    sts = parseOptions(optionsString, &options);
    if (sts < 0)
	return sts;
    options.flags = flags;

#if PM_MULTI_THREAD
    /*
     * If a global timeout has been specified, then start a thread which will
     * sleep for the specified length of time. When it wakes up, it will
     * interrupt the discovery process. If discovery finishes before the
     * timeout period, then the thread will be cancelled.
     * We want the thread to be joinable.
     */
    if (options.timeout.tv_sec || options.timeout.tv_usec) {
	pthread_attr_init(&threadAttr);
	pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_JOINABLE);
	sts = pthread_create(&timeoutThread, &threadAttr,
			     timeoutSleep, &options);
	pthread_attr_destroy(&threadAttr);
	if (sts != 0) {
	    sts = oserror();
	    __pmNotifyErr(LOG_ERR, "Service discovery global timeout could not be set: %s",
			  strerror(sts));
	    return -sts;
	}
	timeoutSet = 1;
    }
#endif

    /*
     * Attempt to discover the requested service(s) using the requested or
     * all available means.
     * If a particular method is not available or not configured, then the
     * respective call will have no effect.
     */
    *urls = NULL;
    numUrls = 0;
    if (mechanism == NULL) {
	/*
	 * Accumulate discovered services using all available mechanisms.
	 * Ensure that the return value from each mechanism is not an error
	 * code before adding it to numUrls.
	 */
	sts = __pmAvahiDiscoverServices(service, mechanism, &options,
					numUrls, urls);
	if (sts < 0) {
	    numUrls = sts;
	    goto done;
	}
	numUrls += sts;
	if (! flags || (*flags & PM_SERVICE_DISCOVERY_INTERRUPTED) == 0) {
	    sts = __pmProbeDiscoverServices(service, mechanism, &options,
					    numUrls, urls);
	    if (sts < 0) {
		numUrls = sts;
		goto done;
	    }
	    numUrls += sts;
	}
    }
    else if (strncmp(mechanism, "avahi", 5) == 0) {
	numUrls = __pmAvahiDiscoverServices(service, mechanism, &options,
					    numUrls, urls);
    }
    else if (strncmp(mechanism, "probe", 5) == 0) {
	numUrls = __pmProbeDiscoverServices(service, mechanism, &options,
					    numUrls, urls);
    }
    else
	numUrls = -EOPNOTSUPP;

 done:
#if PM_MULTI_THREAD
    if (timeoutSet) {
	/* Cancel the timeout thread and then wait for it to join. */
	pthread_cancel(timeoutThread);
	pthread_join(timeoutThread, NULL);
    }
#endif

    return numUrls;
}

/* For manually adding a service. Also used by pmDiscoverServices(). */
int
__pmAddDiscoveredService(__pmServiceInfo *info,
			 const __pmServiceDiscoveryOptions *options,
			 int numUrls,
			 char ***urls)
{
    const char *protocol = info->protocol;
    char *host = NULL;
    char *url;
    size_t size;
    int isIPv6;
    int port;

    /* If address resolution was requested, then do attempt it. */
    if (options->resolve ||
	(options->flags && (*options->flags & PM_SERVICE_DISCOVERY_RESOLVE) != 0))
	host = __pmGetNameInfo(info->address);

    /*
     * If address resolution was not requested, or if it failed, then
     * just use the address.
     */
    if (host == NULL) {
	host = __pmSockAddrToString(info->address);
	if (host == NULL) {
	    __pmNoMem("__pmAddDiscoveredService: can't allocate host buffer",
		      0, PM_FATAL_ERR);
	}
    }

    /*
     * Allocate the new entry. We need room for the URL prefix, the
     * address/host and the port. IPv6 addresses require a set of []
     * surrounding the address in order to distinguish the port.
     */
    port = __pmSockAddrGetPort(info->address);
    size = strlen(protocol) + sizeof("://");
    size += strlen(host) + sizeof(":65535");
    if ((isIPv6 = (strchr(host, ':') != NULL)))
	size += 2;
    url = malloc(size);
    if (url == NULL) {
	__pmNoMem("__pmAddDiscoveredService: can't allocate new entry",
		  size, PM_FATAL_ERR);
    }
    if (isIPv6)
	snprintf(url, size, "%s://[%s]:%u", protocol, host, port);
    else
	snprintf(url, size, "%s://%s:%u", protocol, host, port);
    free(host);

    /*
     * Now search the current list for the new entry.
     * Add it if not found. We don't want any duplicates.
     */
    if (__pmStringListFind(url, numUrls, *urls) == NULL)
	numUrls = __pmStringListAdd(url, numUrls, urls);

    free(url);
    return numUrls;
}
