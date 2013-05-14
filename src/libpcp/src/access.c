/*
 * Copyright (c) 2012-2013 Red Hat.
 * Copyright (c) 1995-2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <limits.h>
#include "pmapi.h"
#include "impl.h"

/* Host access control list */

typedef struct {
    char		*hostspec;	/* Host specification */
    __pmSockAddr	*hostid;	/* Partial host-id to match */
    __pmSockAddr	*hostmask;	/* Mask for wildcarding */
    int			level;		/* Level of wildcarding */
    unsigned int	specOps;	/* Mask of specified operations */
    unsigned int	denyOps;	/* Mask of disallowed operations */
    int			maxcons;	/* Max connections permitted (0 => no limit) */
    int			curcons;	/* Current # connections from matching clients */
} hostinfo;

/* Mask of the operations defined by the user of the routines */

static unsigned int	all_ops;	/* mask of all operations specifiable */

/* This allows the set of valid operations to be specified.
 * Operations must be powers of 2.
 */
int
__pmAccAddOp(unsigned int op)
{
    unsigned int	i, mask;

    if (PM_MULTIPLE_THREADS(PM_SCOPE_ACL))
	return PM_ERR_THREAD;

    /* op must not be zero or clash with existing ops */
    if (op == 0 || (op & all_ops))
	return -EINVAL;

    /* Find the lowest bit in op that is set (WORD_BIT from limits.h is the
     * number of bits in an unsigned int)
     */
    for (i = 0; i < WORD_BIT; i++)
	if (op & (mask = 1 << i))
	    break;

    /* only one bit may be set in op */
    if (op & ~mask)
	return -EINVAL;

    all_ops |= mask;
    return 0;
}

/* Get the host id for this host.  The host id is used when translating
 * references to localhost into the host's real IP address during parsing of
 * the access control section of the config file.  It may also used when
 * checking for incoming connections from localhost.
 */

static int		gotmyhostid;
static __pmHostEnt	*myhostid;
static char		myhostname[MAXHOSTNAMELEN+1];

/*
 * Always called with __pmLock_libpcp already held, so accessing
 * gotmyhostid, myhostname, myhostid and gethostname() call are all 
 * thread-safe.
 */
static int
getmyhostid(void)
{
    if (gethostname(myhostname, MAXHOSTNAMELEN) < 0) {
	__pmNotifyErr(LOG_ERR, "gethostname failure\n");
	return -1;
    }
    myhostname[MAXHOSTNAMELEN-1] = '\0';

    if ((myhostid = __pmGetAddrInfo(myhostname)) == NULL) {
	__pmNotifyErr(LOG_ERR, "__pmGetAddrInfo(%s), %s\n",
		     myhostname, hoststrerror());
	return -1;
    }
    gotmyhostid = 1;
    return 0;
}

/* This is the host access list */

static hostinfo	*hostlist;
static int	nhosts;
static int	szhostlist;

/* Used for saving the current state of the host access list */

static int	saved;
static hostinfo	*oldhostlist;
static int	oldnhosts;
static int	oldszhostlist;

/* Save the current host access list.
 * Returns 0 for success, -1 for error.
 */
int
__pmAccSaveHosts(void)
{
    if (PM_MULTIPLE_THREADS(PM_SCOPE_ACL))
	return PM_ERR_THREAD;
    if (saved)
	return -1;

    saved = 1;
    oldhostlist = hostlist;
    oldnhosts = nhosts;
    oldszhostlist = szhostlist;
    hostlist = NULL;
    nhosts = 0;
    szhostlist = 0;
    return 0;
}

/* Free the current host list.  This is done automatically by
 * __pmAccRestoreHosts so there is no need for it to be made globally visible.
 * A caller of these routines should never need to dispose of the host list
 * once it has been built.
 */
