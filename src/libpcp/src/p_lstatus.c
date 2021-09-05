/*
 * Copyright (c) 2012-2013 Red Hat.
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * Thread-safe note
 *
 * Unlike most of the other __pmSend*() routines, there is no wrapper
 * routine in libpcp for __pmSendLogStatus() so there is no place in
 * the library to enforce serialization between the receiving the
 * LOG_REQUEST_STATUS PDU and calling __pmSendLogStatus().
 *
 * It is assumed that the caller of __pmSendLogStatus() either manages
 * this serialization or is single-threaded, which is true for
 * the only current user of this routine, pmlogger(1).
 */

#include <ctype.h>
#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"

/*
 * PDU for logger status information transfer (PDU_LOG_STATUS)
 * - Version 2
 */
typedef struct {
    __pmPDUHdr		hdr;
    __int32_t		pad;	/* force status to be double word aligned */
    __int32_t		start_sec;
    __int32_t		start_usec;
    __int32_t		last_sec;
    __int32_t		last_usec;
    __int32_t		now_sec;
    __int32_t		now_usec;
    __int32_t		state;
    __int32_t		vol;
    __int64_t		size;
    char		hostname[PM_LOG_MAXHOSTLEN];
    char		fqdn[PM_LOG_MAXHOSTLEN];
    char		pmcd_tz[PM_TZ_MAXLEN];
    char		pmlogger_tz[PM_TZ_MAXLEN];
} logstatus_v2;

int
__pmSendLogStatus(int fd, __pmLoggerStatus *status)
{
    int		sts;
    int		version = __pmVersionIPC(fd);

    if (pmDebugOptions.pdu) {
	fprintf(stderr, "__pmSendLogStatus: sending PDU (version=%d)\n",
		version == UNKNOWN_VERSION ? LOG_PDU_VERSION : version);
    }

    if (version == LOG_PDU_VERSION3) {
	fprintf(stderr, "__pmSendLogStatus TODO LOG_PDU_VERSION3\n");
	sts = 0;
    }
    else if (version == LOG_PDU_VERSION2) {
	logstatus_v2	*pp;

	if ((pp = (logstatus_v2 *)__pmFindPDUBuf(sizeof(logstatus_v2))) == NULL)
	    return -oserror();
	pp->hdr.len = sizeof(logstatus_v2);
	pp->hdr.type = PDU_LOG_STATUS;
	pp->hdr.from = FROM_ANON;	/* context does not matter here */
	memset(&pp->pad, '~', sizeof(pp->pad));  /* initialize padding */

	/* Conditional convertion from host to network byteorder HAVE to be
	 * unconditional if one cares about endianess compatibiltity at all!
	 */
	__pmPutTimeval(&status->start, &pp->start_sec);
	__pmPutTimeval(&status->last, &pp->last_sec);
	__pmPutTimeval(&status->now, &pp->now_sec);
	pp->state = htonl(status->state);
	pp->vol = htonl(status->vol);
	pp->size = status->size;
	__htonll((char *)&pp->size);
	memset(pp->hostname, 0, sizeof(pp->hostname));
	strncpy(pp->hostname, status->pmcd.hostname, PM_LOG_MAXHOSTLEN-1);
	memset(pp->fqdn, 0, sizeof(pp->fqdn));
	strncpy(pp->fqdn, status->pmcd.fqdn, PM_LOG_MAXHOSTLEN-1);
	memset(pp->pmcd_tz, 0, sizeof(pp->pmcd_tz));
	strncpy(pp->pmcd_tz, status->pmcd.timezone, PM_TZ_MAXLEN-1);
	memset(pp->pmlogger_tz, 0, sizeof(pp->pmlogger_tz));
	strncpy(pp->pmlogger_tz, status->pmlogger.timezone, PM_TZ_MAXLEN-1);

	sts = __pmXmitPDU(fd, (__pmPDU *)pp);
	__pmUnpinPDUBuf(pp);
    }
    else {
	sts = PM_ERR_IPC;
    }

    return sts;
}

