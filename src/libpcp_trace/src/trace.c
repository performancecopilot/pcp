/*
 * trace.c - client-side interface for trace PMDA
 *
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 1997-2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <inttypes.h>
#include "pmapi.h"
#include "impl.h"

#if defined(HAVE_PTHREAD_H)
#include <pthread.h>
#elif defined(HAVE_ABI_MUTEX_H)
#include <abi_mutex.h>
#elif !defined(IS_MINGW)
#error !bozo!
#endif

#include "hash.h"
#include "trace.h"
#include "trace_dev.h"

static int	_pmtimedout = 1;
static time_t	_pmttimeout = 0;

static int _pmtraceconnect(int);
static int _pmtracereconnect(void);
static int _pmauxtraceconnect(void);
static void _pmtraceupdatewait(void);
static int _pmtracegetack(int, int);
static int _pmtraceremaperr(int);
static __uint64_t _pmtraceid(void);

int	__pmstate = PMTRACE_STATE_NONE;

/* transaction data unit */
typedef struct {
    __uint64_t		id;
    char		*tag;
    unsigned int	tracetype  : 7;
    unsigned int	inprogress : 1;
    unsigned int	taglength  : 8;
    unsigned int	pad	: 16;
    struct timeval	start;		/* transaction started timestamp */
    double		data;		/* time interval / -1 / user-defined */
} _pmTraceLibdata;

static int
_pmlibcmp(void *a, void *b)
{
    _pmTraceLibdata	*aa = (_pmTraceLibdata *)a;
    _pmTraceLibdata	*bb = (_pmTraceLibdata *)b;

    if (!aa || !bb || (aa->id != bb->id))
	return 0;
    if (aa->tracetype != bb->tracetype)
	return 0;
    if (aa->id != bb->id)
	return 0;
    return !strcmp(aa->tag, bb->tag);
}

static void
_pmlibdel(void *entry)
{
    _pmTraceLibdata	*data = (_pmTraceLibdata *)entry;

    if (data->tag)
	free(data->tag);
    if (data)
	free(data);
}

static int		__pmfd;
static __pmHashTable	_pmtable;

#if defined(HAVE_PTHREAD_MUTEX_T)

/* use portable pthreads for mutex */
static pthread_mutex_t	_pmtracelock;
#define TRACE_LOCK_INIT	pthread_mutex_init(&_pmtracelock, NULL)
#define TRACE_LOCK	pthread_mutex_lock(&_pmtracelock)
#define TRACE_UNLOCK	pthread_mutex_unlock(&_pmtracelock)

#elif defined(HAVE_ABI_MUTEX_H)
/* use an SGI spinlock for mutex */
static abilock_t        _pmtracelock;
#define TRACE_LOCK_INIT	init_lock(&_pmtracelock)
#define TRACE_LOCK	spin_lock(&_pmtracelock)
#define TRACE_UNLOCK	release_lock(&_pmtracelock)

#elif defined(IS_MINGW)
/* use native Win32 primitives */
static HANDLE _pmtracelock;
#define TRACE_LOCK_INIT (_pmtracelock = CreateMutex(NULL, FALSE, NULL), 0)
#define TRACE_LOCK	WaitForSingleObject(_pmtracelock, INFINITE)
#define TRACE_UNLOCK	ReleaseMutex(_pmtracelock)

#else
#error !bozo!
#endif

