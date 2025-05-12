/*
 * Copyright (c) 2012-2015 Red Hat.
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
 * Thread-safe notes
 *
 * namebuf and namebuflen - no serious side-effects, one-trip code and worse
 * outcome would be a small memory leak, so don't bother to make thread-safe
 */

#include <limits.h>
#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"
#if defined(HAVE_PWD_H)
#include <pwd.h>
#endif
#if defined(HAVE_GRP_H)
#include <grp.h>
#endif

static char	*namebuf;
static int	namebuflen;

static void
init_namebuf(void)
{
    if (namebuflen == 0) {
	/*
	 * one trip initialization to setup namebuf to be big
	 * enough for all of:
	 *    getgrgid_r() and getgrnam_r() - _SC_GETGR_R_SIZE_MAX
	 *    getpwnam_r() and getpwuid_r() - _SC_GETPW_R_SIZE_MAX
	 */
	long	len;

	namebuflen = -1;

#if defined(HAVE_GETGRGID_R) || defined(HAVE_GETGRNAM_R)
	len = sysconf(_SC_GETGR_R_SIZE_MAX);
	if (len > namebuflen)
	    namebuflen = len;
#endif
#if defined(HAVE_GETPWUID_R) || defined(HAVE_GETPWNAM_R)
	len = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (len > namebuflen)
	    namebuflen = len;
#endif
	if (namebuflen <= 0) {
	    /*
	     * sysconf() is not helping
	     * empirically 1024 is enough for Linux, but 2048
	     * is not enough for OpenBSD, so ... punt!
	     */
	    namebuflen = 4096;
	}
	if ((namebuf = (char *)malloc(namebuflen)) == NULL) {
	    pmNoMem("init_namebuf malloc", namebuflen, PM_RECOV_ERR);
	    namebuflen = -1;		/* block one trip code in future */
	}
	if (pmDebugOptions.access)
	    fprintf(stderr, "init_namebuf: namebuflen=%d namebuf=%p\n", namebuflen, namebuf);
    }
}

int
__pmEqualUserIDs(__pmUserID uid1, __pmUserID uid2)
{
    return uid1 == uid2;
}

int
__pmEqualGroupIDs(__pmGroupID gid1, __pmGroupID gid2)
{
    return gid1 == gid2;
}

int
__pmValidUserID(__pmUserID uid)
{
    /* compiler may emit warning if uid is unsigned ... no known workaround */
    return uid >= 0;
}

int __pmValidGroupID(__pmGroupID gid)
{
    /* compiler may emit warning if gid is unsigned ... no known workaround */
    return gid >= 0;
}

void
__pmUserIDFromString(const char *userid, __pmUserID *uid)
{
    *uid = atoi(userid);
}

void
__pmGroupIDFromString(const char *groupid, __pmGroupID *gid)
{
    *gid = atoi(groupid);
}

char *
__pmUserIDToString(__pmUserID uid, char *buf, size_t size)
{
    pmsprintf(buf, size, "%u", (unsigned int)uid);
    buf[size-1] = '\0';
    return buf;
}

char *
__pmGroupIDToString(__pmGroupID gid, char *buf, size_t size)
{
    pmsprintf(buf, size, "%u", (unsigned int)gid);
    buf[size-1] = '\0';
    return buf;
}

#if defined(HAVE_GETGRGID_R)
char *
__pmGroupnameFromID(gid_t gid, char *buf, size_t size)
{
    struct group grp, *result;
    int	sts;

    init_namebuf();
    if (namebuf == NULL)
	return "??? no buffer";

    sts = getgrgid_r(gid, &grp, namebuf, namebuflen, &result);
    if (sts < 0 || result == NULL)
	return "unknown";
    else {
	pmsprintf(buf, size, "%s", result->gr_name);
	buf[size-1] = '\0';
	return buf;
    }
}
#elif defined(HAVE_GETGRGID)
char *
__pmGroupnameFromID(gid_t gid, char *buf, size_t size)
{
    struct group *result;

    PM_LOCK(__pmLock_extcall);
    result = getgrgid(gid);		/* THREADSAFE */
    pmsprintf(buf, size, "%s", result ? result->gr_name : "unknown");
    PM_UNLOCK(__pmLock_extcall);
    buf[size-1] = '\0';
    return buf;
}
#else
!bozo!
#endif

