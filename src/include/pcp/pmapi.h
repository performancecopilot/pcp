/*
 * Copyright (c) 2012-2015 Red Hat.
 * Copyright (c) 1997,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifndef PCP_PMAPI_H
#define PCP_PMAPI_H

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>

/*
 * Platform and environment customization
 */
#include "platform_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PMAPI_VERSION_2	2
#define PMAPI_VERSION	PMAPI_VERSION_2

/*
 * -------- Naming Services --------
 */
typedef unsigned int	pmID;		/* Metric Identifier */
#define PM_ID_NULL	0xffffffff

typedef unsigned int	pmInDom;	/* Instance-Domain */
#define PM_INDOM_NULL	0xffffffff
#define PM_IN_NULL	0xffffffff

#define PM_NS_DEFAULT	NULL	/* default name */

/*
 * Encoding for the units (dimensions Time and Space) and scale
 * for Performance Metric Values
 *
 * For example, a pmUnits struct of
 *	{ 1, -1, 0, PM_SPACE_MBYTE, PM_TIME_SEC, 0 }
 * represents Mbytes/sec, while 
 *	{ 0, 1, -1, 0, PM_TIME_HOUR, 6 }
 * represents hours/million-events
 */
typedef struct pmUnits {
#ifdef HAVE_BITFIELDS_LTOR
    signed int		dimSpace : 4;	/* space dimension */
    signed int		dimTime : 4;	/* time dimension */
    signed int		dimCount : 4;	/* event dimension */
    unsigned int	scaleSpace : 4;	/* one of PM_SPACE_* below */
    unsigned int	scaleTime : 4;	/* one of PM_TIME_* below */
    signed int		scaleCount : 4;	/* one of PM_COUNT_* below */
    unsigned int	pad : 8;
#else
    unsigned int	pad : 8;
    signed int		scaleCount : 4;	/* one of PM_COUNT_* below */
    unsigned int	scaleTime : 4;	/* one of PM_TIME_* below */
    unsigned int	scaleSpace : 4;	/* one of PM_SPACE_* below */
    signed int		dimCount : 4;	/* event dimension */
    signed int		dimTime : 4;	/* time dimension */
    signed int		dimSpace : 4;	/* space dimension */
#endif
} pmUnits;			/* dimensional units and scale of value */

/* pmUnits.scaleSpace */
#define PM_SPACE_BYTE	0	/* bytes */
#define PM_SPACE_KBYTE	1	/* Kilobytes (1024) */
#define PM_SPACE_MBYTE	2	/* Megabytes (1024^2) */
#define PM_SPACE_GBYTE	3	/* Gigabytes (1024^3) */
#define PM_SPACE_TBYTE	4	/* Terabytes (1024^4) */
#define PM_SPACE_PBYTE	5	/* Petabytes (1024^5) */
#define PM_SPACE_EBYTE	6	/* Exabytes  (1024^6) */
/* pmUnits.scaleTime */
#define PM_TIME_NSEC	0	/* nanoseconds */
#define PM_TIME_USEC	1	/* microseconds */
#define PM_TIME_MSEC	2	/* milliseconds */
#define PM_TIME_SEC	3	/* seconds */
#define PM_TIME_MIN	4	/* minutes */
#define PM_TIME_HOUR	5	/* hours */
/*
 * pmUnits.scaleCount (e.g. count events, syscalls, interrupts, etc.)
 * -- these are simply powers of 10, and not enumerated here,
 *    e.g. 6 for 10^6, or -3 for 10^-3
 */
#define PM_COUNT_ONE	0	/* 1 */

/* Performance Metric Descriptor */
typedef struct pmDesc {
    pmID	pmid;		/* unique identifier */
    int		type;		/* base data type (see below) */
    pmInDom	indom;		/* instance domain */
    int		sem;		/* semantics of value (see below) */
    pmUnits	units;		/* dimension and units */
} pmDesc;

/* pmDesc.type -- data type of metric values */
#define PM_TYPE_NOSUPPORT	-1	/* not implemented in this version */
#define PM_TYPE_32		0	/* 32-bit signed integer */
#define PM_TYPE_U32		1	/* 32-bit unsigned integer */
#define PM_TYPE_64		2	/* 64-bit signed integer */
#define PM_TYPE_U64		3	/* 64-bit unsigned integer */
#define PM_TYPE_FLOAT		4	/* 32-bit floating point */
#define PM_TYPE_DOUBLE		5	/* 64-bit floating point */
#define PM_TYPE_STRING		6	/* array of char */
#define PM_TYPE_AGGREGATE	7	/* arbitrary binary data (aggregate) */
#define PM_TYPE_AGGREGATE_STATIC 8	/* static pointer to aggregate */
#define PM_TYPE_EVENT		9	/* packed pmEventArray */
#define PM_TYPE_HIGHRES_EVENT	10	/* packed pmHighResEventArray */
#define PM_TYPE_UNKNOWN		255	/* used in pmValueBlock, not pmDesc */

/* pmDesc.sem -- semantics/interpretation of metric values */
#define PM_SEM_COUNTER	1	/* cumulative counter (monotonic increasing) */
				/* was PM_SEM_RATE, no longer used now */