int
pmtracebegin(const char *tag)
{
    static int		first = 1;
    _pmTraceLibdata	*hptr;
    _pmTraceLibdata	hash;
    int			len, a_sts = 0, b_sts = 0, protocol;

    if (tag == NULL || *tag == '\0')
	return PMTRACE_ERR_TAGNAME;
    if ((len = strlen(tag)+1) >= MAXTAGNAMELEN)
	return PMTRACE_ERR_TAGLENGTH;

    hash.tag = (char *)tag;
    hash.taglength = len;
    hash.id = _pmtraceid();
    hash.tracetype = TRACE_TYPE_TRANSACT;

    /*
     * We need to do both the connect and hash table manipulation,
     * otherwise the reconnect isn't reliable and the hash table 
     * (potentially) becomes completely wrong, and we reject some
     * transact calls which actually were in a valid call sequence.
     */

    protocol = __pmtraceprotocol(TRACE_PROTOCOL_QUERY);

    if (_pmtimedout && (a_sts = _pmtraceconnect(1)) < 0) {
	if (first || protocol == TRACE_PROTOCOL_ASYNC)
	    return a_sts;	/* exception to the rule */
	a_sts = _pmtraceremaperr(a_sts);
	if (a_sts == PMTRACE_ERR_IPC && protocol == TRACE_PROTOCOL_SYNC) {
	    _pmtimedout = 1;	/* try reconnect */
	    a_sts = 0;
	}
    }
    if (a_sts >= 0)
	first = 0;

    /* lock hash table for search and subsequent insert/update */
    TRACE_LOCK;

    if ((hptr = __pmhashlookup(&_pmtable, tag, &hash)) == NULL) {
#ifdef PMTRACE_DEBUG
	if (__pmstate & PMTRACE_STATE_API)
	    fprintf(stderr, "pmtracebegin: new transaction '%s' "
			    "(id=0x%" PRIx64 ")\n", tag, hash.id);
#endif
	hash.pad = 0;
	if ((hash.tag = strdup(tag)) == NULL)
	    b_sts = -oserror();
	__pmtimevalNow(&hash.start);
	if (b_sts >= 0) {
	    hash.inprogress = 1;
	    b_sts = __pmhashinsert(&_pmtable, tag, &hash);
	}
	if (b_sts < 0 && hash.tag != NULL)
	    free(hash.tag);
    }
    else if (hptr->inprogress == 1)
	b_sts = PMTRACE_ERR_INPROGRESS;
    else if (hptr->tracetype != TRACE_TYPE_TRANSACT)
	b_sts = PMTRACE_ERR_TAGTYPE;
    else {
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_API)
	fprintf(stderr, "pmtracebegin: updating transaction '%s' "
			"(id=0x%" PRIx64 ")\n", tag, hash.id);
#endif
	__pmtimevalNow(&hptr->start);
	hptr->inprogress = 1;
    }

    /* unlock hash table */
    if (TRACE_UNLOCK != 0)
	b_sts = -oserror();

    if (a_sts < 0)
	return a_sts;
    return b_sts;
}

int
pmtraceend(const char *tag)
{
    _pmTraceLibdata	hash;
    _pmTraceLibdata	*hptr;
    struct timeval	now;
    int			len, protocol, sts = 0;

    if (tag == NULL || *tag == '\0')
	return PMTRACE_ERR_TAGNAME;
    if ((len = strlen(tag)+1) >= MAXTAGNAMELEN)
	return PMTRACE_ERR_TAGLENGTH;

    __pmtimevalNow(&now);

    /* give just enough info for comparison routine */
    hash.tag = (char *)tag;
    hash.taglength = len;
    hash.id = _pmtraceid();
    hash.tracetype = TRACE_TYPE_TRANSACT;

    /* lock hash table for search and update then send data */
    TRACE_LOCK;

    if ((hptr = __pmhashlookup(&_pmtable, tag, &hash)) == NULL)
	sts = PMTRACE_ERR_NOSUCHTAG;
    else if (hptr->inprogress != 1)
	sts = PMTRACE_ERR_NOPROGRESS;
    else if (hptr->tracetype != TRACE_TYPE_TRANSACT)
	sts = PMTRACE_ERR_TAGTYPE;
    else {
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_API)
	fprintf(stderr, "pmtraceend: sending transaction data '%s' "
			"(id=0x%" PRIx64 ")\n", tag, hash.id);
#endif
	hptr->inprogress = 0;
	hptr->data = __pmtimevalSub(&now, &hptr->start);

	if (sts >= 0 && _pmtimedout) {
	    sts = _pmtracereconnect();
	    sts = _pmtraceremaperr(sts);
	}

	if (sts >= 0) {
	    sts = __pmtracesenddata(__pmfd, hptr->tag, hptr->taglength,
					TRACE_TYPE_TRANSACT, hptr->data);
	    sts = _pmtraceremaperr(sts);
	}

	protocol = __pmtraceprotocol(TRACE_PROTOCOL_QUERY);

	if (sts >= 0 && protocol == TRACE_PROTOCOL_SYNC)
	    sts = _pmtracegetack(sts, TRACE_TYPE_TRANSACT);

	if (sts == PMTRACE_ERR_IPC && protocol == TRACE_PROTOCOL_SYNC) {
	    _pmtimedout = 1;	/* try reconnect */
	    sts = 0;
	}
    }

    if (TRACE_UNLOCK != 0)
	return -oserror();

    return sts;
}

