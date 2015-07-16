/*
 * Copyright (c) 2012-2013 Red Hat.
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
 * curcontext needs to be thread-private
 *
 * contexts[] et al and def_backoff[] et al are protected from changes
 * using the libpcp lock
 *
 * The actual contexts (__pmContext) are protected by the (recursive)
 * c_lock mutex which is intialized in pmNewContext() and pmDupContext(),
 * then locked in __pmHandleToPtr() ... it is the responsibility of all
 * __pmHandleToPtr() callers to call PM_UNLOCK(ctxp->c_lock) when they
 * are finished with the context.
 */

#include "pmapi.h"
#include "impl.h"
#include "internal.h"
#include <string.h>

static __pmContext	**contexts;		/* array of context ptrs */
static int		contexts_len;		/* number of contexts */

#ifdef PM_MULTI_THREAD
#ifdef HAVE___THREAD
/* using a gcc construct here to make curcontext thread-private */
static __thread int	curcontext = PM_CONTEXT_UNDEF;	/* current context */
#endif
#else
static int		curcontext = PM_CONTEXT_UNDEF;	/* current context */
#endif

static int		n_backoff;
static int		def_backoff[] = {5, 10, 20, 40, 80};
static int		*backoff;

static void
waitawhile(__pmPMCDCtl *ctl)
{
    /*
     * after failure, compute delay before trying again ...
     */
    PM_LOCK(__pmLock_libpcp);
    if (n_backoff == 0) {
	char	*q;
	/* first time ... try for PMCD_RECONNECT_TIMEOUT from env */
	if ((q = getenv("PMCD_RECONNECT_TIMEOUT")) != NULL) {
	    char	*pend;
	    char	*p;
	    int		val;

	    for (p = q; *p != '\0'; ) {
		val = (int)strtol(p, &pend, 10);
		if (val <= 0 || (*pend != ',' && *pend != '\0')) {
		    __pmNotifyErr(LOG_WARNING,
				 "pmReconnectContext: ignored bad PMCD_RECONNECT_TIMEOUT = '%s'\n",
				 q);
		    n_backoff = 0;
		    if (backoff != NULL)
			free(backoff);
		    break;
		}
		if ((backoff = (int *)realloc(backoff, (n_backoff+1) * sizeof(backoff[0]))) == NULL) {
		    __pmNoMem("pmReconnectContext", (n_backoff+1) * sizeof(backoff[0]), PM_FATAL_ERR);
		}
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
    }
    PM_UNLOCK(__pmLock_libpcp);
    if (ctl->pc_timeout == 0)
	ctl->pc_timeout = 1;
    else if (ctl->pc_timeout < n_backoff)
	ctl->pc_timeout++;
    ctl->pc_again = time(NULL) + backoff[ctl->pc_timeout-1];
}

/*
 * On success, context is locked and caller should unlock it
 */
__pmContext *
__pmHandleToPtr(int handle)
{
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (handle < 0 || handle >= contexts_len ||
	contexts[handle]->c_type == PM_CONTEXT_FREE) {
	PM_UNLOCK(__pmLock_libpcp);
	return NULL;
    }
    else {
	__pmContext	*sts;
	sts = contexts[handle];
	PM_UNLOCK(__pmLock_libpcp);
	PM_LOCK(sts->c_lock);
	return sts;
    }
}

int
__pmPtrToHandle(__pmContext *ctxp)
{
    int		i;
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    for (i = 0; i < contexts_len; i++) {
	if (ctxp == contexts[i]) {
	    PM_UNLOCK(__pmLock_libpcp);
	    return i;
	}
    }
    PM_UNLOCK(__pmLock_libpcp);
    return PM_CONTEXT_UNDEF;
}

/*
 * Determine the hostname associated with the given context.
 */
char *
pmGetContextHostName_r(int ctxid, char *buf, int buflen)
{
    __pmContext *ctxp;
    char	*name;
    pmID	pmid;
    pmResult	*resp;
    int		original;
    int		sts;

    buf[0] = '\0';

    if ((ctxp = __pmHandleToPtr(ctxid)) != NULL) {
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
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmGetContextHostName_r context(%d) -> 0\n", ctxid);
	    original = PM_TPD(curcontext);
	    PM_TPD(curcontext) = ctxid;

	    name = "pmcd.hostname";
	    sts = pmLookupName(1, &name, &pmid);
	    if (sts >= 0)
		sts = pmFetch(1, &pmid, &resp);
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmGetContextHostName_r reset(%d) -> 0\n", original);

	    PM_TPD(curcontext) = original;
	    if (sts >= 0) {
		if (resp->vset[0]->numval > 0) { /* pmcd.hostname present */
		    strncpy(buf, resp->vset[0]->vlist[0].value.pval->vbuf, buflen);
		    pmFreeResult(resp);
		    break;
		}
		pmFreeResult(resp);
		/* FALLTHROUGH */
	    }

	    /*
	     * We could not get the hostname from PMCD.  If the name in the
	     * context structure is a filesystem path (AF_UNIX address) or
	     * 'localhost', then use gethostname(). Otherwise, use the name
	     * from the context structure.
	     */
	    name = ctxp->c_pmcd->pc_hosts[0].name;
	    if (!name || name[0] == __pmPathSeparator() || /* AF_UNIX */
		(strncmp(name, "localhost", 9) == 0)) /* localhost[46] */
		gethostname(buf, buflen);
	    else
		strncpy(buf, name, buflen-1);
	    break;

	case PM_CONTEXT_LOCAL:
	    gethostname(buf, buflen);
	    break;

	case PM_CONTEXT_ARCHIVE:
	    strncpy(buf, ctxp->c_archctl->ac_log->l_label.ill_hostname, buflen-1);
	    break;
	}

	buf[buflen-1] = '\0';
	PM_UNLOCK(ctxp->c_lock);
    }

    return buf;
}

/*
 * Backward-compatibility interface, non-thread-safe variant.
 */
const char *
pmGetContextHostName(int ctxid)
{
    static char	hostbuf[MAXHOSTNAMELEN];
    return (const char *)pmGetContextHostName_r(ctxid, hostbuf, (int)sizeof(hostbuf));
}

int
pmWhichContext(void)
{
    /*
     * return curcontext, provided it is defined
     */
    int		sts;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (PM_TPD(curcontext) > PM_CONTEXT_UNDEF)
	sts = PM_TPD(curcontext);
    else
	sts = PM_ERR_NOCONTEXT;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmWhichContext() -> %d, cur=%d\n",
	    sts, PM_TPD(curcontext));
#endif
    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

int
__pmConvertTimeout(int timeo)
{
    int tout_msec;
    const struct timeval *tv;

    switch (timeo) {
    case TIMEOUT_NEVER:
        tout_msec = -1;
        break;

    case TIMEOUT_DEFAULT:
        tv = __pmDefaultRequestTimeout();
        tout_msec = tv->tv_sec *1000 + tv->tv_usec / 1000;
        break;

    case TIMEOUT_CONNECT:
        tv = __pmConnectTimeout();
        tout_msec = tv->tv_sec *1000 + tv->tv_usec / 1000;
        break;

    default:
        tout_msec = timeo  * 1000;
        break;
    }

    return tout_msec;
}

#ifdef PM_MULTI_THREAD
static void
__pmInitContextLock(pthread_mutex_t *lock)
{
    pthread_mutexattr_t	attr;
    int			sts;
    char		errmsg[PM_MAXERRMSGLEN];

    /*
     * Need context lock to be recursive as we sometimes call
     * __pmHandleToPtr() while the current context is already
     * locked
     */
    if ((sts = pthread_mutexattr_init(&attr)) != 0) {
	pmErrStr_r(-sts, errmsg, sizeof(errmsg));
	fprintf(stderr, "pmNewContext: "
		"context=%d lock pthread_mutexattr_init failed: %s",
		contexts_len-1, errmsg);
	exit(4);
    }
    if ((sts = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE)) != 0) {
	pmErrStr_r(-sts, errmsg, sizeof(errmsg));
	fprintf(stderr, "pmNewContext: "
		"context=%d lock pthread_mutexattr_settype failed: %s",
		contexts_len-1, errmsg);
	exit(4);
    }
    if ((sts = pthread_mutex_init(lock, &attr)) != 0) {
	pmErrStr_r(-sts, errmsg, sizeof(errmsg));
	fprintf(stderr, "pmNewContext: "
		"context=%d lock pthread_mutex_init failed: %s",
		contexts_len-1, errmsg);
	exit(4);
    }
}

