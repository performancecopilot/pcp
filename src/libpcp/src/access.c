/*
 * Copyright (c) 2012 Red Hat.
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
    __pmIPAddr		hostid;		/* Partial host-id to match */
    __pmIPAddr		hostmask;	/* Mask for wildcarding */
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
static __pmIPAddr	myhostid;
static char		myhostname[MAXHOSTNAMELEN+1];

/*
 * Always called with __pmLock_libpcp already held, so accessing
 * gotmyhostid, myhostname, myhostid and gethostname() call are all 
 * thread-safe.
 */
static int
getmyhostid(void)
{
    struct __pmHostEnt	*host;

    if (gethostname(myhostname, MAXHOSTNAMELEN) < 0) {
	__pmNotifyErr(LOG_ERR, "gethostname failure\n");
	return -1;
    }
    myhostname[MAXHOSTNAMELEN-1] = '\0';
    if ((host = __pmAllocHostEnt()) == NULL) {
	__pmNotifyErr(LOG_ERR, "__pmAllocHostEnt failure\n");
	return -1;
    }

    PM_LOCK(__pmLock_libpcp);
    if (__pmGetHostByName(myhostname, host) == NULL) {
	__pmNotifyErr(LOG_ERR, "__pmGetHostByName(%s), %s\n",
		     myhostname, hoststrerror());
	PM_UNLOCK(__pmLock_libpcp);
	__pmFreeHostEnt(host);
	return -1;
    }
    myhostid = __pmHostEntGetIPAddr(host, 0);
    PM_UNLOCK(__pmLock_libpcp);
    __pmFreeHostEnt(host);
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
    int			i, n, sts;
    unsigned int	ip, mask;
    struct __pmHostEnt	*host;
    int			level = 0;	/* Wildcarding level */
    __pmIPAddr		hostid, hostmask;
    const char		*p;
    hostinfo		*hp;

    if (PM_MULTIPLE_THREADS(PM_SCOPE_ACL))
	return PM_ERR_THREAD;
    if (specOps & ~all_ops)
	return -EINVAL;
    if (maxcons < 0)
	return -EINVAL;

    /* Make the host access list larger if required */
    if (nhosts == szhostlist) {
	szhostlist += 8;
	need = szhostlist * sizeof(hostinfo);
	hostlist = (hostinfo *)realloc(hostlist, need);
	if (hostlist == NULL) {
	    __pmNoMem("AddHost enlarge", need, PM_FATAL_ERR);
	}
    }

    /* If it has an asterisk it's a wildcarded host id */
    if ((p = strchr(name, '*')) != NULL) {
	if (p[1] != '\0') {
	    __pmNotifyErr(LOG_ERR,
			 "Wildcard in host pattern \"%s\" is not at the end\n",
			 name);
	    return -EINVAL;
	}
	/* Find the wildcard level, 0 is exact match, 4 is most general */
	level = 4;
	for (p = name; *p; p++)
	    if (*p == '.') level--;
	if (level < 1) {
	    __pmNotifyErr(LOG_ERR,
			 "Too many dots in host pattern \"%s\"\n",
			 name);
	    return -EINVAL;
	}

	/* i is used to shift the IP address components as they are scanned */
	ip = mask = 0;
	for (p = name, i = 24; *p && *p != '*' ; p++, i -= 8) {
	    n = (int)strtol(p, (char **)&p, 10);
	    if ((*p != '.' && *p != '*') || n < 0 || n > 255) {
		__pmNotifyErr(LOG_ERR,
			     "Bad IP address wildcard, %s\n",
			     name);
		return -EINVAL;
	    }
	    ip += n << i;
	    mask += 0xff << i;
	}
	/* IP addresses are kept in the Network Byte Order, so translate 'em
	 * here */
	__pmSetIPAddr(&hostid, htonl(ip));
	__pmSetIPAddr(&hostmask, htonl(mask));
    }
    /* No asterisk: must be a specific host.
     * Map localhost to this host's specific IP address so that host access
     * statements with wildcarding work with it.
     */
    else {
	const char	*realname;

	if (strcasecmp(name, "localhost") == 0) {
	    /* Map "localhost" to full host name & get IP address */
	    PM_INIT_LOCKS();
	    PM_LOCK(__pmLock_libpcp);
	    if (!gotmyhostid)
		if (getmyhostid() < 0) {
		    __pmNotifyErr(LOG_ERR, "Can't get host name/IP address, giving up\n");
		    PM_UNLOCK(__pmLock_libpcp);
		    return -EHOSTDOWN;	/* should never happen! */
		}
	    realname = myhostname;
	    PM_UNLOCK(__pmLock_libpcp);
	}
	else
	    realname = name;

	if ((host = __pmAllocHostEnt()) == NULL) {
	    __pmNotifyErr(LOG_ERR, "__pmAllocHostEnt failure\n");
	    return -ENOMEM;
	}
	PM_INIT_LOCKS();
	PM_LOCK(__pmLock_libpcp);
	if (__pmGetHostByName(realname, host) == NULL) {
	    __pmNotifyErr(LOG_ERR, "__pmGetHostByName(%s), %s\n",
			 realname, hoststrerror());
	    PM_UNLOCK(__pmLock_libpcp);
	    __pmFreeHostEnt(host);
	    return -EHOSTUNREACH;	/* host error unsuitable to return */
	}
	hostid = __pmHostEntGetIPAddr(host, 0);
	PM_UNLOCK(__pmLock_libpcp);
	__pmFreeHostEnt(host);
	__pmSetIPAddr(&hostmask, 0xffffffff);
	level = 0;
    }

    sts = 0;
    for (i = 0; i < nhosts; i++) {
	if (hostlist[i].level > level)
	    break;
	/* hostid AND level must match.  Wildcarded IP addresses have zero in
	 * the unspecified components.  Distinguish between 155.23.6.0 and
	 * 155.23.6.* or 155.23.0.0 and 155.23.* by wildcard level.  IP
	 * addresses shouldn't have zero in last position but to deal with
	 * them just in case.
	 */
	if (__pmCompareIPAddr(&hostid, &hostlist[i].hostid) == 0 &&
	    level == hostlist[i].level) {
	    sts = 1;
	    break;
	}
    }
    hp = &hostlist[i];

    /* Check and augment existing host access list entry for this host id if a
     * match was found (sts == 1) otherwise insert a new entry in list.
     */
    if (sts == 1) {
	/* If the specified operations overlap, they must agree */
	if ((hp->maxcons && maxcons && hp->maxcons != maxcons) ||
	    ((hp->specOps & specOps) &&
	     ((hp->specOps & hp->denyOps) ^ (specOps & denyOps)))) {
	    __pmNotifyErr(LOG_ERR,
			 "Permission clash for %s with earlier statement for %s\n",
			 name, hp->hostspec);
	    return -EINVAL;
	}
	hp->specOps |= specOps;
	hp->denyOps |= (specOps & denyOps);
	if (maxcons)
	    hp->maxcons = maxcons;
    }
    else {
	/* Move any subsequent hosts down to make room for the new entry*/
	if (i < nhosts)
	    memmove(&hostlist[i+1], &hostlist[i],
		    (nhosts - i) * sizeof(hostinfo));
	hp->hostspec = strdup(name);
	hp->hostid = hostid;
	hp->hostmask = hostmask;
	hp->level = level;
	hp->specOps = specOps;
	hp->denyOps = specOps & denyOps;
	hp->maxcons = maxcons;
	hp->curcons = 0;
	nhosts++;
    }

    return 0;
}

