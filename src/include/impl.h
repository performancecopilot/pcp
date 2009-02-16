/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#ifndef _IMPL_H
#define _IMPL_H

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

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This defines the routines, macros and data structures that are used
 * in the Performance Metrics Collection Subsystem (PMCS) below the
 * PMAPI.
 */

/*
 * internal libpcp state ... PM_STATE_APPL means we are at or above the
 * PMAPI in a state where PMAPI calls can safely be made ... PM_STATE_PMCS
 * means we are in the PMCD, or a PMDA, or low-level PDU code, and
 * PMAPI calls are a bad idea.
 */
#define PM_STATE_APPL	0
#define PM_STATE_PMCS	1

extern void __pmSetInternalState(int);
extern int __pmGetInternalState(void);

/*
 * PMCD connections come here by default, over-ride with $PMCD_PORT in
 * environment
 */
#define SERVER_PORT 44321

/*
 * port that clients connect to pmproxy(1) on by default, over-ride with
 * $PMPROXY_PORT in environment
 */
#define PROXY_PORT 44322

/*
 * Internally, this is how to decode a PMID!
 */
typedef struct {
#ifdef HAVE_BITFIELDS_LTOR
	int		pad : 2;
	unsigned int	domain : 8;
	unsigned int	cluster : 12;
	unsigned int	item : 10;
#else
	unsigned int	item : 10;
	unsigned int	cluster : 12;
	unsigned int	domain : 8;
	int		pad : 2;
#endif
} __pmID_int;

static inline __pmID_int *
__pmid_int(pmID *idp)
{
    /* avoid gcc's warning about dereferencing type-punned pointers */
    return (__pmID_int *)idp;
}

static inline unsigned int 
pmid_item(pmID id)
{
    return __pmid_int(&id)->item;
}

static inline unsigned int 
pmid_cluster(pmID id)
{
    return __pmid_int(&id)->cluster;
}

static inline unsigned int 
pmid_domain(pmID id)
{
    return __pmid_int(&id)->domain;
}

static inline pmID
pmid_build(unsigned int domain, unsigned int cluster, unsigned int item)
{
    pmID id = 0;
    __pmid_int(&id)->domain = domain;
    __pmid_int(&id)->cluster = cluster;
    __pmid_int(&id)->item = item;
    return id;
}

/*
 * Internally, this is how to decode an Instance Domain Identifier
 */
typedef struct {
#ifdef HAVE_BITFIELDS_LTOR
	int		pad : 2;
	unsigned int	domain : 8;		/* the administrative PMD */
	unsigned int	serial : 22;		/* unique within PMD */
#else
	unsigned int	serial : 22;		/* unique within PMD */
	unsigned int	domain : 8;		/* the administrative PMD */
	int		pad : 2;
#endif
} __pmInDom_int;

static inline __pmInDom_int *
__pmindom_int(pmInDom *idp)
{
    /* avoid gcc's warning about dereferencing type-punned pointers */
    return (__pmInDom_int *)idp;
}
static inline unsigned int 
pmInDom_domain(pmInDom id)
{
    return __pmindom_int(&id)->domain;
}
static inline unsigned int 
pmInDom_serial(pmInDom id)
{
    return __pmindom_int(&id)->serial;
}

static inline pmInDom
pmInDom_build (unsigned int domain, unsigned int serial)
{
    pmInDom ind = 0;

    __pmindom_int(&ind)->domain = domain;
    __pmindom_int(&ind)->serial = serial;
    return (ind);
}

/*
 * internal structure of a PMNS node
 */
typedef struct pn_s {
    struct pn_s	*parent;
    struct pn_s	*next;
    struct pn_s	*first;
    struct pn_s	*hash;	/* used as "last" in build, then pmid hash synonym */
    char	*name;
    pmID	pmid;
} __pmnsNode;

/*
 * internal structure of a PMNS tree
 */
typedef struct {
    __pmnsNode *root;  /* root of tree structure */
    __pmnsNode **htab; /* hash table of nodes keyed on pmid */
    int htabsize;     /* number of nodes in the table */
    char *symbol;     /* store all names contiguosly */
    int contiguous;   /* is data stored contiguosly ? */
    int mark_state;   /* the total mark value for trimming */
} __pmnsTree;


/* used by pmnscomp... */
extern __pmnsTree* __pmExportPMNS(void); 

/* for PMNS in archives */
extern int __pmNewPMNS(__pmnsTree **);
extern void __pmFreePMNS(__pmnsTree *);
extern void __pmUsePMNS(__pmnsTree *); /* for debugging */
extern int __pmFixPMNSHashTab(__pmnsTree *, int, int);
extern int __pmAddPMNSNode(__pmnsTree *, int, const char *);


/* return true if the named pmns file has changed */
extern int __pmHasPMNSFileChanged(const char *);

/* standard log file set up */
extern FILE *__pmOpenLog(const char *, const char *, FILE *, int *);
extern FILE *__pmRotateLog(const char *, const char *, FILE *, int *);
/* make __pmNotifyErr also add entries to syslog */
extern void __pmSyslog(int);
/* standard error, warning and info wrapper for syslog(3C) */
extern void __pmNotifyErr(int, const char *, ...);

/*
 * These ones are only for debugging and may not appear in the shipped
 * libpmapi ...
 */
