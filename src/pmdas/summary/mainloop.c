/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: mainloop.c,v 2.22 2006/05/03 02:16:02 makc Exp $"

#include <unistd.h>
#include <sys/types.h>
#if defined(sgi)
#include <bstring.h>
#endif
#include <sys/time.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include "./summary.h"

static void (*freeResultCallback)(pmResult *) = __pmFreeResultValues;

void
mainLoopFreeResultCallback(void (*callback)(pmResult *res))
{
    freeResultCallback = callback;
}

void
summaryMainLoop(char *pmdaname, int configfd, int clientfd, pmdaInterface *dtp)
{
    __pmPDU		*pb_pmcd;
    __pmPDU		*pb_client;
    __pmPDU		*pb_config;
    int			sts;
    pmID		pmid;
    pmDesc		desc;
    int			npmids;
    pmID		*pmidlist;
    pmResult		*result;
    int			ctxnum;
    __pmTimeval		when;
    int			ident;
    int			type;
    pmInDom		indom;
    int			inst;
    char		*name;
    __pmInResult		*inres;
    char		*buffer;
    __pmProfile		*profile;
    __pmProfile		*saveprofile = NULL;
    static fd_set	readFds;
    int			maxfd;
    int			configReady, clientReady, pmcdReady;
    int infd, outfd;

    if (dtp->comm.pmda_interface != PMDA_INTERFACE_2) {
	__pmNotifyErr(LOG_CRIT, 
		     "summaryMainLoop supports PMDA protocol version 2 only, "
		     "not %d\n", dtp->comm.pmda_interface);
	exit(1);
    } else {
	infd = dtp->version.two.ext->e_infd;
	outfd = dtp->version.two.ext->e_outfd;
    }

    maxfd = infd+1;
    if (clientfd >= maxfd)
	maxfd = clientfd+1;
    if (configfd >= maxfd)
	maxfd = configfd+1;


    for ( ;; ) {
	FD_ZERO(&readFds);
	FD_SET(infd, &readFds);
	FD_SET(clientfd, &readFds);
	if (configfd >= 0)
	    FD_SET(configfd, &readFds);

	/* select here : block if nothing to do */
	sts = select(maxfd, &readFds, NULL, NULL, NULL);

	clientReady = FD_ISSET(clientfd, &readFds);
	pmcdReady = FD_ISSET(infd, &readFds);
	if (configfd >= 0)
	    configReady = FD_ISSET(configfd, &readFds);
	else
	    configReady = 0;

	if (sts < 0)
	    break;
	if (sts == 0)
	    continue;

	if (configReady) {
	    /*
	     * Service the config
	     */
	    sts = __pmGetPDU(configfd, PDU_BINARY, TIMEOUT_NEVER, &pb_config);
	    if (sts < 0) {
		__pmNotifyErr(LOG_ERR, "config __pmGetPDU: %s\n", pmErrStr(sts));
	    }
	    if (sts <= 0) {
		/* End of File or error. Just close the file or pipe */
		close(configfd);
		configfd = -1;
	    }
	    else
		service_config(pb_config);
	} 

	if (clientReady) {
	    /*
	     * Service the command/client
	     */
	    sts = __pmGetPDU(clientfd, PDU_BINARY, TIMEOUT_NEVER, &pb_client);
	    if (sts < 0)
		__pmNotifyErr(LOG_ERR, "client __pmGetPDU: %s\n", pmErrStr(sts));
	    if (sts <= 0) {
		/* End of File or error */
		goto done;
	    }
	    service_client(pb_client);
	}

	if (pmcdReady) {
	    /* service pmcd */
	    sts = __pmGetPDU(infd, PDU_BINARY, TIMEOUT_NEVER, &pb_pmcd);

	    if (sts < 0)
		__pmNotifyErr(LOG_ERR, "__pmGetPDU: %s\n", pmErrStr(sts));
	    if (sts <= 0) {
		/* End of File or error */
		goto done;
	    }

	    switch (sts) {

		case PDU_PROFILE:
		    /*
		     * can ignore ctxnum, since pmcd has already used this to send
		     * the correct profile, if required
		     */
		    if ((sts = __pmDecodeProfile(pb_pmcd, PDU_BINARY, &ctxnum, &profile)) >= 0)
			sts = dtp->version.two.profile(profile,
                                                       dtp->version.two.ext);
		    if (sts < 0)
			__pmSendError(outfd, PDU_BINARY, sts);
		    else {
			if (saveprofile != NULL)
			    free(saveprofile);
			/*
			 * need to keep the last valid one around, as the DSO
			 * routine just remembers the address
			 */
			saveprofile = profile;
		    }
		    break;

		case PDU_FETCH:
		    /*
		     * can ignore ctxnum, since pmcd has already used this to send
		     * the correct profile, if required
		     */
		    sts = __pmDecodeFetch(pb_pmcd, PDU_BINARY, &ctxnum, &when, &npmids, &pmidlist);

		    /* Ignore "when"; pmcd should intercept archive log requests */
		    if (sts >= 0) {
			sts = dtp->version.two.fetch(npmids, pmidlist, &result,
						     dtp->version.two.ext);
			__pmUnpinPDUBuf(pmidlist);
		    }
		    if (sts < 0)
			__pmSendError(outfd, PDU_BINARY, sts);
		    else {
			int st;
			st =__pmSendResult(outfd, PDU_BINARY, result);
			if (st < 0) {
			    __pmNotifyErr(LOG_ERR, 
					  "Cannot send fetch result: %s\n",
					  pmErrStr(st));
			}
			(*freeResultCallback)(result);
		    }
		    break;

		case PDU_DESC_REQ:
		    if ((sts = __pmDecodeDescReq(pb_pmcd, PDU_BINARY, &pmid)) >= 0) {
			sts = dtp->version.two.desc(pmid, &desc,
						    dtp->version.two.ext);
		    }
		    if (sts < 0)
			__pmSendError(outfd, PDU_BINARY, sts);
		    else
			__pmSendDesc(outfd, PDU_BINARY, &desc);
		    break;

		case PDU_INSTANCE_REQ:
		    if ((sts = __pmDecodeInstanceReq(pb_pmcd, PDU_BINARY, &when, &indom, &inst, &name)) >= 0) {
			/*
			 * Note: when is ignored.
			 *		If we get this far, we are _only_ dealing
			 *		with current data (pmcd handles the other
			 *		cases).
			 */
			sts = dtp->version.two.instance(indom, inst, name,
                                                        &inres,
						        dtp->version.two.ext);
		    }
		    if (sts < 0)
			__pmSendError(outfd, PDU_BINARY, sts);
		    else {
			__pmSendInstance(outfd, PDU_BINARY, inres);
			__pmFreeInResult(inres);
		    }
		    break;

		case PDU_TEXT_REQ:
		    if ((sts = __pmDecodeTextReq(pb_pmcd, PDU_BINARY, &ident, &type)) >= 0) {
			sts = dtp->version.two.text(ident, type, &buffer,
						    dtp->version.two.ext);
		    }
		    if (sts < 0)
			__pmSendError(outfd, PDU_BINARY, sts);
		    else
			__pmSendText(outfd, PDU_BINARY, ident, buffer);
		    break;

		case PDU_RESULT:
		    if ((sts = __pmDecodeResult(pb_pmcd, PDU_BINARY, &result)) >= 0)
			sts = dtp->version.two.store(result,
						     dtp->version.two.ext);
		    __pmSendError(outfd, PDU_BINARY, sts);
		    pmFreeResult(result);
		    break;

		default:
		    fprintf(stderr, "%s: bogus pdu type: 0x%0x?\n", pmdaname, sts);
		    __pmSendError(outfd, PDU_BINARY, PM_ERR_NYI);
		    break;
	    }
	}
    }

done:
    return;
}