/* Called after accepting new client's connection to check that another
 * connection from its host is permitted and to find which operations the
 * client is permitted to perform.
 * hostid is the IP address of the host that the client is running on
 * denyOpsResult is a pointer to return the capability vector
 */
int
__pmAccAddClient(__pmIPAddr hostid, unsigned int *denyOpsResult)
{
    int			i;
    hostinfo		*hp;
    hostinfo		*lastmatch = NULL;
    __pmIPAddr		clientid;
    __pmIPAddr		maskedid;

    if (PM_MULTIPLE_THREADS(PM_SCOPE_ACL))
	return PM_ERR_THREAD;

    clientid = hostid;

    /* Map "localhost" to the real IP address.  Host access statements for
     * localhost are mapped to the "real" IP address so that wildcarding works
     * consistently.
     */
    if (__pmIPAddrIsLoopBack(&clientid)) {
	PM_INIT_LOCKS();
	PM_LOCK(__pmLock_libpcp);
	if (!gotmyhostid)
	    getmyhostid();

	if (gotmyhostid > 0) {
	    clientid = myhostid;
	    PM_UNLOCK(__pmLock_libpcp);
	}
	else {
	    PM_UNLOCK(__pmLock_libpcp);
	    return PM_ERR_PERMISSION;
	}
    }


    *denyOpsResult = 0;			/* deny nothing == allow all */
    if (nhosts == 0)			/* No access controls => allow all */
	return 0;

    for (i = nhosts - 1; i >= 0; i--) {
	hp = &hostlist[i];
	maskedid = clientid;
	if (__pmCompareIPAddr(__pmMaskIPAddr(&maskedid, &hp->hostmask), &hp->hostid) == 0) {
	    /* Clobber specified ops then set. Leave unspecified ops alone. */
	    *denyOpsResult &= ~hp->specOps;
	    *denyOpsResult |= hp->denyOps;
	    lastmatch = hp;
	}
    }
    /* no matching entry in hostlist => allow all */

    /* If no operations are allowed, disallow connection */
    if (*denyOpsResult == all_ops)
	return PM_ERR_PERMISSION;

    /* Check for connection limit */
    if (lastmatch != NULL && lastmatch->maxcons &&
	lastmatch->curcons >= lastmatch->maxcons) {

	*denyOpsResult = all_ops;
	return PM_ERR_CONNLIMIT;
    }

    /* Increment the count of current connections for ALL host specs in the
     * host access list that match the client's IP address.  A client may
     * contribute to several connection counts because of wildcarding.
     */
    for (i = 0; i < nhosts; i++) {
	hp = &hostlist[i];
	maskedid = clientid;
	if (__pmCompareIPAddr(__pmMaskIPAddr(&maskedid, &hp->hostmask), &hp->hostid) == 0)
	    if (hp->maxcons)
		hp->curcons++;
    }

    return 0;
}

