/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * PCP PDU echo server.
 *
 *	with -r (raw)
 *		sends PDUs straightback w/out inspection (uses only
 *		the PDU header to determine the length
 *
 *	without -r
 *		uses libpcp routines to decode the PDU and then the
 *		send the PDU ... exercises local recv and xmit logic
 *		for PDUs
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/trace.h>
#include <pcp/trace_dev.h>
#include <strings.h>
#include "localconfig.h"

static int	raw;		/* if set, echo PDUs, do not decode/encode */

typedef struct {		/* from src/libpcp/src/p_pmns.c */
    __pmPDUHdr   hdr;
    int         sts;      	/* to encode status of pmns op */
    int         numids;
    pmID        idlist[1];
} idlist_t;

/*
 * use pid as "from" context for backwards compatibility to
 * keep QA tests happy, rather than FROM_ANON which would be
 * the more normal value for this usage.
 */
static pid_t		mypid;

static int
decode_encode(int fd, __pmPDU *pb, int type)
{
    int		e;
    int		code;
    int		proto;
    pmResult	*rp;
    __pmProfile	*profp;
    int		ctxnum;
    int		fail = -1;
    __pmTimeval	now;
    int		numpmid;
    pmID	*pmidp;
    pmID	pmid;
    pmDesc	desc;
    pmInDom	indom;
    int		inst;
    char	*name;
    __pmInResult	*inres;
    int		control;
    int		length;
    int		state;
    int		attr;
    int		rate;
    int		ident;
    int		txtype;
    char	*buffer;
    int		xtype;
    int		xlen;
    int		sender;
    int		count;
    __pmCred	*creds;
    idlist_t	*idlist_p;
    static int	numpmidlist;
    static pmID	*pmidlist;
    int		numlist;
    char	**namelist;
    int		*statlist;
    __pmLoggerStatus	*lsp;
    double	value;

    switch (type) {

	case PDU_ERROR:
	    if ((e = __pmDecodeError(pb, &code)) < 0) {
		fprintf(stderr, "%s: Error: DecodeError: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "+ PDU_ERROR: code=%d\n", code);
#endif
	    if ((e = __pmSendError(fd, mypid, code)) < 0) {
		fprintf(stderr, "%s: Error: SendError: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_RESULT:
	    if ((e = __pmDecodeResult(pb, &rp)) < 0) {
		fprintf(stderr, "%s: Error: DecodeResult: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "+ PDU_RESULT:\n");
		__pmDumpResult(stderr, rp);
	    }
#endif
	    e = __pmSendResult(fd, mypid, rp);
	    pmFreeResult(rp);
	    if (e < 0) {
		fprintf(stderr, "%s: Error: SendResult: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_PROFILE:
	    if ((e = __pmDecodeProfile(pb, &ctxnum, &profp)) < 0) {
		fprintf(stderr, "%s: Error: DecodeProfile: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "+ PDU_PROFILE: ctxnum=%d\n", ctxnum);
		__pmDumpProfile(stderr, PM_INDOM_NULL, profp);
	    }
#endif
	    e = __pmSendProfile(fd, mypid, ctxnum, profp);
	    free(profp->profile);
	    free(profp);
	    if (e < 0) {
		fprintf(stderr, "%s: Error: SendProfile: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_FETCH:
	    if ((e = __pmDecodeFetch(pb, &ctxnum, &now, &numpmid, &pmidp)) < 0) {
		fprintf(stderr, "%s: Error: DecodeFetch: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		int		j;
		struct timeval	foo;
		fprintf(stderr, "+ PDU_FETCH: ctxnum=%d now=%d.%06d ",
		    ctxnum, now.tv_sec, now.tv_usec);
		foo.tv_sec = now.tv_sec;
		foo.tv_usec = now.tv_usec;
		__pmPrintStamp(stderr, &foo);
		fprintf(stderr, " numpmid=%d\n+ PMIDs:", numpmid);
		for (j = 0; j < numpmid; j++)
		    fprintf(stderr, " %s", pmIDStr(pmidp[j]));
		fputc('\n', stderr);
	    }
#endif
	    e = __pmSendFetch(fd, mypid, ctxnum, &now, numpmid, pmidp);
	    __pmUnpinPDUBuf(pmidp);
	    if (e < 0) {
		fprintf(stderr, "%s: Error: SendFetch: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_DESC_REQ:
	    if ((e = __pmDecodeDescReq(pb, &pmid)) < 0) {
		fprintf(stderr, "%s: Error: DecodeDescReq: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "+ PDU_DESC_REQ: pmid=%s\n", pmIDStr(pmid));
#endif
	    if ((e = __pmSendDescReq(fd, mypid, pmid)) < 0) {
		fprintf(stderr, "%s: Error: SendDescReq: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_DESC:
	    if ((e = __pmDecodeDesc(pb, &desc)) < 0) {
		fprintf(stderr, "%s: Error: DecodeDesc: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "+ PDU_DESC: ");
		__pmPrintDesc(stderr, &desc);
	    }
#endif
	    if ((e = __pmSendDesc(fd, mypid, &desc)) < 0) {
		fprintf(stderr, "%s: Error: SendDesc: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_INSTANCE_REQ:
	    if ((e = __pmDecodeInstanceReq(pb, &now, &indom, &inst, &name)) < 0) {
		fprintf(stderr, "%s: Error: DecodeInstanceReq: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		struct timeval	foo;
		fprintf(stderr, "+ PDU_INSTANCE_REQ: now=%d.%06d ",
		    now.tv_sec, now.tv_usec);
		foo.tv_sec = now.tv_sec;
		foo.tv_usec = now.tv_usec;
		__pmPrintStamp(stderr, &foo);
		fprintf(stderr, " indom=%s", pmInDomStr(indom));
		if (inst == PM_IN_NULL)
		    fprintf(stderr, " inst=PM_IN_NULL");
		else
		    fprintf(stderr, " inst=%d", inst);
		if (name == NULL)
		    fprintf(stderr, " name=NULL\n");
		else
		    fprintf(stderr, " name=\"%s\"\n", name);
	    }
#endif
	    e = __pmSendInstanceReq(fd, mypid, &now, indom, inst, name);
	    if (name)
		free(name);
	    if (e < 0) {
		fprintf(stderr, "%s: Error: SendInstanceReq: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_INSTANCE:
	    if ((e = __pmDecodeInstance(pb, &inres)) < 0) {
		fprintf(stderr, "%s: Error: DecodeInstance: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "+ PDU_INSTANCE: ");
		__pmDumpInResult(stderr, inres);
	    }
#endif
	    e = __pmSendInstance(fd, mypid, inres);
	    __pmFreeInResult(inres);
	    if (e < 0) {
		fprintf(stderr, "%s: Error: SendInstance: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_TEXT_REQ:
	    if ((e = __pmDecodeTextReq(pb, &ident, &txtype)) < 0) {
		fprintf(stderr, "%s: Error: DecodeTextReq: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "+ PDU_TEXT_REQ: ident=%d ", ident);
		if (txtype & PM_TEXT_INDOM)
		    fprintf(stderr, "INDOM %s", pmInDomStr((pmInDom)ident));
		if (txtype & PM_TEXT_PMID)
		    fprintf(stderr, "PMID %s", pmIDStr((pmID)ident));
		fprintf(stderr, " txtype=%d\n", txtype);
	    }
#endif
	    if ((e = __pmSendTextReq(fd, mypid, ident, txtype)) < 0) {
		fprintf(stderr, "%s: Error: SendTextReq: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_TEXT:
	    if ((e = __pmDecodeText(pb, &ident, &buffer)) < 0) {
		fprintf(stderr, "%s: Error: DecodeText: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		int	len;
		int	c;
		fprintf(stderr, "+ PDU_TEXT: ident=%d", ident);
		len = strlen(buffer);
		c = buffer[len - 1];
		buffer[len - 1] = '\0';
		if (len < 30)
		    fprintf(stderr, " text=\"%s\"\n", buffer);
		else {
		    fprintf(stderr, " text=\"%12.12s ... ", buffer);
		    fprintf(stderr, "%s\"\n", &buffer[len - 18]);
		}
		buffer[len - 1] = c;
	    }
#endif
	    if ((e = __pmSendText(fd, mypid, ident, buffer)) < 0) {
		fprintf(stderr, "%s: Error: SendText: %s\n", pmProgname, pmErrStr(e));
		free(buffer);
		break;
	    }
	    fail = 0;
	    free(buffer);
	    break;

#if PCP_VER >= 3800
	case PDU_AUTH:
	    if ((e = __pmDecodeAuth(pb, &attr, &buffer, &length)) < 0) {
		fprintf(stderr, "%s: Error: DecodeAuth: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		char	buf[32] = { 0 };

		fprintf(stderr, "+ PDU_AUTH: attr=%d length=%d", attr, length);
		if (length < sizeof(buf)-2) {
                    strncpy(buf, buffer, length);
		    fprintf(stderr, " value=\"%s\"\n", buf);
		} else {
                    strncpy(buf, buffer, sizeof(buf)-2);
		    fprintf(stderr, " value=\"%12.12s ... ", buf);
                    strncpy(buf, &buffer[length-18], sizeof(buf)-2);
		    fprintf(stderr, "%s\"\n", buf);
		}
	    }
#endif
	    if ((e = __pmSendAuth(fd, mypid, attr, buffer, length)) < 0) {
		fprintf(stderr, "%s: Error: SendAuth: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;
#endif

	case PDU_CREDS:
	    if ((e = __pmDecodeCreds(pb, &sender, &count, &creds)) < 0) {
		fprintf(stderr, "%s: Error: DecodeCreds: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		int	i;
		fprintf(stderr, "+ PDU_CREDS: sender=%d count=%d\n",
		    sender, count);
		for (i = 0; i < count; i++) {
		    fprintf(stderr, "+ [%d] type=%d a=%d b=%d c=%d\n",
			i, creds[i].c_type, creds[i].c_vala, creds[i].c_valb,
			creds[i].c_valc);
		}
	    }
#endif
	    e = __pmSendCreds(fd, mypid, count, creds);
	    if (count > 0)
		free(creds);
	    if (e < 0) {
		fprintf(stderr, "%s: Error: SendCreds: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_PMNS_IDS:
	    idlist_p = (idlist_t *)pb;
	    numpmid = ntohl(idlist_p->numids);
	    if (numpmid > numpmidlist) {
		if (pmidlist != NULL)
		    free(pmidlist);
		if ((pmidlist = (pmID *)malloc(numpmid * sizeof(pmidlist[0]))) == NULL) {
		    fprintf(stderr, "malloc pmidlist[%d]: %s\n", numpmid, strerror(errno));
		    numpmidlist = 0;
		    break;
		}
		numpmidlist = numpmid;
	    }
	    if ((e = __pmDecodeIDList(pb, numpmid, pmidlist, &code)) < 0) {
		fprintf(stderr, "%s: Error: DecodeIDList: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "+ PDU_PMNS_IDS: sts arg=%d\n", code);
		__pmDumpIDList(stderr, numpmid, pmidlist);
	    }
#endif
	    e = __pmSendIDList(fd, mypid, numpmid, pmidlist, code);
	    if (e < 0) {
		fprintf(stderr, "%s: Error: SendIDList: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_PMNS_NAMES:
	    if ((e = __pmDecodeNameList(pb, &numlist, &namelist, &statlist)) < 0) {
		fprintf(stderr, "%s: Error: DecodeNameList: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "+ PDU_PMNS_NAMES:\n");
		if (namelist != NULL)
		    __pmDumpNameList(stderr, numlist, namelist);
		if (statlist != NULL)
		    __pmDumpStatusList(stderr, numlist, statlist);
	    }
#endif
	    e = __pmSendNameList(fd, mypid, numlist, namelist, statlist);
	    if (namelist != NULL)
		free(namelist);
	    if (statlist != NULL)
		free(statlist);
	    if (e < 0) {
		fprintf(stderr, "%s Error: SendNameList: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_PMNS_CHILD:
	    if ((e = __pmDecodeChildReq(pb, &name, &code)) < 0) {
		fprintf(stderr, "%s Error: DecodeChildReq: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "+ PDU_PMNS_CHILD: name=\"%s\" code=%d\n", name, code);
	    }
#endif
	    e = __pmSendChildReq(fd, mypid, name, code);
	    free(name);
	    if (e < 0) {
		fprintf(stderr, "%s: Error: SendChildReq: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_PMNS_TRAVERSE:
	    if ((e = __pmDecodeTraversePMNSReq(pb, &name)) < 0) {
		fprintf(stderr, "%s: Error: DecodeTraversePMNSReq: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "+ PDU_PMNS_TRAVERSE: name=\"%s\"\n", name);
	    }
#endif
	    e = __pmSendTraversePMNSReq(fd, mypid, name);
	    free(name);
	    if (e < 0) {
		fprintf(stderr, "%s: Error: SendTraversePMNSReq: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_LOG_CONTROL:
	    if ((e = __pmDecodeLogControl(pb, &rp, &control, &state, &rate)) < 0) {
		fprintf(stderr, "%s: Error: DecodeLogControl: %s\n", pmProgname, pmErrStr(e));
	        break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "+ PDU_LOG_CONTROL: control=%d state=%d rate=%d\n",
		    control, state, rate);
		__pmDumpResult(stderr, rp);
	    }
#endif
	    e = __pmSendLogControl(fd, rp, control, state, rate);
	    pmFreeResult(rp);
	    if (e < 0) {
		fprintf(stderr, "%s: Error: SendLogControl: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_LOG_STATUS:
	    if ((e = __pmDecodeLogStatus(pb, &lsp)) < 0) {
		fprintf(stderr, "%s: Error: DecodeLogStatus: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		struct timeval	foo;
		fprintf(stderr, "+ PDU_LOG_STATUS: start=%d.%06d ",
		    lsp->ls_start.tv_sec, lsp->ls_start.tv_usec);
		foo.tv_sec = lsp->ls_start.tv_sec;
		foo.tv_usec = lsp->ls_start.tv_usec;
		__pmPrintStamp(stderr, &foo);
		fprintf(stderr, "\nlast=%d.%06d ",
		    lsp->ls_last.tv_sec, lsp->ls_last.tv_usec);
		foo.tv_sec = lsp->ls_last.tv_sec;
		foo.tv_usec = lsp->ls_last.tv_usec;
		__pmPrintStamp(stderr, &foo);
		fprintf(stderr, " now=%d.%06d ",
		    lsp->ls_timenow.tv_sec, lsp->ls_timenow.tv_usec);
		foo.tv_sec = lsp->ls_timenow.tv_sec;
		foo.tv_usec = lsp->ls_timenow.tv_usec;
		__pmPrintStamp(stderr, &foo);
		fprintf(stderr, "\nstate=%d vol=%d size=%lld host=%s tz=\"%s\" tzlogger=\"%s\"\n",
		    lsp->ls_state, lsp->ls_vol, (long long)lsp->ls_size,
		    lsp->ls_hostname, lsp->ls_tz, lsp->ls_tzlogger);
	    }
#endif
	    e = __pmSendLogStatus(fd, lsp);
	    __pmUnpinPDUBuf(pb);
	    if (e < 0) {
		fprintf(stderr, "%s: Error: SendLogStatus: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case PDU_LOG_REQUEST:
	    if ((e = __pmDecodeLogRequest(pb, &control)) < 0) {
		fprintf(stderr, "%s: Error: DecodeLogRequest: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "+ PDU_LOG_REQUEST: request=");
		if (control == LOG_REQUEST_NEWVOLUME)
		    fprintf(stderr, "new volume\n");
		else if (control == LOG_REQUEST_STATUS)
		    fprintf(stderr, "status\n");
		else if (control == LOG_REQUEST_SYNC)
		    fprintf(stderr, "sync\n");
		else
		    fprintf(stderr, " unknown (%d)!\n", control);
	    }
#endif
	    if ((e = __pmSendLogRequest(fd, control)) < 0) {
		fprintf(stderr, "%s: Error: SendLogRequest: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

	case TRACE_PDU_ACK:
	    if ((e = __pmtracedecodeack(pb, &control)) < 0) {
		fprintf(stderr, "%s: Error: tracedecodeack: %s\n", pmProgname, pmtraceerrstr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "+ TRACE_PDU_ACK: ack=%d\n", control);
	    }
#endif
	    if ((e = __pmtracesendack(fd, control)) < 0) {
		fprintf(stderr, "%s: Error: tracesendack: %s\n", pmProgname, pmtraceerrstr(e));
		break;
	    }
	    fail = 0;
	    break;

	case TRACE_PDU_DATA:
	    if ((e = __pmtracedecodedata(pb, &name, &xlen, &xtype, &proto, &value)) < 0) {
		fprintf(stderr, "%s: Error: tracedecodedata: %s\n", pmProgname, pmtraceerrstr(e));
		break;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "+ TRACE_PDU_DATA: tag=\"%s\" taglen=%d type=",
			name, xlen);
		if (xtype == TRACE_TYPE_TRANSACT)
		    fprintf(stderr, "transact");
		else if (xtype == TRACE_TYPE_POINT)
		    fprintf(stderr, "point");
		else if (xtype == TRACE_TYPE_OBSERVE)
		    fprintf(stderr, "observe");
		else
		    fprintf(stderr, "unknown (%d)!", xtype);
		fprintf(stderr, " value=%g\n", value);
	    }
#endif
	    e = __pmtracesenddata(fd, name, xlen, xtype, value);
	    free(name);
	    if (e < 0) {
		fprintf(stderr, "%s: Error: tracesenddata: %s\n", pmProgname, pmtraceerrstr(e));
		break;
	    }
	    fail = 0;
	    break;

	default:
	    if ((e = __pmSendError(fd, mypid, PM_ERR_NYI)) < 0) {
		fprintf(stderr, "%s: Error: SendError: %s\n", pmProgname, pmErrStr(e));
		break;
	    }
	    fail = 0;
	    break;

    }

    fflush(stderr);

    return fail;
}

int
main(int argc, char *argv[])
{
    int		fd;
    int		port = 4323;
			/* default port for remote connection to pdu-server */
    int		i, sts;
    int		c;
    int		newfd;
    int		new;
    struct sockaddr_in	myAddr;
    struct linger	noLinger = {1, 0};
    char	*endnum;
    int		errflag = 0;
    __pmPDU	*pb;
    __pmPDUHdr	*php;
    char	*fmt;
    char	*p;

    __pmSetProgname(argv[0]);
    mypid = getpid();

    while ((c = getopt(argc, argv, "D:p:rZ:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'r':	/* raw mode, no decode/encode */
	    raw = 1;
	    break;

	case 'p':
	    port = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: port argument must be a numeric internet port number\n", pmProgname);
		exit(1);
	    }
	    break;

	case 'Z':	/* $TZ timezone */
	    pmNewZone(optarg);
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc) {
	fprintf(stderr, "Usage: %s [-D n] [-p port] [-r] [-Z timezone]\n", pmProgname);
	exit(1);
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	perror("socket");
	exit(1);
    }
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &i,
		   sizeof(i)) < 0) {
	perror("setsockopt(nodelay)");
	exit(1);
    }
    /* Don't linger on close */
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &noLinger, sizeof(noLinger)) < 0) {
	perror("setsockopt(nolinger)");
	exit(1);
    }

    memset(&myAddr, 0, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myAddr.sin_port = htons(port);
    sts = bind(fd, (struct sockaddr*)&myAddr, sizeof(myAddr));
    if (sts < 0) {
	fprintf(stderr, "bind(%d): %s\n", port, strerror(errno));
	exit(1);
    }

    sts = listen(fd, 5);	/* Max. of 5 pending connection requests */
    if (sts == -1) {
	perror("listen");
	exit(1);
    }

    fprintf(stderr, "%s: MYPID %" FMT_PID, pmProgname, mypid);
    /* don't have %x equivalent of FMT_PID unfortunately */
    fmt = strdup(" %" FMT_PID "\n");
    p = index(fmt, 'd');
    *p = 'x';
    fprintf(stderr, fmt, mypid);
    free(fmt);

    for ( ; ; ) {

	newfd = accept(fd, (struct sockaddr *)0, 0);
	if (newfd < 0) {
	    fprintf(stderr, "%s: accept: %s\n", pmProgname, strerror(errno));
	    exit(1);
	}

	new = 1;

	if (!raw && __pmSetVersionIPC(newfd, PDU_VERSION) < 0) {
	    fprintf(stderr, "%s: __pmSetVersionIPC: %s\n", pmProgname, pmErrStr(-errno));
	    exit(1);
	}

	for ( ; ; ) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "\n%s: waiting ...\n", pmProgname);
#endif
	    sts = __pmGetPDU(newfd, ANY_SIZE, TIMEOUT_NEVER, &pb);
	    if (sts == 0) {
		fprintf(stderr, "%s: end-of-file\n", pmProgname);
		break;
	    }
	    else if (sts < 0) {
		fprintf(stderr, "%s: __pmGetPDU: %s\n", pmProgname, pmErrStr(sts));
		break;
	    }

	    if (new) {
		php = (__pmPDUHdr *)pb;
		fprintf(stderr, "\n%s: new connection fd=%d CLIENTPID %d %x\n\n",
		    pmProgname, newfd, php->from, php->from);
		new = 0;
	    }

	    if (raw) {
		sts = __pmXmitPDU(newfd, pb);
		if (sts < 0) {
		    fprintf(stderr, "%s: __pmXmitPDU: %s\n", pmProgname, pmErrStr(sts));
		    break;
		}
		continue;
	    }

	    /*
	     * use PDU type to decode via libpcp, and then libpcp to
	     * send (encode + xmit)
	     */
	    if ((sts = decode_encode(newfd, pb, sts)) < 0) {
		fprintf(stderr, "%s: decode_encode error, disconnect\n", pmProgname);
		break;
	    }
	}

	close(newfd);
    }

}