EXTERN int	pmDebug;
#define  DBG_TRACE_PDU		1	/* PDU send and receive */
#define  DBG_TRACE_FETCH	2	/* dump pmFetch results */
#define  DBG_TRACE_PROFILE	4	/* trace profile changes */
#define  DBG_TRACE_VALUE	8	/* metric value conversions */
#define  DBG_TRACE_CONTEXT	16	/* trace PMAPI context changes */
#define  DBG_TRACE_INDOM	32	/* instance domain operations */
#define  DBG_TRACE_PDUBUF	64	/* PDU buffer management */
#define  DBG_TRACE_LOG		128	/* generic archive log operations */
#define  DBG_TRACE_LOGMETA	(1<<8)	/* meta data in archives */
#define  DBG_TRACE_OPTFETCH	(1<<9)	/* optFetch tracing */
#define  DBG_TRACE_AF		(1<<10)	/* trace async timer events */
#define  DBG_TRACE_APPL0	(1<<11)	/* reserved for applications */
#define  DBG_TRACE_APPL1	(1<<12)	/* reserved for applications */
#define  DBG_TRACE_APPL2	(1<<13)	/* reserved for applications */
#define  DBG_TRACE_PMNS		(1<<14)	/* PMNS operations */
#define  DBG_TRACE_LIBPMDA	(1<<15)	/* libpcp_pmda */
#define  DBG_TRACE_TIMECONTROL	(1<<16)	/* time control api */
#define  DBG_TRACE_PMC		(1<<17)	/* metrics class */
#define  DBG_TRACE_INTERP	(1<<20)	/* interpolate mode for archives */
#define  DBG_TRACE_CONFIG	(1<<21) /* configuration parameters */
#define  DBG_TRACE_LOOP		(1<<22) /* pmLoop tracing */

extern int __pmParseDebug(const char *);
extern void __pmDumpResult(FILE *, const pmResult *);
extern void __pmPrintStamp(FILE *, const struct timeval *);
extern void __pmPrintDesc(FILE *, const pmDesc *);
extern void __pmFreeResultValues(pmResult *);
extern const char *__pmPDUTypeStr(int);
extern void __pmDumpNameSpace(FILE *, int);

#ifdef PCP_DEBUG
extern void __pmDumpIDList(FILE *, int, const pmID *);
extern void __pmDumpNameList(FILE *, int, char **);
extern void __pmDumpStatusList(FILE *, int, const int *);
#endif

/*
 * struct timeval is sometimes 2 x 64-bit ... we use a 2 x 32-bit format for
 * PDUs, internally within libpcp and for (external) archive logs
 */
typedef struct {
    __int32_t	tv_sec;		/* seconds since Jan. 1, 1970 */
    __int32_t	tv_usec;	/* and microseconds */
} __pmTimeval;

/*
 * Logs and archives of performance metrics (not to be confused
 * with diagnostic logs for error messages, etc.)
 *
 * __pmLogCtl	log control
 * __pmLogTI	temporal index record
 */

/*
 * Hashed Data Structures for the Processing of Logs and Archives
 */
typedef struct _hashnode {
    struct _hashnode	*next;
    unsigned int	key;
    void		*data;
} __pmHashNode;

typedef struct {
    int		nodes;
    int		hsize;
    __pmHashNode	**hash;
} __pmHashCtl;

extern __pmHashNode *__pmHashSearch(unsigned int, __pmHashCtl *);
extern int __pmHashAdd(unsigned int, void *, __pmHashCtl *);
extern int __pmHashDel(unsigned int, void *, __pmHashCtl *);

/*
 * External file and internal (below PMAPI) format for an archive label
 */
typedef struct {
    int		ill_magic;	/* PM_LOG_MAGIC | log format version no. */
    pid_t	ill_pid;			/* PID of logger */
    __pmTimeval	ill_start;			/* start of this log */
    int		ill_vol;			/* current log volume no. */
    char	ill_hostname[PM_LOG_MAXHOSTLEN];/* name of collection host */
    char	ill_tz[PM_TZ_MAXLEN];		/* $TZ at collection host */
} __pmLogLabel;

/*
 * unfortunately, in this version, PCP archives are limited to no
 * more than 2 Gbytes ...
 */
typedef __uint32_t	__pm_off_t;

/*
 * Temporal Index Record
 */
typedef struct {
    __pmTimeval	ti_stamp;	/* now */
    int		ti_vol;		/* current log volume no. */
    __pm_off_t	ti_meta;	/* end of meta data file */
    __pm_off_t	ti_log;		/* end of metrics log file */
} __pmLogTI;

/*
 * Log/Archive Control
 */
typedef struct {
    int		l_refcnt;	/* number of contexts using this log */
    char	*l_name;	/* external log base name */
    FILE	*l_tifp;	/* temporal index */
    FILE	*l_mdfp;	/* meta data */
    FILE	*l_mfp;		/* current metrics log */
    int		l_curvol;	/* current metrics log volume no. */
    int		l_state;	/* (when writing) log state */
    __pmHashCtl	l_hashpmid;	/* PMID hashed access */
    __pmHashCtl	l_hashindom;	/* instance domain hashed access */
    __pmHashCtl	l_hashrange;	/* ptr to first and last value in log for */
				/* each metric */
    int		l_minvol;	/* (when reading) lowest known volume no. */
    int		l_maxvol;	/* (when reading) highest known volume no. */
    int		l_numseen;	/* (when reading) size of l_seen */
    int		*l_seen;	/* (when reading) volumes opened OK */
    __pmLogLabel	l_label;	/* (when reading) log label */
    __pm_off_t	l_physend;	/* (when reading) offset to physical EOF */
				/*                for last volume */
    __pmTimeval	l_endtime;	/* (when reading) timestamp at logical EOF */
    int		l_numti;	/* (when reading) no. temporal index entries */
    __pmLogTI	*l_ti;		/* (when reading) temporal index */
    __pmnsTree  *l_pmns;        /* namespace from meta data */
} __pmLogCtl;

/* l_state values */
#define PM_LOG_STATE_NEW	0
#define PM_LOG_STATE_INIT	1