int
pmtraceabort(const char *tag)
{
    _pmTraceLibdata	hash;
    _pmTraceLibdata	*hptr;
    int			len, sts = 0;

    if (tag == NULL || *tag == '\0')
	return PMTRACE_ERR_TAGNAME;
    if ((len = strlen(tag)+1) >= MAXTAGNAMELEN)
	return PMTRACE_ERR_TAGLENGTH;

    hash.tag = (char *)tag;
    hash.taglength = len;
    hash.id = _pmtraceid();
    hash.tracetype = TRACE_TYPE_TRANSACT;

    /* lock hash table for search and update */
    TRACE_LOCK;
    if ((hptr = __pmhashlookup(&_pmtable, tag, &hash)) == NULL)
	sts = PMTRACE_ERR_NOSUCHTAG;
    else if (hptr->inprogress != 1)
	sts = PMTRACE_ERR_NOPROGRESS;
    else if (hptr->tracetype != TRACE_TYPE_TRANSACT)
	sts = PMTRACE_ERR_TAGTYPE;
    else {
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_API)
	fprintf(stderr, "pmtraceabort: aborting transaction '%s' "
			"(id=0x%" PRIx64 ")\n", tag, hash.id);
#endif
	hptr->inprogress = 0;
    }

    if (TRACE_UNLOCK != 0)
	return -oserror();

    return sts;
}


static int
_pmtracecommon(const char *label, double value, int type)
{
    static int	first = 1;
    int		taglength;
    int		protocol;
    int		sts = 0;

    if (label == NULL || *label == '\0')
	return PMTRACE_ERR_TAGNAME;

    taglength = (unsigned int)strlen(label)+1;
    if (taglength >= MAXTAGNAMELEN)
	return PMTRACE_ERR_TAGLENGTH;

#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_API)
	fprintf(stderr, "_pmtracecommon: trace tag '%s' (type=%d,value=%f)\n",
		label, type, value);
#endif

    protocol = __pmtraceprotocol(TRACE_PROTOCOL_QUERY);

    if (_pmtimedout && (sts = _pmtraceconnect(1)) < 0) {
	if (first || protocol == TRACE_PROTOCOL_ASYNC)
	    return sts;
	sts = _pmtraceremaperr(sts);
	if (sts == PMTRACE_ERR_IPC && protocol == TRACE_PROTOCOL_SYNC) {
	    _pmtimedout = 1;	/* try reconnect */
	    sts = 0;
	}
	return sts;
    }
    first = 0;

    TRACE_LOCK;

    if (sts >= 0 && _pmtimedout) {
	sts = _pmtracereconnect();
	sts = _pmtraceremaperr(sts);
    }

    if (sts >= 0) {
	sts = __pmtracesenddata(__pmfd, (char *)label, (int)strlen(label)+1,
						    type, value);
	sts = _pmtraceremaperr(sts);
    }

    protocol = __pmtraceprotocol(TRACE_PROTOCOL_QUERY);

    if (sts >= 0 && protocol == TRACE_PROTOCOL_SYNC)
	sts = _pmtracegetack(sts, type);

    if (sts == PMTRACE_ERR_IPC && protocol == TRACE_PROTOCOL_SYNC) {
	_pmtimedout = 1;
	sts = 0;
    }

    if (TRACE_UNLOCK != 0)
	return -oserror();

    return sts;
}


