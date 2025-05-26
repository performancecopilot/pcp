/*
 * Copyright (c) 2012-2022 Red Hat.
 * Copyright (c) 1997,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef PCP_PMAPI_H
#define PCP_PMAPI_H

/*
 * Platform and environment customization
 */
#include "platform_defs.h"

#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h> 
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Historical note:
 *
 * PMAPI_VERSION 1 only lasted for a short time in 1993-1998 (the
 * initial development and the PCP 1.1 release in April 1995).
 * PMAPI_VERSION 2 was part of the PCP 2.1 release in May 1998 and was the
 * long-standing version that evolved over the next 25+ years largely with
 * backwards compatibility.
 * Changes to make PCP Y2038-safe involved moving to 64-bit precision for
 * all timestamps (and for consistency, time intervals), and these
 * changes could not be done in a manner that was backwards compatibible,
 * so the PMAPI_VERSION had to move forwards.  But at some point (the
 * reasons are lost in the mists of time), the libpcp DSO had moved from
 * version 2 to version 3.  To align these two, the new PMAPI_VERSION is
 * 4.
 * Like the Travelling Wilbury's Volume 2, PCP's PMAPI_VERSION 3 never
 * existed.
 */

#define PMAPI_VERSION_2	2	/* traditional PMAPI */
#define PMAPI_VERSION_3	3	/* never existed */
#define PMAPI_VERSION_4	4	/* timeval -> timespec */
#ifndef PMAPI_VERSION
#define PMAPI_VERSION	PMAPI_VERSION_4   /* current default */
#endif

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
#define PM_SPACE_KBYTE	1	/* kibibytes (1024) */
#define PM_SPACE_MBYTE	2	/* mebibytes (1024^2) */
#define PM_SPACE_GBYTE	3	/* gibibytes (1024^3) */
#define PM_SPACE_TBYTE	4	/* tebibytes (1024^4) */
#define PM_SPACE_PBYTE	5	/* pebibytes (1024^5) */
#define PM_SPACE_EBYTE	6	/* exbibytes (1024^6) */
#define PM_SPACE_ZBYTE	7	/* zebibytes (1024^7) */
#define PM_SPACE_YBYTE	8	/* yobibytes (1024^8) */
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
#define PM_ERR_TEXT		(-PM_ERR_BASE-4)    /* One-line or help text is not available */
#define PM_ERR_APPVERSION	(-PM_ERR_BASE-5)    /* Metric not supported by this version of monitored application */
#define PM_ERR_VALUE		(-PM_ERR_BASE-6)    /* Missing metric value(s) */
/* retired PM_ERR_LICENSE (-PM_ERR_BASE-7) Current PCP license does not permit this operation */
#define PM_ERR_TIMEOUT		(-PM_ERR_BASE-8)    /* Timeout waiting for a response from PMCD */
#define PM_ERR_NODATA		(-PM_ERR_BASE-9)    /* Empty archive file */
#define PM_ERR_RESET		(-PM_ERR_BASE-10)   /* PMCD reset or configuration change */
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
#define PM_ERR_EOL		(-PM_ERR_BASE-25)   /* End of PCP archive */
#define PM_ERR_MODE		(-PM_ERR_BASE-26)   /* Illegal mode specification */
#define PM_ERR_LABEL		(-PM_ERR_BASE-27)   /* Illegal label record at start of a PCP archive file */
#define PM_ERR_LOGREC		(-PM_ERR_BASE-28)   /* Corrupted record in a PCP archive */
#define PM_ERR_NOTARCHIVE	(-PM_ERR_BASE-29)   /* Operation requires context with archive source of metrics */
#define PM_ERR_LOGFILE          (-PM_ERR_BASE-30)   /* Missing PCP archive file */
#define PM_ERR_NOCONTEXT	(-PM_ERR_BASE-31)   /* Attempt to use an illegal context */
#define PM_ERR_PROFILESPEC	(-PM_ERR_BASE-32)   /* NULL pmInDom with non-NULL instlist */
#define PM_ERR_PMID_LOG		(-PM_ERR_BASE-33)   /* Metric not defined in the PCP archive */
#define PM_ERR_INDOM_LOG	(-PM_ERR_BASE-34)   /* Instance domain identifier not defined in the PCP archive */
#define PM_ERR_INST_LOG		(-PM_ERR_BASE-35)   /* Instance identifier not defined in the PCP archive */
#define PM_ERR_NOPROFILE	(-PM_ERR_BASE-36)   /* Missing profile - protocol botch */
#define	PM_ERR_NOAGENT		(-PM_ERR_BASE-41)   /* No pmcd agent for domain of request */
#define PM_ERR_PERMISSION	(-PM_ERR_BASE-42)   /* No permission to perform requested operation */
#define PM_ERR_CONNLIMIT	(-PM_ERR_BASE-43)   /* PMCD connection limit for this host exceeded */
#define PM_ERR_AGAIN		(-PM_ERR_BASE-44)   /* Try again. Information not currently available */
#define PM_ERR_ISCONN		(-PM_ERR_BASE-45)   /* Already Connected */
#define PM_ERR_NOTCONN		(-PM_ERR_BASE-46)   /* Not Connected */
#define PM_ERR_NEEDPORT		(-PM_ERR_BASE-47)   /* A non-null port name is required */
/* retired PM_ERR_WANTACK (-PM_ERR_BASE-48) can not send due to pending acks */
#define PM_ERR_NONLEAF		(-PM_ERR_BASE-49)   /* Metric name is not a leaf in PMNS */
/* retired PM_ERR_OBJSTYLE (-PM_ERR_BASE-50) user/kernel object style mismatch */
/* retired PM_ERR_PMCDLICENSE (-PM_ERR_BASE-51) PMCD is not licensed to accept connections */
#define PM_ERR_TYPE		(-PM_ERR_BASE-52)   /* Unknown or illegal metric type */
#define PM_ERR_THREAD		(-PM_ERR_BASE-53)   /* Operation not supported for multi-threaded applications */
#define PM_ERR_NOCONTAINER	(-PM_ERR_BASE-54)   /* Container not found */
#define PM_ERR_BADSTORE		(-PM_ERR_BASE-55)   /* Bad input to pmstore */
#define PM_ERR_LOGOVERLAP	(-PM_ERR_BASE-56)   /* Archives overlap in time */
#define PM_ERR_LOGHOST		(-PM_ERR_BASE-57)   /* Archives differ by host */
  /* retired PM_ERR_LOGTIMEZONE	(-PM_ERR_BASE-58) Archives differ in time zone */
