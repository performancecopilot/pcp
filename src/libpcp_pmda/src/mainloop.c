/*
 * Copyright (c) 2013,2017-2018,2022 Red Hat.
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

/*
 * Output warning and debugging messages with a standard format
 * and datestamp to stderr ... based on pmNotifyErr() from libpcp
 *
 * Lines tagged with "tag" unless this is NULL, in which case 
 * pmGetProgname() is used.
 */
void
logmsg(char *tag, const char *message, ...)
{
    va_list		arg;
    struct timeval	tv;
    time_t		now;
    char		ct_buf[26];	/* for ctime_r() */

    gettimeofday(&tv, NULL);

    now = tv.tv_sec;
    ctime_r(&now, ct_buf);

    if (tag == NULL)
	tag = pmGetProgname();

    /*
     * unlike pmNotifyErr, emit usec precision datestamps
     * TODO
     *     could use an additional pmDebugOptions flag (hires?) to turn
     * 	   on usec precision and have sec precision as the default ...
     * 	   this might be useful elsewhere here and in libpcp
     */
    fprintf(stderr, "[%.19s.%06lu] %s: ", ct_buf, (unsigned long)tv.tv_usec, tag);

    va_start(arg, message);
    vfprintf(stderr, message, arg);
    va_end(arg);
}

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
    int			psts;		/* for PDU send ops */
    int			op_sts;
    pmID		pmid;
    pmDesc		desc;
    int			npmids;
    int			pinpdu;
    pmID		*pmidlist;
    char		**namelist = NULL;
    char		*name;
    char		**offspring = NULL;
    int			*statuslist = NULL;
    int			subtype;
    pmdaResult		*result;
    int			ctxnum;
    int			length;
    pmTimeval		unused;
    int			ident;
    int			type;
    pmInDom		indom;
    int			inst;
    char		*iname;
    pmInResult		*inres;
    pmLabelSet		*labels = NULL;
    char		*buffer;
    pmProfile  		*new_profile;
    static pmProfile	*profile;
    static int		first_time = 1;
    static pmdaExt	*pmda;
    static __pmResult	*rp;

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
	    if (sts != PDU_PROFILE && sts != PDU_ATTR) {
		/* all other PDUs expect an ACK */
		psts = __pmSendError(pmda->e_outfd, FROM_ANON, op_sts);
		if (psts < 0) {
		    logmsg("Warning", "__pmSendError(%d,...,%d) ACK failed:%s\n",
			    pmda->e_outfd, op_sts, pmErrStr(psts));
		}
	    }
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
			logmsg(NULL, "Received PDU_ERROR (end context %d)\n",
				dispatch->version.four.ext->e_context);
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
	    logmsg(NULL, "Received PDU_PROFILE ");

	/*
	 * can ignore ctxnum, since pmcd has already used this to send
	 * the correct profile, if required
	 * Free last profile received (if any)
	 * Note error responses are not sent for PDU_PROFILE
	 */
	if (__pmDecodeProfile(pb, &ctxnum, &new_profile) < 0) {
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "???\n");
	   break;
	}
	if (pmDebugOptions.libpmda) {
	    fprintf(stderr, "state ");
	    if (new_profile->state == PM_PROFILE_INCLUDE)
		fprintf(stderr, "INCLUDE");
	    else if (new_profile->state == PM_PROFILE_EXCLUDE)
		fprintf(stderr, "EXCLUDE");
	    else
		fprintf(stderr, "%d ???", new_profile->state);
	    fprintf(stderr, ", len %d\n", new_profile->profile_len);
	}
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
	    logmsg(NULL, "Received PDU_FETCH ");

	/*
	 * can ignore ctxnum, since pmcd has already used this to send
	 * the correct profile, if required
	 */
	sts = __pmDecodeFetch(pb, &ctxnum, &unused, &npmids, &pmidlist);
	if (sts >= 0) {
	    if (pmDebugOptions.libpmda) {
		char	idbuf[20];
		fprintf(stderr, "npmids %d, [0] %s", npmids, pmIDStr_r(pmidlist[0], idbuf, sizeof(idbuf)));
		if (npmids > 1)
		    fprintf(stderr, " ... [%d] %s", npmids-1, pmIDStr_r(pmidlist[npmids-1], idbuf, sizeof(idbuf)));
		fprintf(stderr, "\n");
	    }
	    sts = dispatch->version.any.fetch(npmids, pmidlist, &result, pmda);
	    __pmUnpinPDUBuf(pmidlist);
	}
	else {
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "???\n");
	}

	/*
	 * Highwater mark sized __pmResult for PDU handling routines;
	 * the PMDA fetch interface continues to use the original
	 * pmdaResult structure for backward compatibility - this is OK
	 * because individual PMDAs do not set the fetch timestamps.
	 */
	if (sts >= 0 && (rp == NULL || npmids > rp->numpmid)) {
	    if (rp) {
		rp->numpmid = 0;
		__pmFreeResult(rp);
	    }
	    if ((rp = __pmAllocResult(npmids)) == NULL) {
		sts = -ENOMEM;
		psts = __pmSendError(pmda->e_outfd, FROM_ANON, sts);
		if (psts < 0) {
		    logmsg("Warning", "__pmSendError(%d,...,%d) ACK failed:%s\n",
			    pmda->e_outfd, sts, pmErrStr(psts));
		}
		break;
	    }
	    memset(&rp->timestamp, 0, sizeof(rp->timestamp));
	    rp->numpmid = npmids;
	}

	if (sts < 0) {
	    psts = __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendError(%d,...,%d) FETCH failed: %s\n",
			pmda->e_outfd, sts, pmErrStr(psts));
	    }
	} else {
	    int	i;

	    rp->timestamp.sec = result->timestamp.tv_sec; /* changed state */
	    for (i = 0; i < result->numpmid; i++)
		rp->vset[i] = result->vset[i];
	    rp->numpmid = result->numpmid;

	    psts = __pmSendResult(pmda->e_outfd, FROM_ANON, rp);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendResult(%d,...) failed: %s\n",
			pmda->e_outfd, pmErrStr(psts));
	    }
	    if (pmda->e_resultCallBack != NULL)
		pmda->e_resultCallBack(result);
	}
	break;

    case PDU_PMNS_NAMES:
	if (pmDebugOptions.libpmda)
	    logmsg(NULL, "Received PDU_PMNS_NAMES ");

	if ((sts = __pmDecodeNameList(pb, &npmids, &namelist, NULL)) >= 0) {
	    if (pmDebugOptions.libpmda) {
		fprintf(stderr, "npmids %d, [0] %s", npmids, namelist[0]);
		if (npmids > 1)
		    fprintf(stderr, " ... [%d] %s", npmids-1, namelist[npmids-1]);
		fprintf(stderr, "\n");
	    }
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
	else {
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "???\n");
	}
	if (sts < 0) {
	    psts = __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendError(%d,...,%d) PMNS_NAMES failed: %s\n",
			pmda->e_outfd, sts, pmErrStr(psts));
	    }
	}
	else {
	    psts = __pmSendIDList(pmda->e_outfd, FROM_ANON, 1, &pmid, sts);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendIDList(%d,...) failed: %s\n",
			pmda->e_outfd, pmErrStr(psts));
	    }
	}
	break;

    case PDU_PMNS_CHILD:
	if (pmDebugOptions.libpmda)
	    logmsg(NULL, "Received PDU_PMNS_CHILD ");

	if ((sts = __pmDecodeChildReq(pb, &name, &subtype)) >= 0) {
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "name %s, subtype %d\n", name, subtype);
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
		if (pmDebugOptions.libpmda) {
		    static int	onetrip = 1;
		    if (onetrip) {
			logmsg("Warning", "PMDA version %d < 4, no PMDA children() callback possible\n", dispatch->comm.pmda_interface);
			onetrip = 0;
		    }
		}
		sts = PM_ERR_NAME;
	    }
	    free(name);
	}
	else {
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "???\n");
	}
	if (sts < 0) {
	    psts = __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendError(%d,...,%d) PMNS_CHILD failed: %s\n",
			pmda->e_outfd, sts, pmErrStr(psts));
	    }
	}
	else {
	    psts = __pmSendNameList(pmda->e_outfd, FROM_ANON, sts, (const char **)offspring, statuslist);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendNameList(%d,...) PMNS_CHILD failed: %s\n",
			pmda->e_outfd, pmErrStr(psts));
	    }
	}
	if (offspring) free(offspring);
	if (statuslist) free(statuslist);
	break;

    case PDU_PMNS_TRAVERSE:
	if (pmDebugOptions.libpmda)
	    logmsg(NULL, "Received PDU_PMNS_TRAVERSE ");

	if ((sts = __pmDecodeTraversePMNSReq(pb, &name)) >= 0) {
	    if (pmDebugOptions.libpmda) {
		fprintf(stderr, "name %s\n", name);
	    }
	    if (HAVE_V_FOUR(dispatch->comm.pmda_interface)) {
		sts = dispatch->version.four.children(name, 1, &offspring, &statuslist, pmda);
		if (sts >= 0) {
		    if (statuslist) free(statuslist);
		    statuslist = NULL;
		}
	    }
	    else {
		/* Not INTERFACE_4 */
		if (pmDebugOptions.libpmda) {
		    static int	onetrip = 1;
		    if (onetrip) {
			logmsg("Warning", "PMDA version %d < 4, no PMDA children() callback possible\n", dispatch->comm.pmda_interface);
			onetrip = 0;
		    }
		}
		sts = PM_ERR_NAME;
	    }
	    free(name);
	}
	else {
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "???\n");
	}
	if (sts < 0) {
	    psts = __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendError(%d,...,%d) PMNS_TRAVERSE failed: %s\n",
			pmda->e_outfd, sts, pmErrStr(psts));
	    }
	}
	else {
	    psts = __pmSendNameList(pmda->e_outfd, FROM_ANON, sts, (const char **)offspring, NULL);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendNameList(%d,...) PMNS_TRAVERSE failed: %s\n",
			pmda->e_outfd, pmErrStr(psts));
	    }
	}
	if (offspring) free(offspring);
	break;

    case PDU_PMNS_IDS:
	if (pmDebugOptions.libpmda)
	    logmsg(NULL, "Received PDU_PMNS_IDS ");

	sts = __pmDecodeIDList(pb, 1, &pmid, &op_sts);
	if (sts >= 0) {
	    sts = op_sts;
	    if (pmDebugOptions.libpmda) {
		char	idbuf[20];
		fprintf(stderr, "pmid %s\n", pmIDStr_r(pmid, idbuf, sizeof(idbuf)));
	    }
	}
	else {
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "???\n");
	}
	if (sts >= 0) {
	    if (HAVE_V_FOUR(dispatch->comm.pmda_interface))
		sts = dispatch->version.four.name(pmid, &namelist, pmda);
	    else {
		/* Not INTERFACE_4 */
		if (pmDebugOptions.libpmda) {
		    static int	onetrip = 1;
		    if (onetrip) {
			logmsg("Warning", "PMDA version %d < 4, no PMDA name() callback possible\n", dispatch->comm.pmda_interface);
			onetrip = 0;
		    }
		}
		sts = PM_ERR_PMID;
	    }
	}
	if (sts < 0) {
	    psts = __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendError(%d,...,%d) PMNS_IDS failed: %s\n",
			pmda->e_outfd, sts, pmErrStr(psts));
	    }
	}
	else {
	    psts = __pmSendNameList(pmda->e_outfd, FROM_ANON, sts, (const char **)namelist, NULL);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendNameList(%d,...) PMNS_IDS failed: %s\n",
			pmda->e_outfd, pmErrStr(psts));
	    }
	}
	if (namelist) free(namelist);
	break;

    case PDU_DESC_REQ:
	if (pmDebugOptions.libpmda)
	    logmsg(NULL, "Received PDU_DESC_REQ ");

	if ((sts = __pmDecodeDescReq(pb, &pmid)) >= 0) {
	    if (pmDebugOptions.libpmda) {
		char	idbuf[20];
		fprintf(stderr, "pmid %s\n", pmIDStr_r(pmid, idbuf, sizeof(idbuf)));
	    }
	    sts = dispatch->version.any.desc(pmid, &desc, pmda);
	}
	else {
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "???\n");
	}
	if (sts < 0) {
	    psts = __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendError(%d,...,%d) DESC_REQ failed: %s\n",
			pmda->e_outfd, sts, pmErrStr(psts));
	    }
	}
	else {
	    psts = __pmSendDesc(pmda->e_outfd, FROM_ANON, &desc);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendDesc(%d,...) failed: %s\n",
			pmda->e_outfd, pmErrStr(psts));
	    }
	}
	break;

    case PDU_LABEL_REQ:
	if (pmDebugOptions.libpmda)
	    logmsg(NULL, "Received PDU_LABEL_REQ ");

	if ((sts = __pmDecodeLabelReq(pb, &ident, &type)) >= 0) {
	    if (pmDebugOptions.libpmda) {
		fprintf(stderr, "ident %d, type %d\n", ident, type);
	    }
	    if (HAVE_V_SEVEN(dispatch->comm.pmda_interface)) {
		ctxnum = dispatch->version.seven.ext->e_context;
		sts = dispatch->version.seven.label(ident, type, &labels, pmda);
	    }
	    else {
		if (pmDebugOptions.libpmda) {
		    static int	onetrip = 1;
		    if (onetrip) {
			logmsg("Warning", "PMDA version %d < 7, no PMDA label() callback possible\n", dispatch->comm.pmda_interface);
			onetrip = 0;
		    }
		}
	    }
	}
	else {
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "???\n");
	}
	if (sts < 0) {
	    psts = __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendError(%d,...,%d) LABEL_REQ failed: %s\n",
			pmda->e_outfd, sts, pmErrStr(psts));
	    }
	}
	else {
	    if (sts > 0 && !(type & PM_LABEL_INSTANCES))
		sts = 1;
	    psts = __pmSendLabel(pmda->e_outfd, FROM_ANON, ident, type, labels, sts);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendLabel(%d,...) failed: %s\n",
			pmda->e_outfd, pmErrStr(psts));
	    }
	    pmFreeLabelSets(labels, sts);
	}
	break;

    case PDU_INSTANCE_REQ:
	if (pmDebugOptions.libpmda)
	    logmsg(NULL, "Received PDU_INSTANCE_REQ ");

	if ((sts = __pmDecodeInstanceReq(pb, &indom, &inst, &iname)) >= 0) {
	    if (pmDebugOptions.libpmda) {
		char	idbuf[20];
		fprintf(stderr, "indom %s, inst %d, iname ",
		    pmInDomStr_r(indom, idbuf, sizeof(idbuf)), inst);
		if (iname == NULL)
		    fprintf(stderr, "NULL\n");
		else
		    fprintf(stderr, "\"%s\"\n", iname);
	    }
	    sts = dispatch->version.any.instance(indom, inst, iname, &inres, pmda);
	}
	else {
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "???\n");
	}
	if (sts < 0) {
	    psts = __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendError(%d,...,%d) INSTANCE_REQ failed: %s\n",
			pmda->e_outfd, sts, pmErrStr(psts));
	    }
	}
	else {
	    psts = __pmSendInstance(pmda->e_outfd, FROM_ANON, inres);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendInstance(%d,...) failed: %s\n",
			pmda->e_outfd, pmErrStr(psts));
	    }
	    __pmFreeInResult(inres);
	}
	if (iname)
	    free(iname);
	break;

    case PDU_TEXT_REQ:
	if (pmDebugOptions.libpmda)
	    logmsg(NULL, "Received PDU_TEXT_REQ ");

	if ((sts = __pmDecodeTextReq(pb, &ident, &type)) >= 0) {
	    if (pmDebugOptions.libpmda) {
		char	idbuf[20];
		fprintf(stderr, "type 0x%x, ", type);
		if (type & PM_TEXT_PMID)
		    fprintf(stderr, "pmid %s\n", pmIDStr_r(ident, idbuf, sizeof(idbuf)));
		else if (type & PM_TEXT_INDOM)
		    fprintf(stderr, "indom %s\n", pmInDomStr_r(ident, idbuf, sizeof(idbuf)));
		else
		    fprintf(stderr, "ident 0x%x\n", ident);
	    }
	    sts = dispatch->version.any.text(ident, type, &buffer, pmda);
	}
	else {
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "???\n");
	}
	if (sts < 0) {
	    psts = __pmSendError(pmda->e_outfd, FROM_ANON, sts);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendError(%d,...,%d) TEXT_REQ failed: %s\n",
			pmda->e_outfd, sts, pmErrStr(psts));
	    }
	}
	else {
	    psts = __pmSendText(pmda->e_outfd, FROM_ANON, ident, buffer);
	    if (psts < 0) {
		logmsg("Warning", "__pmSendText(%d,...) failed: %s\n",
			pmda->e_outfd, pmErrStr(psts));
	    }
	}
	break;

    case PDU_RESULT:
	if (pmDebugOptions.libpmda)
	    logmsg(NULL, "Received PDU_RESULT ");

	if (rp) {
	    rp->numpmid = 0;
	    __pmFreeResult(rp);
	}
	if ((sts = __pmDecodeResult(pb, &rp)) >= 0) {
	    if (pmDebugOptions.libpmda) {
		char	idbuf[20];
		fprintf(stderr, "npmids %d, [0] %s", rp->numpmid, pmIDStr_r(rp->vset[0]->pmid, idbuf, sizeof(idbuf)));
		if (rp->numpmid > 1)
		    fprintf(stderr, " ... [%d] %s", rp->numpmid-1, pmIDStr_r(rp->vset[rp->numpmid-1]->pmid, idbuf, sizeof(idbuf)));
		fprintf(stderr, "\n");
	    }
	    sts = dispatch->version.any.store(__pmOffsetResult_v2(rp), pmda);
	}
	else {
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "???\n");
	}
	__pmSendError(pmda->e_outfd, FROM_ANON, sts);
	break;

    case PDU_CONTROL_REQ:
	if (pmDebugOptions.libpmda)
	    logmsg(NULL, "Error: received PDU_CONTROL_REQ, not expected\n");
	break;

    case PDU_ATTR:
	if (pmDebugOptions.libpmda)
	    logmsg(NULL, "Received PDU_ATTR ");

	if (__pmDecodeAttr(pb, &subtype, &buffer, &length) < 0) {
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "???\n");
	    break;
	}
	if (pmDebugOptions.libpmda) {
	    int		i;
	    char	strbuf[16];	/* see __pmAttrKeyStr_r() */
	    __pmAttrKeyStr_r((__pmAttrKey)subtype, strbuf, sizeof(strbuf));
	    fprintf(stderr, "key %s (%d), value \"", strbuf, subtype);
	    for (i = 0; i < length; i++) {
		if (buffer[i] != '\0')
		    fputc(buffer[i], stderr);
	    }
	    fprintf(stderr, "\" (len %d)", length);
	    if (HAVE_V_SIX(dispatch->comm.pmda_interface))
		fprintf(stderr, ", ctxnum %d", dispatch->version.six.ext->e_context);
	    fputc('\n', stderr);
	}
	if (!HAVE_V_SIX(dispatch->comm.pmda_interface)) {
	    if (pmDebugOptions.libpmda) {
		static int	onetrip = 1;
		if (onetrip) {
		    logmsg("Warning", "PMDA version %d < 6, no PMDA attribute() callback possible\n", dispatch->comm.pmda_interface);
		    onetrip = 0;
		}
	    }
	    break;
	}
	ctxnum = dispatch->version.six.ext->e_context;
	if ((sts = dispatch->version.six.attribute(ctxnum, subtype, buffer, length, pmda)) < 0)
	    /* Note error responses are not sent for PDU_ATTR */
	    logmsg("Warning", "Failed to set attribute: %s\n",
			pmErrStr(sts));
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
