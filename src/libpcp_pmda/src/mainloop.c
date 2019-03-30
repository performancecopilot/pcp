/*
 * Copyright (c) 2013,2017-2018 Red Hat.
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
 */

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"
#include "libdefs.h"

int
__pmdaInFd(pmdaInterface *dispatch)
{
    if (HAVE_ANY(dispatch->comm.pmda_interface))
	return dispatch->version.any.ext->e_infd;

    pmNotifyErr(LOG_CRIT, "PMDA interface version %d not supported",
		    dispatch->comm.pmda_interface);
    return -1;
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
    int			length;
    pmTimeval		when;
    int			ident;
    int			type;
    pmInDom		indom;
    int			inst;
    char		*iname;
    pmInResult		*inres;
    pmLabelSet		*labels = NULL;
    char		*buffer;
    pmProfile  		*new_profile;
    static pmProfile	*profile = NULL;
    static int		first_time = 1;
    static pmdaExt	*pmda = NULL;
    int			pinpdu;

    /* Initial version checks */
    if (first_time) {
	if (dispatch->status != 0) {
	    pmNotifyErr(LOG_ERR, "PMDA Initialisation Failed");
	    return -1;
	}
	if (!HAVE_ANY(dispatch->comm.pmda_interface)) {
	    pmNotifyErr(LOG_CRIT, "PMDA interface version %d not supported",
			 dispatch->comm.pmda_interface);
	    return -1;
	}
	pmda = dispatch->version.any.ext;
	dispatch->comm.pmapi_version = PMAPI_VERSION;
	first_time = 0;
    }

    pinpdu = sts = __pmGetPDU(pmda->e_infd, ANY_SIZE, TIMEOUT_NEVER, &pb);
    if (pmDebugOptions.pdu && pmDebugOptions.desperate) {
	char	strbuf[20];
	fprintf(stderr, "__pmdaMainPDU: got PDU type %s from pmcd\n", __pmPDUTypeStr_r(sts, strbuf, sizeof(strbuf)));
    }
    if (sts == 0)
	return PM_ERR_EOF;
    if (sts < 0) {
	pmNotifyErr(LOG_ERR, "IPC Error: %s\n", pmErrStr(sts));
	return sts;
    }

    if (HAVE_V_FIVE(dispatch->comm.pmda_interface)) {
	/* set up sender context */
	__pmPDUHdr	*php = (__pmPDUHdr *)pb;
	/* ntohl() converted already in __pmGetPDU() */
	dispatch->version.four.ext->e_context = php->from;
    }

    /*
     * if defined, callback once per PDU to check availability, etc.
     */
    if (pmda->e_checkCallBack != NULL) {
	op_sts = (*(pmda->e_checkCallBack))();
	if (op_sts < 0) {
	    if (sts != PDU_PROFILE && sts != PDU_ATTR)
		/* all other PDUs expect an ACK */
		__pmSendError(pmda->e_outfd, FROM_ANON, op_sts);
	    __pmUnpinPDUBuf(pb);
	    return 0;
	}
    }

    switch (sts) {
    case PDU_ERROR:
	/*
	 * If __pmDecodeError() fails, just ignore it as no response PDU
	 * is required nor expected.
	 * Expect PM_ERR_NOTCONN to mark client context being closed.
	 */
	if (__pmDecodeError(pb, &op_sts) >= 0) {
	    if (op_sts == PM_ERR_NOTCONN) {
		if (HAVE_V_FIVE(dispatch->comm.pmda_interface)) {
		    if (pmDebugOptions.context)
			pmNotifyErr(LOG_DEBUG, "Received PDU_ERROR (end context %d)\n", dispatch->version.four.ext->e_context);
		    if (pmda->e_endCallBack != NULL)
			(*(pmda->e_endCallBack))(dispatch->version.four.ext->e_context);
		}
	    }
	    else {
		pmNotifyErr(LOG_ERR,
		      "%s: unexpected error pdu from pmcd: %s?\n",
		      pmda->e_name, pmErrStr(op_sts));
	    }
	}
	break;

    case PDU_PROFILE:
	if (pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_DEBUG, "Received PDU_PROFILE\n");

	/*
	 * can ignore ctxnum, since pmcd has already used this to send
	 * the correct profile, if required
	 * Free last profile received (if any)
	 * Note error responses are not sent for PDU_PROFILE
	 */
	if (__pmDecodeProfile(pb, &ctxnum, &new_profile) < 0) 
	   break;
	sts = dispatch->version.any.profile(new_profile, pmda);
	if (sts < 0) {
	    __pmFreeProfile(new_profile);
	} else {
	    __pmFreeProfile(profile);
	    profile = new_profile;
	}
	break;

    case PDU_FETCH:
	if (pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_DEBUG, "Received PDU_FETCH\n");

	/*
	 * can ignore ctxnum, since pmcd has already used this to send
	 * the correct profile, if required
	 */
	sts = __pmDecodeFetch(pb, &ctxnum, &when, &npmids, &pmidlist);
	if (sts >= 0) {
	    sts = dispatch->version.any.fetch(npmids, pmidlist, &result, pmda);
	    __pmUnpinPDUBuf(pmidlist);
	}
	if (sts < 0) {
	    __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	} else {
	    __pmSendResult(pmda->e_outfd, FROM_ANON, result);
	    if (pmda->e_resultCallBack != NULL)
		pmda->e_resultCallBack(result);
	}
	break;

    case PDU_PMNS_NAMES:
	if (pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_DEBUG, "Received PDU_PMNS_NAMES\n");

	if ((sts = __pmDecodeNameList(pb, &npmids, &namelist, NULL)) >= 0) {
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
		/* Not INTERFACE_4 or later */
		sts = PM_ERR_NAME;
	    }
	    free(namelist);
	}
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	else
	    __pmSendIDList(pmda->e_outfd, FROM_ANON, 1, &pmid, sts);
	break;

    case PDU_PMNS_CHILD:
	if (pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_DEBUG, "Received PDU_PMNS_CHILD\n");

	if ((sts = __pmDecodeChildReq(pb, &name, &subtype)) >= 0) {
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
	    free(name);
	}
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	else
	    __pmSendNameList(pmda->e_outfd, FROM_ANON, sts, offspring, statuslist);
	if (offspring) free(offspring);
	if (statuslist) free(statuslist);
	break;

    case PDU_PMNS_TRAVERSE:
	if (pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_DEBUG, "Received PDU_PMNS_TRAVERSE\n");

	if ((sts = __pmDecodeTraversePMNSReq(pb, &name)) >= 0) {
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
	    free(name);
	}
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	else
	    __pmSendNameList(pmda->e_outfd, FROM_ANON, sts, offspring, NULL);
	if (offspring) free(offspring);
	break;

    case PDU_PMNS_IDS:
	if (pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_DEBUG, "Received PDU_PMNS_IDS\n");

	sts = __pmDecodeIDList(pb, 1, &pmid, &op_sts);
	if (sts >= 0)
	    sts = op_sts;
	if (sts >= 0) {
	    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
		sts = dispatch->version.four.name(pmid, &namelist, pmda);
	    else /* Not INTERFACE_4 */
		sts = PM_ERR_PMID;
	}
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	else
	    __pmSendNameList(pmda->e_outfd, FROM_ANON, sts, namelist, NULL);
	if (namelist) free(namelist);
	break;

    case PDU_DESC_REQ:
	if (pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_DEBUG, "Received PDU_DESC_REQ\n");

	if ((sts = __pmDecodeDescReq(pb, &pmid)) >= 0)
	    sts = dispatch->version.any.desc(pmid, &desc, pmda);
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	else
	    __pmSendDesc(pmda->e_outfd, FROM_ANON, &desc);
	break;

    case PDU_LABEL_REQ:
	if (pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_DEBUG, "Received PDU_LABEL_REQ\n");

	if ((sts = __pmDecodeLabelReq(pb, &ident, &type)) >= 0 &&
	    HAVE_V_SEVEN(dispatch->comm.pmda_interface)) {
	    ctxnum = dispatch->version.seven.ext->e_context;
	    sts = dispatch->version.seven.label(ident, type, &labels, pmda);
	}
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	else {
	    if (sts > 0 && !(type & PM_LABEL_INSTANCES))
		sts = 1;
	    __pmSendLabel(pmda->e_outfd, FROM_ANON, ident, type, labels, sts);
	    pmFreeLabelSets(labels, sts);
	}
	break;

    case PDU_INSTANCE_REQ:
	if (pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_DEBUG, "Received PDU_INSTANCE_REQ\n");

	if ((sts = __pmDecodeInstanceReq(pb, &when, &indom, &inst, &iname)) >= 0)
	    sts = dispatch->version.any.instance(indom, inst, iname, &inres, pmda);
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	else {
	    __pmSendInstance(pmda->e_outfd, FROM_ANON, inres);
	    __pmFreeInResult(inres);
	}
	if (iname)
	    free(iname);
	break;

    case PDU_TEXT_REQ:
	if (pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_DEBUG, "Received PDU_TEXT_REQ\n");

	if ((sts = __pmDecodeTextReq(pb, &ident, &type)) >= 0)
	    sts = dispatch->version.any.text(ident, type, &buffer, pmda);
	if (sts < 0)
	    __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	else
	    __pmSendText(pmda->e_outfd, FROM_ANON, ident, buffer);
	break;

    case PDU_RESULT:
	if (pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_DEBUG, "Received PDU_RESULT\n");

	if ((sts = __pmDecodeResult(pb, &result)) >= 0)
	    sts = dispatch->version.any.store(result, pmda);
	__pmSendError(pmda->e_outfd, FROM_ANON, sts);
	pmFreeResult(result);
	break;

    case PDU_CONTROL_REQ:
	if (pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_DEBUG, "Received PDU_CONTROL_REQ\n");
	break;

    case PDU_ATTR:
	if (pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_DEBUG, "Received PDU_ATTR\n");

	if (__pmDecodeAttr(pb, &subtype, &buffer, &length) < 0) 
	    break;
	if (!HAVE_V_SIX(dispatch->comm.pmda_interface))
	    break;
	ctxnum = dispatch->version.six.ext->e_context;
	if ((sts = dispatch->version.six.attribute(ctxnum, subtype, buffer, length, pmda)) < 0)
	    /* Note error responses are not sent for PDU_ATTR */
	    pmNotifyErr(LOG_ERR, "%s: Failed to set attribute: %s\n",
			pmda->e_name, pmErrStr(sts));
	break;

    default: {
	char	strbuf[20];
	pmNotifyErr(LOG_ERR,
		      "%s: Unrecognised pdu type: %s?\n",
		      pmda->e_name, __pmPDUTypeStr_r(sts, strbuf, sizeof(strbuf)));
	}
	break;
    }

    if (pinpdu > 0)
	__pmUnpinPDUBuf(pb);

    /*
     * if defined, callback once per PDU to do termination checks,
     * stats, etc
     */
    if (pmda->e_doneCallBack != NULL)
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
    if (HAVE_ANY(dispatch->comm.pmda_interface))
	dispatch->version.any.ext->e_resultCallBack = callback;
    else {
	pmNotifyErr(LOG_CRIT, "Unable to set result callback for PMDA interface version %d.",
		     dispatch->comm.pmda_interface);
	dispatch->status = PM_ERR_GENERIC;
    }
}