static void
accfreehosts(void)
{
    int		i;
    char	*p;

    if (szhostlist) {
	for (i = 0; i < nhosts; i++)
	    if ((p = hostlist[i].hostspec) != NULL)
		free(p);
	free(hostlist);
    }
    hostlist = NULL;
    nhosts = 0;
    szhostlist = 0;
}

/* Restore the previously saved host list.  Any current host list is freed.
 * Returns 0 for success, -1 for error.
 */
int
__pmAccRestoreHosts(void)
{
    if (PM_MULTIPLE_THREADS(PM_SCOPE_ACL))
	return PM_ERR_THREAD;
    if (!saved)
	return -1;

    accfreehosts();
    saved = 0;
    hostlist = oldhostlist;
    nhosts = oldnhosts;
    szhostlist = oldszhostlist;
    return 0;
}

/* Free the previously saved host list.  This should be called when the saved
 * host list is no longer required (typically because the new one supercedes
 * it).
 */

void
__pmAccFreeSavedHosts(void)
{
    int		i;
    char	*p;

    if (PM_MULTIPLE_THREADS(PM_SCOPE_ACL))
	return;
    if (!saved)
	return;

    if (oldszhostlist) {
	for (i = 0; i < oldnhosts; i++)
	    if ((p = oldhostlist[i].hostspec) != NULL)
		free(p);
	free(oldhostlist);
    }
    saved = 0;
}

/* Build up strings representing the ip address and the mask. Compute the wildcard
   level as we go. */
static int
parseInetWildCard(const char *name, char *ip, char *mask)
{
    int level;
    int ipIx, maskIx;
    int i, n;
    const char *p;

    /* Accept "*" or ".*" as the inet full wild card spec. */
    level = 4;
    ipIx = maskIx = 0;
    p = name;
    if (*p == '.') {
        ++p;
	if (*p != '*') {
	    __pmNotifyErr(LOG_ERR, "Bad IP address wildcard, %s\n", name);
	    return -EINVAL;
	}
    }
    for (/**/; *p && *p != '*' ; p++) {
        n = (int)strtol(p, (char **)&p, 10);
	if ((*p != '.' && *p != '*') || n < 0 || n > 255) {
	    __pmNotifyErr(LOG_ERR, "Bad IP address wildcard, %s\n", name);
	    return -EINVAL;
	}
	if (ipIx != 0) {
	    ipIx += sprintf(ip + ipIx, ".");
	    maskIx += sprintf(mask + maskIx, ".");
	}
	ipIx += sprintf(ip + ipIx, "%d", n);
	maskIx += sprintf(mask + maskIx, "255");
	--level;
	/* Check the wildcard level, 0 is exact match, 4 is most general */
	if (level < 1) {
	    __pmNotifyErr(LOG_ERR, "Too many dots in host pattern \"%s\"\n", name);
	    return -EINVAL;
	}
    }
    /* Add zeroed components for the wildcarded levels. */
    for (i = 0; i < level; ++i) {
        if (ipIx != 0) {
	    ipIx += sprintf(ip + ipIx, ".");
	    maskIx += sprintf(mask + maskIx, ".");
	}
	ipIx += sprintf(ip + ipIx, "0");
	maskIx += sprintf(mask + maskIx, "0");
    }
    return level;
}