/*
 * Dump the current context (source details + instance profile),
 * for a particular instance domain.
 * If indom == PM_INDOM_NULL, then print all all instance domains
 */
extern void __pmDumpContext(FILE *, int, pmInDom);

/*
 * return the argument if it's a valid filename
 * else return NULL (note: this function could
 * be replaced with a call to access(), but is
 * retained for historical reasons).
 */
extern const char *__pmFindPMDA(const char *);

/*
 * internal instance profile states 
 */
#define PM_PROFILE_INCLUDE 0	/* include all, exclude some */
#define PM_PROFILE_EXCLUDE 1	/* exclude all, include some */

/* Profile entry (per instance domain) */
typedef struct {
    pmInDom	indom;			/* instance domain */
    int		state;			/* include all or exclude all */
    int		instances_len;		/* length of instances array */
    int		*instances;		/* array of instances */
} __pmInDomProfile;

/* Instance profile for all domains */
typedef struct {
    int			state;			/* default global state */
    int			profile_len;		/* length of profile array */
    __pmInDomProfile	*profile;		/* array of instance profiles */
} __pmProfile;

/*
 * Dump the instance profile, for a particular instance domain
 * If indom == PM_INDOM_NULL, then print all instance domains
 */
extern void __pmDumpProfile(FILE *, int, const __pmProfile *);

/*
 * Result structure for instance domain queries
 * Only the PMDAs and pmcd need to know about this.
 */
typedef struct {
    pmInDom	indom;		/* instance domain */
    int		numinst;	/* may be 0 */
    int		*instlist;	/* instance ids, may be NULL */
    char	**namelist;	/* instance names, may be NULL */
} __pmInResult;
extern void __pmDumpInResult(FILE *, const __pmInResult *);

/* instance profile methods */
extern int __pmProfileSetSent(void);
extern void __pmFreeProfile(__pmProfile *);
extern __pmInDomProfile *__pmFindProfile(pmInDom, const __pmProfile *);
extern int __pmInProfile(pmInDom, const __pmProfile *, int);
extern void __pmFreeInResult(__pmInResult *);

/*
 * Version and authorisation info for PDU exchanges
 */

#define UNKNOWN_VERSION	0
#define PDU_VERSION1	1
#define PDU_VERSION2	2
#define PDU_VERSION	PDU_VERSION2

#define PDU_OVERRIDE1	-1001
#define PDU_OVERRIDE2	-1002

typedef struct {
#ifdef HAVE_BITFIELDS_LTOR
    unsigned int	zero : 1;	/* ensure first bit is zero for 1.x compatibility */
    unsigned int	version : 7;	/* PDU_VERSION */
    unsigned int	licensed : 8;
    unsigned int	authorize : 16;
#else
    unsigned int	authorize : 16;
    unsigned int	licensed : 8;
    unsigned int	version : 7;	/* PDU_VERSION */
    unsigned int	zero : 1;	/* ensure first bit is zero for 1.x compatibility */
#endif
} __pmPDUInfo;

typedef enum {
    PC_FETAL = 0,
    PC_CONN_INPROGRESS,
    PC_WAIT_FOR_PMCD,
    PC_READY
} pmcd_ctl_state_t;

/*
 * Host specification allowing one or more pmproxy host, and port numbers
 * within the one string, i.e. pmcd host specifications of the form:
 *		host:port,port@proxy:port,port 
 */
typedef struct {
    char	*name;			/* hostname (always valid) */
    int		*ports;			/* array of host port numbers */
    int		nports;			/* number of ports in host port array */
} pmHostSpec;
extern int __pmParseHostSpec(const char *, pmHostSpec **, int *, char **);
extern void __pmUnparseHostSpec(pmHostSpec *, int, char **, int);
extern void __pmFreeHostSpec(pmHostSpec *, int);

/*
 * Control for connection to a PMCD
 */
typedef struct {
    int		pc_refcnt;		/* number of contexts using this socket */
    int		pc_fd;			/* socket for comm with pmcd */
					/* ... -1 means no connection */
    pmHostSpec	*pc_hosts;		/* pmcd and proxy host specifications */
    int		pc_nhosts;		/* number of pmHostSpec entries */
    int		pc_timeout;		/* set if connect times out */
    time_t	pc_again;		/* time to try again */
    int		pc_curpdu; 		/* current pdu in flight */
    int		pc_tout_sec;		/* timeout for pmGetPDU */
    pmcd_ctl_state_t pc_state;		/* current state of this context */
    int		pc_fdflags;		/* fcntl(2) flags for pc_fd */
    struct sockaddr pc_addr;		/* server address */
} __pmPMCDCtl;

extern int __pmConnectPMCD(pmHostSpec *, int);
extern int __pmConnectLocal(void);
extern int __pmAuxConnectPMCD(const char *);
extern int __pmAuxConnectPMCDPort(const char *, int);
extern int __pmCreateSocket(void);
extern void __pmCloseSocket(int);
extern int __pmAddHostPorts(pmHostSpec *, int *, int);
extern void __pmDropHostPort(pmHostSpec *);
extern void __pmConnectGetPorts(pmHostSpec *);
extern int __pmConnectTo (int, const struct sockaddr *, int);
extern int __pmConnectCheckError(int);
extern int __pmConnectRestoreFlags (int, int);
extern int __pmConnectHandshake(int);
extern const struct timeval *__pmConnectTimeout(void);


/*
 * per context controls for archives and logs
 */
typedef struct {
    __pmLogCtl		*ac_log;	/* global logging and archive control */
    long		ac_offset;	/* fseek ptr for archives */
    int			ac_vol;		/* volume for ac_offset */
    int			ac_serial;	/* serial access pattern for archives */
    __pmHashCtl		ac_pmid_hc;	/* per PMID controls for INTERP */
} __pmArchCtl;

