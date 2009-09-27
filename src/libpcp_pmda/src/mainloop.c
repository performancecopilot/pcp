/*
 * Copyright (c) 1995-2000 Silicon Graphics, Inc.  All Rights Reserved.
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
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "libdefs.h"

extern int __pmdaSetupPDU(int, int, char *);


int
__pmdaInFd(pmdaInterface *dispatch)
{
    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
	return dispatch->version.four.ext->e_infd;
    else if (HAVE_V_TWO(dispatch->comm.pmda_interface))
	return dispatch->version.two.ext->e_infd;
    else {
	__pmNotifyErr(LOG_CRIT, "PMDA interface version %d not supported",
		     dispatch->comm.pmda_interface);
        return -1;
    }
}

int
__pmdaMainPDU(pmdaInterface *dispatch)
{
    __pmPDU		*pb;
    int			sts;
    int			op_sts;
    pmID		pmid;
    pmDesc		desc;
    int			npmids;
    pmID		*pmidlist;
    char		**namelist = NULL;
    char		*name;
    char		**offspring = NULL;
    int			*statuslist = NULL;
    int			subtype;
    pmResult		*result;
    int			ctxnum;
    __pmTimeval		when;
    int			ident;
    int			type;
    pmInDom		indom;
    int			inst;
    char		*iname;
    __pmInResult	*inres;
    char		*buffer;
    __pmProfile  	*new_profile;
    static __pmProfile	*profile = NULL;
    static int		first_time = 1;
    static pmdaExt	*pmda = NULL;

    /* Initial version checks */
    if (first_time) {
	if (dispatch->status != 0) {
	    __pmNotifyErr(LOG_ERR, "PMDA Initialisation Failed");
	    return -1;
	}
	if (HAVE_V_FOUR(dispatch->comm.pmda_interface)) {
	    pmda = dispatch->version.four.ext; /* PMDA Interface V.4 dynamic PMNS */
#if 0
/* TODO ... not needed now? */
	    /*
	     * need PMNS loaded ... if this fails, don't worry as the
	     * errors will come later and be reported in a PDU interchange
	     * context when the PMNS is needed, but not loaded
	     */
	    pmLoadNameSpace(PM_NS_DEFAULT);
#endif
	}
	else if (HAVE_V_TWO(dispatch->comm.pmda_interface))
	    pmda = dispatch->version.two.ext; /* PMDA Interface V.2 or V.3 Extensions */
	else {
	    __pmNotifyErr(LOG_CRIT, "PMDA interface version %d not supported",
			 dispatch->comm.pmda_interface);
	    return -1;
	}
	dispatch->comm.pmapi_version = PMAPI_VERSION;
	first_time = 0;
    }

    sts = __pmGetPDU(pmda->e_infd, PDU_BINARY, TIMEOUT_NEVER, &pb);
    if (sts == 0)
	/* End of File */
	return PM_ERR_EOF;
    if (sts < 0) {
	__pmNotifyErr(LOG_ERR, "IPC Error: %s\n", pmErrStr(sts));
	return sts;
    }

    /*
     * if defined, callback once per PDU to check availability, etc.
     */
    if (pmda->e_checkCallBack) {
	int		i;
	i = (*(pmda->e_checkCallBack))();
	if (i < 0) {
	    if (sts != PDU_PROFILE)
		/* all other PDUs expect an ACK */
		__pmSendError(pmda->e_outfd, PDU_BINARY, i);
	    return 0;
	}
    }

    switch (sts) {

    case PDU_PROFILE:
	/*
	 * can ignore ctxnum, since pmcd has already used this to send
	 * the correct profile, if required
	 */

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LIBPMDA) {
	    __pmNotifyErr(LOG_DEBUG, "Received PDU_PROFILE\n");
	}