#define PM_SEM_INSTANT	3	/* instantaneous value, continuous domain */
#define PM_SEM_DISCRETE	4	/* instantaneous value, discrete domain */

#define PM_ERR_BASE2 12345
#define PM_ERR_BASE  PM_ERR_BASE2

/* PMAPI Error Conditions */

#define PM_ERR_GENERIC		(-PM_ERR_BASE-0)    /* Generic error, already reported above */
#define PM_ERR_PMNS		(-PM_ERR_BASE-1)    /* Problems parsing PMNS definitions */
#define PM_ERR_NOPMNS		(-PM_ERR_BASE-2)    /* PMNS not accessible */
#define PM_ERR_DUPPMNS		(-PM_ERR_BASE-3)    /* Attempt to reload the PMNS */
#define PM_ERR_TEXT		(-PM_ERR_BASE-4)    /* Oneline or help text is not available */
#define PM_ERR_APPVERSION	(-PM_ERR_BASE-5)    /* Metric not supported by this version of monitored application */
#define PM_ERR_VALUE		(-PM_ERR_BASE-6)    /* Missing metric value(s) */
/* retired PM_ERR_LICENSE (-PM_ERR_BASE-7) Current PCP license does not permit this operation */
#define PM_ERR_TIMEOUT		(-PM_ERR_BASE-8)    /* Timeout waiting for a response from PMCD */
#define PM_ERR_NODATA		(-PM_ERR_BASE-9)    /* Empty archive log file */
#define PM_ERR_RESET		(-PM_ERR_BASE-10)   /* pmcd reset or configuration changed */
/* retired PM_ERR_FILE (-PM_ERR_BASE-11) Cannot locate a file */
#define PM_ERR_NAME		(-PM_ERR_BASE-12)   /* Unknown metric name */
#define PM_ERR_PMID		(-PM_ERR_BASE-13)   /* Unknown or illegal metric identifier */
#define PM_ERR_INDOM		(-PM_ERR_BASE-14)   /* Unknown or illegal instance domain identifier */
#define PM_ERR_INST		(-PM_ERR_BASE-15)   /* Unknown or illegal instance identifier */
#define PM_ERR_UNIT		(-PM_ERR_BASE-16)   /* Illegal pmUnits specification */
#define PM_ERR_CONV		(-PM_ERR_BASE-17)   /* Impossible value or scale conversion */
#define PM_ERR_TRUNC		(-PM_ERR_BASE-18)   /* Truncation in value conversion */
#define PM_ERR_SIGN		(-PM_ERR_BASE-19)   /* Negative value in conversion to unsigned */
#define PM_ERR_PROFILE		(-PM_ERR_BASE-20)   /* Explicit instance identifier(s) required */
#define PM_ERR_IPC		(-PM_ERR_BASE-21)   /* IPC protocol failure */
/* retired PM_ERR_NOASCII (-PM_ERR_BASE-22) ASCII format not supported for this PDU */
#define PM_ERR_EOF		(-PM_ERR_BASE-23)   /* IPC channel closed */
#define PM_ERR_NOTHOST		(-PM_ERR_BASE-24)   /* Operation requires context with host source of metrics */
#define PM_ERR_EOL		(-PM_ERR_BASE-25)   /* End of PCP archive log */
#define PM_ERR_MODE		(-PM_ERR_BASE-26)   /* Illegal mode specification */
#define PM_ERR_LABEL		(-PM_ERR_BASE-27)   /* Illegal label record at start of a PCP archive log file */
#define PM_ERR_LOGREC		(-PM_ERR_BASE-28)   /* Corrupted record in a PCP archive log */
#define PM_ERR_NOTARCHIVE	(-PM_ERR_BASE-29)   /* Operation requires context with archive source of metrics */
#define PM_ERR_LOGFILE          (-PM_ERR_BASE-30)   /* Missing archive file */
#define PM_ERR_NOCONTEXT	(-PM_ERR_BASE-31)   /* Attempt to use an illegal context */
#define PM_ERR_PROFILESPEC	(-PM_ERR_BASE-32)   /* NULL pmInDom with non-NULL instlist */
#define PM_ERR_PMID_LOG		(-PM_ERR_BASE-33)   /* Metric not defined in the PCP archive log */
#define PM_ERR_INDOM_LOG	(-PM_ERR_BASE-34)   /* Instance domain identifier not defined in the PCP archive log */
#define PM_ERR_INST_LOG		(-PM_ERR_BASE-35)   /* Instance identifier not defined in the PCP archive log */
#define PM_ERR_NOPROFILE	(-PM_ERR_BASE-36)   /* Missing profile - protocol botch */
#define	PM_ERR_NOAGENT		(-PM_ERR_BASE-41)   /* No pmcd agent for domain of request */
#define PM_ERR_PERMISSION	(-PM_ERR_BASE-42)   /* No permission to perform requested operation */
#define PM_ERR_CONNLIMIT	(-PM_ERR_BASE-43)   /* PMCD connection limit for this host exceeded */
#define PM_ERR_AGAIN		(-PM_ERR_BASE-44)   /* try again. Info not currently available */
#define PM_ERR_ISCONN		(-PM_ERR_BASE-45)   /* already connected */
#define PM_ERR_NOTCONN		(-PM_ERR_BASE-46)   /* not connected */
#define PM_ERR_NEEDPORT		(-PM_ERR_BASE-47)   /* port name required */
/* retired PM_ERR_WANTACK (-PM_ERR_BASE-48) can not send due to pending acks */
#define PM_ERR_NONLEAF		(-PM_ERR_BASE-49)   /* PMNS node is not a leaf node */
/* retired PM_ERR_OBJSTYLE (-PM_ERR_BASE-50) user/kernel object style mismatch */
/* retired PM_ERR_PMCDLICENSE (-PM_ERR_BASE-51) PMCD is not licensed to accept connections */
#define PM_ERR_TYPE		(-PM_ERR_BASE-52)   /* Unknown or illegal metric type */
#define PM_ERR_THREAD		(-PM_ERR_BASE-53)   /* Operation not supported for multi-threaded applications */
#define PM_ERR_NOCONTAINER	(-PM_ERR_BASE-54)   /* Container not found */
#define PM_ERR_BADSTORE		(-PM_ERR_BASE-55)   /* Bad input to pmstore */

