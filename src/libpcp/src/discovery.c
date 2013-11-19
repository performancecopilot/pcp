/*
 * Copyright (c) 2013 Red Hat.
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
#include "avahi.h"

#if defined(HAVE_SERVICE_DISCOVERY)

int __pmDiscoverServices(char ***urls, const char *service,
			 const char *discovery_domain)
{
    int sts;
    /*
     * Attempt to discover the requested service(s) using the requested/available means.
     * If a particular method is not available or not configured, then the
     * respective call will have no effect.
     *
     * Currently, only Avahi is supported, so ignore the discovery_domain
     */
    (void)discovery_domain;
    *urls = NULL;
    sts = __pmAvahiDiscoverServices(urls, service);
    return sts;
}

#else /* !HAVE_SERVICE_DISCOVERY */

int __pmDiscoverServices(char ***urls, const char *service,
			 const char *discovery_domain)
{
    /* No services to discover. */
    (void)service;
    (void)discovery_domain;
    *urls = NULL;
    return 0;
}

#endif /* !HAVE_SERVICE_DISCOVERY */

/* For manually adding a service. Also used by __pmDiscoverServices(). */
void __pmAddDiscoveredService(char ***urls, __pmServiceInfo *info)
{
    const char* prefix;
    char *addressString;
    char *url;
    size_t next;
    size_t size;
    int isIPv6;
    int port;
    /*
     * Add the given address and port to the given list of urls.
     * Build the new entry first, so that we can filter out duplicates.
     * Currently, only "pmcd" is supported.
     */
    if (strcmp(info->spec, SERVER_SERVICE_SPEC) == 0) {
	prefix = "pcp://";
    }
    else {
	__pmNotifyErr(LOG_ERR,
		      "__pmAddDiscoveredService: Unsupported service: '%s'",
		      info->spec);
	return;
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
     */
    if (*urls == NULL)
	next = 0; /* no list to search */
    else {
	for (next = 0; (*urls)[next] != NULL; ++next) {
	    if (strcmp(url, (*urls)[next]) == 0) {
		/* Found a duplicate. */
		free(url);
		return;
	    }
	}
    }

    /*
     * It's not a duplicate, so add it to the end of the list.
     * We grow the list each time, since it is likely to be
     * small.
     */
    size = (next + 2) * sizeof(**urls);
    *urls = realloc(*urls, size);
    if (*urls == NULL) {
	__pmNoMem("__pmAddDiscoveredService: can't allocate service table",
		  size, PM_FATAL_ERR);
    }
    (*urls)[next] = url;

    /* Terminate the list. */
    (*urls)[next + 1] = NULL;
}

/* For freeing a service list. */
void __pmServiceListFree(char **urls)
{
    char **p;
    /* The list may be completely empty. */
    if (urls == NULL)
	return;

    /* Each entry and the entire list, must be freed. */
    for (p = urls; *p != NULL; ++p)
	free(*p);
    free(urls);
}

__pmServiceInfo *__pmServiceInfoAlloc()
{
    return malloc(sizeof(__pmServiceInfo));
}

void __pmServiceInfoFree(__pmServiceInfo *info)
{
    free(info);
}