/*
 * PMAPI context. We keep an array of these,
 * one for each context created by the application.
 */
typedef struct {
    int			c_type;		/* PM_CONTEXT_HOST, _ARCHIVE or _FREE */
    int			c_mode;		/* current mode PM_MODE_* */
    __pmPMCDCtl		*c_pmcd;	/* pmcd control for HOST contexts */
    __pmArchCtl		*c_archctl;	/* log control for ARCHIVE contexts */
    __pmTimeval		c_origin;	/* pmFetch time origin / current time */
    int			c_delta;	/* for updating origin */
    int			c_sent;		/* profile has been sent to pmcd */
    __pmProfile		*c_instprof;	/* instance profile */
} __pmContext;

#define __PM_MODE_MASK	0xffff

#define pmXTBdeltaToTimeval(d, m, t) { \
    (t)->tv_sec = 0; \
    (t)->tv_usec = (long)0; \
    switch(PM_XTB_GET(m)) { \
    case PM_TIME_NSEC: (t)->tv_usec = (long)((d) / 1000); break; \
    case PM_TIME_USEC: (t)->tv_usec = (long)(d); break; \
    case PM_TIME_MSEC: (t)->tv_sec = (d) / 1000; (t)->tv_usec = (long)(1000 * ((d) % 1000)); break; \
    case PM_TIME_SEC: (t)->tv_sec = (d); break; \
    case PM_TIME_MIN: (t)->tv_sec = (d) * 60; break; \
    case PM_TIME_HOUR: (t)->tv_sec = (d) * 360; break; \
    default: (t)->tv_sec = (d) / 1000; (t)->tv_usec = (long)(1000 * ((d) % 1000)); break; \
    } \
}

#define PM_CONTEXT_FREE		-1	/* special type */

/* handle to __pmContext pointer */
extern __pmContext *__pmHandleToPtr(int);
extern int __pmGetHostContextByID(int, __pmContext **);
extern int __pmGetBusyHostContextByID(int, __pmContext **, int);
extern const struct timeval * __pmDefaultRequestTimeout(void);

/*
 * Protocol data unit support
 */
typedef struct {
    int	len;		/* length of pdu_header + PDU */
    int	type;		/* PDU type */
    int	from;		/* pid of PDU originator */
} __pmPDUHdr;

typedef __uint32_t	__pmPDU;

/* Individual credential PDU elements look like this */
typedef struct {
#ifdef HAVE_BITFIELDS_LTOR
    unsigned int	c_type: 8;
    unsigned int	c_vala: 8;
    unsigned int	c_valb: 8;
    unsigned int	c_valc: 8;
#else
    unsigned int	c_valc: 8;
    unsigned int	c_valb: 8;
    unsigned int	c_vala: 8;
    unsigned int	c_type: 8;
#endif
} __pmCred;

/* Types of credential PDUs */
#define CVERSION        1
#define CAUTH           2

/* ASCII PDU line buffer */
#define ABUFSIZE 256
extern char __pmAbuf[ABUFSIZE];

extern int __pmXmitPDU(int, __pmPDU *);
extern int __pmXmitAscii(int, const char *, int);
extern int __pmGetPDU(int, int, int, __pmPDU **);
extern int __pmGetPDUCeiling (void);
extern int __pmSetPDUCeiling (int);

EXTERN unsigned int *__pmPDUCntIn;
EXTERN unsigned int *__pmPDUCntOut;
extern void __pmSetPDUCntBuf(unsigned *, unsigned *);

/* for __pmGetPDU */
#define TIMEOUT_NEVER	 0
#define TIMEOUT_DEFAULT	-1
#define GETPDU_ASYNC	-2
extern int __pmRecvPDU(int, int, __pmPDU **);	/* old interface to __pmGetPDU */

extern int __pmRecvLine(__pmPDU *, int, char *);
extern __pmPDU *__pmFindPDUBuf(int);
extern void __pmPinPDUBuf(void *);
extern int __pmUnpinPDUBuf(void *);
extern void __pmCountPDUBuf(int, int *, int *);

#define PDU_START		0x7000
#define PDU_ERROR		0x7000
#define PDU_RESULT		0x7001
#define PDU_PROFILE		0x7002
#define PDU_FETCH		0x7003
#define PDU_DESC_REQ		0x7004
#define PDU_DESC		0x7005
#define PDU_INSTANCE_REQ	0x7006
#define PDU_INSTANCE		0x7007
#define PDU_TEXT_REQ		0x7008
#define PDU_TEXT		0x7009
#define PDU_CONTROL_REQ		0x700a
#define PDU_DATA_X		0x700b
#define PDU_CREDS		0x700c
#define PDU_PMNS_IDS		0x700d
#define PDU_PMNS_NAMES		0x700e
#define PDU_PMNS_CHILD		0x700f
#define PDU_PMNS_TRAVERSE	0x7010
#define PDU_FINISH		0x7010
#define PDU_MAX		 	(PDU_FINISH - PDU_START)

/*
 * PDU encoding formats
 */
#define PDU_BINARY	0
#define PDU_ASCII	1
#define PDU_CLIENT	2