/* retired PM_ERR_CTXBUSY (-PM_ERR_BASE-97) Context is busy */
#define PM_ERR_TOOSMALL		(-PM_ERR_BASE-98)   /* Insufficient elements in list */
#define PM_ERR_TOOBIG		(-PM_ERR_BASE-99)   /* Result size exceeded */
#define PM_ERR_FAULT		(-PM_ERR_BASE-100)  /* QA fault injected */

#define PM_ERR_PMDAREADY	(-PM_ERR_BASE-1048) /* now ready to respond */
#define PM_ERR_PMDANOTREADY	(-PM_ERR_BASE-1049) /* not yet ready to respond */
#define PM_ERR_NYI		(-PM_ERR_BASE-8999) /* Functionality not yet implemented [end-of-range mark] */

/*
 * Report PMAPI errors messages
 */
extern char *pmErrStr(int);				/* NOT thread-safe */
extern char *pmErrStr_r(int, char *, int);
/* safe size for a pmErrStr_r buffer to accommodate all error messages */
#define PM_MAXERRMSGLEN		128

/*
 * Load a Performance Metrics Name Space
 */
extern int pmLoadNameSpace(const char *);
extern int pmLoadASCIINameSpace(const char *, int);
extern void pmUnloadNameSpace(void);

/*
 * Where is PMNS located - added for distributed PMNS.
 */
extern int pmGetPMNSLocation(void);
#define PMNS_LOCAL   1
#define PMNS_REMOTE  2
#define PMNS_ARCHIVE 3

/*
 * Trim a name space with respect to the current context
 * (usually from an archive, or after processing an archive)
 */
extern int pmTrimNameSpace(void);

/*
 * Expand a list of names to a list of metrics ids
 */
extern int pmLookupName(int, char **, pmID *);

/*
 * Find the names of descendent nodes in the PMNS
 * and in the latter case get the status of each child.
 */
extern int pmGetChildren(const char *, char ***);
extern int pmGetChildrenStatus(const char *, char ***, int **);
#define PMNS_LEAF_STATUS     0	/* leaf node in PMNS tree */
#define PMNS_NONLEAF_STATUS  1	/* non-terminal node in PMNS tree */

/*
 * Reverse Lookup: find name(s) given a metric id
 */
extern int pmNameID(pmID, char **);		/* one */
extern int pmNameAll(pmID, char ***);		/* all */

/*
 * Handy recursive descent of the PMNS
 */
extern int pmTraversePMNS(const char *, void(*)(const char *));
extern int pmTraversePMNS_r(const char *, void(*)(const char *, void *), void *);

/*
 * Given a metric, find it's descriptor (caller supplies buffer for desc),
 * from the current context.
 */
extern int pmLookupDesc(pmID, pmDesc *);

/*
 * Return the internal instance identifier, from the current context,
 * given an instance domain and the external instance name.
 * Archive variant scans the union of the indom entries in the archive
 * log.
 */
extern int pmLookupInDom(pmInDom, const char *);
extern int pmLookupInDomArchive(pmInDom, const char *);

/*
 * Return the external instance name, from the current context,
 * given an instance domain and the internal instance identifier.
 * Archive variant scans the union of the indom entries in the archive
 * log.
 */
extern int pmNameInDom(pmInDom, int, char **);
extern int pmNameInDomArchive(pmInDom, int, char **);

/*
 * Return all of the internal instance identifiers (instlist) and external
 * instance names (namelist) for the given instance domain in the current
 * context.
 * Archive variant returns the union of the indom entries in the archive
 * log.
 */
extern int pmGetInDom(pmInDom, int **, char ***);
extern int pmGetInDomArchive(pmInDom, int **, char ***);

/*
 * Given context ID, return the host name associated with that context,
 * or the empty string if no name can be found
 */
extern const char *pmGetContextHostName(int);
extern char *pmGetContextHostName_r(int, char *, int);

/*
 * Return the handle of the current context
 */
extern int pmWhichContext(void);

