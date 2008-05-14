/*
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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <unistd.h>
#include <limits.h>

#include "pmapi.h"
#include "impl.h"

#include <syslog.h>
#include <netdb.h>

#if !defined(HAVE_HSTRERROR)
static char *
hstrerror(int err)
{
    switch (err) {
	    case 0: return "";
	    case 1: return "Host not found";
	    case 2: return "Try again";
	    case 3: return "Non-recoverable error";
	    case 4: return "No address";
	    default: return "Unknown error";
    }
}
#endif

/* Host access control list */

typedef struct {
    char		*hostSpec;	/* Host specification */
    struct in_addr	hostId;		/* Partial host-id to match */
    struct in_addr	hostMask;	/* Mask for wildcarding */
    int			level;		/* Level of wildcarding */
    unsigned int	specOps;	/* Mask of specified operations */
    unsigned int	denyOps;	/* Mask of disallowed operations */
    int			maxCons;	/* Max connections permitted (0 => no limit) */
    int			curCons;	/* Current # connections from matching clients */
} HostInfo;

/* Mask of the operations defined by the user of the routines */

static unsigned int	all_ops = 0;	/* mask of all operations specifiable */

/* This allows the set of valid operations to be specified.
 * Operations must be powers of 2.
 */
int
__pmAccAddOp(unsigned int op)
{
    unsigned int	i, mask;

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

static int		gotMyHostId = 0;
static struct in_addr	myHostId;
static char		myHostName[MAXHOSTNAMELEN+1];

static int
GetMyHostId(void)
{
    struct hostent	*hep;

    gotMyHostId = -1;
    (void)gethostname(myHostName, MAXHOSTNAMELEN);
    myHostName[MAXHOSTNAMELEN-1] = '\0';

    if ((hep = gethostbyname(myHostName)) == NULL) {
	__pmNotifyErr(LOG_ERR,
		     "gethostbyname(%s), %s\n",
		     myHostName, hstrerror(h_errno));
	return -1;
    }
    myHostId.s_addr = ((struct in_addr *)hep->h_addr_list[0])->s_addr;
    gotMyHostId = 1;
    return 0;
}

/* This is the host access list */

static HostInfo	*hostList = NULL;
static int	nHosts = 0;
static int	szHostList = 0;

/* Used for saving the current state of the host access list */

static int	saved = 0;
static HostInfo	*oldHostList;
static int	oldNHosts;
static int	oldSzHostList;

/* Save the current host access list.
 * Returns 0 for success, -1 for error.
 */
int
__pmAccSaveHosts(void)
{
    if (saved)
	return -1;
    saved = 1;
    oldHostList = hostList;
    oldNHosts = nHosts;
    oldSzHostList = szHostList;
    hostList = NULL;
    nHosts = 0;
    szHostList = 0;
    return 0;
}

/* Free the current host list.  This is done automatically by
 * __pmAccRestoreHosts so there is no need for it to be made globally visible.
 * A caller of these routines should never need to dispose of the host list
 * once it has been built.
 */
static void
_pmAccFreeHosts(void)
{
    int		i;
    char	*p;

    if (szHostList) {
	for (i = 0; i < nHosts; i++)
	    if ((p = hostList[i].hostSpec) != NULL)
		free(p);
	free(hostList);
    }
    hostList = NULL;
    nHosts = 0;
    szHostList = 0;
}

/* Restore the previously saved host list.  Any current host list is freed.
 * Returns 0 for success, -1 for error.
 */
int
__pmAccRestoreHosts(void)
{
    if (!saved)
	return -1;
    _pmAccFreeHosts();
    saved = 0;
    hostList = oldHostList;
    nHosts = oldNHosts;
    szHostList = oldSzHostList;
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

    if (!saved)
	return;
    if (oldSzHostList) {
	for (i = 0; i < oldNHosts; i++)
	    if ((p = oldHostList[i].hostSpec) != NULL)
		free(p);
	free(oldHostList);
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
 * maxCons is a maximum connection limit for clients on hosts matching the host
 *	id.  Zero means unspecified, which will allow unlimited connections or
 *	a subsequent __pmAccAddHost call with the same host to override maxCons.
 *
 * Returns a negated errno code on failure.
 */

int
__pmAccAddHost(const char *name, unsigned int specOps, unsigned int denyOps, int maxCons)
{
    size_t		need;
    int			i, n, sts;
    struct hostent	*hep;
    int			level = 0;	/* Wildcarding level */
#ifndef h_errno
    /* h_errno is a macro in AIX */
    extern int		h_errno;
#endif
    struct in_addr	hostId, hostMask;
    const char		*p;
    HostInfo		*hp;

    if (specOps & ~all_ops)
	return -EINVAL;
    if (maxCons < 0)
	return -EINVAL;

    /* Make the host access list larger if required */
    if (nHosts == szHostList) {
	szHostList += 8;
	need = szHostList * sizeof(HostInfo);
	hostList = (HostInfo *)realloc(hostList, need);
	if (hostList == NULL) {
	    __pmNoMem("AddHost enlarge", need, PM_FATAL_ERR);
	}
#ifdef MALLOC_AUDIT
	_persistent_(hostList);
#endif
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
	hostId.s_addr = hostMask.s_addr = 0;
	for (p = name, i = 24; *p && *p != '*' ; p++, i -= 8) {
	    n = (int)strtol(p, (char **)&p, 10);
	    if ((*p != '.' && *p != '*') || n < 0 || n > 255) {
		__pmNotifyErr(LOG_ERR,
			     "Bad IP address wildcard, %s\n",
			     name);
		return -EINVAL;
	    }
	    hostId.s_addr += n << i;
	    hostMask.s_addr += 0xff << i;
	}
	/* IP addresses are kept in the Network Byte Order, so translate 'em
	 * here */
	hostId.s_addr = htonl (hostId.s_addr);
	hostMask.s_addr = htonl (hostMask.s_addr);
	
    }
    /* No asterisk: must be a specific host.
     * Map localhost to this host's specific IP address so that host access
     * statements with wildcarding work with it.
     */
    else {
	const char	*realName;

	if (strcasecmp(name, "localhost") == 0) {
	    /* Map "localhost" to full host name & get IP address */
	    if (!gotMyHostId)
		if (GetMyHostId() < 0) {
		    __pmNotifyErr(LOG_ERR, "Can't get host name/IP address, giving up\n");
		    return -EHOSTDOWN;	/* should never happen! */
		}
	    realName = myHostName;
	}
	else
	    realName = name;
	if ((hep = gethostbyname(realName)) == NULL) {
	    __pmNotifyErr(LOG_ERR,
			 "gethostbyname(%s), %s\n",
			 realName, hstrerror(h_errno));
	    return -EHOSTUNREACH;	/* h_errno isn't suitable to return */
	}
	hostId.s_addr = ((struct in_addr *)hep->h_addr_list[0])->s_addr;
	hostMask.s_addr = 0xffffffff;
	level = 0;
    }

    sts = 0;
    for (i = 0; i < nHosts; i++) {
	if (hostList[i].level > level)
	    break;
	/* hostId AND level must match.  Wildcarded IP addresses have zero in
	 * the unspecified components.  Distinguish between 155.23.6.0 and
	 * 155.23.6.* or 155.23.0.0 and 155.23.* by wildcard level.  IP
	 * addresses shouldn't have zero in last position but to deal with
	 * them just in case.
	 */
	if (hostId.s_addr == hostList[i].hostId.s_addr &&
	    level == hostList[i].level) {
	    sts = 1;
	    break;
	}
    }
    hp = &hostList[i];

    /* Check and augment existing host access list entry for this host id if a
     * match was found (sts == 1) otherwise insert a new entry in list.
     */
    if (sts == 1) {
	/* If the specified operations overlap, they must agree */
	if ((hp->maxCons && maxCons && hp->maxCons != maxCons) ||
	    ((hp->specOps & specOps) &&
	     ((hp->specOps & hp->denyOps) ^ (specOps & denyOps)))) {
	    __pmNotifyErr(LOG_ERR,
			 "Permission clash for %s with earlier statement for %s\n",
			 name, hp->hostSpec);
	    return -EINVAL;
	}
	hp->specOps |= specOps;
	hp->denyOps |= (specOps & denyOps);
	if (maxCons)
	    hp->maxCons = maxCons;
    }
    else {
	/* Move any subsequent hosts down to make room for the new entry*/
	if (i < nHosts)
	    memmove(&hostList[i+1], &hostList[i],
		    (nHosts - i) * sizeof(HostInfo));
	hp->hostSpec = strdup(name);
	hp->hostId.s_addr = hostId.s_addr;
	hp->hostMask.s_addr = hostMask.s_addr;
	hp->level = level;
	hp->specOps = specOps;
	hp->denyOps = specOps & denyOps;
	hp->maxCons = maxCons;
	hp->curCons = 0;
	nHosts++;
    }

    return 0;
}

/* Called after accepting new client's connection to check that another
 * connection from its host is permitted and to find which operations the
 * client is permitted to perform.
 * hostId is the IP address of the host that the client is running on
 * denyOpsResult is a pointer to return the capability vector
 */
int
__pmAccAddClient(const struct in_addr *hostId, unsigned int *denyOpsResult)
{
    int			i;
    HostInfo		*hp;
    HostInfo		*lastMatch = NULL;
    struct in_addr	clientId;

    clientId.s_addr = hostId->s_addr;

    /* Map "localhost" to the real IP address.  Host access statements for
     * localhost are mapped to the "real" IP address so that wildcarding works
     * consistently.
     */
    if (clientId.s_addr == htonl(INADDR_LOOPBACK)) {
	if (!gotMyHostId)
	    GetMyHostId();

	if (gotMyHostId > 0)
	    clientId.s_addr = myHostId.s_addr;
	else
	    return PM_ERR_PERMISSION;
    }


    *denyOpsResult = 0;			/* deny nothing == allow all */
    if (nHosts == 0)			/* No access controls => allow all */
	return 0;

    for (i = nHosts - 1; i >= 0; i--) {
	hp = &hostList[i];
	if ((hp->hostMask.s_addr & clientId.s_addr) == hp->hostId.s_addr) {
	    /* Clobber specified ops then set. Leave unspecified ops alone. */
	    *denyOpsResult &= ~hp->specOps;
	    *denyOpsResult |= hp->denyOps;
	    lastMatch = hp;
	}
    }
    /* no matching entry in hostList => allow all */

    /* If no operations are allowed, disallow connection */
    if (*denyOpsResult == all_ops)
	return PM_ERR_PERMISSION;

    /* Check for connection limit */
    if (lastMatch != NULL && lastMatch->maxCons &&
	lastMatch->curCons >= lastMatch->maxCons) {

	*denyOpsResult = all_ops;
	return PM_ERR_CONNLIMIT;
    }

    /* Increment the count of current connections for ALL host specs in the
     * host access list that match the client's IP address.  A client may
     * contribute to several connection counts because of wildcarding.
     */
    for (i = 0; i < nHosts; i++) {
	hp = &hostList[i];
	if ((hp->hostMask.s_addr & clientId.s_addr) == hp->hostId.s_addr)
	    if (hp->maxCons)
		hp->curCons++;
    }

    return 0;
}

void
__pmAccDelClient(const struct in_addr *hostId)
{
    int		i;
    HostInfo	*hp;

    /* Increment the count of current connections for ALL host specs in the
     * host access list that match the client's IP address.  A client may
     * contribute to several connection counts because of wildcarding.
     */
    for (i = 0; i < nHosts; i++) {
	hp = &hostList[i];
	if ((hp->hostMask.s_addr & hostId->s_addr) == hp->hostId.s_addr)
 	    if (hp->maxCons)
		hp->curCons--;
    }
}

void
__pmAccDumpHosts(FILE *stream)
{
    int			h, i;
    int			minbit = -1, maxbit;
    unsigned int	mask;
    HostInfo		*hp;

    mask = all_ops;
    for (i = 0; mask; i++) {
	if (mask % 2)
	    if (minbit < 0)
		minbit = i;
	mask = mask >> 1;
    }
    maxbit = i - 1;

    if (nHosts == 0) {
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
    for (h = 0; h < nHosts; h++) {
	hp = &hostList[h];

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
	fprintf(stream, "%5d %5d  %08x  %08x %3d %s\n", hp->curCons, hp->maxCons,
		(int)hp->hostId.s_addr, (int)hp->hostMask.s_addr, hp->level, hp->hostSpec);
    }
    putc('\n', stream);
}