static int
parseIPv6WildCard(const char *name, char *ip, char *mask)
{
    int level;
    int ipIx, maskIx;
    int emptyRegion;
    int n;
    const char *p;

    /* Accept ":*" as the IPv6 full wild card spec. Otherwise,
       if the string starts with ':', then the second character must also be a ':'
       which would form a region of zeroes of unspecified length. */
    level = 8;
    emptyRegion = 0;
    ipIx = maskIx = 0;
    p = name;
    if (*p == ':') {
        ++p;
	if (*p != '*') {
	    if (*p != ':') {
		__pmNotifyErr(LOG_ERR, "Bad IP address wildcard, %s\n", name);
		return -EINVAL;
	    }
	    ipIx = sprintf(ip, ":");
	    maskIx = sprintf(mask, ":");
	    /* The second colon will be detected in the loop below. */
	}
    }

    for (/**/; *p && *p != '*' ; p++) {
        /* Check for an empty region. There can only be one. */
        if (*p == ':') {
	    if (emptyRegion) {
	        __pmNotifyErr(LOG_ERR, "Too many empty regions in host pattern \"%s\"\n", name);
		return -EINVAL;
	    }
	    emptyRegion = 1;
	    ipIx += sprintf(ip + ipIx, ":");
	    maskIx += sprintf(mask + maskIx, ":");
	}
	else {
	    n = (int)strtol(p, (char **)&p, 16);
	    if ((*p != ':' && *p != '*') || n < 0 || n > 0xffff) {
	        __pmNotifyErr(LOG_ERR, "Bad IP address wildcard, %s\n", name);
		return -EINVAL;
	    }
	    if (ipIx != 0) {
	        ipIx += sprintf(ip + ipIx, ":");
		maskIx += sprintf(mask + maskIx, ":");
	    }
	    ipIx += sprintf(ip + ipIx, "%x", n);
	    maskIx += sprintf(mask + maskIx, "ffff");
	}
	--level;
	/* Check the wildcard level, 0 is exact match, 8 is most general */
	if (level < 1) {
	    __pmNotifyErr(LOG_ERR, "Too many colons in host pattern \"%s\"\n", name);
	    return -EINVAL;
	}
    }
    /* Add zeroed components for the wildcarded levels.
       If the entire address is wildcarded then return the zero address. */
    if (level == 8 || (level == 7 && emptyRegion)) {
        /* ":*" or "::*" */
        strcpy(ip, "::");
        strcpy(mask, "::");
	level = 8;
    }
    else if (emptyRegion) {
        /* If there was an empty region, then we assume that the wildcard represents the final
	   segment of the spec only. */
        sprintf(ip + ipIx, ":0");
	sprintf(mask + maskIx, ":0");
    }
    else {
        /* no empty region, so use one to finish off the address and the mask */
        sprintf(ip + ipIx, "::");
	sprintf(mask + maskIx, "::");
    }
    return level;
}

static int
parseWildCard(const char *name, char *ip, char *mask)
{
    /* Names containing ':' are IPv6. The IPv6 full wildcard spec is ":*". */
    if (strchr(name, ':') != NULL)
        return parseIPv6WildCard(name, ip, mask);

    /* Names containing '.' are inet. The inet full wildcard spec ".*". */
    if (strchr(name, '.') != NULL)
        return parseInetWildCard(name, ip, mask);

    __pmNotifyErr(LOG_ERR, "Bad IP address wildcard, %s\n", name);
    return -EINVAL;
}

/* Information representing an access specification. */
struct accessSpec {
    char		*name;
    __pmSockAddr	*hostid;
    __pmSockAddr	*hostmask;
    int			level;
};

/* Construct the proper spec for the given wildcard. */
static int
getWildCardSpec(const char *name, struct accessSpec *spec)
{
    char ip[INET6_ADDRSTRLEN];
    char mask[INET6_ADDRSTRLEN];

    /* Build up strings representing the ip address and the mask. Compute the wildcard
       level as we go. */
    spec->level = parseWildCard(name, ip, mask);
    if (spec->level < 0)
	return spec->level;

    /* Now create socket addresses for the ip address and mask. */
    spec->hostid = __pmStringToSockAddr(ip);
    if (spec->hostid == NULL) {
	__pmNotifyErr(LOG_ERR, "__pmStringToSockAddr failure\n");
	return -ENOMEM;
    }
    spec->hostmask = __pmStringToSockAddr(mask);
    if (spec->hostmask == NULL) {
	__pmNotifyErr(LOG_ERR, "__pmStringToSockAddr failure\n");
	__pmSockAddrFree(spec->hostid);
	return -ENOMEM;
    }

    /* Do this last since a valid name indicates a valid spec. */
    spec->name = strdup(name);
    return 0;
}