#define PM_ERR_LOGCHANGETYPE	(-PM_ERR_BASE-59)   /* The type of a metric has changed in an archive */
#define PM_ERR_LOGCHANGESEM	(-PM_ERR_BASE-60)   /* The semantics of a metric has changed in an archive */
#define PM_ERR_LOGCHANGEINDOM	(-PM_ERR_BASE-61)   /* The instance domain of a metric has changed in an archive */
#define PM_ERR_LOGCHANGEUNITS	(-PM_ERR_BASE-62)   /* The units of a metric have changed in an archive */
#define PM_ERR_NEEDCLIENTCERT	(-PM_ERR_BASE-63)   /* PMCD requires a client certificate */
#define PM_ERR_BADDERIVE	(-PM_ERR_BASE-64)   /* Derived metric definition failed */
#define PM_ERR_NOLABELS		(-PM_ERR_BASE-65)   /* No support for metric label metadata */
#define PM_ERR_PMDAFENCED	(-PM_ERR_BASE-66)   /* PMDA is currently fenced and unable to respond to requests */
#define PM_ERR_RECTYPE		(-PM_ERR_BASE-67)   /* Incorrect record type in an archive */
#define PM_ERR_FEATURE		(-PM_ERR_BASE-68)   /* Archive feature not supported */
#define PM_ERR_TLS		(-PM_ERR_BASE-69)   /* TLS protocol failure */
#define PM_ERR_ARG		(-PM_ERR_BASE-70)   /* Bad value for function argument */

/* retired PM_ERR_CTXBUSY (-PM_ERR_BASE-97) Context is busy */
#define PM_ERR_BOTCH		(-PM_ERR_BASE-97)   /* Internal inconsistency detected or assertion failed */
#define PM_ERR_TOOSMALL		(-PM_ERR_BASE-98)   /* Insufficient elements in list */
#define PM_ERR_TOOBIG		(-PM_ERR_BASE-99)   /* Result size exceeded */
#define PM_ERR_FAULT		(-PM_ERR_BASE-100)  /* QA fault injected */

#define PM_ERR_PMDAREADY	(-PM_ERR_BASE-1048) /* PMDA is now responsive to requests */
#define PM_ERR_PMDANOTREADY	(-PM_ERR_BASE-1049) /* PMDA is not yet ready to respond to requests */
#define PM_ERR_NYI		(-PM_ERR_BASE-8999) /* Functionality not yet implemented */
						    /* this is the [end-of-possible-codes] mark as well */

/*
 * Report PMAPI errors messages
 */
PCP_CALL extern char *pmErrStr(int);			/* NOT thread-safe */
PCP_CALL extern char *pmErrStr_r(int, char *, int);
/* safe size for a pmErrStr_r buffer to accommodate all error messages */
#define PM_MAXERRMSGLEN		128

/*
 * Load a Performance Metrics Name Space
 */
PCP_CALL extern int pmLoadNameSpace(const char *);
PCP_CALL extern int pmLoadASCIINameSpace(const char *, int);
PCP_CALL extern void pmUnloadNameSpace(void);

/*
 * Where is PMNS located - added for distributed PMNS.
 */
PCP_CALL extern int pmGetPMNSLocation(void);
#define PMNS_LOCAL   1
#define PMNS_REMOTE  2
#define PMNS_ARCHIVE 3

/*
 * Trim a name space with respect to the current context
 * (usually from an archive, or after processing an archive)
 */
PCP_CALL extern int pmTrimNameSpace(void);

/*
 * Expand a list of names to a list of metrics ids
 */
PCP_CALL extern int pmLookupName(int, const char **, pmID *);

/*
 * Find the names of descendent nodes in the PMNS
 * and in the latter case get the status of each child.
 */
PCP_CALL extern int pmGetChildren(const char *, char ***);
PCP_CALL extern int pmGetChildrenStatus(const char *, char ***, int **);
#define PMNS_LEAF_STATUS     0	/* leaf node in PMNS tree */
#define PMNS_NONLEAF_STATUS  1	/* non-terminal node in PMNS tree */

/*
 * Reverse Lookup: find name(s) given a metric id
 */
PCP_CALL extern int pmNameID(pmID, char **);		/* one */
PCP_CALL extern int pmNameAll(pmID, char ***);		/* all */

/*
 * Handy recursive descent of the PMNS
 */
PCP_CALL extern int pmTraversePMNS(const char *, void(*)(const char *));
PCP_CALL extern int pmTraversePMNS_r(const char *, void(*)(const char *, void *), void *);

/*
 * Given a metric, find it's descriptor (caller supplies buffer for desc),
 * from the current context.  Both singular and multi-descriptor variants.
 */
PCP_CALL extern int pmLookupDesc(pmID, pmDesc *);
PCP_CALL extern int pmLookupDescs(int, pmID *, pmDesc *);

/*
 * Return the internal instance identifier, from the current context,
 * given an instance domain and the external instance name.
 * Archive variant scans the union of the indom entries in the archive.
 */
PCP_CALL extern int pmLookupInDom(pmInDom, const char *);
PCP_CALL extern int pmLookupInDomArchive(pmInDom, const char *);

/*
 * Return the external instance name, from the current context,
 * given an instance domain and the internal instance identifier.
 * Archive variant scans the union of the indom entries in the archive.
 */
PCP_CALL extern int pmNameInDom(pmInDom, int, char **);
PCP_CALL extern int pmNameInDomArchive(pmInDom, int, char **);

/*
 * Return all of the internal instance identifiers (instlist) and external
 * instance names (namelist) for the given instance domain in the current
 * context.
 * Archive variant returns the union of the indom entries in the archive.
 */
PCP_CALL extern int pmGetInDom(pmInDom, int **, char ***);
PCP_CALL extern int pmGetInDomArchive(pmInDom, int **, char ***);

/*
 * Given context ID, return the host name associated with that context,
 * or the empty string if no name can be found
 */
PCP_CALL extern int pmGetHostName(int, char *, int);
PCP_CALL extern const char *pmGetContextHostName(int);
PCP_CALL extern char *pmGetContextHostName_r(int, char *, int);

/*
 * Return the handle of the current context
 */
PCP_CALL extern int pmWhichContext(void);

/*
 * Destroy a context and close its connection
 */
PCP_CALL extern int pmDestroyContext(int);

/*
 * Establish a new context (source of performance data + instance profile)
 * for the named host
 */
