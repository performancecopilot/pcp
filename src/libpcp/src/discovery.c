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
#include <time.h>
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
int
pmDiscoverServices(const char *service,
		   const char *mechanism,
		   char ***urls)
{
    return pmDiscoverServicesWithOptions(service, mechanism, NULL, urls);
}

static void
destroyTimer(timer_t timerid)
{
    timer_delete(timerid);
}

static int
setTimer(const struct timespec *timeout, timer_t *timerid)
{
    struct itimerspec	timer;
    int			attempt;
    int			sts;

    /*
     * First, create the timer. Specifying NULL as the second argument creates
     * a timer which will generate SIGALRM when ti expires.
     */
    for (attempt = 0; attempt < 1000; ++attempt) {
	sts = timer_create(CLOCK_REALTIME, NULL, timerid);
	if (sts == 0)
	    break;
	if (errno != EAGAIN)
	    return -errno;
    }
    /*
     * Have all the attempts failed? If so, it's due to lack of memory,
     * according to timer_create(2).
     */
    if (sts != 0)
	return -ENOMEM;

    /* Now set the timer so that it expires once. */
    memset(&timer.it_interval, 0, sizeof(timer.it_interval));
    timer.it_value = *timeout;
    sts = timer_settime(*timerid, 0, &timer, NULL);

    if (sts < 0) {
	destroyTimer(*timerid);
	return -errno;
    }

    return 0; /* success! */
}

int
pmDiscoverServicesWithOptions(const char *service,
			      const char *mechanism,
			      pmDiscoveryOptions *options,
			      char ***urls)
{
    timer_t	timerid;
    int		numUrls;
    int		sts;

    /*
     * Make sure that the caller is not expecting a newer API version than ours.
     */
    if (options->version > PM_DISCOVERY_OPTIONS_VERSION)
	return -EOPNOTSUPP;

    /*
     * If a timeout has been specified, then arm a timer to generate SIGALRM
     * when the requested time expires
     */
    if (options->timeout.tv_sec || options->timeout.tv_nsec) {
	sts = setTimer(&options->timeout, &timerid);
	if (sts < 0)
	    return sts;
    }

    /*
     * Attempt to discover the requested service(s) using the requested or
     * all available means.
     * If a particular method is not available or not configured, then the
     * respective call will have no effect.
     */
    *urls = NULL;
    numUrls = 0;
    if (mechanism == NULL) {
	numUrls += __pmAvahiDiscoverServices(service, mechanism, options, numUrls, urls);
	if (! options->interrupted)
	    numUrls += __pmProbeDiscoverServices(service, mechanism, options, numUrls, urls);
    }
    else if (strncmp(mechanism, "avahi", 5) == 0)
	numUrls += __pmAvahiDiscoverServices(service, mechanism, options, numUrls, urls);
    else if (strncmp(mechanism, "probe", 5) == 0)
	numUrls += __pmProbeDiscoverServices(service, mechanism, options, numUrls, urls);
    else
	numUrls = -EOPNOTSUPP;

    if (options->timeout.tv_sec || options->timeout.tv_nsec)
	destroyTimer(timerid);

    return numUrls;
}

/* For manually adding a service. Also used by pmDiscoverServices(). */
int
__pmAddDiscoveredService(__pmServiceInfo *info,
			 const pmDiscoveryOptions *options,
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
    if (options->resolve)
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