static void
__pmInitChannelLock(pthread_mutex_t *lock)
{
    int		sts;
    char	errmsg[PM_MAXERRMSGLEN];

    if ((sts = pthread_mutex_init(lock, NULL)) != 0) {
	pmErrStr_r(-sts, errmsg, sizeof(errmsg));
	fprintf(stderr, "pmNewContext: "
		"context=%d pmcd channel lock pthread_mutex_init failed: %s",
		contexts_len, errmsg);
	exit(4);
    }
}
#else
#define __pmInitContextLock(x)	do { } while (1)
#define __pmInitChannelLock(x)	do { } while (1)
#endif

static int
ctxlocal(__pmHashCtl *attrs)
{
    int sts;
    char *name = NULL;
    char *container = NULL;

    if ((container = getenv("PCP_CONTAINER")) != NULL) {
	if ((name = strdup(container)) == NULL)
	    return -ENOMEM;
	if ((sts = __pmHashAdd(PCP_ATTR_CONTAINER, (void *)name, attrs)) < 0) {
	    free(name);
	    return sts;
	}
    }
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

    if (!secure)
	secure = getenv("PCP_SECURE_SOCKETS");

    if (secure) {
	if (secure[0] == '\0' ||
	   (strcmp(secure, "1")) == 0 ||
	   (strcmp(secure, "enforce")) == 0) {
	    *flags |= PM_CTXFLAG_SECURE;
	} else if (strcmp(secure, "relaxed") == 0) {
	    *flags |= PM_CTXFLAG_RELAXED;
	}
    }

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
    else if ((container = getenv("PCP_CONTAINER")) != NULL) {
	if ((name = strdup(container)) == NULL)
	    return -ENOMEM;
	if ((sts = __pmHashAdd(PCP_ATTR_CONTAINER, (void *)name, attrs)) < 0) {
	    free(name);
	    return sts;
	}
	*flags |= PM_CTXFLAG_CONTAINER;
    }

    return 0;
}