PCP_CALL extern int pmNewContext(int, const char *);
#define PM_CONTEXT_UNDEF	-1	/* current context is undefined */
#define PM_CONTEXT_HOST		1	/* host via pmcd */
#define PM_CONTEXT_ARCHIVE	2	/* PCP archive */
#define PM_CONTEXT_LOCAL	3	/* local host, no pmcd connection */
#define PM_CONTEXT_TYPEMASK	0xff	/* mask to separate types / flags */
#define PM_CTXFLAG_SHALLOW	(1U<<8)	/* DEPRECATED (don't actually connect to host) */
#define PM_CTXFLAG_EXCLUSIVE	(1U<<9)	/* DEPRECATED (don't share socket among ctxts) */
#define PM_CTXFLAG_SECURE	(1U<<10)/* encrypted socket comms channel */
#define PM_CTXFLAG_COMPRESS	(1U<<11)/* compressed socket host channel */
#define PM_CTXFLAG_RELAXED	(1U<<12)/* encrypted if possible else not */
#define PM_CTXFLAG_AUTH		(1U<<13)/* make authenticated connection */
#define PM_CTXFLAG_CONTAINER	(1U<<14)/* container connection attribute */
					/* don't check V3 archive features */
#define PM_CTXFLAG_NO_FEATURE_CHECK	(1U<<15) /* don't check features in label record */
#define PM_CTXFLAG_METADATA_ONLY	(1U<<16) /* only open .meta file of archive */
#define PM_CTXFLAG_LAST_VOLUME	(1U<<17) /* open archive at start of last volume */

/*
 * Duplicate current context -- returns handle to new one for pmUseContext()
 */
PCP_CALL extern int pmDupContext(void);

/*
 * Restore (previously established or duplicated) context
 */
PCP_CALL extern int pmUseContext(int);

/*
 * Reconnect an existing context (when pmcd dies, etc). All existing context
 * settings are preserved and the previous context settings are restored.
 */
PCP_CALL extern int pmReconnectContext(int);

/*
 * Add to instance profile.
 * If pmInDom == PM_INDOM_NULL, then all instance domains are selected.
 * If no inst parameters are given, then all instances are selected.
 * e.g. to select all available instances in all domains,
 * then use pmAddProfile(PM_INDOM_NULL, 0, NULL).
 */
PCP_CALL extern int pmAddProfile(pmInDom, int, int *);

/*
 * Delete from instance profile.
 * Similar (but negated) functional semantics to pmProfileAdd.
 * E.g. to disable all instances in all domains then use
 * pmDelProfile(PM_INDOM_NULL, 0, NULL).
 */
PCP_CALL extern int pmDelProfile(pmInDom, int, int *);

/*
 * Profile entry (per instance domain)
 * Only the PMDAs and pmcd need to know about this.
 */
typedef struct pmInDomProfile {
    pmInDom	indom;			/* instance domain */
    int		state;			/* include all or exclude all */
    int		instances_len;		/* length of instances array */
    int		*instances;		/* array of instances */
} pmInDomProfile;

/* Internal instance profile states: state in pmInDomProfile */
#define PM_PROFILE_INCLUDE 0	/* include all, exclude some */
#define PM_PROFILE_EXCLUDE 1	/* exclude all, include some */

/*
 * Instance profile for all domains
 * Only the PMDAs and pmcd need to know about this.
 */
typedef struct pmProfile {
    int			state;			/* default global state */
    int			profile_len;		/* length of profile array */
    pmInDomProfile	*profile;		/* array of instance profiles */
} pmProfile;

/*
 * Result structure for instance domain queries
 * Only the PMDAs, pmcd and libpcp_archive clients need to know about this.
 */
typedef struct pmInResult {
    pmInDom	indom;		/* instance domain */
    int		numinst;	/* may be 0 */
    int		*instlist;	/* instance ids, may be NULL */
    char	**namelist;	/* instance names, may be NULL */
} pmInResult;

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

/*
 * Result returned by pmFetchHighRes() and high resolution event timer
 * result from pmUnpackHighResEventRecords()
 */
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
 * the number of metrics in the argument.  
 */
PCP_CALL extern int pmFetch(int, pmID *, pmResult **);
PCP_CALL extern int pmFetchHighRes(int, pmID *, pmHighResResult **);
/* older name maintained for backwards compatibility */
PCP_CALL extern int pmHighResFetch(int, pmID *, pmHighResResult **);

/*
 * PMCD state changes returned as fetch function results for PM_CONTEXT_HOST
 * contexts, i.e. when communicating with PMCD
 */
#define PMCD_ADD_AGENT		(1<<0)
#define PMCD_RESTART_AGENT	(1<<1)
#define PMCD_DROP_AGENT		(1<<2)

#define PMCD_NO_CHANGE		0
#define PMCD_AGENT_CHANGE	\
	(PMCD_ADD_AGENT | PMCD_RESTART_AGENT | PMCD_DROP_AGENT)
#define PMCD_LABEL_CHANGE	(1<<3)
#define PMCD_NAMES_CHANGE	(1<<4)
#define PMCD_HOSTNAME_CHANGE	(1<<5)

/*
 * Variant that is used to return a result from an archive.
 */
PCP_CALL extern int pmFetchArchive(pmResult **);
PCP_CALL extern int pmFetchHighResArchive(pmHighResResult **);

/*
 * Support for metric values annotated with name:value pairs (labels).
 *
 * The full set of labels for a given metric instance is the union of
 * those found at the levels: source (host/archive), domain (agent),
 * indom, metric, and finally instances (individual metric values).
 *
 * Individual labels can be intrinsic to a metric value (i.e. they
 * form an inherent part of its identity like the pmDesc metadata)
 * or extrinsic (i.e. they are simply optional annotations).
 */
typedef struct pmLabel {
    unsigned int	name : 16;	/* label name offset in JSONB string */
    unsigned int	namelen : 8;	/* length of name excluding the null */
    unsigned int	flags : 8;	/* information about this label */
    unsigned int	value : 16;	/* offset of the label value */
    unsigned int	valuelen : 16;	/* length of value in bytes */
} pmLabel;

/* Bits for the flags field (above) */
#define PM_LABEL_COMPOUND	(1<<6)	/* name has multiple components */
#define PM_LABEL_OPTIONAL	(1<<7)	/* intrinsic / extrinsic label */

typedef struct pmLabelSet {
    unsigned int 	inst;		/* PM_IN_NULL or the instance ID */
    int			nlabels;	/* count of labels or error code */
    char		*json;		/* JSON formatted labels string */
    unsigned int	jsonlen : 16;	/* JSON string length byte count */
    unsigned int	padding : 15;	/* zero, reserved for future use */
    unsigned int	compound: 1;	/* flag indicating compound names */
    pmLabel		*labels;	/* indexing into the JSON string */
    void		*hash;		/* compound naming hash (opaque) */
} pmLabelSet;