/*
 * Destroy a context and close its connection
 */
extern int pmDestroyContext(int);

/*
 * Establish a new context (source of performance data + instance profile)
 * for the named host
 */
extern int pmNewContext(int, const char *);
#define PM_CONTEXT_UNDEF	-1	/* current context is undefined */
#define PM_CONTEXT_HOST		1	/* host via pmcd */
#define PM_CONTEXT_ARCHIVE	2	/* PCP archive */
#define PM_CONTEXT_LOCAL	3	/* local host, no pmcd connection */
#define PM_CONTEXT_TYPEMASK	0xff	/* mask to separate types / flags */
/* #define PM_CTXFLAG_SHALLOW	(1U<<8)	-- don't actually connect to host */
/* #define PM_CTXFLAG_EXCLUSIVE	(1U<<9)	-- don't share socket among ctxts */
#define PM_CTXFLAG_SECURE	(1U<<10)/* encrypted socket comms channel */
#define PM_CTXFLAG_COMPRESS	(1U<<11)/* compressed socket host channel */
#define PM_CTXFLAG_RELAXED	(1U<<12)/* encrypted if possible else not */
#define PM_CTXFLAG_AUTH		(1U<<13)/* make authenticated connection */
#define PM_CTXFLAG_CONTAINER	(1U<<14)/* container connection attribute */

/*
 * Duplicate current context -- returns handle to new one for pmUseContext()
 */
extern int pmDupContext(void);

/*
 * Restore (previously established or duplicated) context
 */
extern int pmUseContext(int);

/*
 * Reconnect an existing context (when pmcd dies, etc). All existing context
 * settings are preserved and the previous context settings are restored.
 */
extern int pmReconnectContext(int);

/*
 * Add to instance profile.
 * If pmInDom == PM_INDOM_NULL, then all instance domains are selected.
 * If no inst parameters are given, then all instances are selected.
 * e.g. to select all available instances in all domains,
 * then use pmAddProfile(PM_INDOM_NULL, 0, NULL).
 */
extern int pmAddProfile(pmInDom, int, int *);

/*
 * Delete from instance profile.
 * Similar (but negated) functional semantics to pmProfileAdd.
 * E.g. to disable all instances in all domains then use
 * pmDelProfile(PM_INDOM_NULL, 0, NULL).
 */
extern int pmDelProfile(pmInDom, int, int *);

/* 
 * ---------- Collection services ---------- 
 *
 * Result from pmFetch is encoded as a timestamp and vector of pointers
 * to pmValueSet instances (one per PMID in the result).
 * Each pmValueSet has a PMID, a value count, a value format, and a vector of
 * instance-value pairs.  Aggregate, string and non-int values are returned
 * via one further level of indirection using pmValueBlocks.
 *
 * timeStamp
 * ->pmID
 *   value format
 *     instance, value
 *     instance, value
 *     ... etc
 *
 * ->pmID
 *   value format
 *     instance, value
 *     ... etc
 *
 *
 * Notes on pmValueBlock
 *   0. may be used for arbitrary binary data
 *   1. only ever allocated dynamically, and vbuf expands to accommodate
 *	an arbitrary value (don't believe the [1] size!)
 *   2. len is the length of the len field + the real size of vbuf
 *	(which includes the null-byte terminator if there is one)
 */
typedef struct pmValueBlock {
#ifdef HAVE_BITFIELDS_LTOR
    unsigned int	vtype : 8;	/* value type */
    unsigned int	vlen : 24;	/* bytes for vtype/vlen + vbuf */
#else
    unsigned int	vlen : 24;	/* bytes for vtype/vlen + vbuf */
    unsigned int	vtype : 8;	/* value type */
#endif
    char		vbuf[1];	/* the value */
} pmValueBlock;

#define PM_VAL_HDR_SIZE	4		/* bytes for the vtype/vlen header */
#define PM_VAL_VLEN_MAX	0x00ffffff	/* maximum vbuf[] size */

typedef struct pmValue {
    int		inst;		/* instance identifier */
    union {
	pmValueBlock	*pval;	/* pointer to value-block */
	int		lval;	/* integer value insitu (lval 'cuz it WAS a long) */
    } value;
} pmValue;

typedef struct pmValueSet {
    pmID	pmid;		/* metric identifier */
    int		numval;		/* number of values */
    int		valfmt;		/* value style */
    pmValue	vlist[1];	/* set of instances/values */
} pmValueSet;

/* values for valfmt */
#define PM_VAL_INSITU	0	/* value.lval is it */
#define PM_VAL_DPTR	1	/* value.pval->vbuf is it, and dynamic alloc */
#define PM_VAL_SPTR	2	/* value.pval->vbuf is it, and static alloc */


/* Result returned by pmFetch() */
typedef struct pmResult {
    struct timeval	timestamp;	/* time stamped by collector */
    int			numpmid;	/* number of PMIDs */
    pmValueSet		*vset[1];	/* set of value sets, one per PMID */
} pmResult;

/* High resolution event timer result from pmUnpackHighResEventRecords() */
typedef struct pmHighResResult {
    struct timespec	timestamp;	/* time stamped by collector */
    int			numpmid;	/* number of PMIDs */
    pmValueSet		*vset[1];	/* set of value sets, one per PMID */
} pmHighResResult;

