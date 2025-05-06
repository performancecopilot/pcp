/*
 * Copyright (c) 2017 Ken McDonell.  All Rights Reserved.
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
#ifndef PCP_DEPRECATED_H
#define PCP_DEPRECATED_H

/*
 * The functions below are deprecated and while still available
 * in libpcp, they may be removed at some point in the future.
 *
 * Deprecated Symbol		Replacement
 * ----------------------	----------------------	
 * __pmSetProgname()		pmSetProgname()
 * pmProgname			pmGetProgname()
 * __pmParseDebug()		pmSetDebug()
 * __pmSetDebugBits()		pmSetDebug()/pmClearDebug()
 * __pmOptions			pmOptions
 * __pmInDomProfile		pmInDomProfile
 * __pmProfile			pmProfile
 * __pmInResult			pmInResult
 * __pmOpenLog()		pmOpenLog()
 * __pmGetAPIConfig()		pmGetAPIConfig()
 * __pmNoMem()			pmNoMem()
 * __pmNotifyErr()		pmNotifyErr()
 * __pmSyslog()			pmSyslog()
 * __pmPrintDesc()		pmPrintDesc()
 * __pmtimevalNow()		pmtimevalNow()
 * __pmtimevalAdd()		pmtimevalAdd()
 * __pmtimevalSub()		pmtimevalSub()
 * __pmtimevalInc()		pmtimevalInc()
 * __pmtimevalDec()		pmtimevalDec()
 * __pmtimevalToReal()		pmtimevalToReal()
 * __pmtimevalFromReal()	pmtimevalFromReal()
 * __pmPrintStamp()		pmPrintStamp()
 * __pmPrintHighResStamp()	pmPrintHighResStamp()
 * __pmPathSeparator()		pmPathSeparator()
 * __pmGetUsername()		pmGetUsername()
 * __pmSetProcessIdentity()	pmSetProcessIdentity()
 * __pmTimeval			pmTimeval
 * __pmTimespec			pmTimespec
 * pmFreeHighResResult()	__pmFreeHighResResult()
 * __pmSpecLocalPMDA()		pmSpecLocalPMDA()
 * __pmLocalPMDA()		pmLocalPMDA()
 */

PCP_CALL extern int __pmSetProgname(const char *);
PCP_DATA extern char *pmProgname;
PCP_CALL extern int __pmParseDebug(const char *);
PCP_CALL extern void __pmSetDebugBits(int);

/*
 * These are for debugging only (but are present in the shipped libpcp)
 * ... this is the old_style
 */
PCP_DATA extern int pmDebug;
#define DBG_TRACE_PDU		(1<<0)	/* see pdu option below */
#define DBG_TRACE_FETCH		(1<<1)	/* see fetch option below */
#define DBG_TRACE_PROFILE	(1<<2)	/* see profile option below */
#define DBG_TRACE_VALUE		(1<<3)	/* see value option below */
#define DBG_TRACE_CONTEXT	(1<<4)	/* see context option below */
#define DBG_TRACE_INDOM		(1<<5)	/* see indom option below */
#define DBG_TRACE_PDUBUF	(1<<6)	/* see pdubuf option below */
#define DBG_TRACE_LOG		(1<<7)	/* see log option below */
#define DBG_TRACE_LOGMETA	(1<<8)	/* see logmeta option below */
#define DBG_TRACE_OPTFETCH	(1<<9)	/* see optfetch option below */
#define DBG_TRACE_AF		(1<<10)	/* see af option below */
#define DBG_TRACE_APPL0		(1<<11)	/* see appl0 option below */
#define DBG_TRACE_APPL1		(1<<12)	/* see appl1 option below */
#define DBG_TRACE_APPL2		(1<<13)	/* see appl2 option below */
#define DBG_TRACE_PMNS		(1<<14)	/* see pmns option below */
#define DBG_TRACE_LIBPMDA	(1<<15)	/* see libpmda option below */
#define DBG_TRACE_TIMECONTROL	(1<<16)	/* see timecontrol option below */
#define DBG_TRACE_PMC		(1<<17)	/* see pmc option below */
#define DBG_TRACE_DERIVE	(1<<18)	/* see derive option below */
#define DBG_TRACE_LOCK		(1<<19) /* see lock option below */
#define DBG_TRACE_INTERP	(1<<20)	/* see interp option below */
#define DBG_TRACE_CONFIG	(1<<21) /* see config option below */
#define DBG_TRACE_PMAPI		(1<<22) /* see pmapi option below */
#define DBG_TRACE_FAULT		(1<<23) /* see fault option below */
#define DBG_TRACE_AUTH		(1<<24) /* see auth option below */
#define DBG_TRACE_DISCOVERY	(1<<25) /* see discovery option below */
#define DBG_TRACE_ATTR		(1<<26) /* see attr option below */
#define DBG_TRACE_HTTP		(1<<27) /* see http option below */
/* not yet, and never will be, allocated, bits (1<<28) ... (1<<29) */
#define DBG_TRACE_DESPERATE	(1<<30) /* see desperate option below */

/*
 * DO NOT USE
 * Like __pmHandleToPtr(pmWhichContext()), but with no locking
 */
PCP_CALL struct __pmContext *__pmCurrentContext(void);

/* backwards-compatibility support for renamed symbols and types */
#define __pmOptions pmOptions
#define __pmProfile pmProfile
#define __pmInDomProfile pmInDomProfile
#define __pmInResult pmInResult
#define __pmOpenLog pmOpenLog
#define __pmGetAPIConfig pmGetAPIConfig
#define __pmNoMem pmNoMem
#define __pmNotifyErr pmNotifyErr
#define __pmSyslog pmSyslog
#define __pmPrintDesc pmPrintDesc
#define __pmtimevalNow pmtimevalNow
#define __pmtimevalAdd pmtimevalAdd
#define __pmtimevalSub pmtimevalSub
#define __pmtimevalInc pmtimevalInc
#define __pmtimevalDec pmtimevalDec
#define __pmtimevalToReal pmtimevalToReal
#define __pmtimevalFromReal pmtimevalFromReal
#define __pmPrintStamp pmPrintStamp
#define __pmPrintHighResStamp pmPrintHighResStamp
#define __pmPathSeparator pmPathSeparator
#define __pmGetUsername pmGetUsername
#define __pmSetProcessIdentity pmSetProcessIdentity
#define __pmTimeval pmTimeval
#define __pmTimespec pmTimespec
#define pmFreeHighResResult __pmFreeHighResResult
#define __pmSpecLocalPMDA pmSpecLocalPMDA
#define __pmLocalPMDA pmLocalPMDA

#endif /* PCP_DEPRECATED_H */
