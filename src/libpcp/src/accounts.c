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
 */

#include <limits.h>
#include "pmapi.h"
#include "impl.h"
#include "internal.h"
#if defined(HAVE_PWD_H)
#include <pwd.h>
#endif
#if defined(HAVE_GRP_H)
#include <grp.h>
#endif

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
    return uid >= 0;
}

int __pmValidGroupID(__pmGroupID gid)
{
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
    snprintf(buf, size, "%u", (unsigned int)uid);
    buf[size-1] = '\0';
    return buf;
}

char *
__pmGroupIDToString(__pmGroupID gid, char *buf, size_t size)
{
    snprintf(buf, size, "%u", (unsigned int)gid);
    buf[size-1] = '\0';
    return buf;
}

#if defined(HAVE_GETGRGID_R)
char *
__pmGroupnameFromID(gid_t gid, char *buf, size_t size)
{
    char namebuf[1024];
    struct group grp, *result;

    getgrgid_r(gid, &grp, namebuf, sizeof(namebuf), &result);
    snprintf(buf, size, "%s", result ? result->gr_name : "unknown");
    buf[size-1] = '\0';
    return buf;
}
#elif defined(HAVE_GETGRGID)
char *
__pmGroupnameFromID(gid_t gid, char *buf, size_t size)
{
    struct group *result;

    result = getgrgid(gid);
    snprintf(buf, size, "%s", result ? result->gr_name : "unknown");
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
    char namebuf[1024];
    struct passwd pwd, *result;

    getpwuid_r(uid, &pwd, namebuf, sizeof(namebuf), &result);
    snprintf(buf, size, "%s", result ? result->pw_name : "unknown");
    buf[size-1] = '\0';
    return buf;
}
#elif defined(HAVE_GETPWUID)
char *
__pmUsernameFromID(uid_t uid, char *buf, size_t size)
{
    struct passwd *result;

    result = getpwuid(uid);
    snprintf(buf, size, "%s", result ? result->pw_name : "unknown");
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
    char namebuf[1024];
    struct passwd pwd, *result = NULL;

    getpwnam_r(name, &pwd, namebuf, sizeof(namebuf), &result);
    if (!result)
	return -ENOENT;
    *uid = result->pw_uid;
    return 0;
}
#elif defined(HAVE_GETPWNAM)
int
__pmUsernameToID(const char *name, uid_t *uid)
{
    struct passwd *result;

    result = getpwnam(name);
    if (!result)
	return -ENOENT;
    *uid = result->pw_uid;
    return 0;
}
#else
!bozo!
#endif

#if defined(HAVE_GETGRNAM_R)
int
__pmGroupnameToID(const char *name, gid_t *gid)
{
    char namebuf[512];
    struct group grp, *result = NULL;

    getgrnam_r(name, &grp, namebuf, sizeof(namebuf), &result);
    if (result == NULL)
	return -ENOENT;
    *gid = result->gr_gid;
    return 0;
}
#elif defined(HAVE_GETGRNAM)
int
__pmGroupnameToID(const char *name, gid_t *gid)
{
    struct group *result;

    result = getgrnam(name);
    if (result == NULL)
	return -ENOENT;
    *gid = result->gr_gid;
    return 0;
}
#else
!bozo!
#endif

