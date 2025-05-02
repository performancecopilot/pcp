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
    __int32_t		pad;		/* backwards compatibility */
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

/*
 * PDU for logger status information transfer (PDU_LOG_STATUS)
 * - Version 3
 */
typedef struct {
    __pmPDUHdr		hdr;
    __int32_t		start_sec[2];
    __int32_t		start_usec;
    __int32_t		last_sec[2];
    __int32_t		last_usec;
    __int32_t		now_sec[2];
    __int32_t		now_usec;
    __int32_t		state;
    __int32_t		vol;
    __int32_t		size[2];
    __int32_t		pmcd_hostname_len;
    __int32_t		pmcd_fqdn_len;
    __int32_t		pmcd_timezone_len;
    __int32_t		pmcd_zoneinfo_len;
    __int32_t		pmlogger_timezone_len;
    __int32_t		pmlogger_zoneinfo_len;
    __int32_t		data[0];
} logstatus_v3;

int
__pmSendLogStatus(int fd, __pmLoggerStatus *status)
{
    int		sts;
    int		version = __pmVersionIPC(fd);

    if (pmDebugOptions.pmlc)
	fprintf(stderr, "__pmSendLogStatus: sending PDU (version=%d)\n", version);

    if (version == LOG_PDU_VERSION3) {
	logstatus_v3	*pp;
	__int64_t	size;
	int		strings;
	int		pad;
	int		len;
	char		*p;

	strings =  0;
	if (status->pmcd.hostname != NULL)
	    strings += strlen(status->pmcd.hostname)+1;
	if (status->pmcd.fqdn != NULL)
	    strings += strlen(status->pmcd.fqdn)+1;
	if (status->pmcd.timezone != NULL)
	    strings += strlen(status->pmcd.timezone)+1;
	if (status->pmcd.zoneinfo != NULL)
	    strings += strlen(status->pmcd.zoneinfo)+1;
	if (status->pmlogger.timezone != NULL)
	    strings += strlen(status->pmlogger.timezone)+1;
	if (status->pmlogger.zoneinfo != NULL)
	    strings += strlen(status->pmlogger.zoneinfo)+1;
	if ((strings % sizeof(__int32_t)) == 0)
	    pad = 0;
	else
	    pad = sizeof(__int32_t) - (strings % sizeof(__int32_t));

	if ((pp = (logstatus_v3 *)__pmFindPDUBuf(sizeof(logstatus_v3)+strings+pad)) == NULL)
	    return -oserror();
	p = (char *)&pp->data[0];
	pp->hdr.len = sizeof(logstatus_v3) + pad;
	pp->hdr.type = PDU_LOG_STATUS;
	pp->hdr.from = FROM_ANON;	/* context does not matter here */

	__pmPutTimestamp(&status->start, &pp->start_sec[0]);
	__pmPutTimestamp(&status->last, &pp->last_sec[0]);
	__pmPutTimestamp(&status->now, &pp->now_sec[0]);
	pp->state = htonl(status->state);
	pp->vol = htonl(status->vol);
	size = status->size;
	__htonll((char *)&size);
	memcpy(&pp->size[0], &size, sizeof(__int64_t));
	if (status->pmcd.hostname != NULL) {
	    len = strlen(status->pmcd.hostname)+1;
	    strcpy(p, status->pmcd.hostname);
	    p += len;
	    pp->hdr.len +=  len;
	}
	else
	    len = 0;
	pp->pmcd_hostname_len = htonl(len);
	if (status->pmcd.fqdn != NULL) {
	    len = strlen(status->pmcd.fqdn)+1;
	    strcpy(p, status->pmcd.fqdn);
	    p += len;
	    pp->hdr.len +=  len;
	}
	else
	    len = 0;
	pp->pmcd_fqdn_len = htonl(len);
	if (status->pmcd.timezone != NULL) {
	    len = strlen(status->pmcd.timezone)+1;
	    strcpy(p, status->pmcd.timezone);
	    p += len;
	    pp->hdr.len +=  len;
	}
	else
	    len = 0;
	pp->pmcd_timezone_len = htonl(len);
	if (status->pmcd.zoneinfo != NULL) {
	    len = strlen(status->pmcd.zoneinfo)+1;
	    strcpy(p, status->pmcd.zoneinfo);
	    p += len;
	    pp->hdr.len +=  len;
	}
	else
	    len = 0;
	pp->pmcd_zoneinfo_len = htonl(len);
	if (status->pmlogger.timezone != NULL) {
	    len = strlen(status->pmlogger.timezone)+1;
	    strcpy(p, status->pmlogger.timezone);
	    p += len;
	    pp->hdr.len +=  len;
	}
	else
	    len = 0;
	pp->pmlogger_timezone_len = htonl(len);
	if (status->pmlogger.zoneinfo != NULL) {
	    len = strlen(status->pmlogger.zoneinfo)+1;
	    strcpy(p, status->pmlogger.zoneinfo);
	    p += len;
	    pp->hdr.len +=  len;
	}
	else
	    len = 0;
	pp->pmlogger_zoneinfo_len = htonl(len);

	while (pad--) {
	    *p++ = '~';
	}

	sts = __pmXmitPDU(fd, (__pmPDU *)pp);
	__pmUnpinPDUBuf(pp);
    }
    else if (version == LOG_PDU_VERSION2) {
	logstatus_v2	*pp;

	if ((pp = (logstatus_v2 *)__pmFindPDUBuf(sizeof(logstatus_v2))) == NULL)
	    return -oserror();
	pp->hdr.len = sizeof(logstatus_v2);
	pp->hdr.type = PDU_LOG_STATUS_V2;
	pp->hdr.from = FROM_ANON;	/* context does not matter here */
	memset(&pp->pad, '~', sizeof(pp->pad));  /* initialize padding */

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
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmSendLogStatus: PM_ERR_IPC: bad version %d\n",
		version);
	}
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

    if (pmDebugOptions.pmlc)
	fprintf(stderr, "__pmDecodeLogStatus: got PDU (version=%d)\n", version);

    lsp = (__pmLoggerStatus *)malloc(sizeof(__pmLoggerStatus));
    if (lsp == NULL) {
	sts = -oserror();
	pmNoMem("__pmDecodeLogStatus: struct", sizeof(__pmLoggerStatus), PM_RECOV_ERR);
	return sts;
    }
    memset(lsp, 0, sizeof(__pmLoggerStatus));

    if (version == LOG_PDU_VERSION3) {
	logstatus_v3	*pp = (logstatus_v3 *)pdubuf;
	char		*pduend;
	char		*p;
	int		len;

	pduend = (char *)pdubuf + pp->hdr.len;

	__pmLoadTimestamp(&pp->start_sec[0], &lsp->start);
	__pmLoadTimestamp(&pp->last_sec[0], &lsp->last);
	__pmLoadTimestamp(&pp->now_sec[0], &lsp->now);
	lsp->state = ntohl(pp->state);
	lsp->vol = ntohl(pp->vol);
	memcpy(&lsp->size, &pp->size[0], sizeof(__int64_t));
	__ntohll((char *)&lsp->size);
	p = (char *)&pp->data[0];
	len = ntohl(pp->pmcd_hostname_len);
	if (len == 0)
	    lsp->pmcd.hostname = NULL;
	else {
	    if (len > PM_MAX_HOSTNAMELEN) {
		/* cannot be longer than hostname in archive label */
		if (pmDebugOptions.pmlc || pmDebugOptions.pdu) 
		    fprintf(stderr, "__pmDecodeLogStatus: PM_ERR_IPC: pmcd.hostname too long (%d)\n", len);
		__pmFreeLogStatus(lsp, 1);
		return PM_ERR_IPC;
	    }
	    if ((lsp->pmcd.hostname = strdup(p)) == NULL) {
		sts = -oserror();
		pmNoMem("__pmDecodeLogStatus: pmcd.hostname", len, PM_RECOV_ERR);
		__pmFreeLogStatus(lsp, 1);
		return sts;
	    }
	    p += len;
	    if (p > pduend) {
		if (pmDebugOptions.pmlc || pmDebugOptions.pdu) 
		    fprintf(stderr, "__pmDecodeLogStatus: PM_ERR_IPC: pmcd.hostname data[%ld] > PDU len (%d)\n", 
			(long)(p - (char *)&pp->data[0]), pp->hdr.len);
		__pmFreeLogStatus(lsp, 1);
		return PM_ERR_IPC;
	    }
	}
	len = ntohl(pp->pmcd_fqdn_len);
	if (len == 0)
	    lsp->pmcd.fqdn = NULL;
	else {
	    if (len > PM_MAX_HOSTNAMELEN) {
		/* cannot be longer than hostname in archive label */
		if (pmDebugOptions.pmlc || pmDebugOptions.pdu) 
		    fprintf(stderr, "__pmDecodeLogStatus: PM_ERR_IPC: pmcd.fqdn too long (%d)\n", len);
		__pmFreeLogStatus(lsp, 1);
		return PM_ERR_IPC;
	    }
	    if ((lsp->pmcd.fqdn = strdup(p)) == NULL) {
		sts = -oserror();
		pmNoMem("__pmDecodeLogStatus: pmcd.fqdn", len, PM_RECOV_ERR);
		__pmFreeLogStatus(lsp, 1);
		return sts;
	    }
	    p += len;
	    if (p > pduend) {
		if (pmDebugOptions.pmlc || pmDebugOptions.pdu) 
		    fprintf(stderr, "__pmDecodeLogStatus: PM_ERR_IPC: pmcd.fqdn data[%ld] > PDU len (%d)\n", 
			(long)(p - (char *)&pp->data[0]), pp->hdr.len);
		__pmFreeLogStatus(lsp, 1);
		return PM_ERR_IPC;
	    }
	}
	len = ntohl(pp->pmcd_timezone_len);
	if (len == 0)
	    lsp->pmcd.timezone = NULL;
	else {
	    if (len > PM_MAX_TIMEZONELEN) {
		/* cannot be longer than timezone in archive label */
		if (pmDebugOptions.pmlc || pmDebugOptions.pdu) 
		    fprintf(stderr, "__pmDecodeLogStatus: PM_ERR_IPC: pmcd.timezone too long (%d)\n", len);
		__pmFreeLogStatus(lsp, 1);
		return PM_ERR_IPC;
	    }
	    if ((lsp->pmcd.timezone = strdup(p)) == NULL) {
		sts = -oserror();
		pmNoMem("__pmDecodeLogStatus: pmcd.timezone", len, PM_RECOV_ERR);
		__pmFreeLogStatus(lsp, 1);
		return sts;
	    }
	    p += len;
	    if (p > pduend) {
		if (pmDebugOptions.pmlc || pmDebugOptions.pdu) 
		    fprintf(stderr, "__pmDecodeLogStatus: PM_ERR_IPC: pmcd.timezone data[%ld] > PDU len (%d)\n", 
			(long)(p - (char *)&pp->data[0]), pp->hdr.len);
		__pmFreeLogStatus(lsp, 1);
		return PM_ERR_IPC;
	    }
	}
	len = ntohl(pp->pmcd_zoneinfo_len);
	if (len == 0)
	    lsp->pmcd.zoneinfo = NULL;
	else {
	    if (len > PM_MAX_ZONEINFOLEN) {
		/* cannot be longer than zoneinfo in archive label */
		if (pmDebugOptions.pmlc || pmDebugOptions.pdu) 
		    fprintf(stderr, "__pmDecodeLogStatus: PM_ERR_IPC: pmcd.zoneinfo too long (%d)\n", len);
		__pmFreeLogStatus(lsp, 1);
		return PM_ERR_IPC;
	    }
	    if ((lsp->pmcd.zoneinfo = strdup(p)) == NULL) {
		sts = -oserror();
		pmNoMem("__pmDecodeLogStatus: pmcd.zoneinfo", len, PM_RECOV_ERR);
		__pmFreeLogStatus(lsp, 1);
		return sts;
	    }
	    p += len;
	    if (p > pduend) {
		if (pmDebugOptions.pmlc || pmDebugOptions.pdu) 
		    fprintf(stderr, "__pmDecodeLogStatus: PM_ERR_IPC: pmcd.zoneinfo data[%ld] > PDU len (%d)\n", 
			(long)(p - (char *)&pp->data[0]), pp->hdr.len);
		__pmFreeLogStatus(lsp, 1);
		return PM_ERR_IPC;
	    }
	}
	len = ntohl(pp->pmlogger_timezone_len);
	if (len == 0)
	    lsp->pmlogger.timezone = NULL;
	else {
	    if (len > PM_MAX_TIMEZONELEN) {
		/* cannot be longer than timezone in archive label */
		if (pmDebugOptions.pmlc || pmDebugOptions.pdu) 
		    fprintf(stderr, "__pmDecodeLogStatusPM_ERR_IPC: : pmlogger.timezone too long (%d)\n", len);
		__pmFreeLogStatus(lsp, 1);
		return PM_ERR_IPC;
	    }
	    if ((lsp->pmlogger.timezone = strdup(p)) == NULL) {
		sts = -oserror();
		pmNoMem("__pmDecodeLogStatus: pmlogger.timezone", len, PM_RECOV_ERR);
		__pmFreeLogStatus(lsp, 1);
		return sts;
	    }
	    p += len;
	    if (p > pduend) {
		if (pmDebugOptions.pmlc || pmDebugOptions.pdu) 
		    fprintf(stderr, "__pmDecodeLogStatus: PM_ERR_IPC: pmlogger.timezone data[%ld] > PDU len (%d)\n", 
			(long)(p - (char *)&pp->data[0]), pp->hdr.len);
		__pmFreeLogStatus(lsp, 1);
		return PM_ERR_IPC;
	    }
	}
	len = ntohl(pp->pmlogger_zoneinfo_len);
	if (len == 0)
	    lsp->pmlogger.zoneinfo = NULL;
	else {
	    if (len > PM_MAX_ZONEINFOLEN) {
		/* cannot be longer than zoneinfo in archive label */
		if (pmDebugOptions.pmlc || pmDebugOptions.pdu) 
		    fprintf(stderr, "__pmDecodeLogStatus: PM_ERR_IPC: pmlogger.zoneinfo too long (%d)\n", len);
		__pmFreeLogStatus(lsp, 1);
		return PM_ERR_IPC;
	    }
	    if ((lsp->pmlogger.zoneinfo = strdup(p)) == NULL) {
		sts = -oserror();
		pmNoMem("__pmDecodeLogStatus: pmlogger.zoneinfo", len, PM_RECOV_ERR);
		__pmFreeLogStatus(lsp, 1);
		return sts;
	    }
	    p += len;
	    if (p > pduend) {
		if (pmDebugOptions.pmlc || pmDebugOptions.pdu) 
		    fprintf(stderr, "__pmDecodeLogStatus: PM_ERR_IPC: pmlogger.zoneinfo data[%ld] > PDU len (%d)\n", 
			(long)(p - (char *)&pp->data[0]), pp->hdr.len);
		__pmFreeLogStatus(lsp, 1);
		return PM_ERR_IPC;
	    }
	}
    }
    else if (version == LOG_PDU_VERSION2) {
	logstatus_v2	*pp = (logstatus_v2 *)pdubuf;
	char		*pduend;

	pduend = (char *)pdubuf + pp->hdr.len;

	if ((pduend - (char*)pp) != sizeof(logstatus_v2)) {
	    free(lsp);
	    if (pmDebugOptions.pdu) {
		fprintf(stderr, "__pmDecodeLogStatus: PM_ERR_IPC: remainder %d < sizeof(logstatus_v2) %d\n",
		    (int)(pduend - (char*)pp), (int)sizeof(logstatus_v2));
	    }
	    return PM_ERR_IPC;
	}

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
