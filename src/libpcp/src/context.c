/*
 * Copyright (c) 1995-2002,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2007 Aconex.  All Rights Reserved.
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

#ident "$Id: context.c,v 1.4 2006/05/02 05:57:28 makc Exp $"

#include <sys/param.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"

#define PM_CONTEXT_UNDEF	-1	/* current context is undefined */

static __pmContext	*contexts = NULL;		/* array of contexts */
static int		contexts_len = 0;		/* number of contexts */
static int		curcontext = PM_CONTEXT_UNDEF;	/* current context */

static int	n_backoff = 0;
static int	def_backoff[] = {5, 10, 20, 40, 80};
static int	*backoff = NULL;

static void
waitawhile(__pmPMCDCtl *ctl)
{
    /*
     * after failure, compute delay before trying again ...
     */
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
		    /*NOTREACHED*/
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
    if (ctl->pc_timeout == 0)
	ctl->pc_timeout = 1;
    else if (ctl->pc_timeout < n_backoff)
	ctl->pc_timeout++;
    ctl->pc_again = time(NULL) + backoff[ctl->pc_timeout-1];
}

__pmContext *
__pmHandleToPtr(int handle)
{
    if (handle < 0 || handle >= contexts_len ||
	contexts[handle].c_type == PM_CONTEXT_FREE)
	return NULL;
    else
	return &contexts[handle];
}

const char * 
pmGetContextHostName (int ctxid)
{
    __pmContext * ctx;

    if ( (ctx = __pmHandleToPtr(ctxid)) != NULL) {
	switch (ctx->c_type) {
	case PM_CONTEXT_HOST:
	    return (ctx->c_pmcd->pc_hosts[0].name);

	case PM_CONTEXT_ARCHIVE:
	    return (ctx->c_archctl->ac_log->l_label.ill_hostname);
	}
    }

    return ("");
}

#if defined(IRIX6_5)
#pragma optional pmGetContextHostName
#endif

int
pmWhichContext(void)
{
    /*
     * return curcontext, provided it is defined
     */
    int		sts;

    if (curcontext > PM_CONTEXT_UNDEF)
	sts = curcontext;
    else
	sts = PM_ERR_NOCONTEXT;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmWhichContext() -> %d, cur=%d\n",
	    sts, curcontext);
#endif
    return sts;
}


