/*
 * Copyright (c) 2012-2018,2020-2022,2025 Red Hat.
 * Copyright (c) 2007-2008 Aconex.  All Rights Reserved.
 * Copyright (c) 1995-2002,2004,2006,2008 Silicon Graphics, Inc.  All Rights Reserved.
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
 * curr_handle needs to be thread-private
 * curr_ctx needs to be thread-private
 *
 * contexts[], contexts_map[], contexts_len and last_handle are protected
 * from changes * using the local contexts_lock mutex.
 *
 * Ditto for back n_backoff, def_backoff[] and backoff[].
 *
 * The actual contexts (__pmContext) are protected by the c_lock mutex
 * (no longer recursive) which is intialized in pmNewContext()
 * and pmDupContext(), then locked in __pmHandleToPtr() ... it is
 * the responsibility of all __pmHandleToPtr() callers to call
 * PM_UNLOCK(ctxp->c_lock) when they are finished with the context.
 */

#include "pmapi.h"
#include "libpcp.h"
#include "fault.h"
#include "internal.h"
#include <string.h>
#include <assert.h>
#include <ctype.h>

static __pmContext	**contexts;		/* array of context ptrs */
static int		contexts_len;		/* number of contexts */
static int		last_handle = -1;	/* last returned context handle */
/*
 * For handle x above the PMAPI, if the context is valid, then for some
 * j (<=0 and < contexts_len), contexts_map[j] == x, and the real
 * __pmContext is found via contexts[j]
 */
static int		*contexts_map;

/*
 * Special sentinals for contexts_map[] ...
 */
#define MAP_FREE	-1		/* contexts[i] can be reused */
#define MAP_INIT	-2		/* contexts[i] is being setup */
#define MAP_TEARDOWN	-3		/* contexts[i] is being destroyed */

#ifdef PM_MULTI_THREAD
#ifdef HAVE___THREAD
/* using a gcc construct here to make curr_handle thread-private */
static __thread int	curr_handle = PM_CONTEXT_UNDEF;	/* current context # */
static __thread __pmContext	*curr_ctxp = NULL;	/* -> current __pmContext */
#endif
#else
static int		curr_handle = PM_CONTEXT_UNDEF;	/* current context # */
static __pmContext	*curr_ctxp = NULL;		/* -> current __pmContext */
#endif

static int		n_backoff;
static int		def_backoff[] = {5, 10, 20, 40, 80};
static int		*backoff;

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	contexts_lock = PTHREAD_MUTEX_INITIALIZER;
#else
void			*contexts_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == contexts_lock
 */
int
__pmIsContextsLock(void *lock)
{
    return lock == (void *)&contexts_lock;
}
#endif

/*
 * Given a handle above the PMAPI, do the mapping to the index (ctxnum)
 * of the matching contexts[] entry
 */
static int
map_handle_nolock(int handle)
{
    int		ctxnum = -1;
    int		i;

    for (i = 0; i < contexts_len; i++) {
	if (contexts_map[i] == handle && contexts_map[i] >= 0) {
	    if (contexts[i]->c_type != PM_CONTEXT_INIT) {
		ctxnum = i;
		break;
	    }
	}
    }
    return ctxnum;
}

static int
map_handle(int handle)
{
    PM_ASSERT_IS_LOCKED(contexts_lock);

    return map_handle_nolock(handle);
}

static void
waitawhile(__pmPMCDCtl *ctl)
{
    /*
     * after failure, compute delay before trying again ...
     */
    PM_LOCK(contexts_lock);
    if (n_backoff == 0) {
	char	*q;
	int	bad = 0;
	int	*backoff_new;
	/* first time ... try for PMCD_RECONNECT_TIMEOUT from env */
	PM_LOCK(__pmLock_extcall);
	q = getenv("PMCD_RECONNECT_TIMEOUT");		/* THREADSAFE */
	if (q != NULL)
	    q = strdup(q);
	PM_UNLOCK(__pmLock_extcall);
	if (q != NULL) {
	    char	*pend;
	    char	*p;
	    int		val;

	    for (p = q; *p != '\0'; ) {
		val = (int)strtol(p, &pend, 10);
		if (val <= 0 || (*pend != ',' && *pend != '\0')) {
		    /* report error below, after contexts_lock released */
		    if (backoff != NULL)
			free(backoff);
		    n_backoff = 0;
		    bad = 1;
		    break;
		}
		if ((backoff_new = (int *)realloc(backoff, (n_backoff+1) * sizeof(backoff[0]))) == NULL) {
		    pmNoMem("pmReconnectContext", (n_backoff+1) * sizeof(backoff[0]), PM_FATAL_ERR);
		    /* NOTREACHED */
		}
		backoff = backoff_new;
		backoff[n_backoff++] = val;
		if (*pend == '\0')
		    break;
		p = &pend[1];
	    }
	}
	if (n_backoff == 0) {
	    /* use default */
	    n_backoff = 5;
	    backoff = def_backoff;
	}
	if (bad) {
	    PM_UNLOCK(contexts_lock);
	    pmNotifyErr(LOG_WARNING,
			 "pmReconnectContext: ignored bad PMCD_RECONNECT_TIMEOUT = '%s'\n",
			 q);
	    PM_LOCK(contexts_lock);
	}
	if (q != NULL)
	    free(q);
    }
    if (ctl->pc_timeout == 0)
	ctl->pc_timeout = 1;
    else if (ctl->pc_timeout < n_backoff)
	ctl->pc_timeout++;
    ctl->pc_again = time(NULL) + backoff[ctl->pc_timeout-1];
    PM_UNLOCK(contexts_lock);
}

/*
 * On success, context is locked and caller should unlock it
 */
__pmContext *
__pmHandleToPtr(int handle)
{
    int		i;

    PM_LOCK(contexts_lock);
    for (i = 0; i < contexts_len; i++) {
	if (contexts_map[i] == handle && contexts_map[i] >= 0) {
	    if (contexts[i]->c_type > PM_CONTEXT_UNDEF) {
		__pmContext	*sts = contexts[i];
		/*
		 * Important Note:
		 *   Once c_lock is locked for _any_ context, the caller
		 *   cannot call into the routines here where contexts_lock
		 *   is acquired without first releasing the c_lock for all
		 *   contexts that are locked.
		 */
		PM_LOCK(sts->c_lock);
		/*
		 * Note:
		 *   Since we're holding the contexts_lock no
		 *   pmDestroyContext() for this context can happen between
		 *   the test above and the lock being granted ... and
		 *   without a pmContextDestroy() there can be no reuse
		 *   of the __pmContext struct, so the asserts below are
		 *   to-be-sure-to-be-sure.
		 */
		PM_UNLOCK(contexts_lock);
		assert(sts->c_handle == handle);
		assert(sts->c_type > PM_CONTEXT_UNDEF);
		return sts;
	    }
	}
    }
    PM_UNLOCK(contexts_lock);
    return NULL;
}

int
__pmPtrToHandle(__pmContext *ctxp)
{
    return ctxp->c_handle;
}

/*
 * Determine the hostname associated with the given context.
 */
int
pmGetHostName(int handle, char *buf, int buflen)
{
    __pmContext *ctxp;
    const char	*name;
    pmID	pmid;
    __pmResult	*resp;
    __pmContext	*save_ctxp;
    int		save_handle;
    int		sts = 0;

    PM_INIT_LOCKS();

    buf[0] = '\0';

    if ((ctxp = __pmHandleToPtr(handle)) != NULL) {
	switch (ctxp->c_type) {
	case PM_CONTEXT_HOST:
	    /*
	     * Try and establish the hostname from PMCD (possibly remote).
	     * Do not nest the successive actions. That way, if any one of
	     * them fails, we take the default.
	     * Note: we must *temporarily* switch context (see pmUseContext)
	     * in the host case, then switch back afterward. We already hold
	     * locks and have validated the context pointer, so we do a mini
	     * context switch, then switch back.
	     */
	    if (pmDebugOptions.context)
		fprintf(stderr, "pmGetHostName context(%d) -> 0\n", handle);
	    save_handle = PM_TPD(curr_handle);
	    save_ctxp = PM_TPD(curr_ctxp);
	    PM_TPD(curr_handle) = handle;
	    PM_TPD(curr_ctxp) = ctxp;

	    name = "pmcd.hostname";
	    sts = pmLookupName_ctx(ctxp, PM_NOT_LOCKED, 1, &name, &pmid);
	    if (sts >= 0)
		sts = pmFetch_ctx(ctxp, 1, &pmid, &resp);
	    if (pmDebugOptions.context)
		fprintf(stderr, "pmGetHostName reset(%d) -> 0\n", save_handle);

	    PM_TPD(curr_handle) = save_handle;
	    PM_TPD(curr_ctxp) = save_ctxp;
	    if (sts >= 0) {
		if (resp->vset[0]->numval > 0 &&
		    (resp->vset[0]->valfmt == PM_VAL_DPTR || resp->vset[0]->valfmt == PM_VAL_SPTR)) {
		    /* pmcd.hostname present */
		    pmstrncpy(buf, buflen, resp->vset[0]->vlist[0].value.pval->vbuf);
		    __pmFreeResult(resp);
		    break;
		}
		__pmFreeResult(resp);
		/* FALLTHROUGH */
	    }

	    /*
	     * We could not get the hostname from PMCD.  If the name in the
	     * context structure is a filesystem path (AF_UNIX address) or
	     * 'localhost', then use gethostname(). Otherwise, use the name
	     * from the context structure.
	     */
	    name = ctxp->c_pmcd->pc_hosts[0].name;
	    if (!name || name[0] == pmPathSeparator() || /* AF_UNIX */
		(strncmp(name, "localhost", 9) == 0)) /* localhost[46] */
		gethostname(buf, buflen);
	    else
		pmstrncpy(buf, buflen, name);
	    break;

	case PM_CONTEXT_LOCAL:
	    gethostname(buf, buflen);
	    break;

	case PM_CONTEXT_ARCHIVE:
	    pmstrncpy(buf, buflen, ctxp->c_archctl->ac_log->label.hostname);
	    break;
	}

	PM_UNLOCK(ctxp->c_lock);
    }

    return sts;
}

