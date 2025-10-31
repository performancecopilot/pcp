/*
 * Copyright (c) 2013,2015,2025 Red Hat.
 * Copyright (c) 1995-2000,2003 Silicon Graphics, Inc.  All Rights Reserved.
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
 */

#include <time.h>
#include "pmcd.h"
#include "config.h"
#if HAVE_STATIC_PROBES
#include "usdt.h"
#else
#define pmcd_usdt_trace(type,who,p1,p2)	do { } while (0)
#endif

/*
 * Diagnostic event tracing support
 */

typedef struct {
    time_t	t_stamp;	/* timestamp */
    int		t_type;		/* event type */
    int		t_who;		/* originator or principal identifier */
    int		t_p1;		/* optional event parameters */
    int		t_p2;
} tracebuf;

static tracebuf		*trace;
static unsigned int	next;

/*
 * by default, circular buffer last 20 events -- change by modifying
 * pmcd.control.tracebufs
 * by default, tracing is disabled -- change by setting the following
 * to 1, pmcd.control.traceconn (trace connections) and/or
 * pmcd.control.tracepdu (trace PDUs)
 */
PMCD_DATA int		pmcd_trace_nbufs = 20;
PMCD_DATA int		pmcd_trace_mask;

void
pmcd_init_trace(int n)
{
    if (trace != NULL)
	free(trace);
    if ((trace = (tracebuf *)malloc(n * sizeof(tracebuf))) == NULL) {
	 pmNoMem("pmcd_init_trace", n * sizeof(tracebuf), PM_RECOV_ERR);
	 return;
    }
    pmcd_trace_nbufs = n;
    next = 0;
}