#endif

	/*
	 * free last profile received (if any)
	 * Note error responses are not sent for PDU_PROFILE
	 */
	if (__pmDecodeProfile(pb, PDU_BINARY, &ctxnum, &new_profile) < 0) 
	   break;

	if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
	    sts = dispatch->version.four.profile(new_profile, pmda);
	else
	    sts = dispatch->version.two.profile(new_profile, pmda);
	if (sts < 0) {
	    __pmFreeProfile(new_profile);
	} else {
	    __pmFreeProfile(profile);
	    profile = new_profile;
	}
	break;

    case PDU_FETCH:
	/*
	 * can ignore ctxnum, since pmcd has already used this to send
	 * the correct profile, if required
	 */

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LIBPMDA) {
	    __pmNotifyErr(LOG_DEBUG, "Received PDU_FETCH\n");
	}
#endif

	sts = __pmDecodeFetch(pb, PDU_BINARY, &ctxnum, &when, &npmids, &pmidlist);

	/* Ignore "when"; pmcd should intercept archive log requests */
	if (sts >= 0) {
	    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
		sts = dispatch->version.four.fetch(npmids, pmidlist, &result, pmda);
	    else
		sts = dispatch->version.two.fetch(npmids, pmidlist, &result, pmda);

	    __pmUnpinPDUBuf(pmidlist);
	}
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, PDU_BINARY, sts);
	else {
	    /* this is for PURIFY to prevent a UMR in __pmXmitPDU */
	    result->timestamp.tv_sec = 0;
	    result->timestamp.tv_usec = 0;
	    __pmSendResult(pmda->e_outfd, PDU_BINARY, result);
	    (pmda->e_resultCallBack)(result);
	}
	break;

    case PDU_PMNS_NAMES:

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LIBPMDA) {
	    __pmNotifyErr(LOG_DEBUG, "Received PDU_PMNS_NAMES\n");
	}
#endif

	if ((sts = __pmDecodeNameList(pb, PDU_BINARY, &npmids, &namelist, NULL)) >= 0) {
	    if (HAVE_V_FOUR(dispatch->comm.pmda_interface)) {
		if (npmids != 1)
		    /*
		     * expect only one name at a time to be sent to the
		     * pmda from pmcd
		     */
		    sts = PM_ERR_IPC;
		else
		    sts = dispatch->version.four.pmid(namelist[0], &pmid, pmda);
	    }
	    else {
		/* Not INTERFACE_4 */
		sts = PM_ERR_PMID;
	    }
	    __pmUnpinPDUBuf(namelist);
	}
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, PDU_BINARY, sts);
	else
	    __pmSendIDList(pmda->e_outfd, PDU_BINARY, 1, &pmid, sts);
	break;

    case PDU_PMNS_CHILD:

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LIBPMDA) {
	    __pmNotifyErr(LOG_DEBUG, "Received PDU_PMNS_CHILD\n");
	}
#endif

	if ((sts = __pmDecodeChildReq(pb, PDU_BINARY, &name, &subtype)) >= 0) {
	    if (HAVE_V_FOUR(dispatch->comm.pmda_interface)) {
		sts = dispatch->version.four.children(name, 0, &offspring, &statuslist, pmda);
		if (sts >= 0) {
		    if (subtype == 0) {
			if (statuslist) free(statuslist);
			statuslist = NULL;
		    }
		}
	    }
	    else {
		/* Not INTERFACE_4 */
		sts = PM_ERR_NAME;
	    }
	}
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, PDU_BINARY, sts);
	else
	    __pmSendNameList(pmda->e_outfd, PDU_BINARY, sts, offspring, statuslist);
	if (offspring) free(offspring);
	if (statuslist) free(statuslist);
	break;

    case PDU_PMNS_TRAVERSE:

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LIBPMDA) {
	    __pmNotifyErr(LOG_DEBUG, "Received PDU_PMNS_TRAVERSE\n");
	}