extern int __pmSendError(int, int, int);
extern int __pmDecodeError(__pmPDU *, int, int *);
extern int __pmSendXtendError(int, int, int, int);
extern int __pmDecodeXtendError(__pmPDU *, int, int *, int *);
extern int __pmSendResult(int, int, const pmResult *);
extern int __pmEncodeResult(int, const pmResult *, __pmPDU **);
extern int __pmDecodeResult(__pmPDU *, int, pmResult **);
extern int __pmSendProfile(int, int, int, __pmProfile *);
extern int __pmDecodeProfile(__pmPDU *, int, int *, __pmProfile **);
extern int __pmSendFetch(int, int, int, __pmTimeval *, int, pmID *);
extern int __pmDecodeFetch(__pmPDU *, int, int *, __pmTimeval *, int *, pmID **);
extern int __pmSendDescReq(int, int, pmID);
extern int __pmDecodeDescReq(__pmPDU *, int, pmID *);
extern int __pmSendDesc(int, int, pmDesc *);
extern int __pmDecodeDesc(__pmPDU *, int, pmDesc *);
extern int __pmSendInstanceReq(int, int, const __pmTimeval *, pmInDom, int, const char *);
extern int __pmDecodeInstanceReq(__pmPDU *, int, __pmTimeval *, pmInDom *, int *, char **);
extern int __pmSendInstance(int, int, __pmInResult *);
extern int __pmDecodeInstance(__pmPDU *, int, __pmInResult **);
extern int __pmSendTextReq(int, int, int, int);
extern int __pmDecodeTextReq(__pmPDU *, int, int *, int *);
extern int __pmSendText(int, int, int, const char *);
extern int __pmDecodeText(__pmPDU *, int, int *, char **);
extern int __pmSendCreds(int, int, int, const __pmCred *);
extern int __pmDecodeCreds(__pmPDU *, int, int *, int *, __pmCred **);
extern int __pmSendIDList(int, int, int, const pmID *, int);
extern int __pmDecodeIDList(__pmPDU *, int, int, pmID *, int *);
extern int __pmSendNameList(int, int, int, char **, const int *);
extern int __pmDecodeNameList(__pmPDU *, int, int *, char ***, int **);
extern int __pmSendChildReq(int, int, const char *, int);
extern int __pmDecodeChildReq(__pmPDU *, int , char **, int *);
extern int __pmSendTraversePMNSReq(int, int, const char *);
extern int __pmDecodeTraversePMNSReq(__pmPDU *, int, char **);

#if defined(HAVE_64BIT_LONG)

/*
 * A pmValue contains the union of a 32-bit int and a pointer.  In the world
 * of 64-bit pointers, a pmValue is therefore larger than in the 32-bit world.
 * The structures below are used in all PDUs containing pmResults to ensure
 * 32-bit and 64-bit programs exchanging PDUs can communicate.
 * Note that a pmValue can only hold a 32-bit value in situ regardless of
 * whether the pointer size is 32 or 64 bits.
 */

typedef struct {
    int			inst;		/* instance identifier */
    union {
	unsigned int	pval;		/* offset into PDU buffer for value */
	int		lval;		/* 32-bit value in situ */
    } value;
} __pmValue_PDU;

typedef struct {
    pmID		pmid;		/* metric identifier */
    int			numval;		/* number of values */
    int			valfmt;		/* value style */
    __pmValue_PDU	vlist[1];	/* set of instances/values */
} __pmValueSet_PDU;

#elif defined(HAVE_32BIT_LONG)

/* In the 32-bit world, structures may be used in PDUs as defined */

typedef pmValue		__pmValue_PDU;
typedef pmValueSet	__pmValueSet_PDU;

#else
bozo - unknown size of long !!!
#endif

/*
 * For the help text PDUs, the type (PM_TEXT_ONELINE or PM_TEXT_HELP)
 * is 'or'd with the following to encode the request for a PMID or
 * a pmInDom ...
 * Note the values must therefore be (a) bit fields and (b) different
 *	to the public macros PM_TEXT_* in pmapi.h 
 */
#define PM_TEXT_PMID	4
#define PM_TEXT_INDOM	8

/*
 * Quick-and-dirty pool memory allocator ...
 */
extern void *__pmPoolAlloc(size_t);
extern void __pmPoolFree(void *, size_t);
extern void __pmPoolCount(size_t, int *, int *);

/*
 * no mem today, my love has gone away ....
 */
extern void __pmNoMem(const char *, size_t, int);
#define PM_FATAL_ERR 1
#define PM_RECOV_ERR 0

/*
 * program name, as used in __pmNotifyErr() ... default is "pcp"
 */
EXTERN char *pmProgname;
extern int __pmSetProgname(const char *);

/* map Unix errno values to PMAPI errors */
extern int __pmMapErrno(int);

/*
 * __pmLogInDom is used to hold the instance identifiers for an instance
 * domain internally ... if multiple sets are observed over time, these
 * are linked together in reverse chronological order
 * -- externally we write these as
 *	timestamp
 *	indom		<- note, added wrt indom_t
 *	numinst
 *	inst[0], .... inst[numinst-1]
 *	nameindex[0] .... nameindex[numinst-1]
 *	string (name) table, all null-byte terminated
 *
 * NOTE: 3 types of allocation
 * (1)
 * buf is NULL, 
 * namelist and instlist have been allocated
 * separately and so must each be freed.
 * (2)
 * buf is NOT NULL, allinbuf == 1,
 * all allocations were in the buffer and so only
 * the buffer should be freed,
 * (3)
 * buf is NOT NULL, allinbuf == 0,
 * as well as buffer allocation, 
 * the namelist has been allocated separately and so
 * both the buf and namelist should be freed.
 */
typedef struct _indom_t {
    struct _indom_t	*next;
    __pmTimeval		stamp;
    int			numinst;
    int			*instlist;
    char		**namelist;
    int			*buf; 
    int			allinbuf; 
} __pmLogInDom;

/*
 * record header in the metadata log file ... len (by itself) also is
 * used as a trailer
 */
typedef struct {
    int		len;	/* record length, includes header and trailer */
    int		type;	/* see TYPE_* #defines below */
} __pmLogHdr;