int
pmNewContext(int type, const char *name)
{
    __pmContext	*new = NULL;
    __pmContext	**list;
    int		i;
    int		sts;
    int		old_curcontext;
    int		old_contexts_len;

    PM_INIT_LOCKS();

    if (PM_CONTEXT_LOCAL == (type & PM_CONTEXT_TYPEMASK) &&
	PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
	/* Local context requires single-threaded applications */
	return PM_ERR_THREAD;

    PM_LOCK(__pmLock_libpcp);

    old_curcontext = PM_TPD(curcontext);
    old_contexts_len = contexts_len;

    /* See if we can reuse a free context */
    for (i = 0; i < contexts_len; i++) {
	if (contexts[i]->c_type == PM_CONTEXT_FREE) {
	    PM_TPD(curcontext) = i;
	    new = contexts[i];
	    goto INIT_CONTEXT;
	}
    }

    /* Create a new one */
    if (contexts == NULL)
	list = (__pmContext **)malloc(sizeof(__pmContext *));
    else
	list = (__pmContext **)realloc((void *)contexts, (1+contexts_len) * sizeof(__pmContext *));
    new = (__pmContext *)malloc(sizeof(__pmContext));
    if (list == NULL || new == NULL) {
	/* fail : nothing changed, but new may have been allocated (in theory) */
	if (new)
	    memset(new, 0, sizeof(__pmContext));
	sts = -oserror();
	goto FAILED;
    }

    contexts = list;
    PM_TPD(curcontext) = contexts_len;
    contexts[contexts_len] = new;
    contexts_len++;

INIT_CONTEXT:
    /*
     * Set up the default state
     */
    memset(new, 0, sizeof(__pmContext));
    __pmInitContextLock(&new->c_lock);
    new->c_type = (type & PM_CONTEXT_TYPEMASK);
    new->c_flags = (type & ~PM_CONTEXT_TYPEMASK);
    if ((new->c_instprof = (__pmProfile *)calloc(1, sizeof(__pmProfile))) == NULL) {
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
	pmHostSpec	*hosts = NULL;
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

        /* As an optimization, if there is already a connection to the same PMCD,
           we try to reuse (share) it. */
	if (nhosts == 1) { /* not proxied */
	    for (i = 0; i < contexts_len; i++) {
		if (i == PM_TPD(curcontext))
		    continue;
		if (contexts[i]->c_type == new->c_type &&
		    contexts[i]->c_flags == new->c_flags &&
		    strcmp(contexts[i]->c_pmcd->pc_hosts[0].name, hosts[0].name) == 0 &&
                    contexts[i]->c_pmcd->pc_hosts[0].nports == hosts[0].nports) {
                    int j;
                    int ports_same = 1;
                    for (j=0; j<hosts[0].nports; j++)
                        if (contexts[i]->c_pmcd->pc_hosts[0].ports[j] != hosts[0].ports[j])
                            ports_same = 0;
                    if (ports_same)
                        new->c_pmcd = contexts[i]->c_pmcd;
		}
	    }
	}
	if (new->c_pmcd == NULL) {
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
	    __pmInitChannelLock(&new->c_pmcd->pc_lock);
	}
	else {
	    /* duplicate of an existing context, don't need the __pmHostSpec */
	    __pmFreeHostAttrsSpec(hosts, nhosts, attrs);
	    __pmHashClear(attrs);
	}
	new->c_pmcd->pc_refcnt++;
    }
    else if (new->c_type == PM_CONTEXT_LOCAL) {
	if ((sts = ctxlocal(&new->c_attrs)) != 0)
	    goto FAILED;
	if ((sts = __pmConnectLocal(&new->c_attrs)) != 0)
	    goto FAILED;
    }
    else if (new->c_type == PM_CONTEXT_ARCHIVE) {
	if ((new->c_archctl = (__pmArchCtl *)malloc(sizeof(__pmArchCtl))) == NULL) {
	    sts = -oserror();
	    goto FAILED;
	}
	new->c_archctl->ac_log = NULL;
	for (i = 0; i < contexts_len; i++) {
	    if (i == PM_TPD(curcontext))
		continue;
	    if (contexts[i]->c_type == PM_CONTEXT_ARCHIVE &&
		strcmp(name, contexts[i]->c_archctl->ac_log->l_name) == 0) {
		new->c_archctl->ac_log = contexts[i]->c_archctl->ac_log;
	    }
	}
	if (new->c_archctl->ac_log == NULL) {
	    if ((new->c_archctl->ac_log = (__pmLogCtl *)malloc(sizeof(__pmLogCtl))) == NULL) {
		free(new->c_archctl);
		sts = -oserror();
		goto FAILED;
	    }
	    if ((sts = __pmLogOpen(name, new)) < 0) {
		free(new->c_archctl->ac_log);
		free(new->c_archctl);
		goto FAILED;
	    }
        }
	else {
	    /* archive already open, set default starting state as per __pmLogOpen */
	    new->c_origin.tv_sec = (__int32_t)new->c_archctl->ac_log->l_label.ill_start.tv_sec;
	    new->c_origin.tv_usec = (__int32_t)new->c_archctl->ac_log->l_label.ill_start.tv_usec;
	    new->c_mode = (new->c_mode & 0xffff0000) | PM_MODE_FORW;
	}

	/* start after header + label record + trailer */
	new->c_archctl->ac_offset = sizeof(__pmLogLabel) + 2*sizeof(int);
	new->c_archctl->ac_vol = new->c_archctl->ac_log->l_curvol;
	new->c_archctl->ac_serial = 0;		/* not serial access, yet */
	new->c_archctl->ac_pmid_hc.nodes = 0;	/* empty hash list */
	new->c_archctl->ac_pmid_hc.hsize = 0;
	new->c_archctl->ac_end = 0.0;
	new->c_archctl->ac_want = NULL;
	new->c_archctl->ac_unbound = NULL;
	new->c_archctl->ac_cache = NULL;
	new->c_archctl->ac_log->l_refcnt++;
    }
    else {
	/* bad type */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    fprintf(stderr, "pmNewContext(%d, %s): illegal type\n",
		    type, name);
	}
#endif
	PM_UNLOCK(__pmLock_libpcp);
	return PM_ERR_NOCONTEXT;
    }

    /* bind defined metrics if any ... */
    __dmopencontext(new);

    /* return the handle to the new (current) context */
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	fprintf(stderr, "pmNewContext(%d, %s) -> %d\n", type, name, PM_TPD(curcontext));
	__pmDumpContext(stderr, PM_TPD(curcontext), PM_INDOM_NULL);
    }
