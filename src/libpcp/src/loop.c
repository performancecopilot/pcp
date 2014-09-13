/*
 * Copyright (c) 2004-2006 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifdef ASYNC_API
#include "pmapi.h"
#include "impl.h"
#include <assert.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifndef SIGMAX
/* I would use SIGRTMAX...except it's not constant on Linux */
#define SIGMAX	64
#endif

typedef struct loop_input_s loop_input_t;
typedef struct loop_signal_s loop_signal_t;
typedef struct loop_timeout_s loop_timeout_t;
typedef struct loop_child_s loop_child_t;
typedef struct loop_idle_s loop_idle_t;
typedef struct loop_main_s loop_main_t;

struct loop_input_s
{
    loop_input_t *next;
    int tag;
    int fd;
    int flags;
    int (*callback)(int fd, int flags, void *closure);
    void *closure;
    int priority;
};

struct loop_signal_s
{
    loop_signal_t *next;
    int tag;
    int (*callback)(int sig, void *closure);
    void *closure;
};

struct loop_timeout_s
{
    loop_timeout_t *next;
    int tag;
    int delay;
    int tout_msec;
    int (*callback)(void *closure);
    void *closure;
};

struct loop_child_s
{
    loop_child_t *next;
    int tag;
    pid_t pid;
    int (*callback)(pid_t pid, int status, const struct rusage *, void *closure);
    void *closure;
};

struct loop_idle_s
{
    loop_idle_t *next;
    int tag;
    int (*callback)(void *closure);
    void *closure;
};

/*
* per-loop state, kept in an implicit stack
* by the main loop and subsidiary loops.
*/
struct loop_main_s
{
    loop_main_t *next;
    int running;
    loop_timeout_t *current_timeout;
    loop_child_t *current_child;
};

#define pmLoopDebug ((pmDebug & DBG_TRACE_LOOP) != 0)

static int num_inputs;
static loop_input_t *input_list;
static struct pollfd *pfd;
static loop_input_t **inputs;
static int next_tag = 1;
static int inputs_dirty = 1;
static loop_signal_t *signals[SIGMAX];
static volatile int signals_pending[SIGMAX];
static struct timeval poll_start;
static loop_timeout_t *timeout_list;
static loop_main_t *main_stack;
static loop_child_t *child_list;
static int child_pending;
static int sigchld_tag;
static loop_idle_t *idle_list;