#if defined(HAVE_GETPWUID_R)
char *
__pmUsernameFromID(uid_t uid, char *buf, size_t size)
{
    struct passwd pwd, *result;
    int sts;

    init_namebuf();
    if (namebuf == NULL)
	return "??? no buffer";

    sts = getpwuid_r(uid, &pwd, namebuf, namebuflen, &result);
    if (sts < 0 || result == NULL)
	return "unknown";
    else {
	pmsprintf(buf, size, "%s", result->pw_name);
	buf[size-1] = '\0';
	return buf;
    }
}
#elif defined(HAVE_GETPWUID)
char *
__pmUsernameFromID(uid_t uid, char *buf, size_t size)
{
    struct passwd *result;

    PM_LOCK(__pmLock_extcall);
    result = getpwuid(uid);		/* THREADSAFE */
    pmsprintf(buf, size, "%s", result ? result->pw_name : "unknown");
    PM_UNLOCK(__pmLock_extcall);
    buf[size-1] = '\0';
    return buf;
}
#else
!bozo!
#endif

#if defined(HAVE_GETPWNAM_R)
int
__pmUsernameToID(const char *name, uid_t *uid)
{
    struct passwd pwd, *result = NULL;
    int sts;

    init_namebuf();
    if (namebuf == NULL)
	return -ENOMEM;

    sts = getpwnam_r(name, &pwd, namebuf, namebuflen, &result);
    if (sts < 0 || result == NULL)
	return -ENOENT;
    *uid = result->pw_uid;
    return 0;
}
#elif defined(HAVE_GETPWNAM)
int
__pmUsernameToID(const char *name, uid_t *uid)
{
    struct passwd *result;

    PM_LOCK(__pmLock_extcall);
    result = getpwnam(name);		/* THREADSAFE */
    if (result == NULL) {
	PM_UNLOCK(__pmLock_extcall);
	return -ENOENT;
    }
    *uid = result->pw_uid;
    PM_UNLOCK(__pmLock_extcall);
    return 0;
}
#else
!bozo!
#endif

#if defined(HAVE_GETGRNAM_R)
int
__pmGroupnameToID(const char *name, gid_t *gid)
{
    struct group grp, *result = NULL;
    int sts;

    init_namebuf();
    if (namebuf == NULL)
	return -ENOMEM;

    sts = getgrnam_r(name, &grp, namebuf, namebuflen, &result);
    if (sts < 0 || result == NULL)
	return -ENOENT;
    *gid = result->gr_gid;
    return 0;
}
#elif defined(HAVE_GETGRNAM)
int
__pmGroupnameToID(const char *name, gid_t *gid)
{
    struct group *result;

    PM_LOCK(__pmLock_extcall);
    result = getgrnam(name);		/* THREADSAFE */
    if (result == NULL) {
	PM_UNLOCK(__pmLock_extcall);
	return -ENOENT;
    }
    *gid = result->gr_gid;
    PM_UNLOCK(__pmLock_extcall);
    return 0;
}
#else
!bozo!
#endif

#if defined(HAVE_GETPWUID_R)
char *
__pmHomedirFromID(uid_t uid, char *buf, size_t size)
{
    struct passwd pwd, *result;
    char *env;

    init_namebuf();
    if (namebuf == NULL)
	return "??? no buffer";

    /*
     * Use $HOME, if it is set, otherwise get the information from
     * getpwuid_r()
     */
    PM_LOCK(__pmLock_extcall);
    env = getenv("HOME");		/* THREADSAFE */
    if (env != NULL) {
	pmsprintf(buf, size, "%s", env);
	PM_UNLOCK(__pmLock_extcall);
    }
    else {
	int sts;
	PM_UNLOCK(__pmLock_extcall);
	sts = getpwuid_r(uid, &pwd, namebuf, namebuflen, &result);
	if (sts < 0 || result == NULL)
	    return NULL;
	pmsprintf(buf, size, "%s", result->pw_dir);
    }
    buf[size-1] = '\0';
    return buf;
}
#elif defined(HAVE_GETPWUID)
char *
__pmHomedirFromID(uid_t uid, char *buf, size_t size)
{
    struct passwd *result;

    /*
     * Use $HOME, if it is set, otherwise get the information from
     * getpwuid()
     */
    PM_LOCK(__pmLock_extcall);
    env = getenv("HOME");		/* THREADSAFE */
    if (env != NULL) {
	pmsprintf(buf, size, "%s", env);
	PM_UNLOCK(__pmLock_extcall);
    }
    else {
	result = getpwuid(uid);			/* THREADSAFE */
	if (result == NULL) {
	    PM_UNLOCK(__pmLock_extcall);
	    return NULL;
	}
	pmsprintf(buf, size, "%s", result->pw_dir);
	PM_UNLOCK(__pmLock_extcall);
    }
    buf[size-1] = '\0';
    return buf;
}
#else
!bozo!
#endif

