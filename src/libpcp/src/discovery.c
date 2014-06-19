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

static __pmDiscoveryGlobalOptions	globalOptionsInfo;
static int				interrupted;

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

/* Parse the global options. */
static int
parseGlobalOptions(const char *globalOptions)
{
    const char	*end;
    int		len;
    int		sts = 0;

    /* Clear any results from a previous use of this API. */
    memset(&globalOptionsInfo, 0, sizeof(globalOptionsInfo));
	   
    /* No options to parse? */
    if (globalOptions == NULL)
	return sts;

    /*
     * Parse the options. An empty options string is ok as is an extra comma
     * at the end.
     */
    while (*globalOptions) {
	end = strchrnul(globalOptions, ',');
	len = end - globalOptions;

	if (strncmp(globalOptions, "resolve", len) == 0)
	    globalOptionsInfo.resolve = 1;
	else {
	    __pmNotifyErr(LOG_ERR,
			  "pmDiscoverServices: unsupported option '%.*s'",
			  len, globalOptions);
	    sts = -EOPNOTSUPP;
	}

	/* Advance the options pointer. */
	globalOptions = end;
	if (*globalOptions == ',')
	    ++globalOptions;
    }

    return sts;
}

/*
 * Service discovery API entry point.
 */
int
pmDiscoverServices(const char *service,
		   const char *mechanism,
		   const char *globalOptions,
		   char ***urls)
{
    int numUrls;
    int sts;

    /* Parse the global options. */
    sts = parseGlobalOptions(globalOptions);
    if (sts < 0)
	return sts;

    /*
     * Attempt to discover the requested service(s) using the requested or
     * all available means.
     * If a particular method is not available or not configured, then the
     * respective call will have no effect.
     */
    interrupted = 0;
    *urls = NULL;
    numUrls = 0;
    if (mechanism == NULL) {
	numUrls += __pmAvahiDiscoverServices(service, mechanism, numUrls, urls);
	if (! interrupted)
	    numUrls += __pmProbeDiscoverServices(service, mechanism, numUrls, urls);
    }
    else if (strncmp(mechanism, "avahi", 5) == 0)
	numUrls += __pmAvahiDiscoverServices(service, mechanism, numUrls, urls);
    else if (strncmp(mechanism, "probe", 5) == 0)
	numUrls += __pmProbeDiscoverServices(service, mechanism, numUrls, urls);
    else
	return -EOPNOTSUPP;

    return numUrls;
}

/*
 * Service discovery interrupt handling.
 */
void
pmServiceDiscoveryInterrupt(int sig)
{
    /*
     * Interrupt any and all discovery mechanisms. If a given mechanism is not
     * active, then that interrupt will have no effect.
     */
    __pmAvahiServiceDiscoveryInterrupt(sig);
    __pmProbeServiceDiscoveryInterrupt(sig);
}

/* For manually adding a service. Also used by pmDiscoverServices(). */
int
__pmAddDiscoveredService(__pmServiceInfo *info, int numUrls, char ***urls)
{
    const char *protocol = info->protocol;
    char *host = NULL;
    char *url;
    size_t size;
    int isIPv6;
    int port;

    /* If address resolution was requested, then do attempt it. */
    if (globalOptionsInfo.resolve)
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
	snprintf(url, size, "%s://[%s]:%u", protocol, host, (uint16_t)port);
    else
	snprintf(url, size, "%s://%s:%u", protocol, host, (uint16_t)port);
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