void
pmdaSetEndContextCallBack(pmdaInterface *dispatch, pmdaEndContextCallBack callback)
{
    if (HAVE_V_FIVE(dispatch->comm.pmda_interface) || callback == NULL)
	dispatch->version.four.ext->e_endCallBack = callback;
    else {
	pmNotifyErr(LOG_CRIT, "Unable to set end context callback for PMDA interface version %d.",
		     dispatch->comm.pmda_interface);
	dispatch->status = PM_ERR_GENERIC;
    }
}

void
pmdaSetFetchCallBack(pmdaInterface *dispatch, pmdaFetchCallBack callback)
{
    if (HAVE_ANY(dispatch->comm.pmda_interface))
	dispatch->version.any.ext->e_fetchCallBack = callback;
    else {
	pmNotifyErr(LOG_CRIT, "Unable to set fetch callback for PMDA interface version %d.",
		     dispatch->comm.pmda_interface);
	dispatch->status = PM_ERR_GENERIC;
    }
}

void
pmdaSetCheckCallBack(pmdaInterface *dispatch, pmdaCheckCallBack callback)
{
    if (HAVE_ANY(dispatch->comm.pmda_interface))
	dispatch->version.any.ext->e_checkCallBack = callback;
    else {
	pmNotifyErr(LOG_CRIT, "Unable to set check callback for PMDA interface version %d.",
		     dispatch->comm.pmda_interface);
	dispatch->status = PM_ERR_GENERIC;
    }
}

void
pmdaSetDoneCallBack(pmdaInterface *dispatch, pmdaDoneCallBack callback)
{
    if (HAVE_ANY(dispatch->comm.pmda_interface))
	dispatch->version.any.ext->e_doneCallBack = callback;
    else {
	pmNotifyErr(LOG_CRIT, "Unable to set done callback for PMDA interface version %d.",
		     dispatch->comm.pmda_interface);
	dispatch->status = PM_ERR_GENERIC;
    }
}

void
pmdaSetLabelCallBack(pmdaInterface *dispatch, pmdaLabelCallBack callback)
{
    if (HAVE_V_SEVEN(dispatch->comm.pmda_interface))
	dispatch->version.any.ext->e_labelCallBack = callback;
    else {
	pmNotifyErr(LOG_CRIT, "Unable to set label callback for PMDA interface version %d.",
		     dispatch->comm.pmda_interface);
	dispatch->status = PM_ERR_GENERIC;
    }
}

void
pmdaSendError(pmdaInterface *dispatch, int err)
{
    pmdaExt     *pmda = dispatch->version.any.ext;

    /* Usually err is PM_ERR_PMDAREADY or PM_ERR_PMDANOTREADY */
    __pmSendError(pmda->e_outfd, FROM_ANON, err);
}