#endif
    sts = PM_TPD(curcontext);

    PM_UNLOCK(__pmLock_libpcp);
    return sts;

FAILED:
    if (new != NULL) {
	if (new->c_instprof != NULL)
	    free(new->c_instprof);
	/* only free this pointer if it was not reclaimed from old contexts */
	for (i = 0; i < old_contexts_len; i++) {
	    if (contexts[i] != new)
		continue;
	    new->c_type = PM_CONTEXT_FREE;
	    break;
	}
	if (i == old_contexts_len)
	    free(new);
    }
    PM_TPD(curcontext) = old_curcontext;
    contexts_len = old_contexts_len;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmNewContext(%d, %s) -> %d, curcontext=%d\n",
	    type, name, sts, PM_TPD(curcontext));
#endif
    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

int
pmReconnectContext(int handle)
{
    __pmContext	*ctxp;
    __pmPMCDCtl	*ctl;
    int		i, sts;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (handle < 0 || handle >= contexts_len ||
	contexts[handle]->c_type == PM_CONTEXT_FREE) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "pmReconnectContext(%d) -> %d\n", handle, PM_ERR_NOCONTEXT);
#endif
	PM_UNLOCK(__pmLock_libpcp);
	return PM_ERR_NOCONTEXT;
    }

    ctxp = contexts[handle];
    ctl = ctxp->c_pmcd;
    if (ctxp->c_type == PM_CONTEXT_HOST) {
	if (ctl->pc_timeout && time(NULL) < ctl->pc_again) {
	    /* too soon to try again */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "pmReconnectContext(%d) -> %d, too soon (need wait another %d secs)\n",
		handle, (int)-ETIMEDOUT, (int)(ctl->pc_again - time(NULL)));
#endif
	    PM_UNLOCK(__pmLock_libpcp);
	    return -ETIMEDOUT;
	}

	if (ctl->pc_fd >= 0) {
	    /* don't care if this fails */
	    __pmCloseSocket(ctl->pc_fd);
	    ctl->pc_fd = -1;
	}

	if ((sts = __pmConnectPMCD(ctl->pc_hosts, ctl->pc_nhosts,
				   ctxp->c_flags, &ctxp->c_attrs)) < 0) {
	    waitawhile(ctl);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmReconnectContext(%d), failed (wait %d secs before next attempt)\n",
		    handle, (int)(ctl->pc_again - time(NULL)));