#define PM_MAXLABELS		((1<<8)-1)
#define PM_MAXLABELJSONLEN	((1<<16)-1)

/* These identify label set classes. */
#define PM_LABEL_CONTEXT	(1<<0)
#define PM_LABEL_DOMAIN		(1<<1)
#define PM_LABEL_INDOM		(1<<2)
#define PM_LABEL_CLUSTER	(1<<3)
#define PM_LABEL_ITEM		(1<<4)
#define PM_LABEL_INSTANCES	(1<<5)

PCP_CALL extern int pmGetContextLabels(pmLabelSet **);
PCP_CALL extern int pmGetDomainLabels(int, pmLabelSet **);
PCP_CALL extern int pmGetInDomLabels(pmInDom, pmLabelSet **);
PCP_CALL extern int pmGetClusterLabels(pmID, pmLabelSet **);
PCP_CALL extern int pmGetItemLabels(pmID, pmLabelSet **);
PCP_CALL extern int pmGetInstancesLabels(pmInDom, pmLabelSet **);

PCP_CALL extern int pmLookupLabels(pmID, pmLabelSet **);

/*
 * The full set is formed by merging labels from all levels of the
 * hierarchy using the precedence rules described in pmLookupLabels(3).
 */
PCP_CALL extern int pmMergeLabels(char **, int, char *, int);
PCP_CALL extern int pmMergeLabelSets(pmLabelSet **, int, char *, int,
		int (*filter)(const pmLabel *, const char *, void *), void *);

/* Free a labelset array */
PCP_CALL extern void pmFreeLabelSets(pmLabelSet *, int);

/*
 * struct timeval is sometimes 2 x 64-bit ... for backwards compatibility
 * we use a 2 x 32-bit format for down-rev PDUs, and on-disk in version 2
 * archives.  Current PDUs and on-disk format version 3 do not use 32
 * bit seconds in timestamps.
 */
typedef struct pmTimeval {
    __int32_t	tv_sec;		/* seconds since Jan. 1, 1970 */
    __int32_t	tv_usec;	/* and microseconds */
} pmTimeval;

typedef struct pmTimespec {
    __int64_t	tv_sec;		/* seconds since Jan. 1, 1970 */
    __int64_t	tv_nsec;	/* and nanoseconds */
} pmTimespec;

/*
 * Label Record at the start of every archive file - as exported above
 * the PMAPI ...
 * NOTE	that the struct timeval means we have another struct (__pmLogLabel)
 *	for internal use that has a __pmTimestamp in place of the struct
 *	timeval.
 */
#define PM_LOG_MAGIC		0x50052600
#define PM_LOG_VERS02		0x2
#define PM_LOG_VERS03		0x3
#define PM_LOG_VOL_TI		-2	/* temporal index */
#define PM_LOG_VOL_META		-1	/* meta data */
#define PM_LOG_MAXHOSTLEN	64	/* V2 only, deprecated with V3 */
#define PM_TZ_MAXLEN		40	/* V2 only, deprecated with V3 */
#define PM_MAX_HOSTNAMELEN	256	/* max supported for V3 onward */
#define PM_MAX_TIMEZONELEN	256	/* max supported for V3 onward */
#define PM_MAX_ZONEINFOLEN	256	/* max supported (new with V3) */

/*
 * feature bits for V3 archives
 */
#define PM_LOG_FEATURE_NONE	0
#define PM_LOG_FEATURE_QA	(1U<<31)	/* QA not for general use */
/* the currently supported feature bits */
#define PM_LOG_FEATURES		(PM_LOG_FEATURE_NONE | PM_LOG_FEATURE_QA)

typedef struct pmLogLabel {
    int		magic;	/* PM_LOG_MAGIC | archive format version no. */
    pid_t	pid;		/* PID of logger */
    struct timespec start;	/* start of this archive */
    char	hostname[PM_MAX_HOSTNAMELEN];	/* collection host full name */
    char	timezone[PM_MAX_TIMEZONELEN];	/* generic, squashed $TZ */
    char	zoneinfo[PM_MAX_ZONEINFOLEN];	/* local platform $TZ */
} pmLogLabel;

/*
 * Get the label record from the current archive context, and discover
 * when the archive ends
 */
PCP_CALL extern int pmGetArchiveLabel(pmLogLabel *);
PCP_CALL extern int pmGetArchiveEnd(struct timespec *);

/* Free result buffer */
PCP_CALL extern void pmFreeHighResResult(pmHighResResult *);
PCP_CALL extern void pmFreeResult(pmResult *);

/* Value extract from pmValue and type conversion */
PCP_CALL extern int pmExtractValue(int, const pmValue *, int, pmAtomValue *, int);

/* Print single pmValue */
PCP_CALL extern void pmPrintValue(FILE *, int, int, const pmValue *, int);

/* Scale conversion, based on value format, value type and scale */
PCP_CALL extern int pmConvScale(int, const pmAtomValue *, const pmUnits *, pmAtomValue *, 
		       const pmUnits *);

/* Sort instances for each metric within a pmResult */
PCP_CALL extern void pmSortInstances(pmResult *);
PCP_CALL extern void pmSortHighResInstances(pmHighResResult *);

/* Adjust collection time and/or mode for pmFetch */
PCP_CALL extern int pmSetMode(int, const struct timespec *, const struct timespec *);
#define PM_MODE_LIVE	0
#define PM_MODE_INTERP	1
#define PM_MODE_FORW	2
#define PM_MODE_BACK	3

/* Modify the value of one or more metrics */
PCP_CALL extern int pmStore(const pmResult *);
PCP_CALL extern int pmStoreHighRes(const pmHighResResult *);

/* Get help and descriptive text */
PCP_CALL extern int pmLookupText(pmID, int, char **);
PCP_CALL extern int pmLookupInDomText(pmInDom, int, char **);
#define PM_TEXT_ONELINE		1
#define PM_TEXT_HELP		2

/*
 * For the help text PDUs, the type (PM_TEXT_ONELINE or PM_TEXT_HELP)
 * is 'or'd with the following to encode the request for a PMID or
 * a pmInDom.  Default is to fallback to ONELINE if HELP unavailable;
 * the (internal) PM_TEXT_DIRECT flag disables this behaviour.
 * Note the values must therefore be (a) bit fields and (b) different
 *	to the public macros PM_TEXT_* in pmapi.h 
 */
#define PM_TEXT_PMID	4
#define PM_TEXT_INDOM	8
#define PM_TEXT_DIRECT	16

/*
 * Some handy formatting routines for messages, and other output
 */
