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
 * Service discovery by active probing. The given subnet is probed for the requsted
 * service(s).
 */
int
__pmProbeDiscoverServices(const char *service, const char *mechanism, int numUrls, char ***urls)
{
    const char		*addressString;
    const char		*maskString;
    size_t		len;
    char		*buf;
    char		*end;
    __pmSockAddr	*address;
    int			mask;
    int			family;

    /* First extract the subnet argument and parse it. */
    addressString = strchr(mechanism, '=');
    if (addressString == NULL || addressString[1] == '\0') {
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscovereServices: No argument provided");
	return 0;
    }
    ++addressString;
    maskString = strchr(mechanism, '/');
    if (maskString == NULL || maskString[1] == '\0') {
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscovereServices: No subnet mask provided");
	return 0;
    }
    ++maskString;

    /* Convert the address string to a socket address. */
    len = maskString - addressString; /* enough space for the nul */
    buf = malloc(len);
    memcpy(buf, addressString, len - 1);
    buf[len - 1] = '\0';
    address = __pmStringToSockAddr(buf);
    free(buf);
    if (address == NULL) {
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscovereServices: Address %s is not valid",
		      addressString);
	return 0;
    }

    /* Convert the mask string to an integer */
    mask = strtol(maskString, &end, 0);
    if (*end != '\0') {
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscovereServices: Subnet mask %s is not valid",
		      maskString);
	return 0;
    }

    /* Check the number of bits in the mask against the address family. */
    family = __pmSockAddrGetFamily(address);
    switch (family) {
    case AF_INET:
	if (mask > 32) {
	    __pmNotifyErr(LOG_ERR, "__pmProbeDiscovereServices: Inet subnet mask must be <= 32 bits");
	    return 0;
	}
	break;
    case AF_INET6:
	if (mask > 128) {
	    __pmNotifyErr(LOG_ERR, "__pmProbeDiscovereServices: Inet subnet mask must be <= 128 bits");
	    return 0;
	}
	break;
    default:
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscovereServices: Unsupported address family %d",
		      family);
	return 0;
    }

    __pmSockAddrFree(address);
    return 0;
}