#endif
	    PM_UNLOCK(__pmLock_libpcp);
	    return -ETIMEDOUT;
	}
	else {
	    ctl->pc_fd = sts;
	    ctl->pc_timeout = 0;
	    ctxp->c_sent = 0;

	    /* mark profile as not sent for all contexts sharing this socket */
	    for (i = 0; i < contexts_len; i++) {
		if (contexts[i]->c_type != PM_CONTEXT_FREE && contexts[i]->c_pmcd == ctl) {
		    contexts[i]->c_sent = 0;
		}
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmReconnectContext(%d), done\n", handle);
#endif
	}
    }

    /* clear any derived metrics and re-bind */
    __dmclosecontext(ctxp);
    __dmopencontext(ctxp);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmReconnectContext(%d) -> %d\n", handle, handle);
#endif

    PM_UNLOCK(__pmLock_libpcp);
    return handle;
}

int
pmDupContext(void)
{
    int			sts, oldtype;
    int			old, new = -1;
    char		hostspec[4096];
    __pmContext		*newcon, *oldcon;
    __pmInDomProfile	*q, *p, *p_end;
    __pmProfile		*save;
    void		*save_dm;
#ifdef PM_MULTI_THREAD
    pthread_mutex_t	save_lock;
#endif

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if ((old = pmWhichContext()) < 0) {
	sts = old;
	goto done;
    }
    oldcon = contexts[old];
    oldtype = oldcon->c_type | oldcon->c_flags;
    if (oldcon->c_type == PM_CONTEXT_HOST) {
	__pmUnparseHostSpec(oldcon->c_pmcd->pc_hosts,
			oldcon->c_pmcd->pc_nhosts, hostspec, sizeof(hostspec));
	new = pmNewContext(oldtype, hostspec);
    }
    else if (oldcon->c_type == PM_CONTEXT_LOCAL)
	new = pmNewContext(oldtype, NULL);
    else
	/* assume PM_CONTEXT_ARCHIVE */
	new = pmNewContext(oldtype, oldcon->c_archctl->ac_log->l_name);
    if (new < 0) {
	/* failed to connect or out of memory */
	sts = new;
	goto done;
    }
    oldcon = contexts[old];	/* contexts[] may have been relocated */
    newcon = contexts[new];
    save = newcon->c_instprof;	/* need this later */
    save_dm = newcon->c_dm;	/* need this later */
#ifdef PM_MULTI_THREAD
    save_lock = newcon->c_lock;	/* need this later */
#endif
    if (newcon->c_archctl != NULL)
	free(newcon->c_archctl);	/* will allocate a new one below */
    *newcon = *oldcon;		/* struct copy */
    newcon->c_instprof = save;	/* restore saved instprof from pmNewContext */
    newcon->c_dm = save_dm;	/* restore saved derived metrics control also */
#ifdef PM_MULTI_THREAD
    newcon->c_lock = save_lock;	/* restore saved lock with initialized state also */
#endif

    /* clone the per-domain profiles (if any) */
    if (oldcon->c_instprof->profile_len > 0) {
	newcon->c_instprof->profile = (__pmInDomProfile *)malloc(
	    oldcon->c_instprof->profile_len * sizeof(__pmInDomProfile));
	if (newcon->c_instprof->profile == NULL) {
	    sts = -oserror();
	    goto done;
	}
	memcpy(newcon->c_instprof->profile, oldcon->c_instprof->profile,
	    oldcon->c_instprof->profile_len * sizeof(__pmInDomProfile));
	p = oldcon->c_instprof->profile;
	p_end = p + oldcon->c_instprof->profile_len;
	q = newcon->c_instprof->profile;
	for (; p < p_end; p++, q++) {
	    if (p->instances) {
		q->instances = (int *)malloc(p->instances_len * sizeof(int));
		if (q->instances == NULL) {
		    sts = -oserror();
		    goto done;
		}
		memcpy(q->instances, p->instances,
		    p->instances_len * sizeof(int));
	    }
	}
    }

    /*
     * The call to pmNewContext (above) should have connected to the pmcd.
     * Make sure the new profile will be sent before the next fetch.
     */
    newcon->c_sent = 0;

    /* clone the archive control struct, if any */
    if (oldcon->c_archctl != NULL) {
	if ((newcon->c_archctl = (__pmArchCtl *)malloc(sizeof(__pmArchCtl))) == NULL) {
	    sts = -oserror();
	    goto done;
	}
	*newcon->c_archctl = *oldcon->c_archctl;	/* struct assignment */
	/*
	 * Need to make hash list and read cache independent in case oldcon
	 * is subsequently closed via pmDestroyContext() and don't want
	 * __pmFreeInterpData() to trash our hash list and read cache.
	 * Start with an empty hash list and read cache for the dup'd context.
	 */
	newcon->c_archctl->ac_pmid_hc.nodes = 0;
	newcon->c_archctl->ac_pmid_hc.hsize = 0;
	newcon->c_archctl->ac_cache = NULL;
    }

    sts = new;

done:
    /* return an error code, or the handle for the new context */
    if (sts < 0 && new >= 0)
	contexts[new]->c_type = PM_CONTEXT_FREE;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	fprintf(stderr, "pmDupContext() -> %d\n", sts);
	if (sts >= 0)
	    __pmDumpContext(stderr, sts, PM_INDOM_NULL);
    }