#if HAVE_STATIC_PROBES
void
pmcd_usdt_trace(int type, int who, int p1, int p2)
{
    switch (type) {
    case TR_ADD_CLIENT:
	if (USDT_IS_ACTIVE(pmcd, add_client)) {
	    ClientInfo		*cip;
	    __pmSockAddr	*saddr;
	    char		saddrbuf[96];
	    char		*addrs;
	    unsigned int	seq;
	    int			fd;

	    cip = GetClient(who);
	    if (cip == NULL) {
		fd = -1;
		seq = 0;
		pmstrncpy(saddrbuf, sizeof(saddrbuf), "unknown");
	    } else {
		fd = cip->fd;
		seq = cip->seq;
		saddr = (__pmSockAddr *)cip->addr;
		addrs = __pmSockAddrToString(saddr);
		if (addrs == NULL) {
		    pmstrncpy(saddrbuf, sizeof(saddrbuf), "invalid");
		} else {
		    pmstrncpy(saddrbuf, sizeof(saddrbuf), addrs);
		    free(addrs);
    		}
	    }
	    USDT_WITH_SEMA(pmcd, add_client, who, saddrbuf, fd, seq);
	}
	break;

    case TR_DEL_CLIENT:
	if (USDT_IS_ACTIVE(pmcd, del_client)) {
	    const int		fd = p1, err = p2;

	    USDT_WITH_SEMA(pmcd, del_client, who, fd, err);
	}
	break;

    case TR_ADD_AGENT:
	if (USDT_IS_ACTIVE(pmcd, add_agent)) {
	    const int		infd = p1, outfd = p2;
	    char		agent_type[16];

	    if (infd == -1 && outfd == -1)
		pmstrncpy(agent_type, sizeof(agent_type), "DSO");
	    else
		pmstrncpy(agent_type, sizeof(agent_type), "daemon");
	    USDT_WITH_SEMA(pmcd, add_agent, who, agent_type, infd, outfd);
	}
	break;

    case TR_DEL_AGENT:
	if (USDT_IS_ACTIVE(pmcd, del_agent)) {
	    const int		infd = p1, outfd = p2;
	    char		agent_type[8];

	    if (infd == -1 && outfd == -1)
		pmstrncpy(agent_type, sizeof(agent_type), "DSO");
	    else
		pmstrncpy(agent_type, sizeof(agent_type), "daemon");
	    USDT_WITH_SEMA(pmcd, del_agent, who, agent_type, infd, outfd);
	}
	break;

    case TR_EOF:
	if (USDT_IS_ACTIVE(pmcd, early_eof)) {
	    const int		fd = who;
	    char		pdu_type[32];

	    __pmPDUTypeStr_r(p1, pdu_type, sizeof(pdu_type));
	    USDT_WITH_SEMA(pmcd, early_eof, fd, pdu_type);
	}
	break;

    case TR_WRONG_PDU:
	if (USDT_IS_ACTIVE(pmcd, wrong_pdu)) {
	    const int		fd = who;
	    char		wantbuf[64];
	    char		recvbuf[64];
	    char		recvtype[4];

	    if (p1 < 0)
		pmstrncpy(wantbuf, sizeof(wantbuf), "NO");
	    else
		__pmPDUTypeStr_r(p1, wantbuf, sizeof(wantbuf));
	    if (p2 > 0) {
		pmstrncpy(recvtype, sizeof(recvtype), "PDU");
		__pmPDUTypeStr_r(p2, recvbuf, sizeof(recvbuf));
	    } else if (p2 == 0) {
		pmstrncpy(recvtype, sizeof(recvtype), "EOF");
		recvbuf[0] = '\0';
	    } else {
		pmstrncpy(recvtype, sizeof(recvtype), "ERR");
		pmErrStr_r(p2, recvbuf, sizeof(recvbuf));
	    }
	    USDT_WITH_SEMA(pmcd, wrong_pdu, fd, wantbuf, recvtype, recvbuf);
	}
	break;

    case TR_XMIT_ERR:
	if (USDT_IS_ACTIVE(pmcd, xmit_err)) {
	    const int		fd = who, err = p2;
	    char		errorstr[64];
	    char		pdu_type[32];

	    __pmPDUTypeStr_r(p1, pdu_type, sizeof(pdu_type));
	    pmErrStr_r(err, errorstr, sizeof(errorstr));
	    USDT_WITH_SEMA(pmcd, xmit_err, fd, pdu_type, err, errorstr);
	}
	break;

    case TR_RECV_TIMEOUT:
	if (USDT_IS_ACTIVE(pmcd, recv_timeout)) {
	    const int		fd = who;
	    char		pdu_type[32];

	    __pmPDUTypeStr_r(p1, pdu_type, sizeof(pdu_type));
	    USDT_WITH_SEMA(pmcd, recv_timeout, fd, pdu_type);
	}
	break;

    case TR_RECV_ERR:
	if (USDT_IS_ACTIVE(pmcd, recv_err)) {
	    const int		fd = who, err = p2;
	    char		errorstr[64];
	    char		pdu_type[32];

	    __pmPDUTypeStr_r(p1, pdu_type, sizeof(pdu_type));
	    pmErrStr_r(err, errorstr, sizeof(errorstr));
	    USDT_WITH_SEMA(pmcd, recv_err, fd, pdu_type, err, errorstr);
	}
	break;

    case TR_XMIT_PDU:
	if (USDT_IS_ACTIVE(pmcd, xmit_pdu)) {
	    const int		fd = who;
	    char		pdu_info[64];
	    char		pdu_type[32];
	    char		id_str[20];

	    __pmPDUTypeStr_r(p1, pdu_type, sizeof(pdu_type));
	    if (p1 == PDU_ERROR)
		pmErrStr_r(p2, pdu_info, sizeof(pdu_info));
	    else if (p1 == PDU_TEXT_REQ || p1 == PDU_TEXT)
		pmsprintf(pdu_info, sizeof(pdu_info), "id=0x%x", p2);
	    else if (p1 == PDU_DESC_REQ || p1 == PDU_DESC)
		pmsprintf(pdu_info, sizeof(pdu_info), "pmid=%s",
			  pmIDStr_r((pmID)p2, id_str, sizeof(id_str)));
	    else if (p1 == PDU_INSTANCE_REQ || p1 == PDU_INSTANCE)
		pmsprintf(pdu_info, sizeof(pdu_info), "indom=%s",
			  pmInDomStr_r((pmInDom)p2, id_str, sizeof(id_str)));
	    else if (p1 == PDU_PMNS_NAMES || p1 == PDU_PMNS_IDS ||
		     p1 == PDU_RESULT || p1 == PDU_DESCS)
		pmsprintf(pdu_info, sizeof(pdu_info), "numpmid=%d", p2);
	    else if (p1 == PDU_CREDS)
		pmsprintf(pdu_info, sizeof(pdu_info), "numcred=%d", p2);
	    else
		pmsprintf(pdu_info, sizeof(pdu_info), "unknown=%d", p2);
	    USDT_WITH_SEMA(pmcd, xmit_pdu, fd, pdu_type, p2, pdu_info);
	}
	break;

    case TR_RECV_PDU:
	if (USDT_IS_ACTIVE(pmcd, recv_pdu)) {
	    const int		fd = who;
	    char		pdu_type[32];

	    __pmPDUTypeStr_r(p1, pdu_type, sizeof(pdu_type));
	    USDT_WITH_SEMA(pmcd, recv_pdu, fd, pdu_type);
	}
	break;
    }
}
#endif