#define TYPE_DESC	1	/* header, pmDesc, trailer */
#define TYPE_INDOM	2	/* header, __pmLogInDom, trailer */

extern void __pmLogPutIndex(const __pmLogCtl *, const __pmTimeval *);

extern const char *__pmLogName(const char *, int);
extern FILE *__pmLogNewFile(const char *, int);
extern int __pmLogCreate(const char *, const char *, int, __pmLogCtl *);
extern int __pmLogRead(__pmLogCtl *, int, FILE *, pmResult **);
extern int __pmLogWriteLabel(FILE *, const __pmLogLabel *);
extern int __pmLogOpen(const char *, __pmContext *);
extern int __pmLogLoadLabel(__pmLogCtl *, const char *);
extern int __pmLogLoadIndex(__pmLogCtl *);
extern int __pmLogLoadMeta(__pmLogCtl *);
extern void __pmLogClose(__pmLogCtl *);
extern void __pmLogCacheClear(FILE *);

extern int __pmLogPutDesc(__pmLogCtl *, const pmDesc *, int, char **);
extern int __pmLogLookupDesc(__pmLogCtl *, pmID, pmDesc *);
extern int __pmLogPutInDom(__pmLogCtl *, pmInDom, const __pmTimeval *, int, int *, char **);
extern int __pmLogGetInDom(__pmLogCtl *, pmInDom, __pmTimeval *, int **, char ***);
extern int __pmLogLookupInDom(__pmLogCtl *, pmInDom, __pmTimeval *, const char *);
extern int __pmLogNameInDom(__pmLogCtl *, pmInDom, __pmTimeval *, int, char **);

extern int __pmLogPutResult(__pmLogCtl *, __pmPDU *);
extern int __pmLogFetch(__pmContext *, int, pmID *, pmResult **);
extern int __pmLogFetchInterp(__pmContext *, int, pmID *, pmResult **);
extern void __pmLogSetTime(__pmContext *);
extern void __pmLogResetInterp(__pmContext *);

extern int __pmLogChangeVol(__pmLogCtl *, int);
extern int __pmLogChkLabel(__pmLogCtl *, FILE *, __pmLogLabel *, int);
extern int __pmGetArchiveEnd(__pmLogCtl *, struct timeval *);

#if !defined(PM_LOG_PORT_DIR)
/* This is the default directory where pmlogger creates the file containing the port
 * number it is using. It may be overridden in platform_defs.h
 */
#define	PM_LOG_PORT_DIR		"/var/tmp/pmlogger"
#endif

/* The primary logger creates a symbolic link to its own port file
 * in PM_LOG_PORT_DIR.  This is the name of the link.
 */
#define PM_LOG_PRIMARY_LINK	"primary"

/* struct for maintaining information about pmlogger ports */
typedef struct {
    int		pid;		/* process id of logger */
    int		port;		/* internet port for logger control */
    char	*pmcd_host;	/* host pmlogger is collecting from */
    char	*archive;	/* archive base pathname */
    char	*name;		/* file name (minus dirname) */
} __pmLogPort;

/* Returns control port info for a pmlogger given its pid.
 * If pid == PM_LOG_ALL_PIDS, get all pmloggers' control ports.
 * If pid == PM_LOG_PRIMARY_PID, get primar logger's control port.
 * Note: do NOT free any part of result returned via the parameter.
 *
 * __pmLogFindPort(const char *hostname, int pid, __pmLogPort **result);
 */
extern int __pmLogFindPort(const char *, int, __pmLogPort **);

#define PM_LOG_PRIMARY_PID	0	/* symbolic pid for primary logger */
#define PM_LOG_PRIMARY_PORT	0	/* symbolic port for primary pmlogger */
#define PM_LOG_ALL_PIDS		-1	/* symbolic pid for all pmloggers */
#define PM_LOG_NO_PID		-2	/* not a valid pid for pmlogger */
#define PM_LOG_NO_PORT		-2	/* not a valid port for pmlogger */

/* time utils */
extern time_t __pmMktime(struct tm *);

/* reverse ctime and time interval parsing */
extern int __pmParseCtime(const char *, struct tm *, char **);
extern int __pmConvertTime(struct tm *, struct timeval *, struct timeval *);
extern int __pmParseTime(const char *, struct timeval *, struct timeval *,
			 struct timeval *, char **);

/* mainupulate internal timestamps */
extern double __pmTimevalSub(const __pmTimeval *, const __pmTimeval *);

/* 32-bit file checksum */
extern __int32_t __pmCheckSum(FILE *);

/* check for localhost */
extern int __pmIsLocalhost(const char *);

/*
 * struct timeval manipulations
 */
extern double __pmtimevalAdd(const struct timeval *, const struct timeval *);
extern double __pmtimevalSub(const struct timeval *, const struct timeval *);
extern double __pmtimevalToReal(const struct timeval *);
extern void __pmtimevalFromReal(double, struct timeval *);

/*
 * interface to __pmGetPDU() controls
 */
extern int __pmMoreInput(int);
extern void __pmNoMoreInput(int);

/* time control timezone */
typedef struct {
    char		*label;		/* label to name tz */
    char		*tz;		/* env $TZ */
    int			handle;		/* handle from pmNewZone() */
} pmTimeZone;

/*
 * event tracing for monitoring time between events
 */
extern void __pmEventTrace(const char *);

/*
 * More IPC protocol stuff
 */

typedef int (*__pmConnectHostType)(int, int);
extern __pmConnectHostType __pmConnectHostMethod;

extern int __pmSetSocketIPC(int);
extern int __pmSetVersionIPC(int, int);
extern int __pmLastVersionIPC();
extern int __pmVersionIPC(int);
extern int __pmSocketIPC(int);
extern void __pmOverrideLastFd(int);
extern void __pmPrintIPC(void);
extern void __pmResetIPC(int);