#endif

    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

int
pmUseContext(int handle)
{
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (handle < 0 || handle >= contexts_len ||
	contexts[handle]->c_type == PM_CONTEXT_FREE) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmUseContext(%d) -> %d\n", handle, PM_ERR_NOCONTEXT);
#endif
	    PM_UNLOCK(__pmLock_libpcp);
	    return PM_ERR_NOCONTEXT;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmUseContext(%d) -> 0\n", handle);
#endif
    PM_TPD(curcontext) = handle;

    PM_UNLOCK(__pmLock_libpcp);
    return 0;
}

int
pmDestroyContext(int handle)
{
    __pmContext		*ctxp;
    struct linger       dolinger = {0, 1};
#ifdef PM_MULTI_THREAD
    int			psts;
    char		errmsg[PM_MAXERRMSGLEN];
#endif

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (handle < 0 || handle >= contexts_len ||
	contexts[handle]->c_type == PM_CONTEXT_FREE) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmDestroyContext(%d) -> %d\n", handle, PM_ERR_NOCONTEXT);
#endif
	    PM_UNLOCK(__pmLock_libpcp);
	    return PM_ERR_NOCONTEXT;
    }

    ctxp = contexts[handle];
    PM_LOCK(ctxp->c_lock);
    if (ctxp->c_pmcd != NULL) {
	if (--ctxp->c_pmcd->pc_refcnt == 0) {
	    if (ctxp->c_pmcd->pc_fd >= 0) {
		/* before close, unsent data should be flushed */
		__pmSetSockOpt(ctxp->c_pmcd->pc_fd, SOL_SOCKET,
		    SO_LINGER, (char *) &dolinger, (__pmSockLen)sizeof(dolinger));
		__pmCloseSocket(ctxp->c_pmcd->pc_fd);
	    }
	    __pmFreeHostSpec(ctxp->c_pmcd->pc_hosts, ctxp->c_pmcd->pc_nhosts);
	    free(ctxp->c_pmcd);
	}
    }
    if (ctxp->c_archctl != NULL) {
	__pmFreeInterpData(ctxp);
	if (--ctxp->c_archctl->ac_log->l_refcnt == 0) {
	    __pmLogClose(ctxp->c_archctl->ac_log);
	    free(ctxp->c_archctl->ac_log);
	}
	if (ctxp->c_archctl->ac_cache != NULL)
	    free(ctxp->c_archctl->ac_cache);
	free(ctxp->c_archctl);
    }
    __pmFreeAttrsSpec(&ctxp->c_attrs);
    __pmHashClear(&ctxp->c_attrs);
    ctxp->c_type = PM_CONTEXT_FREE;

    if (handle == PM_TPD(curcontext))
	/* we have no choice */
	PM_TPD(curcontext) = PM_CONTEXT_UNDEF;

    __pmFreeProfile(ctxp->c_instprof);
    __dmclosecontext(ctxp);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmDestroyContext(%d) -> 0, curcontext=%d\n",
		handle, PM_TPD(curcontext));