PCP_CALL extern const char *pmIDStr(pmID);			/* NOT thread-safe */
PCP_CALL extern char *pmIDStr_r(pmID, char *, int);
PCP_CALL extern const char *pmInDomStr(pmInDom);		/* NOT thread-safe */
PCP_CALL extern char *pmInDomStr_r(pmInDom, char *, int);
PCP_CALL extern const char *pmTypeStr(int);			/* NOT thread-safe */
PCP_CALL extern char *pmTypeStr_r(int, char *, int);
PCP_CALL extern const char *pmSemStr(int);			/* NOT thread-safe */
PCP_CALL extern char *pmSemStr_r(int, char *, int);
PCP_CALL extern const char *pmUnitsStr(const pmUnits *);	/* NOT thread-safe */
PCP_CALL extern char *pmUnitsStr_r(const pmUnits *, char *, int);
PCP_CALL extern const char *pmAtomStr(const pmAtomValue *, int);/* NOT thread-safe */
PCP_CALL extern char *pmAtomStr_r(const pmAtomValue *, int, char *, int);
PCP_CALL extern const char *pmNumberStr(double);		/* NOT thread-safe */
PCP_CALL extern char *pmNumberStr_r(double, char *, int);
PCP_CALL extern const char *pmEventFlagsStr(int);		/* NOT thread-safe */
PCP_CALL extern char *pmEventFlagsStr_r(int, char *, int);

/* Parse -t, -S, -T, -A and -O options */
PCP_CALL extern int pmParseInterval(const char *, struct timespec *, char **);
PCP_CALL extern int pmParseTimeWindow(
      const char *, const char *, const char *, const char *,
      const struct timespec *, const struct timespec *,
      struct timespec *, struct timespec *, struct timespec *, char **);

/* Sentinel value for end-time parameters in time window parsing */
#if PM_SIZEOF_TIME_T == 8
#define PM_MAX_TIME_T	LONGLONG_MAX
#else
#define PM_MAX_TIME_T	INT_MAX
#endif

/* Reporting timezone */
PCP_CALL extern int pmUseZone(const int);
PCP_CALL extern int pmNewZone(const char *);
PCP_CALL extern int pmNewContextZone(void);
PCP_CALL extern int pmWhichZone(char **);
PCP_CALL extern char *pmCtime(const time_t *, char *);
PCP_CALL extern struct tm *pmLocaltime(const time_t *, struct tm *);

/* Parse host:metric[instances] or archive/metric[instances] */
typedef struct pmMetricSpec {
    int		isarch;         /* source type: 0 -> live host, 1 -> archive, 2 -> local context */
    char	*source;        /* name of source host or archive */
    char	*metric;        /* name of metric */
    int		ninst;          /* number of instances, 0 -> all */
    char	*inst[1];       /* array of instance names */
} pmMetricSpec;

/* Parsing of host:metric[instances] or archive/metric[instances] */
PCP_CALL extern int pmParseMetricSpec(const char *, int, char *, pmMetricSpec **,
			     char **);
PCP_CALL extern void pmFreeMetricSpec(pmMetricSpec *p);

/*
 * Configurable error reporting
 */
#ifdef __GNUC__
# define __PM_PRINTFLIKE(idx,cnt) __attribute__ ((format (printf, idx,cnt)))
#else
# define __PM_PRINTFLIKE(idx,cnt)
#endif

PCP_CALL extern int pmprintf(const char *, ...) __PM_PRINTFLIKE(1,2);
PCP_CALL extern int pmflush(void);

/*
 * Wrapper for string formatting that ensures null termination,
 * even if truncation occurs or the underlying call errors out.
 */
PCP_CALL extern int pmsprintf(char *, size_t, const char *, ...) __PM_PRINTFLIKE(3,4);

/*
 * Safe version of fscanf("...%s...", buf) ... buf dynamically allocated,
 * guaranteed to be null-byte terminated and returns strlen(buf)
 */
PCP_CALL extern ssize_t pmfstring(FILE *f, char **);

/*
 * Safe version of strlen(s) that handles null pointer input, in
 * which case zero is returned.
 */
PCP_CALL extern size_t pmstrlen(const char *);

/*
 * Safe version of strncpy() ... args are deliberately different to
 * strncpy() to guard against accidential misuse ... length is length
 * of dest, not src ... also result is 0/-1 for success/truncation
 * rather than the useless (!) char * = dest
 */
PCP_CALL extern int pmstrncpy(char *, size_t, const char *);

/*
 * Safe version of strncat() ... args are deliberately different to
 * strncat() to guard against accidential misuse ... length is length
 * of dest, not src ... also result is 0/-1 for success/truncation
 * rather than the useless (!) char * = dest
 */
PCP_CALL extern int pmstrncat(char *, size_t, const char *);

/*
 * Wrapper for config/environment variables. Warning: this will exit() with
 * a FATAL error if /etc/pcp.conf does not exist and $PCP_CONF is not set.
 * Use the pmGetOptionalConfig variant if this behaviour is not sought.
 * Return values point to strings in the environment, or NULL in the optional
 * case when the requested configuration variable was not found.
 */
PCP_CALL extern char *pmGetConfig(const char *);
PCP_CALL extern char *pmGetOptionalConfig(const char *);

/* Ditto for library features */
PCP_CALL extern const char *pmGetAPIConfig(const char *);

PCP_CALL extern int pmGetVersion(void);

/*
 * Common command line argument parsing interfaces
 */

#define PMAPI_OPTIONS	"A:a:D:gh:n:O:p:S:s:T:t:VZ:z?"
#define PMAPI_OPTIONS_HEADER(s)	{ "", 0, '-', 0, (s) }
#define PMAPI_OPTIONS_TEXT(s)	{ "", 0, '|', 0, (s) }
#define PMAPI_OPTIONS_END	{ NULL, 0, 0, 0, NULL }

#define PMLONGOPT_ALIGN		"align"
#define PMOPT_ALIGN	{ PMLONGOPT_ALIGN,	1, 'A',	"TIME", \
			"align sample times on natural boundaries" }
#define PMLONGOPT_ARCHIVE	"archive"
#define PMOPT_ARCHIVE	{ PMLONGOPT_ARCHIVE,	1, 'a',	"FILE", \
			"metrics source is a PCP archive" }
#define PMLONGOPT_DEBUG		"debug"
#define PMOPT_DEBUG	{ PMLONGOPT_DEBUG,	1, 'D',	"DBG", \
			"set debug options, see pmdbg(1)" }
#define PMLONGOPT_GUIMODE	"guimode"
#define PMOPT_GUIMODE	{ PMLONGOPT_GUIMODE,	0, 'g',	0, \
			"start in GUI mode with new time control" }