/*
 * Add a group ID into a group list, if it is not there already.
 * The current group ID list and size are passed in, updated if
 * changed, and passed back out.
 */
static int
__pmAddGroupID(gid_t gid, gid_t **gidlist, unsigned int *count)
{
    gid_t		*gidlist_new;
    size_t		need;
    unsigned int	i, total = *count;

    for (i = 0; i < total; i++)
	if ((*gidlist)[i] == gid)
	    return 0;	/* already in the list, we're done */

    need = (total + 1) * sizeof(gid_t);
    if ((gidlist_new = (gid_t *)realloc(*gidlist, need)) == NULL) {
	return -ENOMEM;
    }
    gidlist_new[total++] = gid;
    *gidlist = gidlist_new;
    *count = total;
    return 0;
}

#if defined(HAVE_GETPWNAM_R) && defined(HAVE_GETGRENT_R)
int
__pmUsersGroupIDs(const char *username, gid_t **groupids, unsigned int *ngroups)
{
    int			i, sts;
    unsigned int	count = 0;
    char		grbuf[1024];
    gid_t		*gidlist = NULL;
    struct passwd	pwd, *result = NULL;
    struct group	gr, *grp;

    sts = getpwnam_r(username, &pwd, grbuf, sizeof(grbuf), &result);
    if (sts < 0 || result == NULL)
	return -ENOENT;

    /* add the primary group in right away, before supplementary groups */
    if ((sts = __pmAddGroupID(result->pw_gid, &gidlist, &count)) < 0)
	return sts;

    /* search for groups in which the given user is a member */
    PM_LOCK(__pmLock_extcall);
    setgrent();			/* THREADSAFE */
    while (1) {
	grp = NULL;
#ifdef IS_SOLARIS
	/* THREADSAFE */
	if ((grp = getgrent_r(&gr, grbuf, sizeof(grbuf))) == NULL)
	    break;
#else
	/* THREADSAFE */
	if (getgrent_r(&gr, grbuf, sizeof(grbuf), &grp) != 0 || grp == NULL)
	    break;
#endif
	for (i = 0; grp->gr_mem[i]; i++) {
	    if (strcmp(username, grp->gr_mem[i]) != 0)
		continue;
	    /* THREADSAFE - no locks acquired in __pmAddGroupID() */
	    if ((sts = __pmAddGroupID(grp->gr_gid, &gidlist, &count)) < 0) {
		endgrent();		/* THREADSAFE */
		PM_UNLOCK(__pmLock_extcall);
		return sts;
	    }
	    break;
	}
    }
    endgrent();		/* THREADSAFE */
    PM_UNLOCK(__pmLock_extcall);

    *groupids = gidlist;
    *ngroups = count;
    return 0;
}
#elif defined(HAVE_GETPWNAM) && defined(HAVE_GETGRENT)
int
__pmUsersGroupIDs(const char *username, gid_t **groupids, unsigned int *ngroups)
{
    int			i, sts;
    unsigned int	count = 0;
    gid_t		gid;
    gid_t		*gidlist = NULL;
    struct passwd	*result;
    struct group	*grp;

    PM_LOCK(__pmLock_extcall);
    result = getpwnam(username);		/* THREADSAFE */
    if (result == NULL) {
	PM_UNLOCK(__pmLock_extcall);
	return -ENOENT;
    }
    gid = result->pw_gid;
    PM_UNLOCK(__pmLock_extcall);

    /* add the primary group in right away, before supplementary groups */
    if ((sts = __pmAddGroupID(gid, &gidlist, &count)) < 0)
	return sts;

    /* search for groups in which the given user is a member */
    PM_LOCK(__pmLock_extcall);
    setgrent();		/* THREADSAFE */
    while (1) {
	grp = NULL;
	if ((grp = getgrent()) == NULL)		/* THREADSAFE */
	    break;
	for (i = 0; grp->gr_mem[i]; i++) {
	    if (strcmp(username, grp->gr_mem[i]) != 0)
		continue;
	    /* THREADSAFE - no locks acquired in __pmAddGroupID() */
	    if ((sts = __pmAddGroupID(grp->gr_gid, &gidlist, &count)) < 0) {
		endgrent();		/* THREADSAFE */
		PM_UNLOCK(__pmLock_extcall);
		return sts;
	    }
	    break;
	}
    }
    endgrent();		/* THREADSAFE */
    PM_UNLOCK(__pmLock_extcall);

    *groupids = gidlist;
    *ngroups = count;
    return 0;
}
#else
!bozo!
#endif

