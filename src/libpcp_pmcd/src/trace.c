/*
 * Copyright (c) 2013,2015 Red Hat.
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
#include "probes.h"
#else
#define PCP_PROBE_PMCD_PDU(type,who,p1,p2)
#define PCP_PROBE_PMCD(type,who,p1,p2)
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
 * by default, circular buffer last 20 events -- change by modify
 * pmcd.control.tracebufs
 * by default, tracing is disabled -- change by setting the following
 * to 1, pmcd.control.traceconn (trace connections) and/or
 * pmcd.control.tracepdu (trace PDUs)
 */
PMCD_DATA int		_pmcd_trace_nbufs = 20;
PMCD_DATA int		_pmcd_trace_mask;

void
pmcd_init_trace(int n)
{
    if (trace != NULL)
	free(trace);
    if ((trace = (tracebuf *)malloc(n * sizeof(tracebuf))) == NULL) {
	 __pmNoMem("pmcd_init_trace", n * sizeof(tracebuf), PM_RECOV_ERR);
	 return;
    }
    _pmcd_trace_nbufs = n;
    next = 0;
}

void
pmcd_trace(int type, int who, int p1, int p2)
{
    int		p;

    switch (type) {
	case TR_XMIT_PDU:
	case TR_RECV_PDU:
	    PCP_PROBE_PMCD_PDU(type, who, p1, p2);
	    if ((_pmcd_trace_mask & TR_MASK_PDU) == 0)
		return;
	    break;
	default:
	    PCP_PROBE_PMCD(type, who, p1, p2);
	    if ((_pmcd_trace_mask & TR_MASK_CONN) == 0)
		return;
	    break;
    }

    if (trace == NULL) {
	pmcd_init_trace(_pmcd_trace_nbufs);
	if (trace == NULL)
	    return;
    }

    p = (next++) % _pmcd_trace_nbufs;

    time(&trace[p].t_stamp);
    trace[p].t_type = type;
    trace[p].t_who = who;
    trace[p].t_p1 = p1;
    trace[p].t_p2 = p2;

    if (_pmcd_trace_mask & TR_MASK_NOBUF)
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

    if ((_pmcd_trace_mask & TR_MASK_NOBUF) == 0)
	fprintf(f, "\n->PMCD event trace: ");
    if (trace != NULL && next != 0) {
	if (next < _pmcd_trace_nbufs)
	    i = 0;
	else
	    i = next - _pmcd_trace_nbufs;
	if ((_pmcd_trace_mask & TR_MASK_NOBUF) == 0) {
	    fprintf(f, "starting at %s", ctime(&trace[i % _pmcd_trace_nbufs].t_stamp));
	    last.tm_hour = -1;
	}
	else
	    last.tm_hour = -2;

	for ( ; i < next; i++) {
	    fprintf(f, "->");
	    p = i % _pmcd_trace_nbufs;
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
		    fprintf(f, "End client: fd=%d", trace[p].t_who);
		    if (trace[p].t_p1 != 0)
			fprintf(f, ", err=%d: %s", trace[p].t_p1, pmErrStr(trace[p].t_p1));
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
			    trace[p].t_p2 == -1 ? "NO" : __pmPDUTypeStr_r(trace[p].t_p2, strbuf, sizeof(strbuf)));
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

    if ((_pmcd_trace_mask & TR_MASK_NOBUF) == 0)
	fputc('\n', f);
    next = 0;			/* empty the circular buffer */
}