#if defined(HAVE_GETPWUID_R)
char *
__pmHomedirFromID(uid_t uid, char *buf, size_t size)
{
    char namebuf[1024];
    struct passwd pwd, *result;
    char *env;

    /*
     * Use $HOME, if it is set, otherwise get the information from
     * getpwuid_r()
     */
    env = getenv("HOME");
    if (env != NULL)
	snprintf(buf, size, "%s", env);
    else {
	getpwuid_r(uid, &pwd, namebuf, sizeof(namebuf), &result);
	if (result == NULL)
	    return NULL;
	snprintf(buf, size, "%s", result->pw_dir);
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
    env = getenv("HOME");
    if (env != NULL)
	snprintf(buf, size, "%s", env);
    else {
	result = getpwuid(uid);
	if (result == NULL)
	    return NULL;
	snprintf(buf, size, "%s", result->pw_dir);
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
    gid_t		*gids = *gidlist;
    size_t		need;
    unsigned int	i, total = *count;

    for (i = 0; i < total; i++)
	if (gids[i] == gid)
	    return 0;	/* already in the list, we're done */

    need = (total + 1) * sizeof(gid_t);
    if ((gids = (gid_t *)realloc(gids, need)) == NULL)
	return -ENOMEM;
    gids[total++] = gid;
    *gidlist = gids;
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

    getpwnam_r(username, &pwd, grbuf, sizeof(grbuf), &result);
    if (!result)
	return -ENOENT;

    /* add the primary group in right away, before supplementary groups */
    if ((sts = __pmAddGroupID(result->pw_gid, &gidlist, &count)) < 0)
	return sts;

    /* search for groups in which the given user is a member */
    setgrent();
    while (1) {
	grp = NULL;
#ifdef IS_SOLARIS
	if ((grp = getgrent_r(&gr, grbuf, sizeof(grbuf))) == NULL)
	    break;
#else
	if (getgrent_r(&gr, grbuf, sizeof(grbuf), &grp) != 0 || grp == NULL)
	    break;
#endif
	for (i = 0; grp->gr_mem[i]; i++) {
	    if (strcmp(username, grp->gr_mem[i]) != 0)
		continue;
	    if ((sts = __pmAddGroupID(grp->gr_gid, &gidlist, &count)) < 0) {
		endgrent();
		return sts;
	    }
	    break;
	}
    }
    endgrent();

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
    gid_t		*gidlist = NULL;
    struct passwd	*result;
    struct group	*grp;

    result = getpwnam(username);
    if (!result)
	return -ENOENT;

    /* add the primary group in right away, before supplementary groups */
    if ((sts = __pmAddGroupID(result->pw_gid, &gidlist, &count)) < 0)
	return sts;

    /* search for groups in which the given user is a member */
    setgrent();
    while (1) {
	grp = NULL;
	if ((grp = getgrent()) == NULL)
	    break;
	for (i = 0; grp->gr_mem[i]; i++) {
	    if (strcmp(username, grp->gr_mem[i]) != 0)
		continue;
	    if ((sts = __pmAddGroupID(grp->gr_gid, &gidlist, &count)) < 0) {
		endgrent();
		return sts;
	    }
	    break;
	}
    }
    endgrent();

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
    uid_t		*uids = *uidlist;
    size_t		need;
    unsigned int	i, total = *count;

    for (i = 0; i < total; i++)
	if (uids[i] == uid)
	    return 0;	/* already in the list, we're done */

    need = (total + 1) * sizeof(uid_t);
    if ((uids = (uid_t *)realloc(uids, need)) == NULL)
	return -ENOMEM;
    uids[total++] = uid;
    *uidlist = uids;
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
    getgrnam_r(groupname, &gr, grbuf, sizeof(grbuf), &grp);
    if (grp == NULL)
	return -EINVAL;
    groupid = grp->gr_gid;
    names = grp->gr_mem;	/* supplementaries */

    /* for a given list of usernames, lookup the user IDs */
    setpwent();
    while (1) {
#ifdef IS_SOLARIS
	if ((pwp = getpwent_r(&pw, buf, sizeof(buf))) == NULL)
	    break;
#else
	pwp = NULL;
	if (getpwent_r(&pw, buf, sizeof(buf), &pwp) != 0 || pwp == NULL)
	    break;
#endif
	/* check to see if this user has given group as primary */
	if (pwp->pw_gid == groupid &&
	    (sts = __pmAddUserID(pwp->pw_uid, &uidlist, &count)) < 0) {
	    endpwent();
	    return sts;
	}
	/* check to see if this user is listed in groups file */
	for (i = 0; names[i]; i++) {
	    if (strcmp(pwp->pw_name, names[i]) == 0) {
		if ((sts = __pmAddUserID(pwp->pw_uid, &uidlist, &count)) < 0) {
		    endpwent();
		    return sts;
		}
		break;
	    }
	}
    }
    endpwent();

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
    if ((grp = getgrnam(name)) == NULL)
	return -EINVAL;
    groupid = grp->gr_gid;
    names = grp->gr_mem;

    setpwent();
    while (1) {
	if ((pwp = getpwent()) == NULL)
	    break;
	/* check to see if this user has given group as primary */
	if (pwp->pw_gid == groupid &&
	    (sts = __pmAddUserID(pwp->pw_uid, &uidlist, &count)) < 0) {
	    endpwent();
	    return sts;
	}
	for (i = 0; names[i]; i++) {
	    if (strcmp(pwp->pw_name, names[i]) == 0) {
		if ((sts = __pmAddUserID(pwp->pw_uid, &uidlist, &count)) < 0) {
		    endpwent();
		    return sts;
		}
		break;
	    }
	}
    }
    endpwent();

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
    if (pw == NULL) {
	__pmNotifyErr(LOG_CRIT,
		"cannot find the %s user to switch to\n", username);
	if (mode == PM_FATAL_ERR)
	    exit(1);
	return -ENOENT;
    } else if (sts != 0) {
	__pmNotifyErr(LOG_CRIT, "getpwnam_r(%s) failed: %s\n",
		username, pmErrStr_r(sts, buf, sizeof(buf)));
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
    if ((pw = getpwnam(username)) == 0) {
	__pmNotifyErr(LOG_CRIT,
		"cannot find the %s user to switch to\n", username);
	if (mode == PM_FATAL_ERR)
	    exit(1);
	return -ENOENT;
    } else if (oserror() != 0) {
	__pmNotifyErr(LOG_CRIT, "getpwnam(%s) failed: %s\n",
		username, pmErrStr_r(oserror(), errmsg, sizeof(errmsg)));
	if (mode == PM_FATAL_ERR)
	    exit(1);
	return -ENOENT;
    }
    *uid = pw->pw_uid;
    *gid = pw->pw_gid;
    return 0;
}
#else
!bozo!
#endif

int
__pmSetProcessIdentity(const char *username)
{
    gid_t gid;
    uid_t uid;
    char msg[256];

    __pmGetUserIdentity(username, &uid, &gid, PM_FATAL_ERR);

    if (setgid(gid) < 0) {
	__pmNotifyErr(LOG_CRIT,
		"setgid to gid of %s user (gid=%d): %s",
		username, gid, osstrerror_r(msg, sizeof(msg)));
	exit(1);
    }

    /*
     * We must allow initgroups to fail with EPERM, as this
     * is the behaviour when the parent process has already
     * dropped privileges (e.g. pmcd receives SIGHUP).
     */
    if (initgroups(username, gid) < 0 && oserror() != EPERM) {
	__pmNotifyErr(LOG_CRIT,
		"initgroups with gid of %s user (gid=%d): %s",
		username, gid, osstrerror_r(msg, sizeof(msg)));
	exit(1);
    }

    if (setuid(uid) < 0) {
	__pmNotifyErr(LOG_CRIT,
		"setuid to uid of %s user (uid=%d): %s",
		username, uid, osstrerror_r(msg, sizeof(msg)));
	exit(1);
    }

    return 0;
}