/* safely insert an atom value into a pmValue */
extern int __pmStuffValue(const pmAtomValue *, int, pmValue *, int);

/*
 * "optfetch" api
 * (currently not documented)
 */
typedef struct __optreq {
    struct __optreq	*r_next;	/* next request */
    struct __fetchctl	*r_fetch;	/* back ptr */
    pmDesc		*r_desc;	/* pmDesc for request pmID */
    int			r_numinst;	/* request instances */
    int			*r_instlist;	/* request instances */
    void		*r_aux;		/* generic pointer to aux data */
} optreq_t;

typedef struct __pmidctl {
    struct __pmidctl	*p_next;	/* next pmid control */
    optreq_t		*p_rqp;		/* first request for this metric */
    pmID		p_pmid;		/* my pmID */
    int			p_numinst;	/* union over requests */
    int			*p_instlist;	/* union over requests */
    void		*p_aux;		/* generic pointer to aux data */
} pmidctl_t;

typedef struct __indomctl {
    struct __indomctl	*i_next;	/* next indom control */
    pmidctl_t		*i_pmp;		/* first metric, in this group */
    pmInDom		i_indom;	/* my pmInDom */
    int			i_numinst;	/* arg for pmAddProfile */
    int			*i_instlist;	/* arg for pmAddProfile */
    void		*i_aux;		/* generic pointer to aux data */
} indomctl_t;

typedef struct __fetchctl {
    struct __fetchctl	*f_next;	/* next fetch control */
    indomctl_t		*f_idp;		/* first indom, in this group */
    int			f_state;	/* state changes during updates */
    int			f_cost;		/* used internally for optimization */
    int			f_newcost;	/* used internally for optimization */
    int			f_numpmid;	/* arg for pmFetch() */
    pmID		*f_pmidlist;	/* arg for pmFetch() */
    void		*f_aux;		/* generic pointer to aux data */
} fetchctl_t;

/* states relevant to user */
#define OPT_STATE_NEW		1	/* newly created group */
#define OPT_STATE_PMID		2	/* list of pmids changed */
#define OPT_STATE_PROFILE	4	/* instance profile changed */

/* states used during optimization */
#define OPT_STATE_UMASK		7	/* preserve user state bits */
#define OPT_STATE_XREQ		8	/* things that may have changed */
#define OPT_STATE_XPMID		16
#define OPT_STATE_XINDOM	32
#define OPT_STATE_XFETCH	64
#define OPT_STATE_XPROFILE	128

/*
 * Objective function parameters
 */
typedef struct {
    int		c_pmid;		/* cost per PMD for PMIDs in a fetch */
    int		c_indom;	/* cost per PMD for indoms in a fetch */
    int		c_fetch;	/* cost of a new fetch group */
    int		c_indomsize;	/* expected numer of instances for an indom */
    int		c_xtrainst;	/* cost of retrieving an unwanted metric inst */
    int		c_scope;	/* cost opt., 0 for incremental, 1 for global */
} optcost_t;

#define OPT_COST_INFINITY	0x7fffffff

extern int __pmOptFetchAdd(fetchctl_t **, optreq_t *);
extern int __pmOptFetchDel(fetchctl_t **, optreq_t *);
extern int __pmOptFetchRedo(fetchctl_t **);
extern void __pmOptFetchDump(FILE *, const fetchctl_t *);
extern int __pmOptFetchGetParams(optcost_t *);
extern int __pmOptFetchPutParams(optcost_t *);

/* work out local timezone */
extern char * __pmTimezone(void);

#ifdef HAVE_NETWORK_BYTEORDER
/*
 * no-ops if already in network byte order but
 * the value may be used in an expression.
 */
#define __htonpmUnits(a)	(a)
#define __ntohpmUnits(a)	(a)
#define __htonpmID(a)		(a)
#define __ntohpmID(a)		(a)
#define __htonpmInDom(a)	(a)
#define __ntohpmInDom(a)	(a)
#define __htonpmPDUInfo(a)	(a)
#define __ntohpmPDUInfo(a)	(a)
#define __htonpmCred(a)		(a)
#define __ntohpmCred(a)		(a)

/*
 * For network byte order, the following are noops,
 * but otherwise the function is void, so they are
 * defined as comments to catch code that tries to
 * use them in an expression or assignment.
 */
#define __htonpmValueBlock(a)	/* noop */
#define __ntohpmValueBlock(a)	/* noop */
#define __htonf(a)		/* noop */
#define __ntohf(a)		/* noop */
#define __htond(a)		/* noop */
#define __ntohd(a)		/* noop */
#define __htonll(a)		/* noop */
#define __ntohll(a)		/* noop */

#else

/*
 * Functions to convert to/from network byte order
 * for little-endian platforms (e.g. Intel).
 */
#define __htonpmID(a)		htonl(a)
#define __ntohpmID(a)		ntohl(a)
#define __htonpmInDom(a)	htonl(a)
#define __ntohpmInDom(a)	ntohl(a)

extern pmUnits __htonpmUnits(pmUnits);
extern pmUnits __ntohpmUnits(pmUnits);
extern __pmPDUInfo __htonpmPDUInfo(__pmPDUInfo);
extern __pmPDUInfo __ntohpmPDUInfo(__pmPDUInfo);
extern __pmCred __htonpmCred(__pmCred);
extern __pmCred __ntohpmCred(__pmCred);