/* Generic Union for Value-Type conversions */
typedef union {
    __int32_t		l;	/* 32-bit signed */
    __uint32_t		ul;	/* 32-bit unsigned */
    __int64_t		ll;	/* 64-bit signed */
    __uint64_t		ull;	/* 64-bit unsigned */
    float		f;	/* 32-bit floating point */
    double		d;	/* 64-bit floating point */
    char		*cp;	/* char ptr */
    pmValueBlock	*vbp;	/* pmValueBlock ptr */
} pmAtomValue;

/*
 * Fetch metrics. Value/instances returned depends on current instance profile.
 * By default, all available instances for each requested metric id are
 * returned. The metrics argument is terminated with PM_NULL_ID
 *
 * The value sets returned are in the same order as the metrics argument,
 * and the number of value sets returned is guaranteed to be the same as
 * the number of metrics in the agument.  
 */
extern int pmFetch(int, pmID *, pmResult **);

/*
 * PMCD state changes returned as pmFetch function result for PM_CONTEXT_HOST
 * contexts, i.e. when communicating with PMCD
 */
#define PMCD_NO_CHANGE		0
#define PMCD_ADD_AGENT		1
#define PMCD_RESTART_AGENT	2
#define PMCD_DROP_AGENT		4

/*
 * Variant that is used to return a pmResult from an archive
 */
extern int pmFetchArchive(pmResult **);

/*
 * struct timeval is sometimes 2 x 64-bit ... we use a 2 x 32-bit format for
 * PDUs, internally within libpcp and for (external) archive logs
 */
typedef struct __pmTimeval {
    __int32_t	tv_sec;		/* seconds since Jan. 1, 1970 */
    __int32_t	tv_usec;	/* and microseconds */
} __pmTimeval;

typedef struct __pmTimespec {
    __int64_t	tv_sec;		/* seconds since Jan. 1, 1970 */
    __int64_t	tv_nsec;	/* and nanoseconds */
} __pmTimespec;

/*
 * Label Record at the start of every log file - as exported above
 * the PMAPI ...
 * NOTE MAXHOSTNAMELEN is a bad choice here for ll_hostname[], as
 *	it may vary on different hosts ... we use PM_LOG_MAXHOSTLEN instead, and
 *	size this to be the same as MAXHOSTNAMELEN in IRIX 5.3
 * NOTE	that the struct timeval means we have another struct (__pmLogLabel)
 *	for internal use that has a __pmTimeval in place of the struct timeval.
 */
#define PM_TZ_MAXLEN	40
#define PM_LOG_MAXHOSTLEN		64
#define PM_LOG_MAGIC	0x50052600
#define PM_LOG_VERS02	0x2
#define PM_LOG_VOL_TI	-2	/* temporal index */
#define PM_LOG_VOL_META	-1	/* meta data */
typedef struct pmLogLabel {
    int		ll_magic;	/* PM_LOG_MAGIC | log format version no. */
    pid_t	ll_pid;				/* PID of logger */
    struct timeval	ll_start;		/* start of this log */
    char	ll_hostname[PM_LOG_MAXHOSTLEN];	/* name of collection host */
    char	ll_tz[PM_TZ_MAXLEN];		/* $TZ at collection host */
} pmLogLabel;

/*
 * Get the label record from the current archive context, and discover
 * when the archive ends
 */
extern int pmGetArchiveLabel(pmLogLabel *);
extern int pmGetArchiveEnd(struct timeval *);

/* Free result buffer */
extern void pmFreeResult(pmResult *);
extern void pmFreeHighResResult(pmHighResResult *);

/* Value extract from pmValue and type conversion */
extern int pmExtractValue(int, const pmValue *, int, pmAtomValue *, int);

/* Print single pmValue */
extern void pmPrintValue(FILE *, int, int, const pmValue *, int);

/* Scale conversion, based on value format, value type and scale */
extern int pmConvScale(int, const pmAtomValue *, const pmUnits *, pmAtomValue *, 
		       const pmUnits *);

/* Sort instances for each metric within a pmResult */
extern void pmSortInstances(pmResult *);

/* Adjust collection time and/or mode for pmFetch */
extern int pmSetMode(int, const struct timeval *, int);
#define PM_MODE_LIVE	0
#define PM_MODE_INTERP	1
#define PM_MODE_FORW	2
#define PM_MODE_BACK	3

/* Modify the value of one or more metrics */
extern int pmStore(const pmResult *);

/* Get help and descriptive text */
extern int pmLookupText(pmID, int, char **);
extern int pmLookupInDomText(pmInDom, int, char **);
#define PM_TEXT_ONELINE	1
#define PM_TEXT_HELP	2

/*
 * Some handy formatting routines for messages, and other output
 */