int
pmtracepoint(const char *label)
{
    return _pmtracecommon(label, -1, TRACE_TYPE_POINT);
}


int
pmtracecounter(const char *label, double value)
{
    return _pmtracecommon(label, value, TRACE_TYPE_COUNTER);
}

int
pmtraceobs(const char *label, double value)
{
    return _pmtracecommon(label, value, TRACE_TYPE_OBSERVE);
}


char *
pmtraceerrstr(int code)
{
    static const struct {
	int	code;
	char	*msg;
    } errtab[] = {
	{ PMTRACE_ERR_TAGNAME,
		"Invalid tag name - tag names cannot be NULL" },
	{ PMTRACE_ERR_INPROGRESS,
		"Transaction is already in progress - cannot begin" },
	{ PMTRACE_ERR_NOPROGRESS,
		"Transaction is not currently in progress - cannot end" },
	{ PMTRACE_ERR_NOSUCHTAG,
		"Transaction tag was not successfully initialised" },
	{ PMTRACE_ERR_TAGTYPE,
		"Tag is already in use for a different type of tracing" },
	{ PMTRACE_ERR_TAGLENGTH,
		"Tag name is too long (maximum 256 characters)" },
	{ PMTRACE_ERR_IPC,
		"IPC protocol failure" },
	{ PMTRACE_ERR_ENVFORMAT,
		"Unrecognised environment variable format" },
	{ PMTRACE_ERR_TIMEOUT,
		"Application timed out connecting to the PMDA" },
	{ PMTRACE_ERR_VERSION,
		"Incompatible versions between application and PMDA" },
	{ PMTRACE_ERR_PERMISSION,
		"Cannot connect to PMDA - permission denied" },
        { PMTRACE_ERR_CONNLIMIT,
                "Cannot connect to PMDA - connection limit reached" },
	{ 0, "" }
    };

    if ((code < 0) && (code > -PMTRACE_ERR_BASE))
	/* intro(2) errors */
	return strerror(-code);
    else if (code == 0)
	return "No error.";
    else {
	int	i;
	for (i=0; errtab[i].code; i++) {
	    if (errtab[i].code == code)
		return errtab[i].msg;
	}
    }
    return "Unknown error code.";
}


static int
_pmtraceremaperr(int sts)
{
    int save_oserror;
    int socket_closed;

    /*
     * sts is negative.
     * Use __pmSocketClosed() to decode it, since it may have come from
     * __pmSecureSocketsError(). __pmSocketClosed uses oserror() and expects it to
     * be non-negative.
     */
    save_oserror = oserror();
    setoserror(-sts);
    socket_closed = __pmSocketClosed();
    setoserror(save_oserror);

#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_COMMS)
	fprintf(stderr, "_pmtraceremaperr: status %d remapped to %d\n", sts,
		socket_closed ? PMTRACE_ERR_IPC : sts);
#endif

    if (socket_closed) {
	_pmtimedout = 1;
	return PMTRACE_ERR_IPC;
    }
    return sts;
}


static __uint64_t
_pmtraceid(void)
{
    __uint64_t	myid = 0;
    myid |= getpid();
    return myid;
}


/*
 * gets an ack from the PMDA, based on expected ACK type.
 * if type is zero, expects and returns the version, otherwise
 * ACK is for a sent data PDU.
 */
