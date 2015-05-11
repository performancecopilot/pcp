/*
 * Copyright (c) 1997-2001,2003 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifndef PCP_TRACE_DEV_H
#define PCP_TRACE_DEV_H

#ifndef _PLATFORM_DEFS_H
#include "platform_defs.h"
#endif

#ifdef _cplusplus
extern "C" {
#endif

#define MAXTAGNAMELEN	256

#define TRACE_ENV_HOST		"PCP_TRACE_HOST"
#define TRACE_ENV_PORT		"PCP_TRACE_PORT"
#define TRACE_ENV_TIMEOUT	"PCP_TRACE_TIMEOUT"
#define TRACE_ENV_NOAGENT	"PCP_TRACE_NOAGENT"
#define TRACE_ENV_REQTIMEOUT	"PCP_TRACE_REQTIMEOUT"
#define TRACE_ENV_RECTIMEOUT	"PCP_TRACE_RECONNECT"
#define TRACE_PORT		4323
#define TRACE_PDU_VERSION	1

#define TRACE_TYPE_TRANSACT	1
#define TRACE_TYPE_POINT	2
#define TRACE_TYPE_OBSERVE	3
#define TRACE_TYPE_COUNTER	4
#define TRACE_FIRST_TYPE	TRACE_TYPE_TRANSACT
#define TRACE_LAST_TYPE		TRACE_TYPE_COUNTER

/*
 * Protocol data unit (PDU) support snarfed from impl.h, pdu.c and pdubuf.c
 */
typedef struct {
    int	len;		/* length of pdu_header + PDU */
    int	type;		/* PDU type */
    int	from;		/* pid of PDU originator */
} __pmTracePDUHdr;

typedef __uint32_t	__pmTracePDU;

extern int __pmtracexmitPDU(int, __pmTracePDU *);
extern int __pmtracegetPDU(int, int, __pmTracePDU **);

/* for __pmtracegetPDU */
#define TRACE_TIMEOUT_NEVER	 0
#define TRACE_TIMEOUT_DEFAULT	-1

/* unit of space allocation for PDU buffer.  */
#define TRACE_PDU_CHUNK	1024

extern __pmTracePDU *__pmtracefindPDUbuf(int);
extern void __pmtracepinPDUbuf(void *);
extern int __pmtraceunpinPDUbuf(void *);
extern int __pmtracemoreinput(int);
extern void __pmtracenomoreinput(int);


#define TRACE_PDU_BASE		0x7050
#define TRACE_PDU_ACK		0x7050
#define TRACE_PDU_DATA		0x7051
#define TRACE_PDU_MAX	 	2

extern int __pmtracesendack(int, int);
extern int __pmtracedecodeack(__pmTracePDU *, int *);
extern int __pmtracesenddata(int, char *, int, int, double);
extern int __pmtracedecodedata(__pmTracePDU *, char **, int *, int *, int *, double *);

#define TRACE_PROTOCOL_FINAL    -1
#define TRACE_PROTOCOL_QUERY    0
#define TRACE_PROTOCOL_ASYNC    1
#define TRACE_PROTOCOL_SYNC     2

extern int __pmtraceprotocol(int);

extern int __pmstate;

#ifdef __cplusplus
}
#endif

#endif /* PCP_TRACE_DEV_H */