extern const char *pmIDStr(pmID);			/* NOT thread-safe */
extern char *pmIDStr_r(pmID, char *, int);
extern const char *pmInDomStr(pmInDom);			/* NOT thread-safe */
extern char *pmInDomStr_r(pmInDom, char *, int);
extern const char *pmTypeStr(int);			/* NOT thread-safe */
extern char *pmTypeStr_r(int, char *, int);
extern const char *pmUnitsStr(const pmUnits *);		/* NOT thread-safe */
extern char *pmUnitsStr_r(const pmUnits *, char *, int);
extern const char *pmAtomStr(const pmAtomValue *, int);	/* NOT thread-safe */
extern char *pmAtomStr_r(const pmAtomValue *, int, char *, int);
extern const char *pmNumberStr(double);			/* NOT thread-safe */
extern char *pmNumberStr_r(double, char *, int);
extern const char *pmEventFlagsStr(int);		/* NOT thread-safe */
extern char *pmEventFlagsStr_r(int, char *, int);

/* Extended time base definitions and macros */
#define PM_XTB_FLAG	0x1000000

#define PM_XTB_SET(type) (PM_XTB_FLAG | ((type) << 16))
#define PM_XTB_GET(x) (((x) & PM_XTB_FLAG) ? (((x) & 0xff0000) >> 16) : -1)

/* Parse -t, -S, -T, -A and -O options */
extern int pmParseInterval(const char *, struct timeval *, char **);
extern int pmParseTimeWindow(
      const char *, const char *, const char *, const char *,
      const struct timeval *, const struct timeval *,
      struct timeval *, struct timeval *, struct timeval *, char **);

/* Reporting timezone */
extern int pmUseZone(const int);
extern int pmNewZone(const char *);
extern int pmNewContextZone(void);
extern int pmWhichZone(char **);
extern char *pmCtime(const time_t *, char *);
extern struct tm *pmLocaltime(const time_t *, struct tm *);

/* Parse host:metric[instances] or archive/metric[instances] */
typedef struct pmMetricSpec {
    int		isarch;         /* source type: 0 -> live host, 1 -> archive, 2 -> local context */
    char	*source;        /* name of source host or archive */
    char	*metric;        /* name of metric */
    int		ninst;          /* number of instances, 0 -> all */
    char	*inst[1];       /* array of instance names */
} pmMetricSpec;

/* Parsing of host:metric[instances] or archive/metric[instances] */
extern int pmParseMetricSpec(const char *, int, char *, pmMetricSpec **,
			     char **);
extern void pmFreeMetricSpec(pmMetricSpec *p);

/*
 * Configurable error reporting
 */
#ifdef __GNUC__
# define __PM_PRINTFLIKE(idx,cnt) __attribute__ ((format (printf, idx,cnt)))
#else
# define __PM_PRINTFLIKE(idx,cnt)
#endif

extern int pmprintf(const char *, ...) __PM_PRINTFLIKE(1,2);
extern int pmflush(void);

/*
 * Wrapper for config/environment variables. Warning: this will exit() with
 * a FATAL error if /etc/pcp.conf does not exist and $PCP_CONF is not set.
 * Use the pmGetOptionalConfig variant if this behaviour is not sought.
 * Return values point to strings in the environment, or NULL in the optional
 * case when the requested configuration variable was not found.
 */
extern char *pmGetConfig(const char *);
extern char *pmGetOptionalConfig(const char *);

extern int pmGetVersion(void);

/*
 * Common command line argument parsing interfaces
 */

#define PMAPI_OPTIONS	"A:a:D:gh:n:O:p:S:s:T:t:VZ:z?"
#define PMAPI_OPTIONS_HEADER(s)	{ "", 0, '-', 0, (s) }
#define PMAPI_OPTIONS_TEXT(s)	{ "", 0, '|', 0, (s) }
#define PMAPI_OPTIONS_END	{ NULL, 0, 0, 0, NULL }

#define PMOPT_ALIGN	{ "align",	1, 'A',	"TIME", \
			"align sample times on natural boundaries" }
#define PMOPT_ARCHIVE	{ "archive",	1, 'a',	"FILE", \
			"metrics source is a PCP log archive" }
#define PMOPT_DEBUG	{ "debug",	1, 'D',	"DBG", \
			NULL }
#define PMOPT_GUIMODE	{ "guimode",	0, 'g',	0, \
			"start in GUI mode with new time control" }
#define PMOPT_HOST	{ "host",	1, 'h', "HOST", \
			"metrics source is PMCD on host" }
#define PMOPT_HOSTSFILE	{ "hostsfile",	1, 'H', "FILE", \
			"read metric source hosts from a file" }
#define PMOPT_SPECLOCAL	{ "spec-local",	1, 'K',	"SPEC", \
			"optional additional PMDA spec for local connection" }
#define PMOPT_LOCALPMDA	{ "local-PMDA",	0, 'L', 0, \
			"metrics source is local connection to a PMDA" }
#define PMOPT_NAMESPACE	{ "namespace",	1, 'n', "FILE", \
			"use an alternative PMNS" }
#define PMOPT_UNIQNAMES	{ "uniqnames",	1, 'N', "FILE", \
			"like -n but only one name allowed for each PMID" }
#define PMOPT_ORIGIN	{ "origin",	1, 'O', "TIME", \
			"initial sample time within the time window" }
#define PMOPT_GUIPORT	{ "guiport",	1, 'p', "N", \
			"port for connection to existing time control" }