#define PMLONGOPT_HOST		"host"
#define PMOPT_HOST	{ PMLONGOPT_HOST,	1, 'h', "HOST", \
			"metrics source is PMCD on host" }
#define PMLONGOPT_HOSTSFILE	"hostsfile"
#define PMOPT_HOSTSFILE	{ PMLONGOPT_HOSTSFILE,	1, 'H', "FILE", \
			"read metric source hosts from a file" }
#define PMLONGOPT_SPECLOCAL	"spec-local"
#define PMOPT_SPECLOCAL	{ PMLONGOPT_SPECLOCAL,	1, 'K',	"SPEC", \
			"optional additional PMDA spec for local connection" }
#define PMLONGOPT_LOCALPMDA	"local-PMDA"
#define PMOPT_LOCALPMDA	{ PMLONGOPT_LOCALPMDA,	0, 'L', 0, \
			"metrics source is local connection to a PMDA" }
#define PMLONGOPT_NAMESPACE	"namespace"
#define PMOPT_NAMESPACE	{ PMLONGOPT_NAMESPACE,	1, 'n', "FILE", \
			"use an alternative PMNS" }
#define PMLONGOPT_UNIQNAMES	"uniqnames"
#define PMOPT_UNIQNAMES	{ PMLONGOPT_UNIQNAMES,	1, 'N', "FILE", \
			"like -n but only one name allowed for each PMID" }
#define PMLONGOPT_ORIGIN	"origin"
#define PMOPT_ORIGIN	{ PMLONGOPT_ORIGIN,	1, 'O', "TIME", \
			"initial sample time within the time window" }
#define PMLONGOPT_GUIPORT	"guiport"
#define PMOPT_GUIPORT	{ PMLONGOPT_GUIPORT,	1, 'p', "N", \
			"port for connection to existing time control" }
#define PMLONGOPT_START		"start"
#define PMOPT_START	{ PMLONGOPT_START,	1, 'S', "TIME", \
			"start of the time window" }
#define PMLONGOPT_SAMPLES	"samples"
#define PMOPT_SAMPLES	{ PMLONGOPT_SAMPLES,	1, 's', "N", \
			"terminate after this many samples" }
#define PMLONGOPT_FINISH	"finish"
#define PMOPT_FINISH	{ PMLONGOPT_FINISH,	1, 'T', "TIME", \
			"end of the time window" }
#define PMLONGOPT_INTERVAL	"interval"
#define PMOPT_INTERVAL	{ PMLONGOPT_INTERVAL,	1, 't', "DELTA", \
			"sampling interval" }
#define PMLONGOPT_VERSION	"version"
#define PMOPT_VERSION	{ PMLONGOPT_VERSION,	0, 'V', 0, \
			"display version number and exit" }
#define PMLONGOPT_TIMEZONE	"timezone"
#define PMOPT_TIMEZONE	{ PMLONGOPT_TIMEZONE,	1, 'Z', "TZ", \
			"set reporting timezone" }
#define PMLONGOPT_HOSTZONE	"hostzone"
#define PMOPT_HOSTZONE	{ PMLONGOPT_HOSTZONE,	0, 'z', 0, \
			"set reporting timezone to local time of metrics source" }
#define PMLONGOPT_HELP		"help"
#define PMOPT_HELP	{ PMLONGOPT_HELP,	0, '?', 0, \
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
#define PMLONGOPT_DERIVED "derived"
#define PMOPT_DERIVED { PMLONGOPT_DERIVED, 1, 0, "FILE", \
		"load derived metric definitions from FILE(s)" }

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
    struct timespec	start;
    struct timespec	finish;
    struct timespec	origin;
    struct timespec	interval;
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

PCP_CALL extern int pmgetopt_r(int, char *const *, pmOptions *);
PCP_CALL extern int pmGetOptions(int, char *const *, pmOptions *);
PCP_CALL extern int pmGetContextOptions(int, pmOptions *);
PCP_CALL extern void pmUsageMessage(pmOptions *);
PCP_CALL extern void pmFreeOptions(pmOptions *);

/*
 * Derived Metrics support
 */
PCP_CALL extern int pmLoadDerivedConfig(const char *);
PCP_CALL extern int pmRegisterDerivedMetric(const char *, const char *, char **);
PCP_CALL extern char *pmRegisterDerived(const char *, const char *);
PCP_CALL extern char *pmDerivedErrStr(void);
PCP_CALL extern int pmAddDerivedMetric(const char *, const char *, char **);
PCP_CALL extern char *pmAddDerived(const char *, const char *);
PCP_CALL extern int pmAddDerivedText(const char *, int, const char *);
PCP_CALL extern int pmGetDerivedControl(int, int *);
PCP_CALL extern int pmSetDerivedControl(int, int);

/*
 * pm{Get,Set}DerivedControl() information types ...
 */
#define PCP_DERIVED_GLOBAL_LIMIT	1
#define PCP_DERIVED_CONTEXT_LIMIT	2
#define PCP_DERIVED_DEBUG_SYNTAX	3
#define PCP_DERIVED_DEBUG_SEMANTICS	4
#define PCP_DERIVED_DEBUG_EVAL		5

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
    pmTimeval		er_timestamp;	/* must be 2 x 32-bit format */
    unsigned int	er_flags;	/* event record characteristics */
    int			er_nparams;	/* number of er_param[] entries */
    pmEventParameter	er_param[1];
} pmEventRecord;