void
pmcd_trace(int type, int who, int p1, int p2)
{
    int		p;

    pmcd_usdt_trace(type, who, p1, p2);

    switch (type) {
	case TR_XMIT_PDU:
	case TR_RECV_PDU:
	    if ((pmcd_trace_mask & TR_MASK_PDU) == 0)
		return;
	    break;
	default:
	    if ((pmcd_trace_mask & TR_MASK_CONN) == 0)
		return;
	    break;
    }

    if (trace == NULL) {
	pmcd_init_trace(pmcd_trace_nbufs);
	if (trace == NULL)
	    return;
    }

    p = (next++) % pmcd_trace_nbufs;

    time(&trace[p].t_stamp);
    trace[p].t_type = type;
    trace[p].t_who = who;
    trace[p].t_p1 = p1;
    trace[p].t_p2 = p2;

    if (pmcd_trace_mask & TR_MASK_NOBUF)
	/* unbuffered, dump it now */
	pmcd_dump_trace(stderr);
}

void
pmcd_dump_trace(FILE *f)
{
    int			i;
    int			p;
    struct tm		last = { 0, 0 };
    struct tm		*this;
    char		strbuf[20];

    if ((pmcd_trace_mask & TR_MASK_NOBUF) == 0)
	fprintf(f, "\n->PMCD event trace: ");
    if (trace != NULL && next != 0) {
	if (next < pmcd_trace_nbufs)
	    i = 0;
	else
	    i = next - pmcd_trace_nbufs;
	if ((pmcd_trace_mask & TR_MASK_NOBUF) == 0) {
	    fprintf(f, "starting at %s", ctime(&trace[i % pmcd_trace_nbufs].t_stamp));
	    last.tm_hour = -1;
	}
	else
	    last.tm_hour = -2;

	for ( ; i < next; i++) {
	    fprintf(f, "->");
	    p = i % pmcd_trace_nbufs;
	    this = localtime(&trace[p].t_stamp);
	    if (this->tm_hour != last.tm_hour ||
		this->tm_min != last.tm_min ||
		this->tm_sec != last.tm_sec) {
		if (last.tm_hour == -1)
		    fprintf(f, "         ");
		else
		    fprintf(f, "%02d:%02d:%02d ", this->tm_hour, this->tm_min, this->tm_sec);
		last = *this;	/* struct assignment */
	    }
	    else
		fprintf(f, "         ");

	    switch (trace[p].t_type) {

		case TR_ADD_CLIENT:
		    {
			ClientInfo	*cip;

			fprintf(f, "New client: [%d] ", trace[p].t_who);
			cip = GetClient(trace[p].t_who);
			if (cip == NULL) {
			    fprintf(f, "-- unknown?\n");
			}
			else {
			    __pmSockAddr	*saddr = (__pmSockAddr *)cip->addr;
			    char		*addrbuf;

			    addrbuf = __pmSockAddrToString(saddr);
			    if (addrbuf == NULL)
				fprintf(f, "invalid socket address");
			    else {
				fprintf(f, "addr=%s", addrbuf);
				free(addrbuf);
			    }
			    fprintf(f, ", fd=%d, seq=%u\n", cip->fd, cip->seq);
			}
		    }
		    break;

		case TR_DEL_CLIENT:
		    fprintf(f, "End client: [%d] fd=%d", trace[p].t_who, trace[p].t_p1);
		    if (trace[p].t_p2 != 0)
			fprintf(f, ", err=%d: %s", trace[p].t_p2, pmErrStr(trace[p].t_p2));
		    fputc('\n', f);
		    break;

		case TR_ADD_AGENT:
		    fprintf(f, "Add PMDA: domain=%d, ", trace[p].t_who);
		    if (trace[p].t_p1 == -1 && trace[p].t_p2 == -1)
			fprintf(f, "DSO\n");
		    else
			fprintf(f, "infd=%d, outfd=%d\n", trace[p].t_p1, trace[p].t_p2);
		    break;

		case TR_DEL_AGENT:
		    fprintf(f, "Drop PMDA: domain=%d, ", trace[p].t_who);
		    if (trace[p].t_p1 == -1 && trace[p].t_p2 == -1)
			fprintf(f, "DSO\n");
		    else
			fprintf(f, "infd=%d, outfd=%d\n", trace[p].t_p1, trace[p].t_p2);
		    break;

		case TR_EOF:
		    fprintf(f, "Premature EOF: expecting %s PDU, fd=%d\n",
			trace[p].t_p1 == -1 ? "NO" : __pmPDUTypeStr_r(trace[p].t_p1, strbuf, sizeof(strbuf)),
			trace[p].t_who);
		    break;

		case TR_WRONG_PDU:
		    if (trace[p].t_p2 > 0) {
			fprintf(f, "Wrong PDU type: expecting %s PDU, fd=%d, got %s PDU\n",
			    trace[p].t_p1 == -1 ? "NO" : __pmPDUTypeStr_r(trace[p].t_p1, strbuf, sizeof(strbuf)),
			    trace[p].t_who,
			    __pmPDUTypeStr_r(trace[p].t_p2, strbuf, sizeof(strbuf)));
			__pmDumpPDUTrace(f);
		    }
		    else if (trace[p].t_p2 == 0) {
			fprintf(f, "Wrong PDU type: expecting %s PDU, fd=%d, got EOF\n",
			    trace[p].t_p1 == -1 ? "NO" : __pmPDUTypeStr_r(trace[p].t_p1, strbuf, sizeof(strbuf)),
			    trace[p].t_who);
		    }
		    else {
			fprintf(f, "Wrong PDU type: expecting %s PDU, fd=%d, got err=%d: %s\n",
			    trace[p].t_p1 == -1 ? "NO" : __pmPDUTypeStr_r(trace[p].t_p1, strbuf, sizeof(strbuf)),
			    trace[p].t_who,
			    trace[p].t_p2, pmErrStr(trace[p].t_p2));

		    }
		    break;

		case TR_XMIT_ERR:
		    fprintf(f, "Send %s PDU failed: fd=%d, err=%d: %s\n",
			__pmPDUTypeStr_r(trace[p].t_p1, strbuf, sizeof(strbuf)), trace[p].t_who,
			trace[p].t_p2, pmErrStr(trace[p].t_p2));
		    break;

		case TR_RECV_TIMEOUT:
		    fprintf(f, "Recv timeout: expecting %s PDU, fd=%d\n",
			__pmPDUTypeStr_r(trace[p].t_p1, strbuf, sizeof(strbuf)), trace[p].t_who);
		    break;

		case TR_RECV_ERR:
		    fprintf(f, "Recv error: expecting %s PDU, fd=%d, err=%d: %s\n",
			__pmPDUTypeStr_r(trace[p].t_p1, strbuf, sizeof(strbuf)), trace[p].t_who,
			trace[p].t_p2, pmErrStr(trace[p].t_p2));
		    break;

		case TR_XMIT_PDU:
		    fprintf(f, "Xmit: %s PDU, fd=%d",
			__pmPDUTypeStr_r(trace[p].t_p1, strbuf, sizeof(strbuf)), trace[p].t_who);
		    if (trace[p].t_p1 == PDU_ERROR)
			fprintf(f, ", err=%d: %s",
			    trace[p].t_p2, pmErrStr(trace[p].t_p2));
		    else if (trace[p].t_p1 == PDU_RESULT)
			fprintf(f, ", numpmid=%d", trace[p].t_p2);
		    else if (trace[p].t_p1 == PDU_TEXT_REQ ||
			     trace[p].t_p1 == PDU_TEXT)
			fprintf(f, ", id=0x%x", trace[p].t_p2);
		    else if (trace[p].t_p1 == PDU_DESC_REQ ||
			     trace[p].t_p1 == PDU_DESC)
			fprintf(f, ", pmid=%s", pmIDStr_r((pmID)trace[p].t_p2, strbuf, sizeof(strbuf)));
		    else if (trace[p].t_p1 == PDU_INSTANCE_REQ ||
			     trace[p].t_p1 == PDU_INSTANCE)
			fprintf(f, ", indom=%s", pmInDomStr_r((pmInDom)trace[p].t_p2, strbuf, sizeof(strbuf)));
		    else if (trace[p].t_p1 == PDU_PMNS_NAMES)
			fprintf(f, ", numpmid=%d", trace[p].t_p2);
		    else if (trace[p].t_p1 == PDU_PMNS_IDS)
			fprintf(f, ", numpmid=%d", trace[p].t_p2);
		    else if (trace[p].t_p1 == PDU_CREDS)
			fprintf(f, ", numcreds=%d", trace[p].t_p2);
		    fputc('\n', f);
		    break;

		case TR_RECV_PDU:
		    fprintf(f, "Recv: %s PDU, fd=%d, pdubuf=0x",
			__pmPDUTypeStr_r(trace[p].t_p1, strbuf, sizeof(strbuf)), trace[p].t_who);
		    /* This will only work if sizeof (int) == sizeof (ptr).
		     * On MIPS int is always 32 bits regardless the ABI,
		     * and on Linux we're checking in configure if an int is
		     * anything else but 32 bits, so if pointer is not 
		     * 32 bit, then .... */
#ifndef HAVE_32BIT_PTR
		    fprintf(f, "...");
#endif
		    fprintf(f, "%x\n", trace[p].t_p2);
		    break;

		default:
		    fprintf(f, "Type=%d who=%d p1=%d p2=%d\n",
			trace[p].t_type, trace[p].t_who, trace[p].t_p1,
			trace[p].t_p2);
		    break;
	    }
	}
    }
    else
	fprintf(f, "<empty>\n");

    if ((pmcd_trace_mask & TR_MASK_NOBUF) == 0)
	fputc('\n', f);
    next = 0;			/* empty the circular buffer */
}