/*
 * Add a user ID into a user list, if it is not there already.
 * The current user ID list and size are passed in, updated if
 * changed, and passed back out.
 */
static int
__pmAddUserID(uid_t uid, uid_t **uidlist, unsigned int *count)
{
    uid_t		*uidlist_new;
    size_t		need;
    unsigned int	i, total = *count;

    for (i = 0; i < total; i++)
	if ((*uidlist)[i] == uid)
	    return 0;	/* already in the list, we're done */

    need = (total + 1) * sizeof(uid_t);
    if ((uidlist_new = (uid_t *)realloc(*uidlist, need)) == NULL) {
	return -ENOMEM;
    }
    uidlist_new[total++] = uid;
    *uidlist = uidlist_new;
    *count = total;
    return 0;
}

#if defined(HAVE_GETGRNAM_R) && defined(HAVE_GETPWENT_R)
int
__pmGroupsUserIDs(const char *groupname, uid_t **userids, unsigned int *nusers)
{
    int			sts;
    uid_t		*uidlist = NULL;
    gid_t		groupid;
    char		grbuf[1024];
    char		buf[512];
    char		**names = NULL;
    struct group	gr, *grp = NULL;
    struct passwd	pw, *pwp;
    unsigned int	i, count = 0;

    /* for a given group name, find gid and user names */
    sts = getgrnam_r(groupname, &gr, grbuf, sizeof(grbuf), &grp);
    if (sts < 0 || grp == NULL)
	return -EINVAL;
    groupid = grp->gr_gid;
    names = grp->gr_mem;	/* supplementaries */

    /* for a given list of usernames, lookup the user IDs */
    PM_LOCK(__pmLock_extcall);
    setpwent();		/* THREADSAFE */
    while (1) {
#ifdef IS_SOLARIS
	/* THREADSAFE */
	if ((pwp = getpwent_r(&pw, buf, sizeof(buf))) == NULL)
	    break;
#else
	pwp = NULL;
	/* THREADSAFE */
	if (getpwent_r(&pw, buf, sizeof(buf), &pwp) != 0 || pwp == NULL)
	    break;
#endif
	/* check to see if this user has given group as primary */
	/* THREADSAFE - no locks acquired in __pmAddUserID() */
	if (pwp->pw_gid == groupid &&
	    (sts = __pmAddUserID(pwp->pw_uid, &uidlist, &count)) < 0) {
	    endpwent();		/* THREADSAFE */
	    PM_UNLOCK(__pmLock_extcall);
	    return sts;
	}
	/* check to see if this user is listed in groups file */
	for (i = 0; names[i]; i++) {
	    if (strcmp(pwp->pw_name, names[i]) == 0) {
		if ((sts = __pmAddUserID(pwp->pw_uid, &uidlist, &count)) < 0) {
		    endpwent();		/* THREADSAFE */
		    PM_UNLOCK(__pmLock_extcall);
		    return sts;
		}
		break;
	    }
	}
    }
    endpwent();		/* THREADSAFE */
    PM_UNLOCK(__pmLock_extcall);

    *userids = uidlist;
    *nusers = count;
    return 0;
}
#elif defined(HAVE_GETGRNAM) && defined(HAVE_GETPWENT)
int
__pmGroupsUserIDs(const char *name, uid_t **userids, unsigned int *nusers)
{
    int			sts;
    uid_t		*uidlist = NULL;
    gid_t		groupid;
    char		**names = NULL;
    struct group	*grp = NULL;
    struct passwd	*pwp;
    unsigned int	i, count = 0;

    /* for a given group name, find gid and user names */
    PM_LOCK(__pmLock_extcall);
    grp = getgrnam(name);		/* THREADSAFE */
    if (grp == NULL) {
	PM_UNLOCK(__pmLock_extcall);
	return -EINVAL;
    }
    groupid = grp->gr_gid;
    names = grp->gr_mem;

    setpwent();		/* THREADSAFE */
    while (1) {
	if ((pwp = getpwent()) == NULL)		/* THREADSAFE */
	    break;
	/* check to see if this user has given group as primary */
	/* THREADSAFE - no locks acquired in __pmAddUserID() */
	if (pwp->pw_gid == groupid &&
	    (sts = __pmAddUserID(pwp->pw_uid, &uidlist, &count)) < 0) {
	    endpwent();		/* THREADSAFE */
	    PM_UNLOCK(__pmLock_extcall);
	    return sts;
	}
	for (i = 0; names[i]; i++) {
	    if (strcmp(pwp->pw_name, names[i]) == 0) {
		if ((sts = __pmAddUserID(pwp->pw_uid, &uidlist, &count)) < 0) {
		    endpwent();		/* THREADSAFE */
		    PM_UNLOCK(__pmLock_extcall);
		    return sts;
		}
		break;
	    }
	}
    }
    endpwent();		/* THREADSAFE */
    PM_UNLOCK(__pmLock_extcall);

    *userids = uidlist;
    *nusers = count;
    return 0;
}
#else
!bozo!
#endif