/* insitu swab for these */
extern void __htonpmValueBlock(pmValueBlock * const);
extern void __ntohpmValueBlock(pmValueBlock * const);
extern void __htonf(char *);		/* float */
#define __ntohf(v) __htonf(v)
#define __htond(v) __htonll(v)		/* double */
#define __ntohd(v) __ntohll(v)
extern void __htonll(char *);		/* 64bit int */
#define __ntohll(v) __htonll(v)

#endif /* HAVE_NETWORK_BYTEORDER */

/*
 * access control routines
 */
extern int __pmAccAddOp(unsigned int);
extern int __pmAccAddHost(const char *, unsigned int, unsigned int, int);
extern int __pmAccAddClient(const struct in_addr *, unsigned int *);
extern void __pmAccDelClient(const struct in_addr *);

extern void __pmAccDumpHosts(FILE *);
extern int __pmAccSaveHosts(void);
extern int __pmAccRestoreHosts(void);
extern void __pmAccFreeSavedHosts(void);

extern unsigned int __pmMakeAuthCookie(unsigned int, pid_t);

/*
 * platform independent process routines
 */
extern int __pmProcessExists(pid_t);
extern int __pmProcessTerminate(pid_t, int);
extern int __pmProcessCreate(char **, int *, int *);
extern int __pmProcessDataSize(unsigned long *);
extern int __pmProcessRunTimes(double *, double *);

/*
 * platform independent environment and filesystem path access
 */
typedef void (*__pmConfigCallback)(char *, char *, char *);
EXTERN __pmConfigCallback __pmNativeConfig;
extern void __pmConfig(const char *, __pmConfigCallback);
extern char *__pmNativePath(char *);
extern int __pmPathSeparator();

/*
 * AF - general purpose asynchronous event management routines
 */
extern int __pmAFregister(const struct timeval *, void *, void (*)(int, void *));
extern int __pmAFunregister(int);
extern void __pmAFblock(void);
extern void __pmAFunblock(void);
extern int __pmAFisempty(void);

/*
 * private PDU protocol between pmlc and pmlogger
 */
#define LOG_PDU_VERSION1	1	/* datax pdus & PCP 1.x error codes */
#define LOG_PDU_VERSION2	2	/* private pdus & PCP 2.0 error codes */
#define LOG_PDU_VERSION		LOG_PDU_VERSION2

#define LOG_REQUEST_NEWVOLUME	1
#define LOG_REQUEST_STATUS	2
#define LOG_REQUEST_SYNC	3

typedef struct {
    __pmTimeval  ls_start;	/* start time for log */
    __pmTimeval  ls_last;	/* last time log written */
    __pmTimeval  ls_timenow;	/* current time */
    int		ls_state;	/* state of log (from __pmLogCtl) */
    int		ls_vol;		/* current volume number of log */
    __int64_t	ls_size;	/* size of current volume */
    char	ls_hostname[PM_LOG_MAXHOSTLEN];
				/* name of pmcd host */
    char	ls_fqdn[PM_LOG_MAXHOSTLEN];
				/* fully qualified domain name of pmcd host */
    char	ls_tz[PM_TZ_MAXLEN];
				/* $TZ at collection host */
    char	ls_tzlogger[PM_TZ_MAXLEN];
				/* $TZ at pmlogger */
} __pmLoggerStatus;

#define PDU_LOG_CONTROL		0x8000
#define PDU_LOG_STATUS		0x8001
#define PDU_LOG_REQUEST		0x8002

extern int __pmConnectLogger(const char *, int *, int *);
extern int __pmSendLogControl(int, const pmResult *, int, int, int);
extern int __pmDecodeLogControl(const __pmPDU *, pmResult **, int *, int *, int *);
extern int __pmSendLogRequest(int, int);
extern int __pmDecodeLogRequest(const __pmPDU *, int *);
extern int __pmSendLogStatus(int, __pmLoggerStatus *);
extern int __pmDecodeLogStatus(__pmPDU *, __pmLoggerStatus **);

/*
 * other interfaces shared by pmlc and pmlogger
 */

extern int __pmControlLog(int, const pmResult *, int, int, int, pmResult **);

#define PM_LOG_OFF		0	/* state */
#define PM_LOG_MAYBE		1
#define PM_LOG_ON		2

#define PM_LOG_MANDATORY	11	/* control */
#define PM_LOG_ADVISORY		12
#define PM_LOG_ENQUIRE		13

/* macros for logging control values from __pmControlLog() */
#define PMLC_SET_ON(val, flag) \
        val = (val & ~0x1) | (flag & 0x1)
#define PMLC_GET_ON(val) \
        (val & 0x1)
#define PMLC_SET_MAND(val, flag) \
        val = (val & ~0x2) | ((flag & 0x1) << 1)
#define PMLC_GET_MAND(val) \
        ((val & 0x2) >> 1)
#define PMLC_SET_AVAIL(val, flag) \
        val = (val & ~0x4) | ((flag & 0x1) << 2)
#define PMLC_GET_AVAIL(val) \
        ((val & 0x4) >> 2)
#define PMLC_SET_INLOG(val, flag) \
        val = (val & ~0x8) | ((flag & 0x1) << 3)
#define PMLC_GET_INLOG(val) \
        ((val & 0x8) >> 3)

#define PMLC_SET_STATE(val, state) \
        val = (val & ~0xf) | (state & 0xf)
#define PMLC_GET_STATE(val) \
        (val & 0xf)

/* 28 bits of delta, 32 bits of state */
#define PMLC_MAX_DELTA  0x0fffffff

#define PMLC_SET_DELTA(val, delta) \
        val = (val & 0xf) | (delta << 4)
#define PMLC_GET_DELTA(val) \
        (((val & ~0xf) >> 4) & PMLC_MAX_DELTA)

#ifdef __cplusplus
}
#endif

#endif /* _IMPL_H */