#endif


    PM_UNLOCK(ctxp->c_lock);
#ifdef PM_MULTI_THREAD
    if ((psts = pthread_mutex_destroy(&ctxp->c_lock)) != 0) {
	pmErrStr_r(-psts, errmsg, sizeof(errmsg));
	fprintf(stderr, "pmDestroyContext(context=%d): pthread_mutex_destroy failed: %s\n", handle, errmsg);
	/*
	 * Most likely cause is the mutex still being locked ... this is a
	 * a library bug, but potentially recoverable ...
	 */
	while (PM_UNLOCK(ctxp->c_lock) >= 0) {
	    fprintf(stderr, "pmDestroyContext(context=%d): extra unlock?\n", handle);
	}
	if ((psts = pthread_mutex_destroy(&ctxp->c_lock)) != 0) {
	    pmErrStr_r(-psts, errmsg, sizeof(errmsg));
	    fprintf(stderr, "pmDestroyContext(context=%d): pthread_mutex_destroy failed second try: %s\n", handle, errmsg);
	}
	/* keep going, rather than exit ... */
    }
#endif

    PM_UNLOCK(__pmLock_libpcp);
    return 0;
}

static const char *_mode[] = { "LIVE", "INTERP", "FORW", "BACK" };

/*
 * dump context(s); context == -1 for all contexts, indom == PM_INDOM_NULL
 * for all instance domains.
 */