#endif

	if ((sts = __pmDecodeTraversePMNSReq(pb, PDU_BINARY, &name)) >= 0) {
	    if (HAVE_V_FOUR(dispatch->comm.pmda_interface)) {
		sts = dispatch->version.four.children(name, 1, &offspring, &statuslist, pmda);
		if (sts >= 0) {
		    if (statuslist) free(statuslist);
		    statuslist = NULL;
		}
	    }
	    else {
		/* Not INTERFACE_4 */
		sts = PM_ERR_NAME;
	    }
	}
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, PDU_BINARY, sts);
	else
	    __pmSendNameList(pmda->e_outfd, PDU_BINARY, sts, offspring, NULL);
	if (offspring) free(offspring);
	break;

    case PDU_PMNS_IDS:

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LIBPMDA) {
	    __pmNotifyErr(LOG_DEBUG, "Received PDU_PMNS_IDS\n");
	}
#endif

	sts = __pmDecodeIDList(pb, PDU_BINARY, 1, &pmid, &op_sts);
	if (sts >= 0)
	    sts = op_sts;
	if (sts >= 0) {
	    if (HAVE_V_FOUR(dispatch->comm.pmda_interface)) {
		sts = dispatch->version.four.name(pmid, &namelist, pmda);
	    }
	    else {
		/* Not INTERFACE_4 */
		sts = PM_ERR_PMID;
	    }
	}
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, PDU_BINARY, sts);
	else
	    __pmSendNameList(pmda->e_outfd, PDU_BINARY, sts, namelist, NULL);
	if (namelist) free(namelist);
	break;

    case PDU_DESC_REQ:

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LIBPMDA) {
	    __pmNotifyErr(LOG_DEBUG, "Received PDU_DESC_REQ\n");
	}
#endif

	if ((sts = __pmDecodeDescReq(pb, PDU_BINARY, &pmid)) >= 0) {
	    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
		sts = dispatch->version.four.desc(pmid, &desc, pmda);
	    else
		sts = dispatch->version.two.desc(pmid, &desc, pmda);
	}
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, PDU_BINARY, sts);
	else
	    __pmSendDesc(pmda->e_outfd, PDU_BINARY, &desc);
	break;

    case PDU_INSTANCE_REQ:

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LIBPMDA) {
	    __pmNotifyErr(LOG_DEBUG, "Received PDU_INSTANCE_REQ\n");
	}
#endif

	if ((sts = __pmDecodeInstanceReq(pb, PDU_BINARY, &when, &indom, &inst, 
					 &iname)) >= 0) {
	    /*
	     * Note: when is ignored.
	     *		If we get this far, we are _only_ dealing
	     *		with current data (pmcd handles the other
	     *		cases).
	     */

	    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
		sts = dispatch->version.four.instance(indom, inst, iname, &inres, pmda);
	    else
		sts = dispatch->version.two.instance(indom, inst, iname, &inres, pmda);
	}
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, PDU_BINARY, sts);
	else {
	    __pmSendInstance(pmda->e_outfd, PDU_BINARY, inres);
	    __pmFreeInResult(inres);
	}
	if (iname)
	    free(iname);
	break;

    case PDU_TEXT_REQ:

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LIBPMDA) {
	    __pmNotifyErr(LOG_DEBUG, "Received PDU_TEXT_REQ\n");
	}
#endif

	if ((sts = __pmDecodeTextReq(pb, PDU_BINARY, &ident, &type)) >= 0) {
	    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
		sts = dispatch->version.four.text(ident, type, &buffer, pmda);
	    else
		sts = dispatch->version.two.text(ident, type, &buffer, pmda);
	}
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, PDU_BINARY, sts);
	else
	    __pmSendText(pmda->e_outfd, PDU_BINARY, ident, buffer);
	break;

    case PDU_RESULT:

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LIBPMDA) {
	    __pmNotifyErr(LOG_DEBUG, "Received PDU_RESULT\n");
	}
