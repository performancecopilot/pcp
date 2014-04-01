/*
 * Copyright (c) 2014. Ken McDonell. All Rights Reserved.
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
 * If the PMDA has not received a PDU from pmcd, but knows it is not
 * ready to handle PDUs from pmcd, then call pmdaControl with mode ==
 * PMDA_CONTROL_BUSY.  Should a PDU be received from pmcd, the PMDA
 * will asynchronusly send the PM_ERR_NOTREADY PDU back to pmcd and
 * place the PMDA in the "NOTREADY" state.
 *
 * If the PMDA has received a PDU from pmcd, and knows it is going to
 * take some time before it could respond, then call pmdaControl with
 * mode == PMDA_CONTROL_NOTREADY to send the PM_ERR_NOTREADY PDU to
 * pmcd and place the PMDA in the "NOTREADY" state.
 *
 * Once the PMDA is free to talk to pmcd, call pmdaControl again with
 * mode == PMDA_CONTROL_READY.
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "libdefs.h"
#if defined(HAVE_PTHREAD_H)
#include <pthread.h>
#else
/*
 * Cannot make this work without threading, and pthread is the only
 * implementatation we have at present.
 */
int
pmdaControl(pmdaInterface *dispatch, int mode)
{
    fprintf(stderr, "Warning: pmdaControl: no pthreads, so not implemented\n");
    return PM_ERR_THREAD;
}
#endif

#define STATE_BUSY		0
#define STATE_NOTREADY		1
#define STATE_READY		2

static struct {
    pthread_mutex_t	lock;
    int			state;
    pthread_t		worker;
} ctl = {
    PTHREAD_MUTEX_INITIALIZER,
    STATE_READY
};

int
pmdaControl(pmdaInterface *dispatch, int mode)
{
    int		psts;
    char	errmsg[PM_MAXERRMSGLEN];
    int		sts;
    pmdaExt	*pmda;

    if (mode != PMDA_CONTROL_READY && mode != PMDA_CONTROL_BUSY && mode != PMDA_CONTROL_NOTREADY) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LIBPMDA) {
	    fprintf(stderr, "Error: pmdaControl: bad mode (%d)\n", mode);
	}
#endif
	return PM_ERR_MODE;
    }

    if (!HAVE_ANY(dispatch->comm.pmda_interface)) {
	__pmNotifyErr(LOG_CRIT, "pmdaConnect: PMDA interface version %d not supported (domain=%d)",
		     dispatch->comm.pmda_interface, dispatch->domain);
	return PM_ERR_IPC;
    }
    pmda = dispatch->version.any.ext;



    if ((psts = pthread_mutex_lock(&ctl.lock)) != 0) {
	pmErrStr_r(-psts, errmsg, sizeof(errmsg));
	__pmNotifyErr(LOG_CRIT, "pmdaControl: pthread_mutex_lock failed: %s", errmsg);
	return PM_ERR_IPC;
    }

    sts = 0;
    switch (ctl.state) {
	case STATE_READY:
	    if (mode == PMDA_CONTROL_READY) {
		/* do nothing */
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    fprintf(stderr, "Warning: pmdaControl: recursive PMDA_CONTROL_READY request\n");
		}
#endif
		goto done;
	    }
	    else if (mode == PMDA_CONTROL_BUSY) {
		/* start pthread to monitor connection to pmcd */
		sts = PM_ERR_NYI;
		ctl.state = STATE_BUSY;
	    }
	    else {
		/* PMDA_CONTROL_NOTREADY */
		sts = PM_ERR_NYI;
		ctl.state = STATE_NOTREADY;
	    }
	    break;

	case STATE_BUSY:
	    if (mode == PMDA_CONTROL_READY) {
		/* terminate the pthread */
		sts = PM_ERR_NYI;
		ctl.state = STATE_READY;
	    }
	    else if (mode == PMDA_CONTROL_BUSY) {
		/* do nothing */
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    fprintf(stderr, "Warning: pmdaControl: recursive PMDA_CONTROL_BUSY request\n");
		}
#endif
	    }
	    else {
		/* PMDA_CONTROL_NOTREADY -- not allowed */
		sts = PM_ERR_MODE;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    fprintf(stderr, "Error: pmdaControl: BUSY -> NOTREADY illegal transition\n");
		}
#endif
	    }
	    break;

	case STATE_NOTREADY:
	    if (mode == PMDA_CONTROL_READY) {
		/* send PM_ERR_READY to pmcd */
		sts = PM_ERR_NYI;
		ctl.state = STATE_READY;
	    }
	    else if (mode == PMDA_CONTROL_BUSY) {
		/* not allowed */
		sts = PM_ERR_MODE;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    fprintf(stderr, "Error: pmdaControl: NOTREADY -> BUSY illegal transition\n");
		}
#endif
	    }
	    else {
		/* PMDA_CONTROL_NOTREADY */
		sts = PM_ERR_MODE;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    fprintf(stderr, "Error: pmdaControl: recursive PMDA_CONTROL_NOTREADY request\n");
		}
#endif
	    }
	    break;

	default:
	    sts = PM_ERR_GENERIC;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
		fprintf(stderr, "Botch: pmdaControl: bad state (%d)\n", ctl.state);
	    }
#endif
	    break;
    }

done:
    if ((psts = pthread_mutex_unlock(&ctl.lock)) != 0) {
	pmErrStr_r(-psts, errmsg, sizeof(errmsg));
	__pmNotifyErr(LOG_CRIT, "pmdaControl: pthread_mutex_unlock failed: %s", errmsg);
    }
    return sts;
}