void
__pmDumpContext(FILE *f, int context, pmInDom indom)
{
    int			i;
    __pmContext		*con;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    fprintf(f, "Dump Contexts: current context = %d\n", PM_TPD(curcontext));
    if (PM_TPD(curcontext) < 0) {
	PM_UNLOCK(__pmLock_libpcp);
	return;
    }

    if (indom != PM_INDOM_NULL) {
	char	strbuf[20];
	fprintf(f, "Dump restricted to indom=%d [%s]\n", 
	        indom, pmInDomStr_r(indom, strbuf, sizeof(strbuf)));
    }

    for (i = 0; i < contexts_len; i++) {
	con = contexts[i];
	if (context == -1 || context == i) {
	    fprintf(f, "Context[%d]", i);
	    if (con->c_type == PM_CONTEXT_HOST) {
		fprintf(f, " host %s:", con->c_pmcd->pc_hosts[0].name);
		fprintf(f, " pmcd=%s profile=%s fd=%d refcnt=%d",
		    (con->c_pmcd->pc_fd < 0) ? "NOT CONNECTED" : "CONNECTED",
		    con->c_sent ? "SENT" : "NOT_SENT",
		    con->c_pmcd->pc_fd,
		    con->c_pmcd->pc_refcnt);
		if (con->c_flags)
		    fprintf(f, " flags=%x", con->c_flags);
	    }
	    else if (con->c_type == PM_CONTEXT_LOCAL) {
		fprintf(f, " standalone:");
		fprintf(f, " profile=%s\n",
		    con->c_sent ? "SENT" : "NOT_SENT");
	    }
	    else {
		fprintf(f, " log %s:", con->c_archctl->ac_log->l_name);
		fprintf(f, " mode=%s", _mode[con->c_mode & __PM_MODE_MASK]);
		fprintf(f, " profile=%s tifd=%d mdfd=%d mfd=%d\nrefcnt=%d vol=%d",
		    con->c_sent ? "SENT" : "NOT_SENT",
		    con->c_archctl->ac_log->l_tifp == NULL ? -1 : fileno(con->c_archctl->ac_log->l_tifp),
		    fileno(con->c_archctl->ac_log->l_mdfp),
		    fileno(con->c_archctl->ac_log->l_mfp),
		    con->c_archctl->ac_log->l_refcnt,
		    con->c_archctl->ac_log->l_curvol);
		fprintf(f, " offset=%ld (vol=%d) serial=%d",
		    (long)con->c_archctl->ac_offset,
		    con->c_archctl->ac_vol,
		    con->c_archctl->ac_serial);
	    }
	    if (con->c_type != PM_CONTEXT_LOCAL) {
		fprintf(f, " origin=%d.%06d",
		    con->c_origin.tv_sec, con->c_origin.tv_usec);
		fprintf(f, " delta=%d\n", con->c_delta);
	    }
	    __pmDumpProfile(f, indom, con->c_instprof);
	}
    }

    PM_UNLOCK(__pmLock_libpcp);
}

#ifdef PM_MULTI_THREAD
#ifdef PM_MULTI_THREAD_DEBUG
/*
 * return context if lock == c_lock for a context ... no locking here
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
 * return context if lock == pc_lock for a context ... no locking here
 * to avoid recursion ad nauseum
 */
int
__pmIsChannelLock(void *lock)
{
    int		i;
    for (i = 0; i < contexts_len; i++) {
	if ((void *)&contexts[i]->c_pmcd->pc_lock == lock)
	    return i;
    }
    return -1;
}
#endif
#endif