int
pmNewContext(int type, const char *name)
{
    __pmContext	*new = NULL;
    __pmContext	*list;
    int		i;
    int		sts;
    int		old_curcontext = curcontext;
    int		old_contexts_len = contexts_len;

    /* See if we can reuse a free context */
    for (i = 0; i < contexts_len; i++) {
	if (contexts[i].c_type == PM_CONTEXT_FREE) {
	    curcontext = i;
	    new = &contexts[curcontext];
	    goto INIT_CONTEXT;
	}
    }

#ifdef MALLOC_AUDIT
/* contexts are persistent, and no memory leak here */
#include "no-malloc-audit.h"
#endif

    /* Create a new one */
    if (contexts == NULL)
	list = (__pmContext *)malloc(sizeof(__pmContext));
    else
	list = (__pmContext *)realloc((void *)contexts, (1+contexts_len) * sizeof(__pmContext));

#ifdef MALLOC_AUDIT
/*
 * but other code may contain memory leaks ...
 * note { } 'cause prototypes in the include file
 */
{
#include "malloc-audit.h"
}
#endif

    if (list == NULL) {
	/* fail : nothing changed */
	sts = -errno;
	goto FAILED;
    }

    contexts = list;
    curcontext = contexts_len++;
    new = &contexts[curcontext];

INIT_CONTEXT:
    /*
     * Set up the default state
     */
    memset(new, 0, sizeof(__pmContext));
    new->c_type = (type & PM_CONTEXT_TYPEMASK);
    if ((new->c_instprof = (__pmProfile *)malloc(sizeof(__pmProfile))) == NULL) {
	/*
	 * fail : nothing changed -- actually list is changed, but restoring
	 * contexts_len should make it ok next time through
	 */
	sts = -errno;
	goto FAILED;
    }
    memset(new->c_instprof, 0, sizeof(__pmProfile));
    new->c_instprof->state = PM_PROFILE_INCLUDE;	/* default global state */
    new->c_sent = 0;	/* profile not sent */
    new->c_origin.tv_sec = new->c_origin.tv_usec = 0;	/* default time */

    if (new->c_type == PM_CONTEXT_HOST) {
	pmHostSpec *hosts;
	int nhosts;

	/* deconstruct a host[:port@proxy:port] specification */
	sts = __pmParseHostSpec(name, &hosts, &nhosts, NULL);
	if (sts < 0)
	    goto FAILED;

	if ((type & PM_CTXFLAG_EXCLUSIVE) == 0 && nhosts == 1) {
	    for (i = 0; i < contexts_len; i++) {
		if (i == curcontext)
		    continue;
		if (contexts[i].c_type == PM_CONTEXT_HOST &&
		    (contexts[i].c_pmcd->pc_curpdu == 0) &&
		    strcmp(contexts[i].c_pmcd->pc_hosts[0].name,
			    hosts[0].name) == 0) {
		    new->c_pmcd = contexts[i].c_pmcd;
		    /*new->c_pduinfo = contexts[i].c_pduinfo;*/
		}
	    }
	}
	if (new->c_pmcd == NULL) {
	    pmcd_ctl_state_t inistate;
	    /*
	     * Try to establish the connection.
	     * If this fails, restore the original current context
	     * and return an error.
	     */
	    if (type & PM_CTXFLAG_SHALLOW) {
		sts = __pmCreateSocket();
		inistate = PC_FETAL;
	    } else {
		sts = __pmConnectPMCD(hosts, nhosts);
		inistate = PC_READY;
	    }

	    if (sts < 0) {
		__pmFreeHostSpec(hosts, nhosts);
		goto FAILED;
	    }

	    new->c_pmcd = (__pmPMCDCtl *)calloc(1,sizeof(__pmPMCDCtl));
	    if (new->c_pmcd == NULL) {
		close(sts);
		sts = -errno;
		__pmFreeHostSpec(hosts, nhosts);
		goto FAILED;
	    }
	    new->c_pmcd->pc_fd = sts;
	    new->c_pmcd->pc_state = inistate;
	    new->c_pmcd->pc_hosts = hosts;
	    new->c_pmcd->pc_nhosts = nhosts;
	}
	new->c_pmcd->pc_refcnt++;
    }
    else if (type == PM_CONTEXT_LOCAL) {
#if defined(HAVE_OBJECT_STYLE)
	if ((sts = __pmCheckObjectStyle()) != 0)
	    goto FAILED;
#endif
	if ((sts = __pmConnectLocal()) != 0)
	    goto FAILED;
    }
    else if (type == PM_CONTEXT_ARCHIVE) {
	if ((new->c_archctl = (__pmArchCtl *)malloc(sizeof(__pmArchCtl))) == NULL) {
	    sts = -errno;
	    goto FAILED;
	}
	new->c_archctl->ac_log = NULL;
	for (i = 0; i < contexts_len; i++) {
	    if (i == curcontext)
		continue;
	    if (contexts[i].c_type == PM_CONTEXT_ARCHIVE &&
		strcmp(name, contexts[i].c_archctl->ac_log->l_name) == 0) {
		new->c_archctl->ac_log = contexts[i].c_archctl->ac_log;
	    }
	}
	if (new->c_archctl->ac_log == NULL) {
	    if ((new->c_archctl->ac_log = (__pmLogCtl *)malloc(sizeof(__pmLogCtl))) == NULL) {
		free(new->c_archctl);
		sts = -errno;
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
	return PM_ERR_NOCONTEXT;
    }

    /* return the handle to the new (current) context */
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	fprintf(stderr, "pmNewContext(%d, %s) -> %d\n", type, name, curcontext);
	__pmDumpContext(stderr, curcontext, PM_INDOM_NULL);
    }
#endif
    return curcontext;

FAILED:
    if (new != NULL) {
	new->c_type = PM_CONTEXT_FREE;
	if (new->c_instprof != NULL)
	    free(new->c_instprof);
    }
    curcontext = old_curcontext;
    contexts_len = old_contexts_len;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmNewContext(%d, %s) -> %d, curcontext=%d\n",
	    type, name, sts, curcontext);
#endif
    return sts;
}