#define PMOPT_START	{ "start",	1, 'S', "TIME", \
			"start of the time window" }
#define PMOPT_SAMPLES	{ "samples",	1, 's', "N", \
			"terminate after this many samples" }
#define PMOPT_FINISH	{ "finish",	1, 'T', "TIME", \
			"end of the time window" }
#define PMOPT_INTERVAL	{ "interval",	1, 't', "DELTA", \
			"sampling interval" }
#define PMOPT_VERSION	{ "version",	0, 'V', 0, \
			"display version number and exit" }
#define PMOPT_TIMEZONE	{ "timezone",	1, 'Z', "TZ", \
			"set reporting timezone" }
#define PMOPT_HOSTZONE	{ "hostzone",	0, 'z', 0, \
			"set reporting timezone to local time of metrics source" }
#define PMOPT_HELP	{ "help",	0, '?', 0, \
			"show this usage message and exit" }

#define PMAPI_GENERAL_OPTIONS	\
	PMAPI_OPTIONS_HEADER("General options"), \
	PMOPT_ALIGN, \
	PMOPT_ARCHIVE, \
	PMOPT_DEBUG, \
	PMOPT_GUIMODE, \
	PMOPT_HOST, \
	PMOPT_NAMESPACE, \
	PMOPT_ORIGIN, \
	PMOPT_GUIPORT, \
	PMOPT_START, \
	PMOPT_SAMPLES, \
	PMOPT_FINISH, \
	PMOPT_INTERVAL, \
	PMOPT_TIMEZONE, \
	PMOPT_HOSTZONE, \
	PMOPT_VERSION, \
	PMOPT_HELP

/* long-only standard options */
#define PMLONGOPT_ARCHIVE_LIST "archive-list"
#define PMOPT_ARCHIVE_LIST { PMLONGOPT_ARCHIVE_LIST, 1, 0, "FILES", \
		"comma-separated list of metric source archives" }
#define PMLONGOPT_ARCHIVE_FOLIO "archive-folio"
#define PMOPT_ARCHIVE_FOLIO { PMLONGOPT_ARCHIVE_FOLIO, 1, 0, "FILE", \
		"read metric source archives from a folio" }
#define PMLONGOPT_HOST_LIST "host-list"
#define PMOPT_HOST_LIST { PMLONGOPT_HOST_LIST, 1, 0, "HOSTS", \
		"comma-separated list of metric source hosts" }
#define PMLONGOPT_CONTAINER "container"
#define PMOPT_CONTAINER { PMLONGOPT_CONTAINER, 1, 0, "NAME", \
		"specify an individual container to be queried" }

/* pmOptions flags */
#define PM_OPTFLAG_INIT		(1<<0)	/* initialisation done */
#define PM_OPTFLAG_DONE		(1<<1)	/* parsing is complete */
#define PM_OPTFLAG_MULTI	(1<<2)	/* allow multi-context */
#define PM_OPTFLAG_USAGE_ERR	(1<<3)	/* argument parse fail */
#define PM_OPTFLAG_RUNTIME_ERR	(1<<4)	/* any runtime failure */
#define PM_OPTFLAG_EXIT		(1<<5)	/* tool should exit(0) */
#define PM_OPTFLAG_POSIX	(1<<6)	/* POSIXLY_CORRECT set */
#define PM_OPTFLAG_MIXED	(1<<7)	/* allow hosts+archives */
#define PM_OPTFLAG_ENV_ONLY	(1<<8)	/* use env options only */
#define PM_OPTFLAG_LONG_ONLY	(1<<9)	/* use long options only */
#define PM_OPTFLAG_BOUNDARIES	(1<<10)	/* calculate time window */
#define PM_OPTFLAG_STDOUT_TZ	(1<<11)	/* write timezone change */
#define PM_OPTFLAG_NOFLUSH	(1<<12)	/* caller issues pmflush */
#define PM_OPTFLAG_QUIET	(1<<13)	/* silence getopt errors */

struct pmOptions;
#define __pmOptions pmOptions /* backwards-compatible */
typedef int (*pmOptionOverride)(int, struct pmOptions *);

typedef struct pmLongOptions {
    const char *	long_opt;
    int			has_arg;
    int			short_opt;
    const char *	argname;
    const char *	message;
} pmLongOptions;

typedef struct pmOptions {
    int			version;
    int			flags;

    /* in: define set of all options */
    const char *	short_options;
    pmLongOptions *	long_options;
    const char *	short_usage;

    /* in: method for general override */
    pmOptionOverride	override;

    /* out: usual getopt information */
    int			index;
    int			optind;
    int			opterr;
    int			optopt;
    char		*optarg;

    /* internals; do not ever access */
    int			__initialized;
    char *		__nextchar;
    int			__ordering;
    int			__posixly_correct;
    int			__first_nonopt;
    int			__last_nonopt;

    /* out: error count */
    int 		errors;

    /* out: PMAPI options and values */
    int			context;	/* PM_CONTEXT_{HOST,ARCHIVE,LOCAL} */
    int			nhosts;
    int			narchives;
    char **		hosts;
    char **		archives;
    struct timeval	start;
    struct timeval	finish;
    struct timeval	origin;
    struct timeval	interval;
    char *		align_optarg;
    char *		start_optarg;
    char *		finish_optarg;
    char *		origin_optarg;
    char *		guiport_optarg;
    char *		timezone;
    int			samples;
    int			guiport;
    int			padding;
    unsigned int	guiflag : 1;
    unsigned int	tzflag  : 1;
    unsigned int	nsflag  : 1;
    unsigned int	Lflag   : 1;
    unsigned int	zeroes  : 28;
} pmOptions;