/*
 * Backward-compatibility interface, does not return status code.
 */
char *
pmGetContextHostName_r(int handle, char *buf, int buflen)
{
    pmGetHostName(handle, buf, buflen);
    return buf;
}

/*
 * Backward-compatibility interface, non-thread-safe variant.
 */
const char *
pmGetContextHostName(int handle)
{
    static char	hostbuf[MAXHOSTNAMELEN];
    return (const char *)pmGetContextHostName_r(handle, hostbuf, (int)sizeof(hostbuf));
}

int
pmWhichContext(void)
{
    /*
     * return curr_handle, provided it is defined
     */
    int		sts;

    PM_INIT_LOCKS();

    if (PM_TPD(curr_handle) > PM_CONTEXT_UNDEF)
	sts = PM_TPD(curr_handle);
    else
	sts = PM_ERR_NOCONTEXT;

    if (pmDebugOptions.context)
	fprintf(stderr, "pmWhichContext() -> %d, cur=%d\n",
	    sts, PM_TPD(curr_handle));
    return sts;
}

int
__pmConvertTimeout(int timeo)
{
    double tout_msec;

    switch (timeo) {
    case TIMEOUT_NEVER:
	return -1;

    case TIMEOUT_DEFAULT:
	tout_msec = __pmRequestTimeout() * 1000.0;
	break;

    case TIMEOUT_CONNECT:
	tout_msec = __pmConnectTimeout() * 1000.0;
	break;

    default:
	tout_msec = timeo * 1000.0;
	break;
    }

    return (int)tout_msec;
}

#ifdef PM_MULTI_THREAD
/*
 * Called with contexts_lock mutex held.
 */
static void
initcontextlock(pthread_mutex_t *lock)
{

    PM_ASSERT_IS_LOCKED(contexts_lock);

    __pmInitMutex(lock);
}

#else
#define initcontextlock(x)	do { } while (0)
#endif

int
__pmCheckAttribute(__pmAttrKey attr, const char *name)
{
    const char *p;

    switch (attr) {
    case PCP_ATTR_CONTAINER:
	/*
	 * Container names are parsed later as part of JSONB labelsets,
	 * ensure the name is drawn from a limited character set that
	 * is not going to result in conflicts with any other uses.
	 */
	for (p = name; *p; p++) {
	    if (isalnum(*p) || *p == '.' || *p == '-' || *p == '_')
		continue;
	    return -EINVAL;
	}
	if (p == name)
	    return -EINVAL;
	break;

    case PCP_ATTR_USERNAME:
	/*
	 * Check for POSIX user names - alphabetics, digits, period,
	 * underscore, and hyphen, with the further restriction that
	 * hyphen is not allowed as first character of a user name.
	 */
	if (name[0] == '-' || name[0] == '\0')
	    return -EINVAL;
	for (p = name; *p; p++) {
	    if (isalnum(*p) || *p == '.' || *p == '-' || *p == '_')
		continue;
	    return -EINVAL;
	}
	break;

    case PCP_ATTR_USERID:
    case PCP_ATTR_GROUPID:
    case PCP_ATTR_PROCESSID:
	/*
	 * PID, UID or GID must contain numeric characters only.
	 */
	for (p = name; *p; p++) {
	    if (isdigit(*p))
		continue;
	    return -EINVAL;
	}
	if (p == name)
	    return -EINVAL;
	break;

    case PCP_ATTR_PROTOCOL:
    case PCP_ATTR_SECURE:
    case PCP_ATTR_COMPRESS:
    case PCP_ATTR_USERAUTH:
    case PCP_ATTR_AUTHNAME:
    case PCP_ATTR_PASSWORD:
    case PCP_ATTR_METHOD:
    case PCP_ATTR_REALM:
    case PCP_ATTR_UNIXSOCK:
    case PCP_ATTR_LOCAL:
    case PCP_ATTR_EXCLUSIVE:
	break;

    default:
	return -EINVAL;
    }
    return 0;
}

static int
ctxlocal(__pmHashCtl *attrs)
{
    int sts;
    char *name = NULL;
    char *container = NULL;

    PM_LOCK(__pmLock_extcall);
    if ((container = getenv("PCP_CONTAINER")) != NULL) {	/* THREADSAFE */
	if ((sts = __pmCheckAttribute(PCP_ATTR_CONTAINER, container)) < 0) {
	    PM_UNLOCK(__pmLock_extcall);
	    return sts;
	}
	if ((name = strdup(container)) == NULL) {
	    PM_UNLOCK(__pmLock_extcall);
	    return -ENOMEM;
	}
	PM_UNLOCK(__pmLock_extcall);
	if ((sts = __pmHashAdd(PCP_ATTR_CONTAINER, (void *)name, attrs)) < 0) {
	    free(name);
	    return sts;
	}
    }
    else
	PM_UNLOCK(__pmLock_extcall);
    return 0;
}

static int
ctxflags(__pmHashCtl *attrs, int *flags)
{
    int sts;
    char *name = NULL;
    char *secure = NULL;
    char *container = NULL;
    __pmHashNode *node;

    if ((node = __pmHashSearch(PCP_ATTR_PROTOCOL, attrs)) != NULL) {
	if (strcmp((char *)node->data, "pcps") == 0) {
	    if ((node = __pmHashSearch(PCP_ATTR_SECURE, attrs)) != NULL)
		secure = (char *)node->data;
	    else
		secure = "enforce";
	}
    }

    PM_LOCK(__pmLock_extcall);
    if (!secure)
	secure = getenv("PCP_SECURE_SOCKETS");		/* THREADSAFE */

    if (secure) {
	if (secure[0] == '\0' ||
	   (strcmp(secure, "1")) == 0 ||
	   (strcmp(secure, "enforce")) == 0) {
	    *flags |= PM_CTXFLAG_SECURE;
	} else if (strcmp(secure, "relaxed") == 0) {
	    *flags |= PM_CTXFLAG_RELAXED;
	}
    }
    PM_UNLOCK(__pmLock_extcall);

    if (__pmHashSearch(PCP_ATTR_COMPRESS, attrs) != NULL)
	*flags |= PM_CTXFLAG_COMPRESS;

    if (__pmHashSearch(PCP_ATTR_USERAUTH, attrs) != NULL ||
	__pmHashSearch(PCP_ATTR_USERNAME, attrs) != NULL ||
	__pmHashSearch(PCP_ATTR_PASSWORD, attrs) != NULL ||
	__pmHashSearch(PCP_ATTR_METHOD, attrs) != NULL ||
	__pmHashSearch(PCP_ATTR_REALM, attrs) != NULL)
	*flags |= PM_CTXFLAG_AUTH;

    if (__pmHashSearch(PCP_ATTR_CONTAINER, attrs) != NULL)
	*flags |= PM_CTXFLAG_CONTAINER;
    else {
	PM_LOCK(__pmLock_extcall);
	container = getenv("PCP_CONTAINER");		/* THREADSAFE */
	if (container != NULL) {
	    if ((sts = __pmCheckAttribute(PCP_ATTR_CONTAINER, container)) < 0) {
		PM_UNLOCK(__pmLock_extcall);
		return sts;
	    }
	    if ((name = strdup(container)) == NULL) {
		PM_UNLOCK(__pmLock_extcall);
		return -ENOMEM;
	    }
	    PM_UNLOCK(__pmLock_extcall);
	    if ((sts = __pmHashAdd(PCP_ATTR_CONTAINER, (void *)name, attrs)) < 0) {
		free(name);
		return sts;
	    }
	    *flags |= PM_CTXFLAG_CONTAINER;
	}
	else
	    PM_UNLOCK(__pmLock_extcall);
    }

    /*
     * PCP_ATTR_EXCLUSIVE attr -> PM_CTXFLAG_EXCLUSIVE mapping used to
     * happen here, but has been deprecated when mulitplexing of the
     * client-pmcd socket was abandonded.
     */

    return 0;
}

