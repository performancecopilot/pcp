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

#if defined(HAVE_SERVICE_DISCOVERY)

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

int pmDiscoverServices(const char *service,
		       const char *mechanism,
		       char ***urls)
{
    int numUrls;
    /*
     * Attempt to discover the requested service(s) using the requested or
     * all available means.
     * If a particular method is not available or not configured, then the
     * respective call will have no effect.
     */
    *urls = NULL;
    numUrls = 0;
    if (mechanism == NULL || strncmp(mechanism, "avahi", 5) == 0)
	numUrls += __pmAvahiDiscoverServices(service, mechanism, numUrls, urls);
    else
	return -EOPNOTSUPP;

    return numUrls;
}


/* For manually adding a service. Also used by pmDiscoverServices(). */
int
__pmAddDiscoveredService(__pmServiceInfo *info, int numUrls, char ***urls)
{
    const char* prefix;
    char *addressString;
    char *url;
    size_t size;
    int isIPv6;
    int port;
    /*
     * Add the given address and port to the given list of urls.
     * Build the new entry first, so that we can filter out duplicates.
     * Currently, only "pmcd" is supported.
     */
    if (strcmp(info->spec, PM_SERVER_SERVICE_SPEC) == 0) {
	prefix = "pcp://";
    }
    else {
	__pmNotifyErr(LOG_ERR,
		      "__pmAddDiscoveredService: Unsupported service: '%s'",
		      info->spec);
	return EOPNOTSUPP;
    }

    /*
     * Allocate the new entry. We need room for the url prefix, the address
     * and the port. IPv6 addresses require a set of [] surrounding the
     * address in order to distinguish the port.
     */
    port = __pmSockAddrGetPort(info->address);
    addressString = __pmSockAddrToString(info->address);
    if (addressString == NULL) {
	__pmNoMem("__pmAddDiscoveredService: can't allocate address buffer",
		  0, PM_FATAL_ERR);
    }
    size = strlen(prefix) + strlen(addressString) + sizeof(":65535");
    if ((isIPv6 = (__pmSockAddrGetFamily(info->address) == AF_INET6)))
	size += 2;
    url = malloc(size);
    if (url == NULL) {
	__pmNoMem("__pmAddDiscoveredService: can't allocate new entry",
		  size, PM_FATAL_ERR);
    }
    if (isIPv6)
	snprintf(url, size, "%s[%s]:%u", prefix, addressString, (uint16_t)port);
    else
	snprintf(url, size, "%s%s:%u", prefix, addressString, (uint16_t)port);
    free(addressString);

    /*
     * Now search the current list for the new entry.
     * Add it if not found. We don't want any duplicates.
     */
    if (__pmStringListFind(url, numUrls, *urls) == NULL)
	numUrls = __pmStringListAdd(url, numUrls, urls);

    free(url);
    return numUrls;
}

#else /* !HAVE_SERVICE_DISCOVERY */

__pmServerPresence *
__pmServerAdvertisePresence(const char *serviceSpec, int port)
{
    (void)serviceSpec;
    (void)port;
    return NULL;
}

void
__pmServerUnadvertisePresence(__pmServerPresence *s)
{
    (void)s;
}

int pmDiscoverServices(const char *service, const char *mechanism, char ***urls)
{
    /* No services to discover. */
    (void)service;
    (void)mechanism;
    (void)urls;
    return -EOPNOTSUPP;
}

#endif /* !HAVE_SERVICE_DISCOVERY */