void
__pmAccDelClient(__pmIPAddr hostid)
{
    int		i;
    hostinfo	*hp;

    if (PM_MULTIPLE_THREADS(PM_SCOPE_ACL))
	return;

    /* Increment the count of current connections for ALL host specs in the
     * host access list that match the client's IP address.  A client may
     * contribute to several connection counts because of wildcarding.
     */
    for (i = 0; i < nhosts; i++) {
	hp = &hostlist[i];
	if (__pmCompareIPAddr(__pmMaskIPAddr(&hostid, &hp->hostmask), &hp->hostid) == 0)
 	    if (hp->maxcons)
		hp->curcons--;
    }
}

void
__pmAccDumpHosts(FILE *stream)
{
    int			h, i;
    int			minbit = -1, maxbit;
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

    fprintf(stream, "\nHost access list:\n");
    for (i = minbit; i <= maxbit; i++)
	if (all_ops & (1 << i))
	    fprintf(stream, "%02d ", i);
    fprintf(stream, "Cur/MaxCons host-spec host-mask lvl host-name\n");
    for (i = minbit; i <= maxbit; i++)
	if (all_ops & (1 << i))
	    fputs("== ", stream);
    fprintf(stream, "=========== ========= ========= === ==============\n");
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
	fprintf(stream, "%5d %5d  %08x  %08x %3d %s\n", hp->curcons, hp->maxcons,
		__pmIPAddrToInt(&hp->hostid), __pmIPAddrToInt(&hp->hostmask), hp->level, hp->hostspec);
    }
    putc('\n', stream);
}