static int
tv_sub(const struct timeval *a, const struct timeval *b)
{
    struct timeval t;

    t = a;
    __pmtimevalDec(&t, &b);
    return (t.tv_sec * 1000 + t.tv_usec / 1000);
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

int
pmLoopRegisterInput(
    int fd,
    int flags,
    int (*callback)(int fd, int flags, void *closure),
    void *closure,
    int priority)
{
    loop_input_t *ii;

    if (pmLoopDebug)
	__pmNotifyErr(LOG_DEBUG,
		      "loop_register_input: fd=%d flags=0x%x "
		      "callback=%p closure=%p priority=%d",
		      fd, flags, callback, closure, priority);

    if ((ii = (loop_input_t *)malloc(sizeof(loop_input_t))) == NULL) {
	return (-ENOMEM);
    }

    ii->tag = next_tag++;
    ii->fd = fd;
    ii->flags = flags;
    ii->callback = callback;
    ii->closure = closure;

    ii->next = input_list;
    input_list = ii;
    num_inputs++;

    inputs_dirty = 1;

    return ii->tag;
}

void
pmLoopUnregisterInput(int tag)
{
    loop_input_t *ii, *previi;

    if (pmLoopDebug)
	__pmNotifyErr(LOG_DEBUG, "loop_unregister_input: tag=%d", tag);

    for (ii = input_list, previi = NULL ;
	 ii != NULL && ii->tag != tag ;
	 previi = ii, ii = ii->next)
	;
    if (ii == NULL)
	return;

    if (previi == NULL)
	input_list = ii->next;
    else
	previi->next = ii->next;
    num_inputs--;
    free(ii);

    inputs_dirty = 1;
}

static int
loop_compare_by_priority(const void *av, const void *bv)
{
    const loop_input_t *a = *(const loop_input_t **)av;
    const loop_input_t *b = *(const loop_input_t **)bv;

    return a->priority - b->priority;
}

static int
loop_setup_inputs(void)
{
    loop_input_t *ii;
    int i;

    if (num_inputs <= 0) {
	return (0);
    }

    if (inputs_dirty) {
	pfd = (struct pollfd *)realloc(pfd,
				       num_inputs * sizeof(struct pollfd));
	inputs = (loop_input_t **)realloc(inputs,
					  num_inputs * sizeof(loop_input_t *));
	if ((pfd == NULL) || (inputs == NULL)) {
		return (-ENOMEM);
	}
	inputs_dirty = 0;
    }

    for (ii = input_list, i = 0; ii != NULL ; ii = ii->next, i++)
	inputs[i] = ii;
    qsort(inputs, num_inputs, sizeof(loop_input_t *), loop_compare_by_priority);

    for (i = 0 ; i < num_inputs ; i++)
    {
	ii = inputs[i];
	if (pmLoopDebug)
	    __pmNotifyErr(LOG_DEBUG, 
			  "loop_setup_inputs: inputs[%d] = (fd=%d "
			  "callback=%p closure=%p)",
			  i, ii->fd, ii->callback, ii->closure);

	pfd[i].fd = ii->fd;
	pfd[i].events = ii->flags;
	pfd[i].revents = 0;
    }
    return (num_inputs);
}

static void
loop_dispatch_inputs(void)
{
    int i;
    loop_input_t *ii;
    int n = num_inputs; /* because num_inputs can change inside the loop */

    for (i = 0 ; i < n; i++) {
	ii = inputs[i];

	if ((pfd[i].revents & POLLNVAL)) {
	    /* invalid fd... */
	    pmLoopUnregisterInput(ii->tag);
	    continue;
	}

	if (pmLoopDebug)
	    __pmNotifyErr(LOG_DEBUG,
			  "loop_dispatch_inputs: pfd[%i]=(fd=%d "
			  "events=0x%x revents=0x%x)",
			  i, pfd[i].fd, pfd[i].events, pfd[i].revents);

	if ((pfd[i].revents & (ii->flags | POLLHUP | POLLERR)))	{
	    if ((*ii->callback)(ii->fd, pfd[i].revents, ii->closure)) {
		if (pmLoopDebug)
		    __pmNotifyErr(LOG_DEBUG,
				  "loop_dispatch_inputs: deregistering "
				  "input with tag %d\n",
				  ii->tag);
		
		pmLoopUnregisterInput(ii->tag);
	    }
	}
    }
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

static void
loop_sig_handler(int sig)
{
    signals_pending[sig] = 1;
}

int
pmLoopRegisterSignal(
    int sig,
    int (*callback)(int sig, void *closure),
    void *closure)
{
    loop_signal_t *ss;
    int doinstall;

    if (sig < 0 || sig >= SIGMAX)
	return -EINVAL;

    if ((ss = (loop_signal_t *)malloc(sizeof(loop_signal_t))) == NULL)
	return -ENOMEM;

    ss->tag = next_tag++;
    ss->callback = callback;
    ss->closure = closure;

    doinstall = (signals[sig] == NULL);
    ss->next = signals[sig];
    signals[sig] = ss;

    if (doinstall) {
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = 0;
	sa.sa_handler = loop_sig_handler;
	if (sigaction(sig, &sa, NULL) < 0) {
	    int ee = oserror();

	    __pmNotifyErr(LOG_WARNING, 
			  "sigaction failed - %s", osstrerror());
	    return -ee;
	}
    }

    return ss->tag;
}

void
pmLoopUnregisterSignal(int tag)
{
    int sig;
    loop_signal_t *ss, *prevss;

    if (pmLoopDebug)
	__pmNotifyErr(LOG_DEBUG,"loop_unregister_signal: tag=%d", tag);

    for (sig = 0 ; sig < SIGMAX ; sig++)
    {
	for (ss = signals[sig], prevss = NULL ;
	     ss != NULL && ss->tag != tag ;
	     prevss = ss, ss = ss->next)
	    ;
	if (ss == NULL)
	    continue;

	if (prevss == NULL)
	    signals[sig] = ss->next;
	else
	    prevss->next = ss->next;
	free(ss);

	if (signals[sig] == NULL)
	{
	    struct sigaction sa;

	    memset(&sa, 0, sizeof(sa));
	    sa.sa_flags = 0;
	    sa.sa_handler = SIG_DFL;
	    if (sigaction(sig, &sa, NULL) < 0) {
		__pmNotifyErr(LOG_WARNING, 
			      "sigaction failed - %s", osstrerror());
		return;
	    }
	}
	break;
    }
}

static void
loop_dispatch_signals(void)
{
    int sig;
    loop_signal_t *ss, *nextss;

    for (sig = 0 ; sig < SIGMAX ; sig++) {
	if (signals_pending[sig]) {
	    signals_pending[sig] = 0;

	    for (ss = signals[sig]; ss != NULL;  ss = nextss) {
		nextss = ss->next;
		if ((*ss->callback)(sig, ss->closure)) {
		    pmLoopUnregisterSignal(ss->tag);
		}
	    }
	}
    }
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

/*
 * A few words about the timeout data structure.  This is the classic
 * (i.e. simple and not scalable) data structure for multiplexing
 * multiple virtual timeouts onto one real one, as described in any
 * OS internals book.
 *
 * A singly-linked list of loop_timeout_t structures is kept, the head
 * is pointed to by timeout_list and threaded by tt->next.  Each entry
 * stores in tt->delay a timeout in millisecons which expresses when
 * the entry is scheduled to fire as the elapsed time after the
 * previous entry is scheduled to fire.  Thus the delay field in the
 * head of the list is logically the amount of time from now until the
 * first timeout is scheduled, and so is used directly as the timeout
 * for poll().
 *
 * From this data structure we can derive the insert algorithm.
 * The algorithm walks down the list from the head, keeping a running
 * relative delay from the last entry iterated over by subtracting
 * from it the tt->delay of each entry skipped.  When iterating to the
 * next entry would cause this running relative delay to go negative,
 * we know we've arrived at the right place to insert the new entry.
 * Note that the check is for negative, not negative or zero: this
 * ensures that multiple entries for the same scheduled time are
 * stored in the same order that they were inserted, which is the
 * most intuitive behaviour for the application programmer.
 *
 * The remove algorithm is simpler, it just scans the list trying
 * to match the unique tag.
 *
 * There are some more hairy parts as well.  It is possible for poll()
 * to return before the timeout expires, for example if input becomes
 * available on a file descriptor.  The poll() call does not give any
 * indication of how much time remained until the timeout would have
 * fired (on some operating systems, the select(2) does this).  So a
 * sample of the system time is taken before and after every call
 * to poll(), the elapsed time in the poll() is calculated, and the
 * tt->delay in the head of the timeout_list is adjusted to account
 * for the elapsed time.  This is necessary to avoid restarting the
 * poll() with too long a timeout.  An example of the resulting bug
 * would be a timeout registered for 10 seconds from now, but every
 * 1 second input becomes available on some file descriptor; if the
 * poll() timeout were not adjusted the timeout callback would never
 * be called and would always be 10 seconds in the future.
 */

static void
loop_dump_timeouts(void)
{
    loop_timeout_t *tt;

    __pmNotifyErr(LOG_DEBUG,"timeout_list {");
    for (tt = timeout_list ; tt != NULL ; tt = tt->next) {
	__pmNotifyErr(LOG_DEBUG,"    %dms %p %p",
		      tt->delay, tt->callback, tt->closure);
    }
    __pmNotifyErr(LOG_DEBUG,"}");
}

static void
loop_insert_timeout(loop_timeout_t *tt)
{
    loop_timeout_t *next, *prev;
    int delay = tt->delay;

    if (pmLoopDebug)
	__pmNotifyErr(LOG_DEBUG, "loop_insert_timeout: %d %p %p",
		      tt->delay, tt->callback, tt->closure);

    for (next = timeout_list, prev = NULL ;
	 (next != NULL) && ((delay - next->delay) >= 0);
	 prev = next, next = next->next)
	delay -= next->delay;

    if (prev == NULL)
	timeout_list = tt;
    else
	prev->next = tt;
    tt->next = next;

    if (next != NULL)
	next->delay -= delay;

    tt->delay = delay;

    if (pmLoopDebug)
	loop_dump_timeouts();
}

int
pmLoopRegisterTimeout(
    int tout_msec,
    int (*callback)(void *closure),
    void *closure)
{
    loop_timeout_t *tt;

    if (tout_msec < 0) {
	return (-EINVAL);
    }

    if ((tt = (loop_timeout_t *)malloc(sizeof(loop_timeout_t))) == NULL) {
	return (-ENOMEM);
    }

    tt->tag = next_tag++;
    tt->delay = tt->tout_msec = tout_msec;
    tt->callback = callback;
    tt->closure = closure;

    loop_insert_timeout(tt);

    return tt->tag;
}

void
pmLoopUnregisterTimeout(int tag)
{
    loop_main_t *lm;
    loop_timeout_t *tt, *prevtt;

    if (pmLoopDebug)
	__pmNotifyErr(LOG_DEBUG,"loop_unregister_timeout: tag=%d", tag);

    /*
     * Because the timeout object is detached from the
     * global timeout list while its being dispatched
     * (and yes there are good reasons for this), we
     * have to search for the timeout tag in all the
     * currently stacked loops.
     */
    for (lm = main_stack ; lm != NULL ; lm = lm->next)
    {
	if (lm->current_timeout != NULL &&
	    lm->current_timeout->tag == tag)
	{
	    free(lm->current_timeout);
	    lm->current_timeout = NULL;
	    return;
	}
    }

    for (tt = timeout_list, prevtt = NULL;
	 tt != NULL && tt->tag != tag ;
	 prevtt = tt, tt = tt->next)
	;

    if (tt == NULL)
	return;

    if (prevtt == NULL)
	timeout_list = tt->next;
    else
	prevtt->next = tt->next;

    if (tt->next != NULL)
	tt->next->delay += tt->delay;

    free(tt);
}

/* returns milliseconds */
static int
loop_setup_timeouts(void)
{
    __pmtimevalNow(&poll_start);

    if (idle_list != NULL)
	return 0;   /* poll() returns immediately */
    if (timeout_list == NULL)
	return -1;  /* poll() waits forever */
    return (timeout_list->delay);
}

static void
loop_dispatch_timeouts(void)
{
    if (timeout_list == NULL)
	return;

    timeout_list->delay = 0;
    while (timeout_list != NULL && (timeout_list->delay == 0)) {
	loop_main_t *lm = main_stack;
	int isdone;
	assert(lm != NULL);
	assert(lm->current_timeout == NULL);
	lm->current_timeout = timeout_list;
	timeout_list = timeout_list->next;

	isdone = (*lm->current_timeout->callback)(lm->current_timeout->closure);

	assert(lm == main_stack);

	if (!isdone && (lm->current_timeout != NULL)) {
	    lm->current_timeout->delay = lm->current_timeout->tout_msec;
	    loop_insert_timeout(lm->current_timeout);
	} else {
	    pmLoopUnregisterTimeout(lm->current_timeout->tag);
	}
	lm->current_timeout = NULL;
    }
}

static void
loop_adjust_timeout(void)
{
    struct timeval now;
    int spent;

    if (timeout_list == NULL)
	return;

    __pmtimevalNow(&now);
    spent = tv_sub(&now, &poll_start);
    if (spent >= timeout_list->delay) {
	timeout_list->delay = 0;
	loop_dispatch_timeouts();
    } else {
	timeout_list->delay -= spent;
    }
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

static int
loop_sigchld_handler(int sig, void *closure)
{
    if (pmLoopDebug)
	__pmNotifyErr(LOG_DEBUG,"loop_sigchld_handler");
    child_pending = 1;
    return (0);
}


int
pmLoopRegisterChild(
    pid_t pid,
    int (*callback)(pid_t pid, int status, const struct rusage *, void *closure),
    void *closure)
{
    loop_child_t *cc;
    int doinstall;

    if (pmLoopDebug)
	__pmNotifyErr(LOG_DEBUG,"loop_register_child: pid=%" FMT_PID " callback=%p closure=%p",
		pid, callback, closure);

    if (pid <= (pid_t)0)
	return -1;
    if ((cc = (loop_child_t *)malloc(sizeof(loop_child_t))) == NULL) {
	return (-ENOMEM);
    }

    cc->tag = next_tag++;
    cc->pid = pid;
    cc->callback = callback;
    cc->closure = closure;

    doinstall = (child_list == NULL);
    cc->next = child_list;
    child_list = cc;

    if (doinstall)
	sigchld_tag = pmLoopRegisterSignal(SIGCHLD,
					   loop_sigchld_handler,
					   NULL);

    return cc->tag;
}

void
pmLoopUnregisterChild(int tag)
{
    loop_main_t *lm;
    loop_child_t *cc, *prevcc;

    if (pmLoopDebug)
	__pmNotifyErr(LOG_DEBUG,"loop_unregister_child: tag=%d", tag);

    for (cc = child_list, prevcc = NULL ;
	 cc != NULL && cc->tag != tag ;
	 prevcc = cc, cc = cc->next)
	;
    if (cc == NULL)
	return;

    if (prevcc == NULL)
	child_list = cc->next;
    else
	prevcc->next = cc->next;

    for (lm = main_stack ; lm != NULL ; lm = lm->next)
    {
	if (cc == lm->current_child)
	    lm->current_child = NULL;
    }
    free(cc);

    if (child_list == NULL)
    {
	pmLoopUnregisterSignal(sigchld_tag);
	sigchld_tag = -1;
    }
}

static void
loop_dispatch_children(void)
{
    loop_child_t *cc, *nextcc;
    int status;
    int r;
    struct rusage rusage;

    memset (&rusage, 0, sizeof(rusage));

    child_pending = 0;

    if (pmLoopDebug)
	__pmNotifyErr(LOG_DEBUG,"loop_dispatch_children");

    /* We don't support callback on process groups.  Sorry */
    while ((r = wait3(&status, WNOHANG, &rusage)) > 0)
    {
	if (pmLoopDebug)
	    __pmNotifyErr(LOG_DEBUG,"loop_dispatch_children: r=%d", r);

	for (cc = child_list ; cc != NULL ; cc = nextcc)
	{
	    nextcc = cc->next;

	    if (r == (int)cc->pid) {
		loop_main_t *lm = main_stack;
		int isdone;

		assert(lm != NULL);
		lm->current_child = cc;
		isdone = (*cc->callback)((pid_t)r, status, &rusage,
					 cc->closure);

		if (isdone ||
		    (lm->current_child != NULL &&
		     (WIFEXITED(status) || WIFSIGNALED(status)))) {
		    /*
		     * This pid won't be coming back or we were told
		     * that callback has fulfilled its purpose, so
		     * unregister.
		     */
		    pmLoopUnregisterChild(cc->tag);
		}
		assert(lm == main_stack);
		assert(lm->current_child == NULL || lm->current_child == cc);
		lm->current_child = NULL;
		break;
	    }
	}
    }
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

int
pmLoopRegisterIdle(
    int (*callback)(void *closure),
    void *closure)
{
    loop_idle_t *ii;

    if (pmLoopDebug)
	__pmNotifyErr(LOG_DEBUG,"loop_register_idle: callback=%p closure=%p",
		callback, closure);

    if ((ii = (loop_idle_t *)malloc(sizeof(loop_idle_t))) == NULL) {
	return (-ENOMEM);
    }

    ii->tag = next_tag++;
    ii->callback = callback;
    ii->closure = closure;

    ii->next = idle_list;
    idle_list = ii;

    return ii->tag;
}

void
pmLoopUnregisterIdle(int tag)
{
    loop_idle_t *ii, *previi;

    if (pmLoopDebug)
	__pmNotifyErr(LOG_DEBUG, "loop_unregister_idle: tag=%d", tag);

    for (ii = idle_list, previi = NULL ;
	 ii != NULL && ii->tag != tag ;
	 previi = ii, ii = ii->next)
	;
    if (ii == NULL)
	return;

    if (previi == NULL)
	idle_list = ii->next;
    else
	previi->next = ii->next;

    free(ii);
}

static void
loop_dispatch_idle(void)
{
    loop_idle_t *ii, *nextii;

    for (ii = idle_list ; ii != NULL ; ii = nextii) {
	nextii = ii->next;

	if ((*ii->callback)(ii->closure)) {
	    pmLoopUnregisterIdle(ii->tag);
	}
    }
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

void
pmLoopStop(void)
{
    if (main_stack != NULL)
	main_stack->running = 0;
}

int
pmLoopMain(void)
{
    int r;
    int timeout;
    loop_main_t lmain;

    memset(&lmain, 0, sizeof(lmain));
    lmain.next = main_stack;
    main_stack = &lmain;

    lmain.running = 1;
    while (lmain.running) {
	int ee;

	if ((ee = loop_setup_inputs()) < 0)
	    return ee;
	timeout = loop_setup_timeouts();
	loop_dispatch_idle();

	if ((ee == 0) && (timeout == -1) && (idle_list == NULL))
	    return 0;

	r = poll(pfd, num_inputs, timeout);
	if (r < 0) {
	    if (oserror() == EINTR) {
		loop_dispatch_signals();
		if (child_pending)
		    loop_dispatch_children();
	    	continue;
	    }
	    __pmNotifyErr(LOG_ERR, "pmLoopMain: poll failed - %s",
			  osstrerror());
	    break;
	} else if (r == 0) {
	    if (timeout > 0)
		loop_dispatch_timeouts();
	    else
		loop_adjust_timeout();
	} else {
	    loop_dispatch_inputs();
	    loop_adjust_timeout();
	}
    }

    assert(main_stack == &lmain);
    assert(lmain.current_child == NULL);
    assert(lmain.current_timeout == NULL);
    main_stack = lmain.next;
    return 0;
}

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/
#endif /*ASYNC_API*/