int
__pmDecodeLogStatus(__pmPDU *pdubuf, __pmLoggerStatus **result)
{
    int			version = __pmLastVersionIPC();
    __pmLoggerStatus	*lsp;
    int			sts;

    if (pmDebugOptions.pdu) {
	fprintf(stderr, "__pmDecodeLogStatus: got PDU (version=%d)\n",
		version == UNKNOWN_VERSION ? LOG_PDU_VERSION : version);
    }

    lsp = (__pmLoggerStatus *)malloc(sizeof(__pmLoggerStatus));
    if (lsp == NULL) {
	sts = -oserror();
	pmNoMem("__pmDecodeLogStatus: struct", sizeof(__pmLoggerStatus), PM_RECOV_ERR);
	return sts;
    }
    memset(lsp, 0, sizeof(__pmLoggerStatus));

    if (version == LOG_PDU_VERSION3) {
	fprintf(stderr, "__pmDecodeLogStatus TODO LOG_PDU_VERSION3\n");
    }
    else if (version == LOG_PDU_VERSION2) {
	logstatus_v2	*pp = (logstatus_v2 *)pdubuf;
	char		*pduend;

	pduend = (char *)pdubuf + pp->hdr.len;

	if ((pduend - (char*)pp) != sizeof(logstatus_v2)) {
	    free(lsp);
	    return PM_ERR_IPC;
	}

	/* Conditional convertion from host to network byteorder HAVE to be
	 * unconditional if one cares about endianess compatibiltity at all!
	 */
	__pmLoadTimeval(&pp->start_sec, &lsp->start);
	__pmLoadTimeval(&pp->last_sec, &lsp->last);
	__pmLoadTimeval(&pp->now_sec, &lsp->now);
	lsp->state = ntohl(pp->state);
	lsp->vol = ntohl(pp->vol);
	lsp->size = pp->size;
	__ntohll((char *)&lsp->size);
	if ((lsp->pmcd.hostname = strdup(pp->hostname)) == NULL) {
	    sts = -oserror();
	    pmNoMem("__pmDecodeLogStatus: hostname", strlen(pp->hostname), PM_RECOV_ERR);
	    __pmFreeLogStatus(lsp, 1);
	    return sts;
	}
	if ((lsp->pmcd.fqdn = strdup(pp->fqdn)) == NULL) {
	    sts = -oserror();
	    pmNoMem("__pmDecodeLogStatus: fqdn", strlen(pp->fqdn), PM_RECOV_ERR);
	    __pmFreeLogStatus(lsp, 1);
	    return sts;
	}
	if ((lsp->pmcd.timezone = strdup(pp->pmcd_tz)) == NULL) {
	    sts = -oserror();
	    pmNoMem("__pmDecodeLogStatus: pmcd_tz", strlen(pp->pmcd_tz), PM_RECOV_ERR);
	    __pmFreeLogStatus(lsp, 1);
	    return sts;
	}
	if ((lsp->pmlogger.timezone = strdup(pp->pmlogger_tz)) == NULL) {
	    sts = -oserror();
	    pmNoMem("__pmDecodeLogStatus: pmlogger_tz", strlen(pp->pmlogger_tz), PM_RECOV_ERR);
	    __pmFreeLogStatus(lsp, 1);
	    return sts;
	}
    }

    *result = lsp;
    return 0;
}

/*
 * Free the strdup'd strings hanging off a __pmLoggerStatus struct.
 * If freestruct == 1, then free the struct as well.
 */
void
__pmFreeLogStatus(__pmLoggerStatus *lsp, int freestruct)
{
    if (lsp->pmcd.hostname != NULL)
	free(lsp->pmcd.hostname);
    if (lsp->pmcd.fqdn != NULL)
	free(lsp->pmcd.fqdn);
    if (lsp->pmcd.timezone != NULL)
	free(lsp->pmcd.timezone);
    if (lsp->pmcd.zoneinfo != NULL)
	free(lsp->pmcd.zoneinfo);
    if (lsp->pmlogger.timezone != NULL)
	free(lsp->pmlogger.timezone);
    if (lsp->pmlogger.zoneinfo != NULL)
	free(lsp->pmlogger.zoneinfo);

    if (freestruct)
	free(lsp);
}