static int
_pmtracegetack(int sts, int type)
{
    if (sts >= 0) {
	__pmTracePDU	*ack;
	int		status, acktype;

#ifdef PMTRACE_DEBUG
	if (__pmstate & PMTRACE_STATE_NOAGENT) {
	    fprintf(stderr, "_pmtracegetack: awaiting ack (skipped)\n");
	    return 0;
	}
	else if (__pmstate & PMTRACE_STATE_COMMS)
	    fprintf(stderr, "_pmtracegetack: awaiting ack ...\n");
#endif

	status = __pmtracegetPDU(__pmfd, TRACE_TIMEOUT_DEFAULT, &ack);

	if (status < 0) {
	    if ((status = _pmtraceremaperr(status)) == PMTRACE_ERR_IPC)
		return 0;	/* hide this - try reconnect later */
	    return status;
	}
	else if (status == 0) {
	    _pmtimedout = 1;
	    return PMTRACE_ERR_IPC;
	}
	else if (status == TRACE_PDU_ACK) {
	    if ((status = __pmtracedecodeack(ack, &acktype)) < 0)
		return _pmtraceremaperr(status);
	    else if (type != 0 && acktype == type)
		return 0;	/* alls well */
	    else if (acktype < 0)
		return _pmtraceremaperr(acktype);
	    else if (type == 0)
	        /* acktype contains PDU version if needed (not currently) */
		return acktype;
	    return PMTRACE_ERR_IPC;
	}
	else {
	    fprintf(stderr, "_pmtracegetack: unknown PDU type (0x%x)\n", status);
	    return PMTRACE_ERR_IPC;
	}
    }
    return _pmtraceremaperr(sts);
}


static void
_pmtraceupdatewait(void)
{
    static int	defbackoff[] = {5, 10, 20, 40, 80};
    static int	*backoff = NULL;
    static int	n_backoff = 0;
    char	*q;

#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_COMMS)
	fprintf(stderr, "_pmtraceupdatewait: updating reconnect back-off\n");
#endif
    if (n_backoff == 0) {	/* compute backoff before trying again */
	if ((q = getenv(TRACE_ENV_RECTIMEOUT)) != NULL) {
	    char	*pend, *p;
	    int		val;

	    for (p=q; *p != '\0'; ) {
		val = (int)strtol(p, &pend, 10);
		if (val <= 0 || (*pend != ',' && *pend != '\0')) {
		    n_backoff = 0;
		    if (backoff != NULL)
			free(backoff);
#ifdef PMTRACE_DEBUG
		    if (__pmstate & PMTRACE_STATE_COMMS)
			fprintf(stderr, "_pmtraceupdatewait: bad reconnect "
				"format in %s.\n", TRACE_ENV_RECTIMEOUT);
#endif
		    break;
		}
		if ((backoff = (int *)realloc(backoff, n_backoff * 
						sizeof(backoff[0]))) == NULL)
		    break;
		backoff[n_backoff++] = val;
		if (*pend == '\0')
		    break;
		p = &pend[1];
	    }
	}
	if (n_backoff == 0) {	/* use defaults */
	    n_backoff = 5;
	    backoff = defbackoff;
	}
    }
    if (_pmtimedout == 0)
	_pmtimedout = 1;
    else if (_pmtimedout < n_backoff)
	_pmtimedout++;
    _pmttimeout = time(NULL) + backoff[_pmtimedout-1];
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_COMMS)
	fprintf(stderr, "_pmtraceupdatewait: next attempt after %d seconds\n",
		backoff[_pmtimedout-1]);
#endif
}


static int
_pmtracereconnect(void)
{
#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_NOAGENT) {
	fprintf(stderr, "_pmtracereconnect: reconnect attempt (skipped)\n");
	return 0;
    }
    else if (__pmstate &  PMTRACE_STATE_COMMS) {
	fprintf(stderr, "_pmtracereconnect: attempting PMDA reconnection\n");
    }
#endif

    if (_pmtimedout && time(NULL) < _pmttimeout) {	/* too soon to retry */
#ifdef PMTRACE_DEBUG
	if (__pmstate & PMTRACE_STATE_COMMS)
	    fprintf(stderr, "_pmtracereconnect: too soon to retry "
			    "(%d seconds remain)\n", (int)(_pmttimeout - time(NULL)));
#endif
	return -ETIMEDOUT;
    }
    if (__pmfd >= 0) {
	__pmtracenomoreinput(__pmfd);
	__pmCloseSocket(__pmfd);
	__pmfd = -1;
    }
    if (_pmtraceconnect(1) < 0) {
#ifdef PMTRACE_DEBUG
	if (__pmstate & PMTRACE_STATE_COMMS)
	    fprintf(stderr, "_pmtracereconnect: failed to reconnect\n");
#endif
	_pmtraceupdatewait();
	return -ETIMEDOUT;
    }
    else {
#ifdef PMTRACE_DEBUG
	if (__pmstate & PMTRACE_STATE_COMMS)
	    fprintf(stderr, "_pmtracereconnect: reconnect succeeded!\n");
#endif
	_pmtimedout = 0;
    }
    return 0;
}