#endif

	if ((sts = __pmDecodeResult(pb, PDU_BINARY, &result)) >= 0) {
	    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
		sts = dispatch->version.four.store(result, pmda);
	    else
		sts = dispatch->version.two.store(result, pmda);
	}
	__pmSendError(pmda->e_outfd, PDU_BINARY, sts);
	pmFreeResult(result);
	break;

    case PDU_CONTROL_REQ:
	/*
	 * The protocol followed by the PMCD is to send these as
	 * advisory notices, but not to expect any response, and
	 * in particular no response PDU is required.
	 */

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LIBPMDA) {
	    __pmNotifyErr(LOG_DEBUG, "Received PDU_CONTROL_REQ\n");
	}
#endif

#if 0
	/*
	 * this function moved to libpcp_dev.a - this isn't being used
	 * by any PMDA, so its out of 2.0 libpcp.
	 */
	if ((sts = __pmDecodeControlReq(pb, PDU_BINARY, &result, &control, &state, &delta)) >= 0) {
	    __pmNotifyErr(LOG_ERR, "PDU_CONTROL_REQ not supported");
	}
#endif
	break;

    default:
	__pmNotifyErr(LOG_ERR,
		      "%s: Unrecognised pdu type: %s?\n",
		      pmda->e_name, __pmPDUTypeStr(sts));
	break;
    }

    /*
     * if defined, callback once per PDU to do termination checks,
     * stats, etc
     */
    if (pmda->e_doneCallBack)
	(*(pmda->e_doneCallBack))();

    return 0;
}


void 
pmdaMain(pmdaInterface *dispatch)
{
    for ( ; ; ) {
	if (__pmdaMainPDU(dispatch) < 0)
	    break;
    }
}

void
pmdaSetResultCallBack(pmdaInterface *dispatch, pmdaResultCallBack callback)
{
    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
	dispatch->version.four.ext->e_resultCallBack = callback;
    else if (HAVE_V_TWO(dispatch->comm.pmda_interface))
	dispatch->version.two.ext->e_resultCallBack = callback;
    else {
	__pmNotifyErr(LOG_CRIT, "Unable to set result callback for PMDA interface version %d.",
		     dispatch->comm.pmda_interface);
	dispatch->status = PM_ERR_GENERIC;
    }
}

void
pmdaSetFetchCallBack(pmdaInterface *dispatch, pmdaFetchCallBack callback)
{
    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
	dispatch->version.four.ext->e_fetchCallBack = callback;
    else if (HAVE_V_TWO(dispatch->comm.pmda_interface))
	dispatch->version.two.ext->e_fetchCallBack = callback;
    else {
	__pmNotifyErr(LOG_CRIT, "Unable to set fetch callback for PMDA interface version %d.",
		     dispatch->comm.pmda_interface);
	dispatch->status = PM_ERR_GENERIC;
    }
}

void
pmdaSetCheckCallBack(pmdaInterface *dispatch, pmdaCheckCallBack callback)
{
    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
	dispatch->version.four.ext->e_checkCallBack = callback;
    else if (HAVE_V_TWO(dispatch->comm.pmda_interface))
	dispatch->version.two.ext->e_checkCallBack = callback;
    else {
	__pmNotifyErr(LOG_CRIT, "Unable to set check callback for PMDA interface version %d.",
		     dispatch->comm.pmda_interface);
	dispatch->status = PM_ERR_GENERIC;
    }
}

void
pmdaSetDoneCallBack(pmdaInterface *dispatch, pmdaDoneCallBack callback)
{
    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
	dispatch->version.four.ext->e_doneCallBack = callback;
    else if (HAVE_V_TWO(dispatch->comm.pmda_interface))
	dispatch->version.two.ext->e_doneCallBack = callback;
    else {
	__pmNotifyErr(LOG_CRIT, "Unable to set done callback for PMDA interface version %d.",
		     dispatch->comm.pmda_interface);
	dispatch->status = PM_ERR_GENERIC;
    }
}