int
pmReconnectContext(int handle)
{
    __pmContext	*ctxp;
    __pmPMCDCtl	*ctl;
    int		sts;

    if (handle < 0 || handle >= contexts_len ||
	contexts[handle].c_type == PM_CONTEXT_FREE) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmReconnectContext(%d) -> %d\n", handle, PM_ERR_NOCONTEXT);
#endif
	    return PM_ERR_NOCONTEXT;
    }

    ctxp = &contexts[handle];
    ctl = ctxp->c_pmcd;
    if (ctxp->c_type == PM_CONTEXT_HOST) {
	if (ctl->pc_timeout && time(NULL) < ctl->pc_again) {
	    /* too soon to try again */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "pmReconnectContext(%d) -> %d, too soon (need wait another %d secs)\n",
		handle, -ETIMEDOUT, (int)(ctl->pc_again - time(NULL)));
#endif
	    return -ETIMEDOUT;
	}

	if (ctl->pc_fd >= 0) {
	    /* don't care if this fails */
	    close(ctl->pc_fd);
	    ctl->pc_fd = -1;
	}

	if ((sts = __pmConnectPMCD(ctl->pc_hosts, ctl->pc_nhosts)) >= 0) {
	    ctl->pc_fd = sts;
	    ctxp->c_sent = 0;
	}

	if (sts < 0) {
	    waitawhile(ctl);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmReconnectContext(%d), failed (wait %d secs before next attempt)\n",
		    handle, (int)(ctl->pc_again - time(NULL)));
#endif
	    return -ETIMEDOUT;
	}
	else {
	    int		i;
	    /*
	     * mark profile as not sent for all contexts sharing this
	     * socket
	     */
	    for (i = 0; i < contexts_len; i++) {
		if (contexts[i].c_type != PM_CONTEXT_FREE && contexts[i].c_pmcd == ctl) {
		    contexts[i].c_sent = 0;
		}
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmReconnectContext(%d), done\n", handle);
#endif
	    ctl->pc_timeout = 0;
	}
    }
    else {
	/*
	 * assume PM_CONTEXT_ARCHIVE or PM_CONTEXT_LOCAL reconnect,
	 * this is a NOP in either case.
	 */
	;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmReconnectContext(%d) -> %d\n", handle, handle);
#endif
    return handle;
}