#ifndef IS_MINGW
static struct itimerval	_pmmyitimer, off_itimer;
static void _pmtracealarm(int dummy) { }
static void _pmtraceinit(void) { }
#else
static void _pmtraceinit(void)
{
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;
    WSAStartup(wVersionRequested, &wsaData);
    _fmode = O_BINARY;
}
#endif

static int
_pmtraceconnect(int doit)
{
    static int	first = 1;
    int		sts = 0;

    if (!_pmtimedout)
	return 0;
    else if (first) {	/* once-off, not to be done on reconnect */
	_pmtraceinit();
	if (TRACE_LOCK_INIT < 0)
	    return -oserror();
	first = 0;
	TRACE_LOCK;
	sts = __pmhashinit(&_pmtable, 0, sizeof(_pmTraceLibdata),
						_pmlibcmp, _pmlibdel);
    if (TRACE_UNLOCK != 0)
	return -oserror();
    }
    else if (__pmtraceprotocol(TRACE_PROTOCOL_QUERY) == TRACE_PROTOCOL_ASYNC)
	return PMTRACE_ERR_IPC;

    if (sts >= 0 && doit)
	sts = _pmauxtraceconnect();
    if (sts >= 0)
	__pmtraceprotocol(TRACE_PROTOCOL_FINAL);

    return sts;
}