/* Determine all of the access specs which result from the given name. */
static struct accessSpec *
getAccessSpecs(const char *name, int *sts)
{
    struct accessSpec	*specs;
    size_t		specSize;
    size_t		specIx;
    size_t		ix;
    size_t		need;
    __pmSockAddr	*myAddr;
    __pmHostEnt		*servInfo;
    void		*enumIx;
    int			family;
    const char		*realname;
    const char		*p;
    static int		cando_ipv6 = -1;

    /* If the general wildcard ("*") is specified, then generate individual wildcards for
       inet and, if supported, IPv6. */
    *sts = 0;
    if (strcmp(name, "*") == 0) {
	/* Use calloc so that the final entry is zeroed. */
	specs = calloc(3, sizeof(*specs));
	if (specs == NULL)
	    __pmNoMem("Access Spec List", 3 * sizeof(*specs), PM_FATAL_ERR);
	getWildCardSpec(".*", &specs[0]);

	if (cando_ipv6 == -1) {
	    /*
	     * one trip check to see if IPv6 is supported in the
	     * current run-time
	     */
	    const char	*config = __pmGetAPIConfig("ipv6");
	    if (config != NULL && strcmp(config, "true") == 0)
		cando_ipv6 = 1;
	    else
		cando_ipv6 = 0;
	}
	if (cando_ipv6)
	    getWildCardSpec(":*", &specs[1]); /* Guaranteed to succeed. */
	return specs;
    }

    /* If any other wildcard is specified, then our list will contain that single item. */
    if ((p = strchr(name, '*')) != NULL) {
	if (p[1] != '\0') {
	    __pmNotifyErr(LOG_ERR,
			  "Wildcard in host pattern \"%s\" is not at the end\n",
			  name);
	    *sts = -EINVAL;
	    return NULL;
	}
	/* Use calloc so that the final entry is zeroed. */
	specs = calloc(2, sizeof(*specs));
	if (specs == NULL)
	    __pmNoMem("Access Spec List", 2 * sizeof(*specs), PM_FATAL_ERR);
	*sts = getWildCardSpec(name, &specs[0]);
	return specs;
    }

    /* Assume we have a host name or address. Resolve it and contruct a list containing all of the
       resolved addresses. If the name is "localhost", then resolve using the actual host name. */
    if (strcasecmp(name, "localhost") == 0) {
	if (!gotmyhostid) {
	    if (getmyhostid() < 0) {
		__pmNotifyErr(LOG_ERR, "Can't get host name/IP address, giving up\n");
		*sts = -EHOSTDOWN;
		return NULL;	/* should never happen! */
	    }
	}
	realname = myhostname;
    }
    else
	realname = name;

    *sts = -EHOSTUNREACH;
    specs = NULL;
    specSize = 0;
    specIx = 0;
    if ((servInfo = __pmGetAddrInfo(realname)) != NULL) {
	/* Collect all of the resolved addresses. Check for the end of the list within the
	   loop since we need to add an empty entry and the code to grow the list is within the
	   loop. */
	enumIx = NULL;
	for (myAddr = __pmHostEntGetSockAddr(servInfo, &enumIx);
	     /**/;
	     myAddr = __pmHostEntGetSockAddr(servInfo, &enumIx)) {
	    if (specIx == specSize) {
		specSize = specSize == 0 ? 4 : specSize * 2;
		need = specSize * sizeof(*specs);
		specs = realloc(specs, need);
		if (specs == NULL) {
		    __pmNoMem("Access Spec List", need, PM_FATAL_ERR);
		}
	    }
	    /* No more addresses? */
	    if (myAddr == NULL) {
		specs[specIx].name = NULL;
		break;
	    }
	    /* Don't add any duplicate entries. It causes false permission clashes. */
	    for (ix = 0; ix < specIx; ++ix) {
		if (__pmSockAddrCompare(myAddr, specs[ix].hostid) == 0)
		    break;
	    }
	    if (ix < specIx){
		__pmSockAddrFree(myAddr);
		continue;
	    }
	    /* Add the new address and its corrsponding mask. */
	    family = __pmSockAddrGetFamily(myAddr);
	    if (family == AF_INET)
		specs[specIx].hostmask = __pmStringToSockAddr("255.255.255.255");
	    else if (family == AF_INET6)
		specs[specIx].hostmask = __pmStringToSockAddr("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
	    else {
		__pmNotifyErr(LOG_ERR, "Unsupported socket address family: %d\n", family);
		__pmSockAddrFree(myAddr);
		continue;
	    }
	    specs[specIx].hostid = myAddr;
	    specs[specIx].level = 0;
	    specs[specIx].name = strdup(name);
	    *sts = 0;
	    ++specIx;
	}
	__pmHostEntFree(servInfo);
    }
    else {
	__pmNotifyErr(LOG_ERR, "__pmGetAddrInfo(%s), %s\n",
		      realname, hoststrerror());
    }

    /* Return NULL if nothing was discovered. *sts is already set. */
    if (specIx == 0 && specs != NULL) {
	free(specs);
	specs = NULL;
    }
    return specs;
}

/* Routine to add a host to the host access list with a specified set of
 * permissions and a maximum connection limit.
 * specOps is a mask.  Only bits corresponding to operations specified by
 *	__pmAccAddOp have significance.  A 1 bit indicates that the
 *	corresponding bit in the denyOps mask is to be used.  A zero bit in
 *	specOps means the corresponding bit in denyOps should be ignored.
 * denyOps is a mask where a 1 bit indicates that permission to perform the
 *	corresponding operation should be denied.
 * maxcons is a maximum connection limit for clients on hosts matching the host
 *	id.  Zero means unspecified, which will allow unlimited connections or
 *	a subsequent __pmAccAddHost call with the same host to override maxcons.
 *
 * Returns a negated system error code on failure.
 */

int
__pmAccAddHost(const char *name, unsigned int specOps, unsigned int denyOps, int maxcons)
{
    size_t		need;
    int			i, sts;
    struct accessSpec	*specs;
    struct accessSpec	*spec;
    hostinfo		*hp;
    int			found;
    char		*prevHost;
    char		*prevName;

    if (PM_MULTIPLE_THREADS(PM_SCOPE_ACL))
	return PM_ERR_THREAD;
    if (specOps & ~all_ops)
	return -EINVAL;
    if (maxcons < 0)
	return -EINVAL;

    /* The specified name may result in more than one access specification. */
    specs = getAccessSpecs(name, &sts);
    if (specs == NULL)
	return sts;

    /* Search for a match to each spec in the existing table of hosts. We will either use
       or free the host id, mask and name of each spec as we go. */
    prevHost = NULL;
    prevName = NULL;
    found = 0;
    for (spec = specs; spec->name != NULL; ++spec) {
	sts = 0;
	for (i = 0; i < nhosts; i++) {
	    if (hostlist[i].level > spec->level)
		break;
	    /* hostid AND level must match.  Wildcarded IP addresses have zero in
	     * the unspecified components.  Distinguish between 155.23.6.0 and
	     * 155.23.6.* or 155.23.0.0 and 155.23.* by wildcard level.  IP
	     * addresses shouldn't have zero in last position but to deal with
	     * them just in case.
	     */
	    if (__pmSockAddrCompare(spec->hostid, hostlist[i].hostid) == 0 &&
		spec->level == hostlist[i].level) {
		sts = 1;
		break;
	    }
	}

	/* Check and augment existing host access list entry for this host id if a
	 * match was found (sts == 1) otherwise insert a new entry in list.
	 */
	if (sts == 1) {
	    __pmSockAddrFree(spec->hostid);
	    __pmSockAddrFree(spec->hostmask);

	    /* If the specified operations overlap, they must agree */
	    hp = &hostlist[i];
	    if ((hp->maxcons && maxcons && hp->maxcons != maxcons) ||
		((hp->specOps & specOps) &&
		 ((hp->specOps & hp->denyOps) ^ (specOps & denyOps)))) {
		/* Suppress duplicate messages. These can occur when a host resolves to more
		   than one address. */
		if (prevName == NULL ||
		    strcmp(prevName, spec->name) != 0 || strcmp(prevHost, hp->hostspec) != 0) {
		    __pmNotifyErr(LOG_ERR,
				  "Permission clash for %s with earlier statement for %s\n",
				  spec->name, hp->hostspec);
		    if (prevName != NULL) {
			free(prevName);
			free(prevHost);
		    }
		    prevName = strdup(spec->name);
		    prevHost = strdup(hp->hostspec);
		}
		free(spec->name);
		continue;
	    }
	    free(spec->name);
	    hp->specOps |= specOps;
	    hp->denyOps |= (specOps & denyOps);
	    if (maxcons)
		hp->maxcons = maxcons;
	}
	else {
	    /* Make the host access list larger if required */
	    if (nhosts == szhostlist) {
		szhostlist += 8;
		need = szhostlist * sizeof(hostinfo);
		hostlist = (hostinfo *)realloc(hostlist, need);
		if (hostlist == NULL) {
		    __pmNoMem("AddHost enlarge", need, PM_FATAL_ERR);
		}
	    }

	    /* Move any subsequent hosts down to make room for the new entry*/
	    hp = &hostlist[i];
	    if (i < nhosts)
		memmove(&hostlist[i+1], &hostlist[i],
			(nhosts - i) * sizeof(hostinfo));
	    hp->hostspec = spec->name;
	    hp->hostid = spec->hostid;
	    hp->hostmask = spec->hostmask;
	    hp->level = spec->level;
	    hp->specOps = specOps;
	    hp->denyOps = specOps & denyOps;
	    hp->maxcons = maxcons;
	    hp->curcons = 0;
	    nhosts++;
	}
	/* Count the found hosts. */
	++found;
    } /* loop over addresses */

    if (prevName != NULL) {
	free(prevName);
	free(prevHost);
    }
    free(specs);
    return found != 0 ? 0 : -EINVAL;
}

static __pmSockAddr **
getClientIds(const __pmSockAddr *hostid, int *sts)
{
    __pmSockAddr	**clientIds;
    __pmSockAddr	*myAddr;
    size_t		clientIx;
    size_t		clientSize;
    size_t		need;
    void		*enumIx;

    *sts = 0;

    /* If the address is not for "localhost", then return a list containing only
       the given address. */
    if (! __pmSockAddrIsLoopBack(hostid)) {
	clientIds = calloc(2, sizeof(*clientIds));
	if (clientIds == NULL)
	    __pmNoMem("Client Ids", 2 * sizeof(*clientIds), PM_FATAL_ERR);
	clientIds[0] = __pmSockAddrDup(hostid);
	return clientIds;
    }

    /* Map "localhost" to the real IP addresses.  Host access statements for
     * localhost are mapped to the "real" IP addresses so that wildcarding works
     * consistently. First get the real host address;
     */
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (!gotmyhostid)
	getmyhostid();

    *sts = PM_ERR_PERMISSION;
    if (gotmyhostid <= 0) {
	PM_UNLOCK(__pmLock_libpcp);
	return NULL;
    }
    PM_UNLOCK(__pmLock_libpcp);

    /* Now construct a list containing each address. Check for the end of the list within the
       loop since we need to add an empty entry and the code to grow the list is within the
       loop. */
    clientIds = NULL;
    clientIx = 0;
    clientSize = 0;
    enumIx = NULL;
    for (myAddr = __pmHostEntGetSockAddr(myhostid, &enumIx);
	 /**/;
	 myAddr = __pmHostEntGetSockAddr(myhostid, &enumIx)) {
	if (clientIx == clientSize) {
	    clientSize = clientSize == 0 ? 4 : clientSize * 2;
	    need = clientSize * sizeof(*clientIds);
	    clientIds = realloc(clientIds, need);
	    if (clientIds == NULL) {
		PM_UNLOCK(__pmLock_libpcp);
		__pmNoMem("Client Ids", need, PM_FATAL_ERR);
	    }
	}
	/* No more addresses? */
	if (myAddr == NULL) {
	    clientIds[clientIx] = NULL;
	    break;
	}
	/* Add the new address and its corrsponding mask. */
	clientIds[clientIx] = myAddr;
	++clientIx;
	*sts = 0;
    }

    /* If no addresses were discovered, then return NULL. *sts is already set. */
    if (clientIx == 0 && clientIds != NULL) {
	free(clientIds);
	clientIds = NULL;
    }
    return clientIds;
}

static void
freeClientIds(__pmSockAddr **clientIds)
{
    int i;
    for (i = 0; clientIds[i] != NULL; ++i)
	free(clientIds[i]);
    free(clientIds);
}

/* Called after accepting new client's connection to check that another
 * connection from its host is permitted and to find which operations the
 * client is permitted to perform.
 * hostid is the address of the host that the client is running on
 * denyOpsResult is a pointer to return the capability vector
 */
int
__pmAccAddClient(__pmSockAddr *hostid, unsigned int *denyOpsResult)
{
    int			i;
    int			sts;
    hostinfo		*hp;
    hostinfo		*lastmatch = NULL;
    int			clientIx;
    __pmSockAddr	**clientIds;
    __pmSockAddr	*clientId;
    __pmSockAddr	*maskedId;

    if (PM_MULTIPLE_THREADS(PM_SCOPE_ACL))
	return PM_ERR_THREAD;

    /* There could be more than one address associated with this host.*/
    clientIds = getClientIds(hostid, &sts);
    if (clientIds == NULL)
	return sts;

    *denyOpsResult = 0;			/* deny nothing == allow all */
    if (nhosts == 0)			/* No access controls => allow all */
	return 0;

    /* Accumulate permissions for each client address. */
    for (clientIx = 0; clientIds[clientIx] != NULL; ++clientIx) {
	clientId = clientIds[clientIx];
	for (i = nhosts - 1; i >= 0; i--) {
	    hp = &hostlist[i];
	    maskedId = __pmSockAddrDup(clientId);
	    if (__pmSockAddrGetFamily(maskedId) == __pmSockAddrGetFamily(hp->hostmask) &&
		__pmSockAddrCompare(__pmSockAddrMask(maskedId, hp->hostmask), hp->hostid) == 0) {
		/* Clobber specified ops then set. Leave unspecified ops alone. */
		*denyOpsResult &= ~hp->specOps;
		*denyOpsResult |= hp->denyOps;
		lastmatch = hp;
	    }
	    __pmSockAddrFree(maskedId);
	}
	/* no matching entry in hostlist => allow all */

	/* If no operations are allowed, disallow connection */
	if (*denyOpsResult == all_ops) {
	    freeClientIds(clientIds);
	    return PM_ERR_PERMISSION;
	}

	/* Check for connection limit */
	if (lastmatch != NULL && lastmatch->maxcons &&
	    lastmatch->curcons >= lastmatch->maxcons) {

	    *denyOpsResult = all_ops;
	    freeClientIds(clientIds);
	    return PM_ERR_CONNLIMIT;
	}

	/* Increment the count of current connections for ALL host specs in the
	 * host access list that match the client's IP address.  A client may
	 * contribute to several connection counts because of wildcarding.
	 */
	for (i = 0; i < nhosts; i++) {
	    hp = &hostlist[i];
	    maskedId = __pmSockAddrDup(clientId);
	    if (__pmSockAddrGetFamily(maskedId) == __pmSockAddrGetFamily(hp->hostmask) &&
		__pmSockAddrCompare(__pmSockAddrMask(maskedId, hp->hostmask), hp->hostid) == 0)
		if (hp->maxcons)
		    hp->curcons++;
	    __pmSockAddrFree(maskedId);
	}
    }

    freeClientIds(clientIds);
    return 0;
}

void
__pmAccDelClient(__pmSockAddr *hostid)
{
    int		i;
    int		sts;
    hostinfo	*hp;
    int		clientIx;
    __pmSockAddr **clientIds;
    __pmSockAddr *clientId;
    __pmSockAddr *maskedId;

    if (PM_MULTIPLE_THREADS(PM_SCOPE_ACL))
	return;

    /* There could be more than one address associated with this host.*/
    clientIds = getClientIds(hostid, &sts);
    if (clientIds == NULL)
	return;

    /* Decrement the count of current connections for ALL host specs in the
     * host access list that match the client's IP addresses.  A client may
     * contribute to several connection counts because of wildcarding.
     */
    for (clientIx = 0; clientIds[clientIx] != NULL; ++clientIx) {
	clientId = clientIds[clientIx];
	for (i = 0; i < nhosts; i++) {
	    hp = &hostlist[i];
	    maskedId = __pmSockAddrDup(clientId);
	    if (__pmSockAddrGetFamily(maskedId) == __pmSockAddrGetFamily(hp->hostmask) &&
		__pmSockAddrCompare(__pmSockAddrMask(maskedId, hp->hostmask), hp->hostid) == 0)
		if (hp->maxcons)
		    hp->curcons--;
	    __pmSockAddrFree(maskedId);
	}
    }
    freeClientIds(clientIds);
}

void
__pmAccDumpHosts(FILE *stream)
{
    int			h, i, j;
    int			minbit = -1, maxbit;
    int			addrWidth;
    unsigned int	mask;
    hostinfo		*hp;

    if (PM_MULTIPLE_THREADS(PM_SCOPE_ACL))
	return;

    mask = all_ops;
    for (i = 0; mask; i++) {
	if (mask % 2)
	    if (minbit < 0)
		minbit = i;
	mask = mask >> 1;
    }
    maxbit = i - 1;

    if (nhosts == 0) {
	fprintf(stream, "\nHost access list empty: access control turned off\n\n");
	return;
    }

    addrWidth = 39; /* Full IPv6 address */
    fprintf(stream, "\nHost access list:\n");
    for (i = minbit; i <= maxbit; i++)
	if (all_ops & (1 << i))
	    fprintf(stream, "%02d ", i);
    fprintf(stream, "Cur/MaxCons %-*s %-*s lvl host-name\n",
	    addrWidth, "host-spec", addrWidth, "host-mask");
    for (i = minbit; i <= maxbit; i++)
	if (all_ops & (1 << i))
	    fputs("== ", stream);
    fprintf(stream, "=========== ");
    for (i = 0; i < 2; ++i) {
        for (j = 0; j < addrWidth; ++j)
	    fprintf(stream, "=");
	fprintf(stream, " ");
    }
    fprintf(stream, "=== ==============\n");
    for (h = 0; h < nhosts; h++) {
	hp = &hostlist[h];

	for (i = minbit; i <= maxbit; i++) {
	    if (all_ops & (mask = 1 << i)) {
		if (hp->specOps & mask) {
		    fputs((hp->denyOps & mask) ? " n " : " y ", stream);
		}
		else {
		    fputs("   ", stream);
		}
	    }
	}
	fprintf(stream, "%5d %5d %-*s %-*s %3d %s\n", hp->curcons, hp->maxcons,
		addrWidth, __pmSockAddrToString(hp->hostid),
		addrWidth, __pmSockAddrToString(hp->hostmask),
		hp->level, hp->hostspec);
    }
    putc('\n', stream);
}