int
__pmFindOrOpenArchive(__pmContext *ctxp, const char *name, int multi_arch)
{
    __pmArchCtl *acp;
    __pmLogCtl	*lcp;
    __pmContext	*ctxp2;
    int		i;
    int		sts;

    PM_INIT_LOCKS();

    /*
     * We're done with the current archive, if any. Close it, if necessary.
     * If we do close it, keep the __pmLogCtl block for re-use of the pmns.
     */
    acp = ctxp->c_archctl;
    lcp = acp->ac_log;
    if (lcp) {
	PM_LOCK(lcp->lc_lock);
	if (--lcp->refcnt == 0) {
	    PM_UNLOCK(lcp->lc_lock);
	    __pmLogClose(acp);
	}
	else {
	    PM_UNLOCK(lcp->lc_lock);
	    lcp = NULL;
	}
    }

    /*
     * See if an archive by this name is already open in another context.
     * We can't share archive control structs for multi-archive contexts
     * because, for those, there is a global pmns which is shared among the
     * archives in the context.
     *
     * We must take the contexts_lock mutex for this search, and we need
     * to lock the __pmLogCtl structures in turn.
     */
    if (! multi_arch) {
	__pmArchCtl	*acp2;
	__pmLogCtl	*lcp2 = NULL;

	PM_LOCK(contexts_lock);
	for (i = 0; i < contexts_len; i++) {
	    if (i == PM_TPD(curr_handle))
		continue;
	    if (contexts_map[i] < 0)
		continue;

	    /*
	     * See if there is already an archive opened with this name and
	     * not part of a multi-archive context.
	     */
	    ctxp2 = contexts[i];
	    if (ctxp2->c_type == PM_CONTEXT_ARCHIVE &&
	        (ctxp2->c_flags & PM_CTXFLAG_STREAMING_WRITER) == 0) {
		acp2 = ctxp2->c_archctl;
		PM_LOCK(acp2->ac_log->lc_lock);
		if (!acp2->ac_log->multi &&
		    strcmp(name, acp2->ac_log->name) == 0) {
		    lcp2 = acp2->ac_log;
		    break;
		}
		PM_UNLOCK(acp2->ac_log->lc_lock);
	    }
	}

	/*
	 * If we found an in-use archive with the same name, then use it
	 * ... it is already locked from above.
	 * Free the current log controls, if necessary.
	 */
	if (lcp2 != NULL) {
	    if (lcp) {
#ifdef PM_MULTI_THREAD
		__pmDestroyMutex(&lcp->lc_lock);
#endif
		free(lcp);
	    }
	    ++lcp2->refcnt;
	    __pmLogWriterInit(acp, lcp2);
	    PM_UNLOCK(acp2->ac_log->lc_lock);
	    PM_UNLOCK(contexts_lock);
	    /*
	     * Setup the per-context part of the controls ...
	     */
	    acp->ac_mfp = NULL;
	    acp->ac_curvol = -1;
	    sts = __pmLogChangeVol(acp, acp->ac_log->minvol);
	    if  (sts < 0) {
		if (pmDebugOptions.log) {
		    char	errmsg[PM_MAXERRMSGLEN];
		    fprintf(stderr, "__pmFindOrOpenArchive(..., %s, ...): __pmLogChangeVol(..., %d) failed: %s\n",
		    	name, acp->ac_log->minvol, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
		}
		acp->ac_log = NULL;
		return sts;
	    }
	    return 0;
	}
	PM_UNLOCK(contexts_lock);
    }

    /*
     * No usable, active archive with this name was found. Open one.
     * Allocate a new log control block, if necessary.
     */
    if (lcp == NULL) {
	if ((lcp = (__pmLogCtl *)calloc(1, sizeof(*lcp))) == NULL) {
	    pmNoMem("__pmFindOrOpenArchive", sizeof(*lcp), PM_FATAL_ERR);
	    /* NOTREACHED */
	}
#ifdef PM_MULTI_THREAD
	__pmInitMutex(&lcp->lc_lock);
#endif
	lcp->multi = multi_arch;
	__pmLogWriterInit(acp, lcp);
    }
    sts = __pmLogOpen(name, ctxp);
    if (sts < 0) {
	if (pmDebugOptions.log && pmDebugOptions.desperate) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "__pmFindOrOpenArchive(..., %s, ...): __pmLogOpen: %s\n",
		name, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
#ifdef PM_MULTI_THREAD
	__pmDestroyMutex(&lcp->lc_lock);
#endif
	free(lcp);
	acp->ac_log = NULL;
    }
    else {
	/*
	 * Note: we don't need the lc_lock here, this is a new __pmLogCtl
	 * structure and we hold the context lock for the only context
	 * that will point to this __pmLogCtl ... no one else can see
	 * it yet
	 */
	lcp->refcnt = 1;
    }

    return sts;
}

static char *
addName(const char *dirname, char *list, size_t *listsize,
		const char *item, size_t itemsize)
{
    size_t	dirsize;
    char	*list_new;

    /* Was there a directory specified? */
    if (dirname != NULL)
	dirsize = strlen(dirname) + 1; /* room for the path separator */
    else
	dirsize = 0;

    /* Allocate more space */
    if (list == NULL) {
	if ((list = malloc(dirsize + itemsize + 1)) == NULL) {
	    pmNoMem("addName", itemsize + 1, PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	*listsize = 0;
    }
    else {
	/* The comma goes where the previous nul was */
	if ((list_new = realloc(list, dirsize + *listsize + itemsize + 1)) == NULL) {
	    pmNoMem("addName", *listsize + itemsize + 1, PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	list = list_new;
	list[*listsize - 1] = ',';
    }

    /* Add the new name */
    if (dirname != NULL) {
	strcpy(list + *listsize, dirname);
	*listsize += dirsize;
	list[*listsize - 1] = pmPathSeparator();
    }
    memcpy(list + *listsize, item, itemsize);
    *listsize += itemsize + 1;
    list[*listsize - 1] = '\0';
    return list;
}

/*
 * The list of names may contain one or more directories. Examine the
 * list and replace the directories with the archives contained within.
 */
static char *
expandArchiveList(const char *names)
{
    const char	*current;
    const char	*end;
    size_t	length = 0;
    char	*newlist = NULL;
    size_t	newlistsize = 0;
    char	*dirname;
    const char	*suffix;
    DIR		*dirp = NULL;
    struct dirent	*direntp;
 
    current = names;
    while (*current) {
	/* Find the end of the current archive name. */
	end = strchr(current, ',');
	if (end)
	    length = end - current;
	else
	    length = strlen (current);

	/*
	 * If newname specifies a directory, then add each archive in the
	 * directory. Use opendir(3) directly instead of stat(3) or fstat(3) 
	 * in order to avoid a TOCTOU race between checking and opening the
	 * directory.
	 * We need nul terminated copy of the name fpr opendir(3).
	 */
	if ((dirname = malloc(length + 1)) == NULL) {
	    pmNoMem("expandArchiveList", length + 1, PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	memcpy(dirname, current, length);
	dirname[length] = '\0';

	/* dirp is an on-stack variable, so readdir*() is THREADSAFE */
	if ((dirp = opendir(dirname)) != NULL) {
	    while ((direntp = readdir(dirp)) != NULL) {		/* THREADSAFE */
		/*
		 * If this file is part of an archive, then add it.
		 * Look for names ending in .meta. These are unique to
		 * each archive.
		 *
		 * direntp->d_name is defined as an array by POSIX, so we
		 * can pass it to __pmLogBaseName, which will strip the
		 * suffix by modifying the data in place. The suffix can
		 * still be found after the base name.
		 */
		if (__pmLogBaseName(direntp->d_name) == NULL)
		    continue; /* not an archive file */

		suffix = direntp->d_name + strlen(direntp->d_name) + 1;
		if (strcmp(suffix, "meta") != 0)
		    continue;

		/*
		 * THREADSAFE because addName() acquires no locks (other than
		 * on the fatal pmNoMem() paths)
		 */
		--suffix;
		newlist = addName(dirname, newlist, &newlistsize,
				   direntp->d_name, suffix - direntp->d_name);
	    }
	    closedir(dirp);
	}
	else {
	    newlist = addName(NULL, newlist, &newlistsize, current, length);
	}
	free(dirname);

	/* Reset for the next iteration. */
	current += length;
	if (*current == ',')
	    ++current;
    }

    return newlist;
}

static int
initstreaming(__pmArchCtl *acp)
{
    int			sts;

    acp->ac_vol = acp->ac_curvol;
    acp->ac_meta_loaded = 1;

    if ((acp->ac_log = (__pmLogCtl *)calloc(1, sizeof(__pmLogCtl))) == NULL) {
	pmNoMem(__FUNCTION__, sizeof(__pmLogCtl), PM_RECOV_ERR);
	return -ENOMEM;
    }
#ifdef PM_MULTI_THREAD
    __pmInitMutex(&acp->ac_log->lc_lock);
#endif
    acp->ac_log->refcnt = 1;

    if ((sts = __pmNewPMNS(&acp->ac_log->pmns)) < 0)
	return sts;
    __pmFixPMNSHashTab(acp->ac_log->pmns, acp->ac_log->numpmid, 1);
    return sts;
}

/*
 * Initialize the given archive(s) for this context.
 *
 * 'name' may be a single archive name or a list of archive names separated by
 * commas.
 *
 * Coming soon:
 * - name can be one or more glob expressions specifying the archives of
 *   interest.
 *
 * NB: no locks are being held at entry.
 */
static int
initarchive(__pmContext	*ctxp, const char *name)
{
    int			i;
    int			sts;
    char		*namelist = NULL;
    const char		*current;
    char		*end;
    __pmArchCtl		*acp;
    __pmMultiLogCtl	*mlcp = NULL;
    int			multi_arch = 0;
    int			ignore;
    double		tdiff;
    __pmLogLabel	*lp;
    __pmTimestamp	tmpTime;

    /*
     * Catch these early. Formerly caught by __pmLogOpenFind(), but with
     * multi-archive support, things are more complex now.
     */
    if (name == NULL || *name == '\0')
	return PM_ERR_LOGFILE;

    /* Allocate the structure for overall control of the archive(s). */
    if ((acp = (__pmArchCtl *)calloc(1, sizeof(__pmArchCtl))) == NULL) {
	pmNoMem("initarchive", sizeof(__pmArchCtl), PM_FATAL_ERR);
	/* NOTREACHED */
    }
    ctxp->c_archctl = acp;
    acp->ac_curvol = -1;
    acp->ac_flags = ctxp->c_flags;

    if ((acp->ac_flags & PM_CTXFLAG_STREAMING_WRITER))
	return initstreaming(acp);
    __pmLogWriterInit(acp, NULL); /* prepare callbacks for file-based I/O */

    /*
     * The list of names may contain one or more directories. Examine the
     * list and replace the directories with the archives contained within.
     */
    if ((namelist = expandArchiveList(name)) == NULL) {
	sts = PM_ERR_LOGFILE;
	goto error;
    }

    /*
     * Initialize a __pmMultiLogCtl structure for each of the named archives.
     * sort them in order of start time and check for overlaps. Keep the final
     * archive open.
     */
    current = namelist;
    while (*current) {
	/* Find the end of the current archive name. */
	end = strchr(current, ',');
	if (end) {
	    multi_arch = 1;
	    *end = '\0';
	}

	/*
	 * Obtain a handle for the named archive.
	 * __pmFindOrOpenArchive() will take care of closing the active archive,
	 * if necessary
	 */
	sts = __pmFindOrOpenArchive(ctxp, current, multi_arch);
	if (sts < 0) {
	    if (pmDebugOptions.log && pmDebugOptions.desperate) {
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "initarchive(..., %s, ...): __pmFindOrOpenArchive: %s\n",
		    name, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    }
	    goto error;
	}

	/*
	 * Obtain the start time of this archive. The end time could change
	 * on the fly and needs to be re-checked as needed.
	 */
	lp = &ctxp->c_archctl->ac_log->label;

	/*
	 * Insert this new entry into the list in sequence by time. Check for
	 * overlaps. Also check for duplicates.
	 */
	tmpTime = lp->start;
	ignore = 0;
	for (i = 0; i < acp->ac_num_logs; i++) {
	    tdiff = __pmTimestampSub(&tmpTime, &acp->ac_log_list[i]->starttime);
	    if (tdiff < 0.0) /* found insertion point */
		break;
	    if (tdiff == 0.0) {
		/* Is it a duplicate? */
		if (strcmp(current, acp->ac_log_list[i]->name) == 0) {
		    ignore = 1;
		    break;
		}
		/* timespan overlap */
		sts = PM_ERR_LOGOVERLAP;
		goto error;
	    }
	    /* Keep checking */
	}

	if (! ignore) {
	    __pmMultiLogCtl	**list_new;
	    /* Initialize a new ac_log_list entry for this archive. */
	    list_new = (__pmMultiLogCtl **)realloc(acp->ac_log_list,
						   (acp->ac_num_logs + 1) *
						   sizeof(*acp->ac_log_list));
	    if (list_new == NULL) {
		pmNoMem("initarchive: list_new",
			  (acp->ac_num_logs + 1) * sizeof(*acp->ac_log_list),
			  PM_FATAL_ERR);
		/* NOTREACHED */
	    }
	    acp->ac_log_list = list_new;
	    if ((mlcp = (__pmMultiLogCtl *)malloc(sizeof(__pmMultiLogCtl))) == NULL) {
		pmNoMem("initarchive: __pmMultiLogCtl", sizeof(__pmMultiLogCtl), PM_FATAL_ERR);
		/* NOTREACHED */
	    }
	    if ((mlcp->name = strdup(current)) == NULL) {
		pmNoMem("initarchive: name", strlen(current) + 1, PM_FATAL_ERR);
		/* NOTREACHED */
	    }
	    if ((mlcp->hostname = strdup(lp->hostname)) == NULL) {
		pmNoMem("initarchive: hostname", strlen(lp->hostname) + 1, PM_FATAL_ERR);
		/* NOTREACHED */
	    }
	    if (lp->timezone != NULL) {
		if ((mlcp->timezone = strdup(lp->timezone)) == NULL) {
		    pmNoMem("initarchive: timezone", strlen(lp->timezone) + 1, PM_FATAL_ERR);
		    /* NOTREACHED */
		}
	    }
	    else
		mlcp->timezone = NULL;
	    if (lp->zoneinfo != NULL) {
		if ((mlcp->zoneinfo = strdup(lp->zoneinfo)) == NULL) {
		    pmNoMem("initarchive: zoneinfo", strlen(lp->zoneinfo) + 1, PM_FATAL_ERR);
		    /* NOTREACHED */
		}
	    }
	    else
		mlcp->zoneinfo = NULL;
	    mlcp->starttime = tmpTime;

	    /*
	     * If we found the insertion point, then make room for the current
	     * archive in that slot. Otherwise, i refers to the end of the list,
	     * which is the correct slot.
	     */
	    if (i < acp->ac_num_logs) {
		memmove(&acp->ac_log_list[i + 1], &acp->ac_log_list[i],
			 (acp->ac_num_logs - i) * sizeof(*acp->ac_log_list));
	    }
	    acp->ac_log_list[i] = mlcp;
	    mlcp = NULL;
	    acp->ac_cur_log = acp->ac_num_logs;
	    ++acp->ac_num_logs;
	}

	/* Set up to process the next name. */
	if (! end)
	    break;
	current = end + 1;
    }
    free(namelist);
    namelist = NULL;
    acp->ac_meta_loaded = 1;

    /*
     * All archives processed now, build PMNS hash
     */
    __pmFixPMNSHashTab(acp->ac_log->pmns, acp->ac_log->numpmid, 1);

    /* Check for duplicate label sets. */
    __pmCheckDupLabels(acp);

    if (acp->ac_num_logs > 1) {
	/*
	 * In order to maintain API semantics with the old single archive
	 * implementation, open the first archive and switch to the first volume.
	 */
	sts = __pmLogChangeArchive(ctxp, 0);
	if (sts < 0) {
	    if (pmDebugOptions.log && pmDebugOptions.desperate) {
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "initarchive(..., %s, ...): __pmLogChangeArchive: %s\n",
		    name, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    }
	    goto error;
	}
	sts = __pmLogChangeVol(acp, acp->ac_log->minvol);
	if (sts < 0) {
	    if (pmDebugOptions.log && pmDebugOptions.desperate) {
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "initarchive(..., %s, ...): __pmLogChangeVol(..., %d, ...): %s\n",
		    name, acp->ac_log->minvol, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    }
	    goto error;
	}
    }

    /* start after header + label record + trailer */
    ctxp->c_origin = acp->ac_log->label.start;
    ctxp->c_mode = PM_MODE_FORW;
    acp->ac_offset = __pmLogLabelSize(acp->ac_log);
    acp->ac_vol = acp->ac_curvol;

    return 0; /* success */

 error:
    if (mlcp) {
	if (mlcp->name)
	    free (mlcp->name);
	if (mlcp->hostname)
	    free (mlcp->hostname);
	if (mlcp->timezone)
	    free (mlcp->timezone);
	if (mlcp->zoneinfo)
	    free (mlcp->zoneinfo);
	free(mlcp);
    }
    if (namelist)
	free(namelist);
    if (acp->ac_log_list) {
	while (acp->ac_num_logs > 0) {
	    --acp->ac_num_logs;
	    if (acp->ac_log_list[acp->ac_num_logs]) {
		free(acp->ac_log_list[acp->ac_num_logs]->name);
		free(acp->ac_log_list[acp->ac_num_logs]->hostname);
		free(acp->ac_log_list[acp->ac_num_logs]->timezone);
		free(acp->ac_log_list[acp->ac_num_logs]->zoneinfo);
		free(acp->ac_log_list[acp->ac_num_logs]);
	    }
	}
	free(acp->ac_log_list);
    }
    if (acp->ac_log && --acp->ac_log->refcnt == 0)
	free(acp->ac_log);
    free(acp);
    ctxp->c_archctl = NULL;
    return sts;
}

static struct {
    int		flag;
    const char	*name;
} _flagmap[] = {
    { PM_CTXFLAG_SHALLOW,	"shallow" },
    { PM_CTXFLAG_EXCLUSIVE,	"exclusive" },
    { PM_CTXFLAG_SECURE,	"secure" },
    { PM_CTXFLAG_COMPRESS,	"compress" },
    { PM_CTXFLAG_RELAXED,	"relaxed" },
    { PM_CTXFLAG_AUTH,	"auth" },
    { PM_CTXFLAG_CONTAINER,	"container" },
    { PM_CTXFLAG_NO_FEATURE_CHECK,	"no_feature_check" },
    { PM_CTXFLAG_METADATA_ONLY,	"metadata_only" },
    { PM_CTXFLAG_LAST_VOLUME,	"last_volume" },
    { PM_CTXFLAG_STREAMING_WRITER,	"streaming_writer" },
    { 0,			NULL }
};

/*
 * print context flags, with separator |
 * type maybe type as passed to pmNewContext, or c_flags as stored
 * in __pmContext
 */
static void
print_ctx_flags(FILE *f, int type)
{
    int	j;
    int	first = 1;

    for (j = 0; _flagmap[j].name != NULL; j++) {
	if (type & _flagmap[j].flag) {
	    if (first)
		first = 0;
	    else
		fputc('|', f);
	    fprintf(f, "%s", _flagmap[j].name);
	}
    }
}

int
pmNewContext(int type, const char *name)
{
    __pmContext	*new = NULL;
    __pmContext	**list;
    int		*list_map;
    int		i;
    int		sts;
    int		old_curr_handle;
    __pmContext	*old_curr_ctxp;
    int		ctxnum = -1;	/* index into contexts[] for new context */
    /* A pointer to this stub object is put in contexts[] while a real __pmContext is being built. */
    static /*const*/ __pmContext being_initialized = { .c_type = PM_CONTEXT_INIT };

    if (pmDebugOptions.pmapi) {
	fprintf(stderr, "pmNewContext(%d", type & PM_CONTEXT_TYPEMASK);
	if (type != (type & PM_CONTEXT_TYPEMASK)) {
	    fputc('+', stderr);
	    print_ctx_flags(stderr, type);
	}
	fprintf(stderr, ", %s) <:", name != NULL ? name : "NULL");
    }

    PM_INIT_LOCKS();

    if (PM_CONTEXT_LOCAL == (type & PM_CONTEXT_TYPEMASK)) {
	if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA)) {
	    /* Local context requires single-threaded applications */
	    sts = PM_ERR_THREAD;
	    goto pmapi_return;
	}
    } else if (name == NULL) {
	/* Allow NULL name only in local context mode of operation */
	sts = PM_ERR_NOCONTEXT;
	goto pmapi_return;
    }

    old_curr_handle = PM_TPD(curr_handle);
    old_curr_ctxp = PM_TPD(curr_ctxp);

    PM_LOCK(contexts_lock);
    /* See if we can reuse a free context */
    for (i = 0; i < contexts_len; i++) {
	if (contexts_map[i] == MAP_FREE) {
	    ctxnum = i;
	    new = contexts[ctxnum];
	    goto INIT_CONTEXT;
	}
    }

    /* Create a new one */
    if (contexts == NULL) {
	list = (__pmContext **)malloc(sizeof(__pmContext *));
	list_map = (int *)malloc(sizeof(int));
    }
    else {
	list = (__pmContext **)realloc((void *)contexts, (1+contexts_len) * sizeof(__pmContext *));
	list_map = (int *)realloc((void *)contexts_map, (1+contexts_len) * sizeof(int));
    }
    if (list == NULL || list_map == NULL) {
	if (list != NULL)
	    free(list);
	if (list_map != NULL)
	    free(list_map);
	sts = -oserror();
	goto FAILED_LOCKED;
    }
    contexts = list;
    contexts_map = list_map;
    /*
     * NB: it is harmless (not a leak) if contexts[] and/or contexts_map[]
     * is realloc'd a little larger, and then the last slot is not
     * initialized (since context_len is not incremented, and/or
     * initialization fails.
     * A subsequent pmNewContext allocation attempt will just do the
     * realloc again, and that time it'll be a trivial success.
     */

    new = (__pmContext *)malloc(sizeof(__pmContext));
    if (new == NULL) {
	sts = -oserror();
	goto FAILED_LOCKED;
    }
    memset(new, 0, sizeof(__pmContext));
    initcontextlock(&new->c_lock);

    ctxnum = contexts_len;
    contexts_len++;

    /*
     * We do not need to hold contexts_lock just for filling of the
     * new __pmContext structure.  This is good because archive and
     * remote-host setup operations can take centiseconds through
     * decaseconds of time.  We will need to re-lock to put the
     * initialized __pmContext into the contexts[] slot though
     * (e.g. for memory barrier purposes).
     */
INIT_CONTEXT:

    /*
     * Set up the default state
     */
    PM_TPD(curr_ctxp) = new;
    PM_TPD(curr_handle) = new->c_handle = ++last_handle;
    new->c_slot = ctxnum;
    contexts[ctxnum] = &being_initialized;
    contexts_map[ctxnum] = MAP_INIT;
    PM_UNLOCK(contexts_lock);
    /* c_lock not re-initialized, created once from initcontextlock() above */
    new->c_type = (type & PM_CONTEXT_TYPEMASK);
    new->c_mode = 0;
    new->c_origin.sec = new->c_origin.nsec = 0;
    new->c_delta.sec = new->c_delta.nsec = 0;
    new->c_direction = 0;
    new->c_sent = 0;
    new->c_flags = (type & ~PM_CONTEXT_TYPEMASK);
    if ((new->c_instprof = (pmProfile *)calloc(1, sizeof(pmProfile))) == NULL) {
	/*
	 * fail : nothing changed -- actually list is changed, but restoring
	 * contexts_len should make it ok next time through
	 */
	sts = -oserror();
	goto FAILED;
    }
    new->c_instprof->state = PM_PROFILE_INCLUDE;	/* default global state */

    if (new->c_type == PM_CONTEXT_HOST) {
	__pmHashCtl	*attrs = &new->c_attrs;
	__pmHostSpec	*hosts = NULL;
	int		nhosts;
	char		*errmsg;

	/* break down a host[:port@proxy:port][?attributes] specification */
	__pmHashInit(attrs);
	sts = __pmParseHostAttrsSpec(name, &hosts, &nhosts, attrs, &errmsg);
	if (sts < 0) {
	    pmprintf("pmNewContext: bad host specification\n%s", errmsg);
	    pmflush();
	    free(errmsg);
	    if (hosts != NULL)
		__pmFreeHostAttrsSpec(hosts, nhosts, attrs);
	    __pmHashClear(attrs);
	    goto FAILED;
	} else if (nhosts == 0) {
	    if (hosts != NULL)
		__pmFreeHostAttrsSpec(hosts, nhosts, attrs);
	    __pmHashClear(attrs);
	    sts = PM_ERR_NOTHOST;
	    goto FAILED;
	} else if ((sts = ctxflags(attrs, &new->c_flags)) < 0) {
	    if (hosts != NULL)
		__pmFreeHostAttrsSpec(hosts, nhosts, attrs);
	    __pmHashClear(attrs);
	    goto FAILED;
	}

	/*
	 * Try to establish the connection.
	 * If this fails, restore the original current context
	 * and return an error.
	 */
	sts = __pmConnectPMCD(hosts, nhosts, new->c_flags, &new->c_attrs);
	if (sts < 0) {
	    __pmFreeHostAttrsSpec(hosts, nhosts, attrs);
	    __pmHashClear(attrs);
	    goto FAILED;
	}

	new->c_pmcd = (__pmPMCDCtl *)calloc(1,sizeof(__pmPMCDCtl));
	if (new->c_pmcd == NULL) {
	    sts = -oserror();
	    __pmCloseSocket(sts);
	    __pmFreeHostAttrsSpec(hosts, nhosts, attrs);
	    __pmHashClear(attrs);
	    goto FAILED;
	}
	new->c_pmcd->pc_fd = sts;
	new->c_pmcd->pc_hosts = hosts;
	new->c_pmcd->pc_nhosts = nhosts;
	new->c_pmcd->pc_tout_sec = __pmConvertTimeout(TIMEOUT_DEFAULT) / 1000;
    }
    else if (new->c_type == PM_CONTEXT_LOCAL) {
	if ((sts = ctxlocal(&new->c_attrs)) != 0)
	    goto FAILED;
	if ((sts = __pmConnectLocal(&new->c_attrs)) != 0)
	    goto FAILED;
    }
    else if (new->c_type == PM_CONTEXT_ARCHIVE) {
        /*
         * Unlock during the archive initial file opens, which can take
         * a noticeable amount of time, esp. for multi-archives.  This
         * is OK because no other thread can validly touch our
         * partly-initialized context.
         */
        sts = initarchive(new, name);
        if (sts < 0)
	    goto FAILED;
    }
    else {
	/* bad type */
	if (pmDebugOptions.context) {
	    fprintf(stderr, "pmNewContext(%d", type & PM_CONTEXT_TYPEMASK);
	    if (type != (type & PM_CONTEXT_TYPEMASK)) {
		fputc('+', stderr);
		print_ctx_flags(stderr, type);
	    }
	    fprintf(stderr, ", %s): illegal type 0x%x\n", name != NULL ? name : "NULL", type);
	}
	sts = PM_ERR_NOCONTEXT;
	goto pmapi_return;
    }

    /* Take contexts_lock mutex to update contexts[] with this fully operational
       battle station ^W context. */
    PM_LOCK(contexts_lock);
    contexts[ctxnum] = new;
    contexts_map[ctxnum] = new->c_handle;
    PM_UNLOCK(contexts_lock);

    /* return the handle to the new (current) context */
    if (pmDebugOptions.context) {
	fprintf(stderr, "pmNewContext(%d", type & PM_CONTEXT_TYPEMASK);
	if (type != (type & PM_CONTEXT_TYPEMASK)) {
	    fputc('+', stderr);
	    print_ctx_flags(stderr, type);
	}
	fprintf(stderr, ", %s) -> %d\n",
			name != NULL ? name : "NULL", PM_TPD(curr_handle));
	__pmDumpContext(stderr, PM_TPD(curr_handle), PM_INDOM_NULL);
    }

    /*
     * Bind defined metrics if any ..., after the new context is in place.
     *
     * Need to lock context because routines called from _dmopencontext()
     * may assume the context is locked.
     */
    PM_LOCK(new->c_lock);
    __dmopencontext(new);
    PM_UNLOCK(new->c_lock);

    sts = PM_TPD(curr_handle);
    goto pmapi_return;

FAILED:
    /*
     * We're contexts_lock unlocked at this stage.  We may have allocated a
     * __pmContext; we may have partially initialized it, but
     * something went wrong.  Let's install it as a blank
     * free entry in contexts[] to replace the PM_CONTEXT_INIT
     * stub we left in its place.
    */
    PM_LOCK(contexts_lock);

FAILED_LOCKED:
    if (new != NULL) {
	/* new has been allocated and ctxnum set */
	if (new->c_instprof != NULL) {
	    free(new->c_instprof);
            new->c_instprof = NULL;
        }
        /* We could memset-0 the struct, but this is not really
           necessary.  That's the first thing we'll do in INIT_CONTEXT. */
        contexts[ctxnum] = new;
	contexts_map[ctxnum] = MAP_FREE;
    }
    PM_TPD(curr_handle) = old_curr_handle;
    PM_TPD(curr_ctxp) = old_curr_ctxp;
    if (pmDebugOptions.context) {
	fprintf(stderr, "pmNewContext(%d", type);
	if (type != (type & PM_CONTEXT_TYPEMASK)) {
	    fputc('+', stderr);
	    print_ctx_flags(stderr, type);
	}
	fprintf(stderr, ", %s) -> %d, curr_handle=%d\n",
	    name != NULL ? name : "NULL", sts, PM_TPD(curr_handle));
    }
    PM_UNLOCK(contexts_lock);

pmapi_return:

    if (pmDebugOptions.pmapi) {
	fprintf(stderr, ":> returns ");
	if (sts >= 0)
	    fprintf(stderr, "%d\n", sts);
	else {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
    }

    return sts;
}

int
pmReconnectContext(int handle)
{
    __pmContext	*ctxp;
    __pmPMCDCtl	*ctl;
    int		sts;
    int		ctxnum;

    if (pmDebugOptions.pmapi)
	fprintf(stderr, "pmReconnectContext(%d) <:", handle);

    PM_LOCK(contexts_lock);
    if ((ctxnum = map_handle(handle)) < 0) {
	if (pmDebugOptions.context)
	    fprintf(stderr, "pmReconnectContext(%d) -> %d\n", handle, PM_ERR_NOCONTEXT);
	PM_UNLOCK(contexts_lock);
	sts = PM_ERR_NOCONTEXT;
	goto pmapi_return;
    }

    ctxp = contexts[ctxnum];
    PM_UNLOCK(contexts_lock);
    PM_LOCK(ctxp->c_lock);
    ctl = ctxp->c_pmcd;
    if (ctxp->c_type == PM_CONTEXT_HOST) {
	if (ctl->pc_timeout && time(NULL) < ctl->pc_again) {
	    /* too soon to try again */
	    if (pmDebugOptions.context)
		fprintf(stderr, "pmReconnectContext(%d) -> %d, too soon (need wait another %d secs)\n",
			handle, (int)-ETIMEDOUT, (int)(ctl->pc_again - time(NULL)));
	    PM_UNLOCK(ctxp->c_lock);
	    sts = -ETIMEDOUT;
	    goto pmapi_return;
	}

	if (ctl->pc_fd >= 0) {
	    /* don't care if this fails */
	    __pmCloseSocket(ctl->pc_fd);
	    ctl->pc_fd = -1;
	}

	if ((sts = __pmConnectPMCD(ctl->pc_hosts, ctl->pc_nhosts,
				   ctxp->c_flags, &ctxp->c_attrs)) < 0) {
	    PM_UNLOCK(ctxp->c_lock);
	    waitawhile(ctl);
	    if (pmDebugOptions.context)
		fprintf(stderr, "pmReconnectContext(%d), failed (wait %d secs before next attempt)\n",
		    handle, (int)(ctl->pc_again - time(NULL)));
	    sts = -ETIMEDOUT;
	    goto pmapi_return;
	}
	else {
	    ctl->pc_fd = sts;
	    ctl->pc_timeout = 0;
	    ctxp->c_sent = 0;

	    if (pmDebugOptions.context)
		fprintf(stderr, "pmReconnectContext(%d), done\n", handle);
	}
    }

    /* clear any derived metrics and re-bind */
    __dmclosecontext(ctxp);
    __dmopencontext(ctxp);
    PM_UNLOCK(ctxp->c_lock);

    if (pmDebugOptions.context)
	fprintf(stderr, "pmReconnectContext(%d) -> %d\n", handle, handle);

    sts = handle;

pmapi_return:

    if (pmDebugOptions.pmapi) {
	fprintf(stderr, ":> returns ");
	if (sts >= 0)
	    fprintf(stderr, "%d\n", sts);
	else {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
    }

    return sts;
}

int
pmDupContext(void)
{
    int			sts, oldtype, old_c_type;
    int			old, new = -1;
    char		hostspec[4096];
    char		archivename[MAXPATHLEN];
    __pmContext		*newcon, *oldcon;
    __pmContext		tmpcon = { 0 };		/* used to build new context */
    __pmMultiLogCtl	*newmlcp, *oldmlcp;
    pmInDomProfile	*q, *p, *p_end;
    int			i;
    int			ctxnum;
    int			vol;

    if (pmDebugOptions.pmapi) {
	fprintf(stderr, "pmDupContext() <:");
    }

    if ((old = pmWhichContext()) < 0) {
	sts = old;
	goto done;
    }
    PM_LOCK(contexts_lock);
    if ((ctxnum = map_handle(old)) < 0) {
	if (pmDebugOptions.context)
	    fprintf(stderr, "pmDupContext(%d) -> %d\n", old, PM_ERR_NOCONTEXT);
	PM_UNLOCK(contexts_lock);
	sts = PM_ERR_NOCONTEXT;
	goto pmapi_return;
    }

    oldcon = contexts[ctxnum];
    PM_LOCK(oldcon->c_lock);
    PM_UNLOCK(contexts_lock);
    /*
     * Build a skeletal __pmContext in tmpcon with oldcon->c_lock held.
     */
    tmpcon.c_mode = oldcon->c_mode;
    tmpcon.c_origin = oldcon->c_origin;
    tmpcon.c_delta = oldcon->c_delta;
    tmpcon.c_direction = oldcon->c_direction;
    tmpcon.c_flags = oldcon->c_flags;

    /* clone the per-domain profiles (if any) */
    if ((tmpcon.c_instprof = (pmProfile *)calloc(1, sizeof(pmProfile))) == NULL) {
	sts = -oserror();
	goto setup_fail;
    }
    tmpcon.c_instprof->state = oldcon->c_instprof->state;
    if (oldcon->c_instprof->profile_len > 0) {
	tmpcon.c_instprof->profile_len = oldcon->c_instprof->profile_len;
PM_FAULT_POINT("libpcp/" __FILE__ ":0", PM_FAULT_ALLOC);
	tmpcon.c_instprof->profile = (pmInDomProfile *)malloc(
	    oldcon->c_instprof->profile_len * sizeof(pmInDomProfile));
	if (tmpcon.c_instprof->profile == NULL) {
	    sts = -oserror();
	    free(tmpcon.c_instprof);
	    tmpcon.c_instprof = NULL;
	    goto setup_fail;
	}
	memcpy(tmpcon.c_instprof->profile, oldcon->c_instprof->profile,
	    oldcon->c_instprof->profile_len * sizeof(pmInDomProfile));
	p = oldcon->c_instprof->profile;
	p_end = p + oldcon->c_instprof->profile_len;
	q = tmpcon.c_instprof->profile;
	for (; p < p_end; p++, q++) {
	    if (p->instances) {
PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_ALLOC);
		q->instances = (int *)malloc(p->instances_len * sizeof(int));
		if (q->instances == NULL) {
		    sts = -oserror();
		    /* free any allocated profiles up to the failue */
		    for (q--; q >= tmpcon.c_instprof->profile; q--)
			free(q->instances);
		    free(tmpcon.c_instprof->profile);
		    free(tmpcon.c_instprof);
		    tmpcon.c_instprof = NULL;
		    goto setup_fail;
		}
		memcpy(q->instances, p->instances,
		    p->instances_len * sizeof(int));
	    }
	}
    }

    /* clone the archive control struct, if any */
    if (oldcon->c_archctl != NULL) {
PM_FAULT_POINT("libpcp/" __FILE__ ":2", PM_FAULT_ALLOC);
	if ((tmpcon.c_archctl = (__pmArchCtl *)malloc(sizeof(__pmArchCtl))) == NULL) {
	    sts = -oserror();
	    goto setup_fail;
	}
	*tmpcon.c_archctl = *oldcon->c_archctl;	/* struct assignment */

	/*
	 * force new and independent reload of metadata in the new context
	 * */
	tmpcon.c_archctl->ac_meta_loaded = 0;

	/*
	 * Need to make hash list and read cache independent in case oldcon
	 * is subsequently closed via pmDestroyContext() and don't want
	 * __pmFreeInterpData() to trash our hash list and read cache.
	 * Start with an empty hash list and read cache for the dup'd context.
	 */
	tmpcon.c_archctl->ac_pmid_hc.nodes = 0;
	tmpcon.c_archctl->ac_pmid_hc.hsize = 0;
	tmpcon.c_archctl->ac_cache = NULL;

	/*
	 * Need a new ac_mfp, but pointing at the same volume so ac_offset
	 * is OK
	 */
	tmpcon.c_archctl->ac_mfp = NULL;
	vol = oldcon->c_archctl->ac_curvol;
	tmpcon.c_archctl->ac_curvol = -1;
	sts = __pmLogChangeVol(tmpcon.c_archctl, vol);
	if  (sts < 0) {
	    if (pmDebugOptions.log) {
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "pmDupContext: __pmLogChangeVol(tmpcon, %d) failed: %s\n",
		    vol, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    }
	    tmpcon.c_archctl->ac_num_logs = 0;
	    goto setup_fail;
	}

	/*
	 * We need to copy the log lists and bump up the reference counts of
	 * any open logs.
	 */
	if (oldcon->c_archctl->ac_log_list != NULL) {
	    size_t size = oldcon->c_archctl->ac_num_logs *
		sizeof(*oldcon->c_archctl->ac_log_list);
PM_FAULT_POINT("libpcp/" __FILE__ ":3", PM_FAULT_ALLOC);
	    if ((tmpcon.c_archctl->ac_log_list = malloc(size)) == NULL) {
		sts = -oserror();
		tmpcon.c_archctl->ac_num_logs = 0;
		__pmFclose(tmpcon.c_archctl->ac_mfp);
		goto setup_fail;
	    }
	    /* We need to duplicate each ac_log_list entry. */
	    for (i = 0; i < tmpcon.c_archctl->ac_num_logs; i++) {
PM_FAULT_POINT("libpcp/" __FILE__ ":4", PM_FAULT_ALLOC);
		tmpcon.c_archctl->ac_log_list[i] =
		    malloc(sizeof(*tmpcon.c_archctl->ac_log_list[i]));
		if (tmpcon.c_archctl->ac_log_list[i] == NULL) {
		    sts = -oserror();
		    tmpcon.c_archctl->ac_num_logs = i;
		    __pmFclose(tmpcon.c_archctl->ac_mfp);
		    goto setup_fail;
		}
		newmlcp = tmpcon.c_archctl->ac_log_list[i];
		oldmlcp = oldcon->c_archctl->ac_log_list[i];
		*newmlcp = *oldmlcp;
		/*
		 * We need to duplicate the name and the hostname of each
		 * archive in the list.
		 */
PM_FAULT_POINT("libpcp/" __FILE__ ":5", PM_FAULT_ALLOC);
		if ((newmlcp->name = strdup(newmlcp->name)) == NULL) {
		    sts = -oserror();
		    tmpcon.c_archctl->ac_num_logs = i+1;
		    /* don't free the ones we've not yet strdup'd */
		    newmlcp->hostname = NULL;
		    newmlcp->timezone = NULL;
		    newmlcp->zoneinfo = NULL;
		    __pmFclose(tmpcon.c_archctl->ac_mfp);
		    goto setup_fail;
		}
PM_FAULT_POINT("libpcp/" __FILE__ ":6", PM_FAULT_ALLOC);
		if ((newmlcp->hostname = strdup(newmlcp->hostname)) == NULL) {
		    sts = -oserror();
		    tmpcon.c_archctl->ac_num_logs = i+1;
		    /* don't free the ones we've not yet strdup'd */
		    newmlcp->timezone = NULL;
		    newmlcp->zoneinfo = NULL;
		    __pmFclose(tmpcon.c_archctl->ac_mfp);
		    goto setup_fail;
		}
		if (newmlcp->timezone != NULL) {
PM_FAULT_POINT("libpcp/" __FILE__ ":7", PM_FAULT_ALLOC);
		    if ((newmlcp->timezone = strdup(newmlcp->timezone)) == NULL) {
			sts = -oserror();
			tmpcon.c_archctl->ac_num_logs = i+1;
			/* don't free the ones we've not yet strdup'd */
			newmlcp->zoneinfo = NULL;
			__pmFclose(tmpcon.c_archctl->ac_mfp);
			goto setup_fail;
		    }
		}
		if (newmlcp->zoneinfo != NULL) {
PM_FAULT_POINT("libpcp/" __FILE__ ":8", PM_FAULT_ALLOC);
		    if ((newmlcp->zoneinfo = strdup(newmlcp->zoneinfo)) == NULL) {
			sts = -oserror();
			tmpcon.c_archctl->ac_num_logs = i+1;
			__pmFclose(tmpcon.c_archctl->ac_mfp);
			goto setup_fail;
		    }
		}
	    }
	    /* We need to bump up the reference count of the ac_log. */
	    if (tmpcon.c_archctl->ac_log != NULL)
		++tmpcon.c_archctl->ac_log->refcnt;
	}
    }

    oldtype = oldcon->c_type | oldcon->c_flags;
    old_c_type = oldcon->c_type;
    if (old_c_type == PM_CONTEXT_HOST)
	__pmUnparseHostSpec(oldcon->c_pmcd->pc_hosts,
			oldcon->c_pmcd->pc_nhosts, hostspec, sizeof(hostspec));
    else if (old_c_type == PM_CONTEXT_ARCHIVE) {
	pmstrncpy(archivename, MAXPATHLEN, oldcon->c_archctl->ac_log->name);
    }

    /*
     * we now have local copies of everything we need to preserve/use
     * from the old __pmContext
     */
    PM_UNLOCK(oldcon->c_lock);

    if (old_c_type == PM_CONTEXT_HOST)
	new = pmNewContext(oldtype, hostspec);
    else if (old_c_type == PM_CONTEXT_LOCAL)
	new = pmNewContext(oldtype, NULL);
    else if (old_c_type == PM_CONTEXT_ARCHIVE)
	new = pmNewContext(oldtype, archivename);
    if (new < 0) {
	/* failed to connect or out of memory */
	sts = new;
	goto new_fail;
    }
    PM_LOCK(contexts_lock);
    ctxnum = map_handle(new);
PM_FAULT_POINT("libpcp/" __FILE__ ":9", PM_FAULT_MISC);
/* begin fault-injection */
    if (PM_FAULT_CHECK) {
	PM_FAULT_CLEAR;
	ctxnum = -1;
    }
/* end fault-injection */
    if (ctxnum < 0) {
	sts = PM_ERR_NOCONTEXT;
	PM_UNLOCK(contexts_lock);
	goto new_fail;
    }
    newcon = contexts[ctxnum];
    PM_LOCK(newcon->c_lock);
    PM_UNLOCK(contexts_lock);
    /*
     * cherry-pick the fields of __pmContext that need to be copied
     */
    newcon->c_mode = tmpcon.c_mode;
    newcon->c_origin = tmpcon.c_origin;
    newcon->c_delta = tmpcon.c_delta;
    newcon->c_direction = tmpcon.c_direction;
    newcon->c_flags = tmpcon.c_flags;
    // newcon->c_instprof->state = tmpcon.c_instprof->state;
    newcon->c_instprof->profile_len = tmpcon.c_instprof->profile_len;
    newcon->c_instprof->profile = tmpcon.c_instprof->profile;
    free (tmpcon.c_instprof);

    /*
     * For a HOST context, the call to pmNewContext (above) should
     * have connected to the pmcd.
     * Make sure the new profile will be sent before the next fetch.
     */
    newcon->c_sent = 0;

    /* clone the archive control struct, if any */
    if (newcon->c_archctl != NULL)
	__pmArchCtlFree(newcon->c_archctl); /* will allocate a new one below */
    newcon->c_archctl = tmpcon.c_archctl;
    PM_UNLOCK(newcon->c_lock);

    sts = new;
    goto done;

setup_fail:
    /* failure, get here with c_lock held on oldcon */
    PM_UNLOCK(oldcon->c_lock);
    /* FALLTHROUGH */

new_fail:
    /*
     * failure, get here with no locks held, but tmpcon may have
     * malloc'd memory we need to free
     */
    if (tmpcon.c_archctl != NULL) {
	if (tmpcon.c_archctl->ac_log_list != NULL) {
	    /*
	     * ac_log_list is messy because we can fail in a number of
	     * places while setting this up ... and note that
	     * tmpcon.c_archctl->ac_num_logs may be < ac_num_logs in the
	     * old context in this case
	     */
	    for (i = 0; i < tmpcon.c_archctl->ac_num_logs; i++) {
		if (tmpcon.c_archctl->ac_log_list[i]->name != NULL)
		    free(tmpcon.c_archctl->ac_log_list[i]->name);
		if (tmpcon.c_archctl->ac_log_list[i]->hostname != NULL)
		    free(tmpcon.c_archctl->ac_log_list[i]->hostname);
		if (tmpcon.c_archctl->ac_log_list[i]->timezone != NULL)
		    free(tmpcon.c_archctl->ac_log_list[i]->timezone);
		if (tmpcon.c_archctl->ac_log_list[i]->zoneinfo != NULL)
		    free(tmpcon.c_archctl->ac_log_list[i]->zoneinfo);
		free(tmpcon.c_archctl->ac_log_list[i]);
	    }
	    free(tmpcon.c_archctl->ac_log_list);
	}
	free(tmpcon.c_archctl);
    }
    if (tmpcon.c_instprof != NULL)
	__pmFreeProfile(tmpcon.c_instprof);
    /* FALLTHROUGH */

done:
    /* return an error code, or the handle for the new context */
    if (sts < 0 && new >= 0 && ctxnum >= 0) {
	PM_LOCK(contexts_lock);
	contexts_map[ctxnum] = MAP_FREE;
	PM_UNLOCK(contexts_lock);
    }

    if (pmDebugOptions.context) {
	fprintf(stderr, "pmDupContext() -> %d\n", sts);
	if (sts >= 0)
	    __pmDumpContext(stderr, sts, PM_INDOM_NULL);
    }

pmapi_return:
    /* no locks held */
    if (pmDebugOptions.pmapi) {
	fprintf(stderr, ":> returns ");
	if (sts >= 0)
	    fprintf(stderr, "%d\n", sts);
	else {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
    }

    return sts;
}

int
pmUseContext(int handle)
{
    int		ctxnum;
    int		sts;

    if (pmDebugOptions.pmapi) {
	fprintf(stderr, "pmUseContext(%d) <:", handle);
    }

    PM_INIT_LOCKS();

    PM_LOCK(contexts_lock);
    if ((ctxnum = map_handle(handle)) < 0) {
	if (pmDebugOptions.context)
	    fprintf(stderr, "pmUseContext(%d) -> %d\n", handle, PM_ERR_NOCONTEXT);
	PM_UNLOCK(contexts_lock);
	sts = PM_ERR_NOCONTEXT;
	goto pmapi_return;
    }

    if (pmDebugOptions.context)
	fprintf(stderr, "pmUseContext(%d) -> contexts[%d]\n", handle, ctxnum);
    PM_TPD(curr_handle) = handle;
    PM_TPD(curr_ctxp) = contexts[ctxnum];

    PM_UNLOCK(contexts_lock);

    sts = 0;

pmapi_return:

    if (pmDebugOptions.pmapi) {
	fprintf(stderr, ":> returns ");
	if (sts >= 0)
	    fprintf(stderr, "%d\n", sts);
	else {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
    }

    return sts;
}

static void
__pmPMCDCtlFree(__pmPMCDCtl *cp)
{
    struct linger	dolinger = {0, 1};

    if (cp->pc_fd >= 0) {
	/* before close, unsent data should be flushed */
	__pmSetSockOpt(cp->pc_fd, SOL_SOCKET, SO_LINGER,
			(char *)&dolinger, (__pmSockLen)sizeof(dolinger));
	__pmCloseSocket(cp->pc_fd);
    }
    __pmFreeHostSpec(cp->pc_hosts, cp->pc_nhosts);
    free(cp);
}

int
pmDestroyContext(int handle)
{
    __pmContext	*ctxp;
    int		ctxnum;
    int		sts;

    if (pmDebugOptions.pmapi) {
	fprintf(stderr, "pmDestroyContext(%d) <:", handle);
    }

    PM_INIT_LOCKS();

    PM_LOCK(contexts_lock);
    if ((ctxnum = map_handle(handle)) < 0) {
	if (pmDebugOptions.context)
	fprintf(stderr, "pmDestroyContext(%d) -> %d\n", handle, PM_ERR_NOCONTEXT);
	PM_UNLOCK(contexts_lock);
	sts = PM_ERR_NOCONTEXT;
	goto pmapi_return;
    }

    ctxp = contexts[ctxnum];
    PM_LOCK(ctxp->c_lock);
    contexts_map[ctxnum] = MAP_TEARDOWN;
    PM_UNLOCK(contexts_lock);
    if (ctxp->c_pmcd != NULL) {
	__pmPMCDCtlFree(ctxp->c_pmcd);
	ctxp->c_pmcd = NULL;
    }
    if (ctxp->c_archctl != NULL) {
	__pmFreeInterpData(ctxp);
	__pmArchCtlFree(ctxp->c_archctl);
	ctxp->c_archctl = NULL;
    }
    __pmFreeAttrsSpec(&ctxp->c_attrs);
    /* Note: __pmHashClear sets c_attrs.hsize = 0 and c_attrs.hash = NULL */
    __pmHashClear(&ctxp->c_attrs);

    if (handle == PM_TPD(curr_handle)) {
	/* we have no choice */
	PM_TPD(curr_handle) = PM_CONTEXT_UNDEF;
	PM_TPD(curr_ctxp) = NULL;
    }

    if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	/*
	 * for local context, may need to clear the profile flag in the
	 * associated DSO table entry
	 */
	__pmClearDSOProfile(handle);
    }
    __pmFreeProfile(ctxp->c_instprof);
    ctxp->c_instprof = NULL;
    /* Note: __dmclosecontext sets ctxp->c_dm = NULL */
    __dmclosecontext(ctxp);
    if (pmDebugOptions.context)
	fprintf(stderr, "pmDestroyContext(%d) -> 0, curr_handle=%d\n",
		handle, PM_TPD(curr_handle));

    PM_UNLOCK(ctxp->c_lock);

    PM_LOCK(contexts_lock);
    contexts_map[ctxnum] = MAP_FREE;
    PM_UNLOCK(contexts_lock);

    sts = 0;

pmapi_return:

    if (pmDebugOptions.pmapi) {
	fprintf(stderr, ":> returns ");
	if (sts >= 0)
	    fprintf(stderr, "%d\n", sts);
	else {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "%s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
    }

    return sts;
}

static const char *_mode[] = { "LIVE", "INTERP", "FORW", "BACK" };

/*
 * dump context(s); context == -1 for all contexts, indom == PM_INDOM_NULL
 * for all instance domains.
 *
 * Threadsafe Note:
 *	contexts_lock mutex is not acquired here ... need to avoid
 *	nested locking, and this is only a diagnostic routine so
 *	any data race is an acceptable trade-off
 */
void
__pmDumpContext(FILE *f, int context, pmInDom indom)
{
    int			i, j;
    __pmContext		*con;
    int			idx;

    PM_INIT_LOCKS();

    PM_LOCK(contexts_lock);
    idx = map_handle_nolock(PM_TPD(curr_handle));
    PM_UNLOCK(contexts_lock);
    fprintf(f, "Dump Contexts: current -> contexts[%d] handle %d\n",
	idx, PM_TPD(curr_handle));
    if (PM_TPD(curr_handle) < 0)
	return;

    if (indom != PM_INDOM_NULL) {
	char	strbuf[20];
	fprintf(f, "Dump restricted to indom=%d [%s]\n", 
	        indom, pmInDomStr_r(indom, strbuf, sizeof(strbuf)));
    }

    PM_LOCK(contexts_lock);
    for (i = 0; i < contexts_len; i++) {
	con = contexts[i];
	if (context == -1 || context == i) {
	    fprintf(f, "contexts[%d]", i);
            if (contexts_map[i] == MAP_FREE) {
		fprintf(f, " free\n");
                continue;
            }
            else if (contexts_map[i] == MAP_TEARDOWN) {
		fprintf(f, " being destroyed\n");
                continue;
            }
            else if (con->c_type == PM_CONTEXT_INIT) {
		fprintf(f, " init\n");
                continue;
            }
	    else if (con->c_type == PM_CONTEXT_HOST) {
		fprintf(f, " handle %d:", contexts_map[i]);
		fprintf(f, " host %s:", con->c_pmcd->pc_hosts[0].name);
		fprintf(f, " pmcd=%s profile=%s fd=%d",
		    (con->c_pmcd->pc_fd < 0) ? "NOT CONNECTED" : "CONNECTED",
		    con->c_sent ? "SENT" : "NOT_SENT",
		    con->c_pmcd->pc_fd);
		if (con->c_flags)
		    fprintf(f, " flags=%x", con->c_flags);
	    }
	    else if (con->c_type == PM_CONTEXT_LOCAL) {
		fprintf(f, " handle %d:", contexts_map[i]);
		fprintf(f, " standalone:");
		fprintf(f, " profile=%s\n",
		    con->c_sent ? "SENT" : "NOT_SENT");
	    }
	    else if (con->c_type == PM_CONTEXT_ARCHIVE) {
		fprintf(f, " handle %d:", contexts_map[i]);
		for (j = 0; j < con->c_archctl->ac_num_logs; j++) {
		    fprintf(f, " log %s:",
			    con->c_archctl->ac_log_list[j]->name);
		    if (con->c_archctl->ac_log == NULL ||
			con->c_archctl->ac_log->refcnt == 0 ||
			strcmp (con->c_archctl->ac_log_list[j]->name,
				con->c_archctl->ac_log->name) != 0) {
			fprintf(f, " not open\n");
			continue;
		    }
		    fprintf(f, " mode=%s", _mode[con->c_mode]);
		    fprintf(f, " profile=%s tifd=%d mdfd=%d mfd=%d\nrefcnt=%d vol=%d",
			    con->c_sent ? "SENT" : "NOT_SENT",
			    con->c_archctl->ac_log->tifp == NULL ? -1 : __pmFileno(con->c_archctl->ac_log->tifp),
			    __pmFileno(con->c_archctl->ac_log->mdfp),
			    __pmFileno(con->c_archctl->ac_mfp),
			    con->c_archctl->ac_log->refcnt,
			    con->c_archctl->ac_curvol);
		    fprintf(f, " offset=%ld (vol=%d) serial=%d",
			    (long)con->c_archctl->ac_offset,
			    con->c_archctl->ac_vol,
			    con->c_archctl->ac_serial);
		}
	    }
	    if (con->c_type == PM_CONTEXT_HOST ||
	        (con->c_type == PM_CONTEXT_ARCHIVE &&
		 ! (con->c_flags & PM_CTXFLAG_STREAMING_WRITER))) {
		fprintf(f, " origin=%" FMT_INT64 ".%09d",
		    con->c_origin.sec, con->c_origin.nsec);
		fprintf(f, " delta=%s%" FMT_INT64 ".%09d",
		    con->c_direction < 0 ? "-" : "",
		    con->c_delta.sec, con->c_delta.nsec);
	    }
	    if (con->c_flags) {
		/* we have context flags */
		fprintf(f, " flags: ");
		print_ctx_flags(f, con->c_flags);
	    }
	    fputc('\n', f);
	    __pmDumpProfile(f, indom, con->c_instprof);
	}
    }

    PM_UNLOCK(contexts_lock);
}

#ifdef PM_MULTI_THREAD
#ifdef PM_MULTI_THREAD_DEBUG
/*
 * return context slot # if lock == c_lock for a context ... no locking here
 * to avoid recursion ad nauseum
 */
int
__pmIsContextLock(void *lock)
{
    int		i;
    for (i = 0; i < contexts_len; i++) {
	if ((void *)&contexts[i]->c_lock == lock)
	    return i;
    }
    return -1;
}

/*
 * return list of context handles if lock == lc_lock for an associated
 * __pmLogCtl ... no locking here to avoid recursion ad nauseum
 */
char *
__pmIsLogCtlLock(void *lock)
{
    int		i;
    int		reslen = 0;
    char	*result = NULL;
    __pmContext	*ctxp;

    for (i = 0; i < contexts_len+1; i++) {
	if (i < contexts_len)
	    ctxp = contexts[i];
	else if (PM_TPD(curr_ctxp) != NULL)
	    ctxp = PM_TPD(curr_ctxp);
	else
	    continue;
	if (ctxp->c_archctl == NULL)
	    continue;
	if (ctxp->c_archctl->ac_log == NULL)
	    /* this should not happen, just being careful */
	    continue;
	if ((void *)&ctxp->c_archctl->ac_log->lc_lock == lock) {
	    char	*result_new;
	    char	number[16];
	    int		numlen;

	    numlen = pmsprintf(number, sizeof(number), "%d", ctxp->c_handle);
	    if (reslen == 0) {
		reslen = numlen + 1;
		if ((result = malloc(reslen)) == NULL) {
		    pmNoMem("__pmIsLogCtlLock: malloc", reslen, PM_FATAL_ERR);
		    /* NOTREACHED */
		}
		pmstrncpy(result, reslen, number);
	    }
	    else {
		reslen += 1 + numlen + 1;
		if ((result_new = (char *)realloc(result, reslen)) == NULL) {
		    pmNoMem("__pmIsLogCtlLock: realloc", reslen, PM_FATAL_ERR);
		    /* NOTREACHED */
		}
		result = result_new;
		pmstrncat(result, reslen, ",");
		pmstrncat(result, reslen, number);
	    }
	}
    }
    return result;
}


#endif
#endif
