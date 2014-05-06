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
static int
attemptConnection (
    const char *service,
    const __pmSockAddr *address,
    int port,
    int *numUrls,
    char ***urls
)
{
    char *addrString = __pmSockAddrToString(address);
    if (addrString != NULL) {
	printf("Attempting connection with %s:%d\n", addrString, port);
	free(addrString);
    }
    else
	fprintf(stderr, "Bad Address\n");

    return 0;
}

static int
probeForServices (
    const char *service,
    const __pmSockAddr *netAddress,
    int maskBits,
    int numUrls,
    char ***urls
)
{
    char		*end;
    int			port;
    const char		*env;
    __pmSockAddr	*address;
    int			connections;

    /* The service is either a service name (e.g. pmcd) or a port number. */
    if (strcmp(service, "pmcd") == 0) {
	if ((env = getenv("PMCD_PORT")) != NULL) {
	    port = strtol(env, &end, 0);
	    if (*end != '\0' || port < 0) {
		__pmNotifyErr(LOG_WARNING,
			      "__pmProbeDiscovereServices: ignored bad PMCD_PORT = '%s'\n",
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
	    __pmNotifyErr(LOG_ERR, "__pmProbeDiscovereServices: service '%s; is not valid",
			  service);
	    return 0;
	}
    }

    /* We have a network address, a subnet and a port. Iterate over the addresses in the subnet,
       trying to connect. */
    connections = 0;
    for (address = __pmSockAddrFirstSubnetAddr(netAddress, maskBits);
	 address != NULL;
	 address = __pmSockAddrNextSubnetAddr(address, maskBits)) {
	connections += attemptConnection (service, address, port, &numUrls, urls);
    }

    return connections;
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
    netAddress = __pmStringToSockAddr(buf);
    if (netAddress == NULL) {
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscovereServices: Address '%s' is not valid",
		      buf);
	free(buf);
	return 0;
    }
    free(buf);

    /* Convert the mask string to an integer */
    maskBits = strtol(maskString, &end, 0);
    if (*end != '\0') {
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscovereServices: Subnet mask '%s' is not valid",
		      maskString);
	return 0;
    }

    /* Check the number of bits in the mask against the address family. */
    if (maskBits < 0) {
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscovereServices: Inet subnet mask must be >= 0 bits");
	return 0;
    }
    family = __pmSockAddrGetFamily(netAddress);
    switch (family) {
    case AF_INET:
	if (maskBits > 32) {
	    __pmNotifyErr(LOG_ERR, "__pmProbeDiscovereServices: Inet subnet mask must be <= 32 bits");
	    return 0;
	}
	break;
    case AF_INET6:
	if (maskBits > 128) {
	    __pmNotifyErr(LOG_ERR, "__pmProbeDiscovereServices: Inet subnet mask must be <= 128 bits");
	    return 0;
	}
	break;
    default:
	__pmNotifyErr(LOG_ERR, "__pmProbeDiscovereServices: Unsupported address family, %d",
		      family);
	return 0;
    }

    /* Everything checks out. Now do the actual probing. */
    numUrls = probeForServices (service, netAddress, maskBits, numUrls, urls);

    __pmSockAddrFree(netAddress);

    return numUrls;
}