static int
_pmauxtraceconnect(void)
{
    int			port = TRACE_PORT;
    char		hostname[MAXHOSTNAMELEN];
    struct timeval	timeout = { 3, 0 };     /* default 3 secs */
    __pmSockAddr	*myaddr;
    __pmHostEnt		*servinfo;
    void		*enumIx;
#ifndef IS_MINGW
    struct itimerval	_pmolditimer;
    void		(*old_handler)(int foo);
#endif
    int			rc, sts;
    int			flags = 0;
    char		*sptr, *endptr, *endnum;
    struct timeval	canwait = { 5, 000000 };
    struct timeval	stv;
    struct timeval	*pstv;
    __pmFdSet		wfds;

#ifdef PMTRACE_DEBUG
    if (__pmstate & PMTRACE_STATE_NOAGENT) {
	fprintf(stderr, "_pmtraceconnect: connecting to PMDA (skipped)\n");
	return 0;
    }
    else if (__pmstate & PMTRACE_STATE_COMMS)
	fprintf(stderr, "_pmtraceconnect: connecting to PMDA ...\n");
#endif

    /*
     * get optional stuff from environment ...
     *  PCP_TRACE_HOST, PCP_TRACE_PORT, PCP_TRACE_TIMEOUT, and
     *  PCP_TRACE_NOAGENT
     */
    if ((sptr = getenv(TRACE_ENV_HOST)) != NULL)
	strcpy(hostname, sptr);
    else {
       (void)gethostname(hostname, MAXHOSTNAMELEN);
       hostname[MAXHOSTNAMELEN-1] = '\0';
    }
    if ((sptr = getenv(TRACE_ENV_PORT)) != NULL) {
	port = (int)strtol(sptr, &endnum, 0);
	if (*endnum != '\0' || port < 0) {
	    fprintf(stderr, "trace warning: bad PCP_TRACE_PORT ignored.");
	    port = TRACE_PORT;
	}
    }
    if ((sptr = getenv(TRACE_ENV_TIMEOUT)) != NULL) {
	double timesec = strtod(sptr, &endptr);
	if (*endptr != '\0' || timesec < 0.0)
	    fprintf(stderr, "trace warning: bogus PCP_TRACE_TIMEOUT.");
	else {
	    __pmtimevalFromReal(timesec, &timeout);
	}
    }
    if (getenv(TRACE_ENV_NOAGENT) != NULL)
	__pmstate |= PMTRACE_STATE_NOAGENT;

    if ((servinfo = __pmGetAddrInfo(hostname)) == NULL) {
#ifdef PMTRACE_DEBUG
	if (__pmstate & PMTRACE_STATE_COMMS)
	    fprintf(stderr, "_pmtraceconnect(__pmGetAddrInfo(hostname=%s): "
		    "hosterror=%d, ``%s''\n", hostname, hosterror(),
		    hoststrerror());
#endif
	return -EHOSTUNREACH;
    }

    /* Try each address in turn until one connects. */
    sts = EHOSTUNREACH;
    __pmfd = -1;
    enumIx = NULL;
    for (myaddr = __pmHostEntGetSockAddr(servinfo, &enumIx);
	 myaddr != NULL;
	 myaddr = __pmHostEntGetSockAddr(servinfo, &enumIx)) {
	/* Create a socket */
	if (__pmSockAddrIsInet(myaddr))
	    __pmfd = __pmCreateSocket();
	else if (__pmSockAddrIsIPv6(myaddr))
	    __pmfd = __pmCreateIPv6Socket();
	else {
	    fprintf(stderr, "_pmtraceconnect(invalid address family): %d\n",
		    __pmSockAddrGetFamily(myaddr));
	}
	if (__pmfd < 0) {
	    sts = neterror();
	    __pmSockAddrFree(myaddr);
	    continue; /* Try the next address */
	}

	/* Set the port. */
	__pmSockAddrSetPort(myaddr, port);

#ifndef IS_MINGW
	/* arm interval timer */
	_pmmyitimer.it_value.tv_sec = timeout.tv_sec;
	_pmmyitimer.it_value.tv_usec = timeout.tv_usec;
	_pmmyitimer.it_interval.tv_sec = 0;
	_pmmyitimer.it_interval.tv_usec = 0;
	old_handler = signal(SIGALRM, _pmtracealarm);
	setitimer(ITIMER_REAL, &_pmmyitimer, &_pmolditimer);
#endif

#ifdef PMTRACE_DEBUG
	if (__pmstate & PMTRACE_STATE_COMMS) {
	    char *name = __pmHostEntGetName (servinfo);
	    fprintf(stderr, "_pmtraceconnect: PMDA host=%s port=%d timeout=%d"
		    "secs\n", name == NULL ? "unknown" : name, port, (int)timeout.tv_sec);
	    if (name != NULL)
		free(name);
	}
#endif

	/* Attempt to connect */
	flags = __pmConnectTo(__pmfd, myaddr, port);
	__pmSockAddrFree(myaddr);

	if (flags < 0) {
	    /*
	     * Mark failure in case we fall out the end of the loop
	     * and try next address. __pmfd has been closed in __pmConnectTo().
	     */
	    sts = -flags;
	    __pmfd = -1;
	    continue;
	}

	/* FNDELAY and we're in progress - wait on select */
	stv = canwait;
	pstv = (stv.tv_sec || stv.tv_usec) ? &stv : NULL;
	__pmFD_ZERO(&wfds);
	__pmFD_SET(__pmfd, &wfds);
	if ((rc = __pmSelectWrite(__pmfd+1, &wfds, pstv)) == 1) {
	    sts = __pmConnectCheckError(__pmfd);
	}
	else if (rc == 0) {
	    sts = ETIMEDOUT;
	}
	else {
	    sts = (rc < 0) ? neterror() : EINVAL;
	}
 
#ifndef IS_MINGW
	/* re-arm interval timer */
	setitimer(ITIMER_REAL, &off_itimer, &_pmmyitimer);
	signal(SIGALRM, old_handler);
	if (_pmolditimer.it_value.tv_sec != 0 && _pmolditimer.it_value.tv_usec != 0) {
	    _pmolditimer.it_value.tv_usec -= timeout.tv_usec - _pmmyitimer.it_value.tv_usec;
	    while (_pmolditimer.it_value.tv_usec < 0) {
		_pmolditimer.it_value.tv_usec += 1000000;
		_pmolditimer.it_value.tv_sec--;
	    }
	    while (_pmolditimer.it_value.tv_usec > 1000000) {
		_pmolditimer.it_value.tv_usec -= 1000000;
		_pmolditimer.it_value.tv_sec++;
	    }
	    _pmolditimer.it_value.tv_sec -= timeout.tv_sec - _pmmyitimer.it_value.tv_sec;
	    if (_pmolditimer.it_value.tv_sec < 0) {
		/* missed the user's itimer, pretend there is 1 msec to go! */
		_pmolditimer.it_value.tv_sec = 0;
		_pmolditimer.it_value.tv_usec = 1000;
	    }
	    setitimer(ITIMER_REAL, &_pmolditimer, &_pmmyitimer);
	}
#endif

	/* Was the connection successful? */
	if (sts == 0)
	    break;

	/* Unsuccessful connection. */
	__pmCloseSocket(__pmfd);
	__pmfd = -1;
    } /* loop over addresses */

    __pmHostEntFree(servinfo);

    /* Was the connection successful? */
    if (__pmfd < 0) {
#ifdef PMTRACE_DEBUG
	if (__pmstate & PMTRACE_STATE_COMMS)
	    fprintf(stderr, "_pmtraceconnect(socket failed): %s\n",
		    netstrerror());
#endif
	return -sts;
    }

    _pmtimedout = 0;

    /* Restore the original file status flags. */
    if (__pmSetFileStatusFlags(__pmfd, flags) < 0) {
#ifdef PMTRACE_DEBUG
	if (__pmstate & PMTRACE_STATE_COMMS)
	    fprintf(stderr, ":_pmtraceconnect: cannot restore file status flags\n");
#endif
	return -oserror();
    }

    /* make sure this file descriptor is closed if exec() is called */
    if ((flags = __pmGetFileDescriptorFlags(__pmfd)) != -1)
	sts = __pmSetFileDescriptorFlags(__pmfd, flags | FD_CLOEXEC);
    else
	sts = -1;
    if (sts == -1)
	return -oserror();

    if (__pmtraceprotocol(TRACE_PROTOCOL_QUERY) == TRACE_PROTOCOL_ASYNC) {
	/* in the asynchronoous protocol - ensure no delay after close */
	if ((flags = __pmGetFileStatusFlags(__pmfd)) != -1)
	    sts = __pmSetFileStatusFlags(__pmfd, flags | FNDELAY);
	else
	    sts = -1;
	if (sts == -1)
	    return -oserror();
#ifdef PMTRACE_DEBUG
	if (__pmstate & PMTRACE_STATE_COMMS)
	    fprintf(stderr, "_pmtraceconnect: async protocol setup complete\n");
#endif
    }
    else
#ifdef PMTRACE_DEBUG
	if (__pmstate & PMTRACE_STATE_COMMS)
	    fprintf(stderr, "_pmtraceconnect: sync protocol setup complete\n");
#endif

    /* trace PMDA sends an ACK on successful connect */
    sts = _pmtracegetack(sts, 0);

    return sts;
}


/* this is used internally to maintain connection state */
int
__pmtraceprotocol(int update)
{
    static int	fixedinstone = 0;
    static int	protocol = TRACE_PROTOCOL_SYNC;

    if (update == TRACE_PROTOCOL_QUERY)		/* just a state request */
	return protocol;
    else if (update == TRACE_PROTOCOL_FINAL)	/* no more changes allowed */
	fixedinstone = 1;
    else if (!fixedinstone && update == TRACE_PROTOCOL_ASYNC) {
	__pmstate |= PMTRACE_STATE_ASYNC;
	protocol = update;
    }

    return protocol;
}

int
pmtracestate(int code)
{
    int	old = __pmstate;

    if (code & PMTRACE_STATE_ASYNC) {
	if (__pmtraceprotocol(TRACE_PROTOCOL_ASYNC) != TRACE_PROTOCOL_ASYNC)
	    /* only can do this before connection established */
	    return -EINVAL;
    }

    __pmstate = code;
    return old;
}