#if defined(HAVE_GETPWNAM_R)
int
__pmGetUserIdentity(const char *username, uid_t *uid, gid_t *gid, int mode)
{
    int sts;
    char buf[4096];
    struct passwd pwd, *pw;

    sts = getpwnam_r(username, &pwd, buf, sizeof(buf), &pw);
    if (sts < 0) {
	if (mode == PM_FATAL_ERR || pmDebugOptions.access)
	    pmNotifyErr(LOG_CRIT, "getpwnam_r(%s) failed: %s\n",
		    username, pmErrStr_r(sts, buf, sizeof(buf)));
	if (mode == PM_FATAL_ERR)
	    exit(1);
	return -ENOENT;
    }
    else if (pw == NULL) {
	if (mode == PM_FATAL_ERR || pmDebugOptions.access)
	    pmNotifyErr(LOG_CRIT,
		    "cannot find the %s user to switch to\n", username);
	if (mode == PM_FATAL_ERR)
	    exit(1);
	return -ENOENT;
    }
    *uid = pwd.pw_uid;
    *gid = pwd.pw_gid;
    return 0;
}
#elif defined(HAVE_GETPWNAM)
int
__pmGetUserIdentity(const char *username, uid_t *uid, gid_t *gid, int mode)
{
    int sts;
    char errmsg[128];
    struct passwd *pw;

    setoserror(0);
    PM_LOCK(__pmLock_extcall);
    pw = getpwnam(username);		/* THREADSAFE */
    if (pw == NULL) {
	PM_UNLOCK(__pmLock_extcall);
	if (mode == PM_FATAL_ERR || pmDebugOptions.access)
	    pmNotifyErr(LOG_INFO,
		    "cannot find the %s user to switch to\n", username);
	if (mode == PM_FATAL_ERR)
	    exit(1);
	return -ENOENT;
    }
    else if (oserror() != 0) {
	PM_UNLOCK(__pmLock_extcall);
	if (mode == PM_FATAL_ERR || pmDebugOptions.access)
	     pmNotifyErr(LOG_CRIT, "getpwnam(%s) failed: %s\n",
		    username, pmErrStr_r(oserror(), errmsg, sizeof(errmsg)));
	if (mode == PM_FATAL_ERR)
	    exit(1);
	return -ENOENT;
    }
    *uid = pw->pw_uid;
    *gid = pw->pw_gid;
    PM_UNLOCK(__pmLock_extcall);
    return 0;
}
#else
!bozo!
#endif

int
pmSetProcessIdentity(const char *username)
{
    gid_t gid;
    uid_t uid;
    char msg[256];

    __pmGetUserIdentity(username, &uid, &gid, PM_FATAL_ERR);

    if (setgid(gid) < 0) {
	pmNotifyErr(LOG_CRIT,
		"setgid to gid of %s user (gid=%d): %s",
		username, gid, osstrerror_r(msg, sizeof(msg)));
	exit(1);
    }

    /*
     * initgroups(3) has been observed to be expensive in
     * terms of its memory footprint - which blows out the
     * PCP daemons (all call this, including PMDAs) - as a
     * result we check first and call it only when needed.
     * Usually it is not, as PCP_USER has no supplementary
     * groups by default.
     *
     * We must allow initgroups to fail with EPERM, as this
     * is the behaviour when the parent process has already
     * dropped privileges (e.g. pmcd receives SIGHUP).
     */
    if (getgroups(0, NULL) > 0 &&
	initgroups(username, gid) < 0 && oserror() != EPERM) {
	pmNotifyErr(LOG_CRIT,
		"initgroups with gid of %s user (gid=%d): %s",
		username, gid, osstrerror_r(msg, sizeof(msg)));
	exit(1);
    }

    if (setuid(uid) < 0) {
	pmNotifyErr(LOG_CRIT,
		"setuid to uid of %s user (uid=%d): %s",
		username, uid, osstrerror_r(msg, sizeof(msg)));
	exit(1);
    }

    return 0;
}