typedef struct pmHighResEventRecord {
    pmTimespec		er_timestamp;	/* must be 2 x 64-bit format */
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
PCP_CALL extern int pmUnpackEventRecords(pmValueSet *, int, pmResult ***);

/* Free set of pmResults from pmUnpackEventRecords */
PCP_CALL extern void pmFreeEventResult(pmResult **);

/* Unpack a PM_TYPE_HIGHRES_EVENT value into a set on pmHighResResults */
PCP_CALL extern int pmUnpackHighResEventRecords(pmValueSet *, int, pmHighResResult ***);

/* Free set of pmHighResResults from pmUnpackEventRecords */
PCP_CALL extern void pmFreeHighResEventResult(pmHighResResult **);

/* Service discovery, for clients. */
#define PM_SERVER_SERVICE_SPEC	"pmcd"
#define PM_SERVER_PROXY_SPEC	"pmproxy"
#define PM_SERVER_WEBAPI_SPEC	"pmwebapi"

PCP_CALL extern int pmDiscoverServices(const char *, const char *, char ***);

PCP_CALL extern int pmParseUnitsStr(const char *, pmUnits *, double *, char **);

typedef struct __pmFetchGroup *pmFG;	/* opaque handle */
PCP_CALL extern int pmCreateFetchGroup(pmFG *, int, const char *);
PCP_CALL extern int pmGetFetchGroupContext(pmFG);
PCP_CALL extern int pmExtendFetchGroup_item(pmFG, const char *, const char *,
			const char *, pmAtomValue *, int, int *);
PCP_CALL extern int pmExtendFetchGroup_indom(pmFG, const char *, const char *,
			int[], char *[], pmAtomValue[], int, int[],
			unsigned int, unsigned int *, int *);
PCP_CALL extern int pmExtendFetchGroup_event(pmFG, const char *, const char *,
			const char *, const char *,
			struct timespec[], pmAtomValue[], int, int[],
			unsigned int, unsigned int *, int *);
PCP_CALL extern int pmExtendFetchGroup_timestamp(pmFG, struct timeval *);
PCP_CALL extern int pmExtendFetchGroup_timespec(pmFG, struct timespec *);
PCP_CALL extern int pmFetchGroup(pmFG);
PCP_CALL extern int pmDestroyFetchGroup(pmFG);

/* libpcp debug/tracing */
PCP_CALL extern int pmSetDebug(const char *);
PCP_CALL extern int pmClearDebug(const char *);

/*
 * New style ...
 * Note that comments are important ... these are extracted and
 * built into pmdbg.h.
 * For the "add a new debug flag" recipe, see ../../libpcp/src/mk.pmdbg
 */
typedef struct {
    int	pdu;		/* PDU traffic at the Xmit and Recv level */
    int	fetch;		/* Results from pmFetch */
    int	profile;	/* Changes and xmits for instance profile */
    int	value;		/* Metric value extraction and conversion */
    int	context;	/* Changes to PMAPI contexts */
    int	indom;		/* Instance domain operations */
    int	pdubuf;		/* PDU buffer operations */
    int	log;		/* Archive manipulations */
    int	logmeta;	/* Archive metadata operations */
    int	optfetch;	/* optFetch magic */
    int	af;		/* Asynchronous event scheduling */
    int	appl0;		/* Application-specific flag 0 */
    int	appl1;		/* Application-specific flag 1 */
    int	appl2;		/* Application-specific flag 2 */
    int	pmns;		/* PMNS operations */
    int	libpmda;	/* PMDA operations in libpcp_pmda */
    int	timecontrol;	/* Time control API */
    int	pmc;		/* Metrics class operations */
    int	derive;		/* Derived metrics functionality */
    int	lock;		/* Synchronization and lock tracing */
    int	interp;		/* Interpolate mode for archives */
    int	config;		/* Configuration parameters */
    int	pmapi;		/* PMAPI call tracing */
    int	fault;		/* Fault injection tracing */
    int	auth;		/* Authentication tracing */
    int	discovery;	/* Service discovery tracing */
    int	attr;		/* Connection attribute handling */
    int	http;		/* Trace HTTP operations in libpcp_web */
    int	desperate;	/* Verbose/Desperate level (developers only) */
/* new ones start here, no DBG_TRACE_xxx macro and no backwards compatibility */
    int	deprecated;	/* Report use of deprecated services */
    int	exec;	 	/* __pmProcessExec and related calls */
    int labels;		/* Metric label metadata operations */
    int series;		/* Time series tracing */
    int	libweb;		/* Trace services from libpcp_web */
    int	alloc;		/* Miscellaneous alloc/free operations */
    int	services;	/* Services and daemons */
    int	appl3;		/* Application-specific flag 3 */
    int	appl4;		/* Application-specific flag 4 */
    int	appl5;		/* Application-specific flag 5 */
    int access;		/* Access controls */
    int qa;		/* QA (transient, developers only) */
    int search;		/* Text search tracing */
    int query;		/* libpcp_web query parsing and evaluation */
    int	compress;	/* Archive compress/decompress operations */
    int dev0;		/* Developer flag 0 */
    int dev1;		/* Developer flag 1 */
    int dev2;		/* Developer flag 2 */
    int	pmlc;		/* Protocol between pmlc and pmlogger */
    int	appl6;		/* Application-specific flag 6 */
    int	appl7;		/* Application-specific flag 7 */
    int	appl8;		/* Application-specific flag 8 */
    int	appl9;		/* Application-specific flag 9 */
    int	tls;		/* Transport Layer Security operations */
    int	misc;		/* Miscellaneous odds and sods */
    int	qed;		/* Methods in libpcp_qed */
    int getopt;		/* Processing of command-line arguments */
} pmdebugoptions_t;

PCP_DATA extern pmdebugoptions_t	pmDebugOptions;

/*
 * Startup handling:
 * set/get program name, as used in pmNotifyErr() ... default is "pcp"
 */
PCP_CALL extern void pmSetProgname(const char *);
PCP_CALL extern char *pmGetProgname(void);

/*
 * Special case PMIDs
 *   Domain DYNAMIC_PMID (number 511) is reserved for PMIDs representing
 *   the root of a dynamic subtree in the PMNS (and in this case the real
 *   domain number is encoded in the cluster field and the item field is
 *   zero).
 *   Domain DYNAMIC_PMID is also reserved for the PMIDs of derived metrics
 *   and in this case the item field is non-zero.  If a derived metric is
 *   written to a PCP archive, then the top bit is set in the cluster field
 *   (to disambiguate this from derived metics that must be evaluted on
 *   the pmFetch() path).
 */
#define DYNAMIC_PMID	511
#define IS_DYNAMIC_ROOT(x) (pmID_domain(x) == DYNAMIC_PMID && pmID_item(x) == 0)
#define IS_DERIVED(x) (pmID_domain(x) == DYNAMIC_PMID && (pmID_cluster(x) & 2048) != 2048 && pmID_item(x) != 0)
#define IS_DERIVED_LOGGED(x) (pmID_domain(x) == DYNAMIC_PMID && (pmID_cluster(x) & 2048) == 2048 && pmID_item(x) != 0)

/*
 * pmID helper functions
 */
PCP_CALL extern pmID pmID_build(unsigned int, unsigned int, unsigned int);
PCP_CALL extern unsigned int pmID_domain(pmID);
PCP_CALL extern unsigned int pmID_cluster(pmID);
PCP_CALL extern unsigned int pmID_item(pmID);

/*
 * pmInDom helper functions
 */
PCP_CALL extern pmInDom pmInDom_build(unsigned int, unsigned int);
PCP_CALL extern unsigned int pmInDom_domain(pmInDom);
PCP_CALL extern unsigned int pmInDom_serial(pmInDom);

/*
 * Create a diagnostic log file (not an archive)
 */
PCP_CALL extern FILE *pmOpenLog(const char *, const char *, FILE *, int *);

/*
 * no mem today, my love has gone away ....
 */
PCP_CALL extern void pmNoMem(const char *, size_t, int);
#define PM_FATAL_ERR 1
#define PM_RECOV_ERR 0

/* standard error, warning and info wrapper for syslog(3) */
PCP_CALL extern void pmNotifyErr(int, const char *, ...) __PM_PRINTFLIKE(2,3);
/* make pmNotifyErr also add entries to syslog */
PCP_CALL extern void pmSyslog(int);

PCP_CALL extern void pmPrintDesc(FILE *, const pmDesc *);
PCP_CALL extern void pmPrintLabelSets(FILE *, int, int, pmLabelSet *, int);

/* struct timeval manipulations */
PCP_CALL extern void pmtimevalNow(struct timeval *);
PCP_CALL extern void pmtimevalInc(struct timeval *, const struct timeval *);
PCP_CALL extern void pmtimevalDec(struct timeval *, const struct timeval *);
PCP_CALL extern double pmtimevalAdd(const struct timeval *, const struct timeval *);
PCP_CALL extern double pmtimevalSub(const struct timeval *, const struct timeval *);
PCP_CALL extern double pmtimevalToReal(const struct timeval *);
PCP_CALL extern void pmtimevalFromReal(double, struct timeval *);
PCP_CALL extern void pmPrintStamp(FILE *, const struct timeval *);

/* struct timespec manipulations */
PCP_CALL extern int pmtimespecNow(struct timespec *);
PCP_CALL extern void pmtimespecInc(struct timespec *, const struct timespec *);
PCP_CALL extern void pmtimespecDec(struct timespec *, const struct timespec *);
PCP_CALL extern double pmtimespecAdd(const struct timespec *, const struct timespec *);
PCP_CALL extern double pmtimespecSub(const struct timespec *, const struct timespec *);
PCP_CALL extern double pmtimespecToReal(const struct timespec *);
PCP_CALL extern void pmtimespecFromReal(double, struct timespec *);
PCP_CALL extern void pmPrintHighResStamp(FILE *, const struct timespec *);
PCP_CALL extern void pmPrintInterval(FILE *, const struct timespec *);

/* timespec <-> timeval conversions */
PCP_CALL extern void pmtimevalTotimespec(struct timeval *, struct timespec *);
PCP_CALL extern void pmtimespecTotimeval(struct timespec *, struct timeval *);

/* filesystem path name separator */
PCP_CALL extern int pmPathSeparator(void);

/* platform independent set process identity */
PCP_CALL extern int pmSetProcessIdentity(const char *);

/*
 * get special PCP user name (for pmSetProcessIdentity() use) ...
 * default is "pcp"
 */
PCP_CALL extern int pmGetUsername(char **);

/* DSO PMDA helpers */
PCP_CALL extern char *pmSpecLocalPMDA(const char *);

/*
 * PMAPI_VERSION_2 interfaces
 */
PCP_CALL extern int pmGetArchiveEnd_v2(struct timeval *);

struct pmOptions_v2;
typedef int (*pmOptionOverride_v2)(int, struct pmOptions_v2 *);

typedef struct pmOptions_v2 {
    int			version;
    int			flags;

    /* in: define set of all options */
    const char *	short_options;
    pmLongOptions *	long_options;
    const char *	short_usage;

    /* in: method for general override */
    pmOptionOverride_v2	override;

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
} pmOptions_v2;

PCP_CALL extern int pmgetopt_r_v2(int, char *const *, pmOptions_v2 *);
PCP_CALL extern int pmGetOptions_v2(int, char *const *, pmOptions_v2 *);
PCP_CALL extern int pmGetContextOptions_v2(int, pmOptions_v2 *);
PCP_CALL extern void pmUsageMessage_v2(pmOptions_v2 *);
PCP_CALL extern void pmFreeOptions_v2(pmOptions_v2 *);

PCP_CALL extern int pmParseTimeWindow_v2(
      const char *, const char *, const char *, const char *,
      const struct timeval *, const struct timeval *,
      struct timeval *, struct timeval *, struct timeval *, char **);


typedef struct pmLogLabel_v2 {
    int		ll_magic;	/* PM_LOG_MAGIC | archive format version no. */
    pid_t	ll_pid;				/* PID of logger */
    struct timeval	ll_start;		/* start of this archive */
    char	ll_hostname[PM_LOG_MAXHOSTLEN];	/* name of collection host */
    char	ll_tz[PM_TZ_MAXLEN];		/* $TZ at collection host */
} pmLogLabel_v2;

PCP_CALL extern int pmGetArchiveLabel_v2(pmLogLabel_v2 *);
PCP_CALL extern int pmParseInterval_v2(const char *, struct timeval *, char **);
PCP_CALL extern int pmSetMode_v2(int, const struct timeval *, int);

#if PMAPI_VERSION == PMAPI_VERSION_2
/*
 * old names with API changes mapped to _v2 variants
 */
#define pmGetArchiveEnd pmGetArchiveEnd_v2
#define pmOptionOverride pmOptionOverride_v2
#define pmOptions pmOptions_v2
#define pmgetopt_r pmgetopt_r_v2
#define pmGetOptions pmGetOptions_v2
#define pmGetContextOptions pmGetContextOptions_v2
#define pmUsageMessage pmUsageMessage_v2
#define pmFreeOptions pmFreeOptions_v2
#define pmParseTimeWindow pmParseTimeWindow_v2
#define pmLogLabel pmLogLabel_v2
#define pmGetArchiveLabel pmGetArchiveLabel_v2
#define pmParseInterval pmParseInterval_v2
#define pmSetMode pmSetMode_v2

/*
 * Extended time base definitions and macros
 * - only for deprecated pmSetMode_v2()
 */
#define PM_XTB_FLAG	0x1000000
#define PM_XTB_SET(m)	(PM_XTB_FLAG | ((m) << 16))
#define PM_XTB_GET(m)	(((m) & PM_XTB_FLAG) ? (((m) & 0xff0000) >> 16) : -1)
#endif

/* bad name, preserved for Version 2 */
#define pmHighResFetch pmFetchHighRes

#if PMAPI_VERSION >= PMAPI_VERSION_4
/*
 * retire HighRes interfaces
 */
#define pmGetHighResArchiveEnd pmGetArchiveEnd
#define pmParseHighResTimeWindow pmParseTimeWindow
#define pmGetHighResArchiveLabel pmGetArchiveLabel
#define pmParseHighResInterval pmParseInterval
#define pmSetModeHighRes pmSetMode
#endif

#ifdef __cplusplus
}
#endif

#endif /* PCP_PMAPI_H */