int
pmDupContext(void)
{
    int			sts;
    int			old, new = -1;
    char		hostspec[4096], *h;
    __pmContext		*newcon, *oldcon;
    __pmInDomProfile	*q, *p, *p_end;
    __pmProfile		*save;

    if ((old = pmWhichContext()) < 0) {
	sts = old;
	goto done;
    }
    oldcon = &contexts[old];
    if (oldcon->c_type == PM_CONTEXT_HOST) {
	h = &hostspec[0];
	__pmUnparseHostSpec(oldcon->c_pmcd->pc_hosts,
			oldcon->c_pmcd->pc_nhosts, &h, sizeof(hostspec));
	new = pmNewContext(oldcon->c_type, hostspec);
    }
    else if (oldcon->c_type == PM_CONTEXT_LOCAL)
	new = pmNewContext(oldcon->c_type, NULL);
    else
	/* assume PM_CONTEXT_ARCHIVE */
	new = pmNewContext(oldcon->c_type, oldcon->c_archctl->ac_log->l_name);
    if (new < 0) {
	/* failed to connect or out of memory */
	sts = new;
	goto done;
    }
    oldcon = &contexts[old];	/* contexts[] may have been relocated */
    newcon = &contexts[new];
    save = newcon->c_instprof;	/* need this later */
    *newcon = *oldcon;		/* struct copy */
    newcon->c_instprof = save;	/* restore saved instprof from pmNewContext */

    /* clone the per-domain profiles (if any) */
    if (oldcon->c_instprof->profile_len > 0) {
	newcon->c_instprof->profile = (__pmInDomProfile *)malloc(
	    oldcon->c_instprof->profile_len * sizeof(__pmInDomProfile));
	if (newcon->c_instprof->profile == NULL) {
	    sts = -errno;
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
		    sts = -errno;
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
	    sts = -errno;
	    goto done;
	}
	*newcon->c_archctl = *oldcon->c_archctl;	/* struct assignment */
    }

    sts = new;

done:
    /* return an error code, or the handle for the new context */
    if (sts < 0 && new >= 0)
	contexts[new].c_type = PM_CONTEXT_FREE;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	fprintf(stderr, "pmDupContext() -> %d\n", sts);
	if (sts >= 0)
	    __pmDumpContext(stderr, sts, PM_INDOM_NULL);
    }
#endif
    return sts;
}

int
pmUseContext(int handle)
{
    if (handle < 0 || handle >= contexts_len ||
	contexts[handle].c_type == PM_CONTEXT_FREE) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmUseContext(%d) -> %d\n", handle, PM_ERR_NOCONTEXT);
#endif
	    return PM_ERR_NOCONTEXT;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmUseContext(%d) -> 0\n", handle);
#endif
    curcontext = handle;
    return 0;
}

int
pmDestroyContext(int handle)
{
    __pmContext		*ctxp;
    struct linger       dolinger = {0, 1};

    if (handle < 0 || handle >= contexts_len ||
	contexts[handle].c_type == PM_CONTEXT_FREE) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmDestroyContext(%d) -> %d\n", handle, PM_ERR_NOCONTEXT);
#endif
	    return PM_ERR_NOCONTEXT;
    }

    ctxp = &contexts[handle];
    if (ctxp->c_pmcd != NULL) {
	if (--ctxp->c_pmcd->pc_refcnt == 0) {
	    if (ctxp->c_pmcd->pc_fd >= 0) {
		/* before close, unsent data should be flushed */
		setsockopt(ctxp->c_pmcd->pc_fd, SOL_SOCKET,
		    SO_LINGER, (char *) &dolinger, (mysocklen_t)sizeof(dolinger));
		close(ctxp->c_pmcd->pc_fd);
	    }
	    __pmFreeHostSpec(ctxp->c_pmcd->pc_hosts, ctxp->c_pmcd->pc_nhosts);
	    free(ctxp->c_pmcd);
	}
    }
    if (ctxp->c_archctl != NULL) {
	if (--ctxp->c_archctl->ac_log->l_refcnt == 0) {
	    __pmLogClose(ctxp->c_archctl->ac_log);
	    free(ctxp->c_archctl->ac_log);
	}
	free(ctxp->c_archctl);
    }
    ctxp->c_type = PM_CONTEXT_FREE;

    if (handle == curcontext)
	/* we have no choice */
	curcontext = PM_CONTEXT_UNDEF;

    __pmFreeProfile(ctxp->c_instprof);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "pmDestroyContext(%d) -> 0, curcontext=%d\n",
		handle, curcontext);
#endif

    return 0;
}

static char *_mode[] = { "LIVE", "INTERP", "FORW", "BACK" };

/*
 * dump context(s); context == -1 for all contexts, indom == PM_INDOM_NULL
 * for all instance domains.
 */