extern int pmgetopt_r(int, char *const *, pmOptions *);
extern int pmGetOptions(int, char *const *, pmOptions *);
extern int pmGetContextOptions(int, pmOptions *);
extern void pmUsageMessage(pmOptions *);
extern void pmFreeOptions(pmOptions *);

/*
 * Derived Metrics support
 */
extern int pmLoadDerivedConfig(const char *);
extern char *pmRegisterDerived(const char *, const char *);
extern char *pmDerivedErrStr(void);

/*
 * Event Record support
 */
typedef struct pmEventParameter {
    pmID		ep_pmid;
    /* vtype and vlen fields the format same as for pmValueBlock */
#ifdef HAVE_BITFIELDS_LTOR
    unsigned int	ep_type : 8;	/* value type */
    unsigned int	ep_len : 24;	/* bytes for type/len + vbuf */
#else
    unsigned int	ep_len : 24;	/* bytes for type/len + vbuf */
    unsigned int	ep_type : 8;	/* value type */
#endif
    /* actual value (vbuf) goes here ... */
} pmEventParameter;

typedef struct pmEventRecord {
    __pmTimeval		er_timestamp;	/* must be 2 x 32-bit format */
    unsigned int	er_flags;	/* event record characteristics */
    int			er_nparams;	/* number of er_param[] entries */
    pmEventParameter	er_param[1];
} pmEventRecord;

typedef struct pmHighResEventRecord {
    __pmTimespec	er_timestamp;	/* must be 2 x 64-bit format */
    unsigned int	er_flags;	/* event record characteristics */
    int			er_nparams;	/* number of er_param[] entries */
    pmEventParameter	er_param[1];
} pmHighResEventRecord;

/* Potential flags bits set in er_flags (above) */
#define PM_EVENT_FLAG_POINT	(1U<<0)	/* an observation, default type */
#define PM_EVENT_FLAG_START	(1U<<1)	/* marking start of a new event */
#define PM_EVENT_FLAG_END	(1U<<2)	/* completion of a traced event */
#define PM_EVENT_FLAG_ID	(1U<<3)	/* 1st parameter is a trace ID */
#define PM_EVENT_FLAG_PARENT	(1U<<4)	/* 2nd parameter is parents ID */
#define PM_EVENT_FLAG_MISSED	(1U<<31)/* nparams shows #missed events */

typedef struct pmEventArray {
		/* align initial declarations with start of pmValueBlock */
#ifdef HAVE_BITFIELDS_LTOR
    unsigned int	ea_type : 8;	/* value type */
    unsigned int	ea_len : 24;	/* bytes for type/len + vbuf */
#else
    unsigned int	ea_len : 24;	/* bytes for type/len + vbuf */
    unsigned int	ea_type : 8;	/* value type */
#endif
		/* real event records start here */
    int			ea_nrecords;    /* number of ea_record[] entries */
    pmEventRecord	ea_record[1];
} pmEventArray;

typedef struct pmHighResEventArray {
		/* align initial declarations with start of pmValueBlock */
#ifdef HAVE_BITFIELDS_LTOR
    unsigned int	ea_type : 8;	/* value type */
    unsigned int	ea_len : 24;	/* bytes for type/len + vbuf */
#else
    unsigned int	ea_len : 24;	/* bytes for type/len + vbuf */
    unsigned int	ea_type : 8;	/* value type */
#endif
		/* real event records start here */
    int			ea_nrecords;    /* number of ea_record[] entries */
    pmHighResEventRecord ea_record[1];
} pmHighResEventArray;

/* Unpack a PM_TYPE_EVENT value into a set on pmResults */
extern int pmUnpackEventRecords(pmValueSet *, int, pmResult ***);

/* Free set of pmResults from pmUnpackEventRecords */
extern void pmFreeEventResult(pmResult **);

/* Unpack a PM_TYPE_HIGHRES_EVENT value into a set on pmHighResResults */
extern int pmUnpackHighResEventRecords(pmValueSet *, int, pmHighResResult ***);

/* Free set of pmHighResResults from pmUnpackEventRecords */
extern void pmFreeHighResEventResult(pmHighResResult **);

/* Service discovery, for clients. */
#define PM_SERVER_SERVICE_SPEC	"pmcd"
#define PM_SERVER_PROXY_SPEC	"pmproxy"
#define PM_SERVER_WEBD_SPEC	"pmwebd"

extern int pmDiscoverServices(const char *, const char *, char ***);

extern int pmParseUnitsStr(const char *, pmUnits *, double *, char **);

#ifdef __cplusplus
}
#endif

#endif /* PCP_PMAPI_H */