void
__pmDumpContext(FILE *f, int context, pmInDom indom)
{
    int			i;
    __pmContext		*con;

    fprintf(f, "Dump Contexts: current context = %d\n", curcontext);
    if (curcontext < 0)
	return;

    if (indom != PM_INDOM_NULL) {
	fprintf(f, "Dump restricted to indom=%d [%s]\n", 
	        indom, pmInDomStr(indom));
    }

    for (con=contexts, i=0; i < contexts_len; i++, con++) {
	if (context == -1 || context == i) {
	    fprintf(f, "Context[%d]", i);
	    if (con->c_type == PM_CONTEXT_HOST) {
		fprintf(f, " host %s:", con->c_pmcd->pc_hosts[0].name);
		fprintf(f, " pmcd=%s profile=%s fd=%d refcnt=%d",
		    (con->c_pmcd->pc_fd < 0) ? "NOT CONNECTED" : "CONNECTED",
		    con->c_sent ? "SENT" : "NOT_SENT",
		    con->c_pmcd->pc_fd,
		    con->c_pmcd->pc_refcnt);
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
}

int
__pmGetHostContextByID (int ctxid, __pmContext **cp)
{
    __pmContext *ctxp = __pmHandleToPtr(ctxid);

    if (ctxp == NULL) {
	return (PM_ERR_NOCONTEXT);
    } else if (ctxp->c_type != PM_CONTEXT_HOST) {
	return (PM_ERR_NOTHOST);
    } else if ((ctxp->c_pmcd->pc_fd < 0) ||
	       (ctxp->c_pmcd->pc_state != PC_READY)) {
	return (PM_ERR_NOTCONN);
    }
    
    *cp = ctxp;

    return (0);
}

int
__pmGetBusyHostContextByID (int ctxid, __pmContext **cp, int pdu)
{
    int n;

    if ((n = __pmGetHostContextByID (ctxid, cp)) >= 0) {
	if ((*cp)->c_pmcd->pc_curpdu != pdu) {
	    *cp = NULL;
	    n = PM_ERR_CTXBUSY;
	}
    }
    return (n);
}

int
pmGetContextFD (int ctxid)
{
    __pmContext *ctxp = __pmHandleToPtr(ctxid);

    if (ctxp == NULL) {
	return (PM_ERR_NOCONTEXT);
    } else if (ctxp->c_type != PM_CONTEXT_HOST) {
	return (PM_ERR_NOTHOST);
    } else if (ctxp->c_pmcd->pc_fd < 0) {
	return (PM_ERR_NOTCONN);
    }
    return (ctxp->c_pmcd->pc_fd);
}

int
pmGetContextTimeout (int ctxid, int *tout_msec)
{
    __pmContext *ctxp = __pmHandleToPtr(ctxid);
    const struct timeval *tv;

    if (ctxp == NULL) {
	return (PM_ERR_NOCONTEXT);
    } else if (ctxp->c_type != PM_CONTEXT_HOST) {
	return (PM_ERR_NOTHOST);
    } else if (tout_msec == NULL) {
	return (-EINVAL);
    }

    switch (ctxp->c_pmcd->pc_tout_sec) {
    case TIMEOUT_NEVER:
	*tout_msec = -1;
	break;
    case TIMEOUT_DEFAULT:
	tv = __pmDefaultRequestTimeout();

	*tout_msec = tv->tv_sec *1000 + tv->tv_usec / 1000;
	break;

    default:
	*tout_msec = ctxp->c_pmcd->pc_tout_sec * 1000;
	break;
    }

    return (0);
}

int
pmContextConnectTo (int ctxid, const struct sockaddr *addr)
{
    int f;
    pmHostSpec *pmcd;
    __pmPMCDCtl *pc;
    __pmContext *ctxp = __pmHandleToPtr(ctxid);

    if (ctxp == NULL) {
	return (PM_ERR_NOCONTEXT);
    } else if (ctxp->c_type != PM_CONTEXT_HOST) {
	return (PM_ERR_NOTHOST);
    } else if (ctxp->c_pmcd->pc_fd < 0) {
	return (PM_ERR_NOTCONN);
    } else if (ctxp->c_pmcd->pc_state != PC_FETAL) {
	return (PM_ERR_ISCONN);
    }

    pc = ctxp->c_pmcd;
    pmcd = &pc->pc_hosts[0];
    memcpy(&pc->pc_addr, addr, sizeof (pc->pc_addr));
    if (pmcd->nports < 1)
	__pmConnectGetPorts(pmcd);

    if ((f =__pmConnectTo(pc->pc_fd, addr, pmcd->ports[0])) >= 0) {
	const struct timeval *tv = __pmConnectTimeout();

	pc->pc_fdflags = f;
	pc->pc_state = PC_CONN_INPROGRESS;
	pc->pc_tout_sec = tv->tv_sec;
        return (0);
    }

    close (pc->pc_fd);
    pc->pc_fd = -1;
    
    return (f);
}

int
pmContextConnectChangeState (int ctxid)
{
    int f;
    __pmContext *ctxp = __pmHandleToPtr(ctxid);
    __pmPMCDCtl *pc;

    if (ctxp == NULL) {
	return (PM_ERR_NOCONTEXT);
    } else if (ctxp->c_type != PM_CONTEXT_HOST) {
	return (PM_ERR_NOTHOST);
    } else if (ctxp->c_pmcd->pc_fd < 0) {
	return (PM_ERR_NOTCONN);
    }

    /* The assumption is that if pc_fd is less then 0 then state does
     * not matter */
    pc = ctxp->c_pmcd;
    switch (pc->pc_state) {
    case PC_CONN_INPROGRESS:
	if (((f = __pmConnectCheckError (pc->pc_fd)) == 0) &&
	    ((f = __pmConnectRestoreFlags (pc->pc_fd,
					   pc->pc_fdflags)) == pc->pc_fd)) {
	    pc->pc_tout_sec = TIMEOUT_DEFAULT;
	    pc->pc_state = PC_WAIT_FOR_PMCD;
	    f = 0;
	} else if (pc->pc_hosts[0].nports > 1) {
	    int fd;
	    close (pc->pc_fd);

	    if ((fd = __pmCreateSocket ()) >= 0) {
		if (fd != pc->pc_fd) {
		    if ((f = dup2 (fd, pc->pc_fd)) == pc->pc_fd) {
			close (fd);
		    } else {
			fd = -errno;
		    }
		}

		if (fd > 0) {
		    __pmDropHostPort(pc->pc_hosts);
		    pc->pc_state = PC_FETAL;

		    if ((f = __pmConnectTo(pc->pc_fd, &pc->pc_addr,
				   pc->pc_hosts[0].ports[0])) >= 0) {
			pc->pc_fdflags = f;
			pc->pc_state = PC_CONN_INPROGRESS;
			f = 0;
		    }
		} else {
		    f = fd;
		}
	    } else {
		f = fd;
	    }
	} else if (f > 0) {
	    f = __pmMapErrno(-f);
	}
	break;

    case PC_WAIT_FOR_PMCD:
	if ((f = __pmConnectHandshake (pc->pc_fd)) >= 0) {
	    pc->pc_state = PC_READY;
	    f = 0;
	}
	break;

    case PC_READY:
	f = PM_ERR_ISCONN;
	break;

    case PC_FETAL:
	f = PM_ERR_NOTCONN;
	break;

    default:
	f = -EINVAL;
	break;
    }

    if (f) {
	close (pc->pc_fd);
	pc->pc_fd = -1;
    } else if (pc->pc_state != PC_READY) {
	f = PM_ERR_AGAIN;
    }

    return (f);
}


void
pmContextUndef()
{
    curcontext = PM_CONTEXT_UNDEF;
}


