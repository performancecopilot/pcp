/*
 * Copyright (c) 2012-2015 Red Hat.
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
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
 */

#ifndef PCP_IMPL_H
#define PCP_IMPL_H

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

/*
 * Thread-safe support ... #define to enable thread-safe protection of
 * global data structures and mutual exclusion when required.
 *
 * We require pthread.h and working mutex, the rest can be faked
 * by the libpcp itself.
 */
#if defined(HAVE_PTHREAD_H) && defined(HAVE_PTHREAD_MUTEX_T)
#define PM_MULTI_THREAD 1
#include <pthread.h>
typedef pthread_mutex_t __pmMutex;
#else
typedef void * __pmMutex;
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
#define SERVER_PROTOCOL "pcp"
/*
 * port that clients connect to pmproxy(1) on by default, over-ride with
 * $PMPROXY_PORT in environment
 */
#define PROXY_PORT 44322
#define PROXY_PROTOCOL "proxy"

/*
 * port that clients connect to pmwebd(1) by default
 */
#define PMWEBD_PORT 44323
#define PMWEBD_PROTOCOL "http"

/*
 * Internally, this is how to decode a PMID!
 * - flag is to denote state internally in some operations
 * - domain is usually the unique domain number of a PMDA, but DYNAMIC_PMID
 *   (number 511) is reserved for PMIDs representing the root of a
 *   dynamic subtree in the PMNS (and in this case the real domain number
 *   is encoded in the cluster field)
 * - cluster and item together uniquely identify a metric within a domain
 */
#define DYNAMIC_PMID	511
typedef struct {
#ifdef HAVE_BITFIELDS_LTOR
	unsigned int	flag : 1;
	unsigned int	domain : 9;
	unsigned int	cluster : 12;
	unsigned int	item : 10;
#else
	unsigned int	item : 10;
	unsigned int	cluster : 12;
	unsigned int	domain : 9;
	unsigned int	flag : 1;
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
    pmID id;
    __pmID_int idint;

    idint.flag = 0;
    idint.domain = domain;
    idint.cluster = cluster;
    idint.item = item;
    memcpy(&id, &idint, sizeof(id));
    return id;
}

/*
 * Internally, this is how to decode an Instance Domain Identifier
 * - flag is to denote state internally in some operations
 * - domain is usually the unique domain number of a PMDA, but DYNAMIC_PMID
 *   (number 511) is reserved (see above for PMID encoding rules)
 * - serial uniquely identifies an InDom within a domain
 */
typedef struct {
#ifdef HAVE_BITFIELDS_LTOR
	int		flag : 1;
	unsigned int	domain : 9;
	unsigned int	serial : 22;
#else
	unsigned int	serial : 22;
	unsigned int	domain : 9;
	int		flag : 1;
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
pmInDom_build(unsigned int domain, unsigned int serial)
{
    pmInDom ind;
    __pmInDom_int indint;

    indint.flag = 0;
    indint.domain = domain;
    indint.serial = serial;
    memcpy(&ind, &indint, sizeof(ind));
    return ind;
}

/*
 * internal structure of a PMNS node
 */
typedef struct __pmnsNode {
    struct __pmnsNode	*parent;
    struct __pmnsNode	*next;
    struct __pmnsNode	*first;
    struct __pmnsNode	*hash;	/* used as "last" in build, then pmid hash synonym */
    char		*name;
    pmID		pmid;
} __pmnsNode;

/*
 * internal structure of a PMNS tree
 */
typedef struct __pmnsTree {
    __pmnsNode		*root;  /* root of tree structure */
    __pmnsNode		**htab; /* hash table of nodes keyed on pmid */
    int			htabsize;     /* number of nodes in the table */
    char		*symbol;     /* store all names contiguously */
    int			contiguous;   /* is data stored contiguously ? */
    int			mark_state;   /* the total mark value for trimming */
} __pmnsTree;


/* used by pmnsmerge... */
extern __pmnsTree *__pmExportPMNS(void); 

/* for PMNS in archives */
extern int __pmNewPMNS(__pmnsTree **);
extern void __pmFreePMNS(__pmnsTree *);
extern void __pmUsePMNS(__pmnsTree *); /* for debugging */
extern int __pmFixPMNSHashTab(__pmnsTree *, int, int);
extern int __pmAddPMNSNode(__pmnsTree *, int, const char *);

/* helper routine to print all names of a metric */
extern void __pmPrintMetricNames(FILE *, int, char **, char *);

/* return true if the named pmns file has changed */
extern int __pmHasPMNSFileChanged(const char *);

/* standard log file set up */
extern FILE *__pmOpenLog(const char *, const char *, FILE *, int *);
extern FILE *__pmRotateLog(const char *, const char *, FILE *, int *);
/* make __pmNotifyErr also add entries to syslog */
extern void __pmSyslog(int);
/* standard error, warning and info wrapper for syslog(3C) */
extern void __pmNotifyErr(int, const char *, ...) __PM_PRINTFLIKE(2,3);

/*
 * These are for debugging only (but are present in the shipped libpcp)
 */
EXTERN int	pmDebug;
#define DBG_TRACE_PDU		1	/* PDU send and receive */
#define DBG_TRACE_FETCH		2	/* dump pmFetch results */
#define DBG_TRACE_PROFILE	4	/* trace profile changes */
#define DBG_TRACE_VALUE		8	/* metric value conversions */
#define DBG_TRACE_CONTEXT	16	/* trace PMAPI context changes */
#define DBG_TRACE_INDOM		32	/* instance domain operations */
#define DBG_TRACE_PDUBUF	64	/* PDU buffer management */
#define DBG_TRACE_LOG		128	/* generic archive log operations */
#define DBG_TRACE_LOGMETA	(1<<8)	/* meta data in archives */
#define DBG_TRACE_OPTFETCH	(1<<9)	/* optFetch tracing */
#define DBG_TRACE_AF		(1<<10)	/* trace async timer events */
#define DBG_TRACE_APPL0		(1<<11)	/* reserved for applications */
#define DBG_TRACE_APPL1		(1<<12)	/* reserved for applications */
#define DBG_TRACE_APPL2		(1<<13)	/* reserved for applications */
#define DBG_TRACE_PMNS		(1<<14)	/* PMNS operations */
#define DBG_TRACE_LIBPMDA	(1<<15)	/* libpcp_pmda */
#define DBG_TRACE_TIMECONTROL	(1<<16)	/* time control api */
#define DBG_TRACE_PMC		(1<<17)	/* metrics class */
#define DBG_TRACE_DERIVE	(1<<18)	/* derived metrics */
#define DBG_TRACE_LOCK		(1<<19) /* lock tracing */
#define DBG_TRACE_INTERP	(1<<20)	/* interpolate mode for archives */
#define DBG_TRACE_CONFIG	(1<<21) /* configuration parameters */
#define DBG_TRACE_LOOP		(1<<22) /* pmLoop tracing */
#define DBG_TRACE_FAULT		(1<<23) /* fault injection tracing */
#define DBG_TRACE_AUTH		(1<<24) /* authentication tracing */
#define DBG_TRACE_DISCOVERY	(1<<25) /* service discovery tracing */
#define DBG_TRACE_ATTR		(1<<26) /* all connection attributes */
/* not yet allocated, bits (1<<27) ... (1<<29) */
#define DBG_TRACE_DESPERATE	(1<<30) /* verbose/desperate level */

extern int __pmParseDebug(const char *);
extern void __pmDumpResult(FILE *, const pmResult *);
extern void __pmDumpHighResResult(FILE *, const pmHighResResult *);
extern void __pmPrintStamp(FILE *, const struct timeval *);
extern void __pmPrintHighResStamp(FILE *, const struct timespec *);
extern void __pmPrintTimespec(FILE *, const __pmTimespec *);
extern void __pmPrintTimeval(FILE *, const __pmTimeval *);
extern void __pmPrintDesc(FILE *, const pmDesc *);
extern void __pmFreeResultValues(pmResult *);
extern char *__pmPDUTypeStr_r(int, char *, int);
extern const char *__pmPDUTypeStr(int);			/* NOT thread-safe */
extern void __pmDumpNameSpace(FILE *, int);
extern void __pmDumpNameNode(FILE *, __pmnsNode *, int);
extern void __pmDumpStack(FILE *);
EXTERN int __pmLogReads;

#ifdef PCP_DEBUG
extern void __pmDumpIDList(FILE *, int, const pmID *);
extern void __pmDumpNameList(FILE *, int, char **);
extern void __pmDumpStatusList(FILE *, int, const int *);
extern void __pmDumpNameAndStatusList(FILE *, int, char **, int *);
#endif

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
typedef struct __pmHashNode {
    struct __pmHashNode	*next;
    unsigned int	key;
    void		*data;
} __pmHashNode;

typedef struct __pmHashCtl {
    int			nodes;
    int			hsize;
    __pmHashNode	**hash;
    __pmHashNode	*next;
    unsigned int	index;
} __pmHashCtl;

typedef enum {
    PM_HASH_WALK_START = 0,
    PM_HASH_WALK_NEXT,
    PM_HASH_WALK_STOP,
    PM_HASH_WALK_DELETE_NEXT,
    PM_HASH_WALK_DELETE_STOP,
} __pmHashWalkState;

extern void __pmHashInit(__pmHashCtl *);
typedef __pmHashWalkState(*__pmHashWalkCallback)(const __pmHashNode *, void *);
extern void __pmHashWalkCB(__pmHashWalkCallback, void *, const __pmHashCtl *);
extern __pmHashNode *__pmHashWalk(__pmHashCtl *, __pmHashWalkState);
extern __pmHashNode *__pmHashSearch(unsigned int, __pmHashCtl *);
extern int __pmHashAdd(unsigned int, void *, __pmHashCtl *);
extern int __pmHashDel(unsigned int, void *, __pmHashCtl *);
extern void __pmHashClear(__pmHashCtl *);

/*
 * External file and internal (below PMAPI) format for an archive label
 * Note: int is OK here, because configure ensures int is a 32-bit integer
 */
typedef struct {
    int		ill_magic;	/* PM_LOG_MAGIC | log format version no. */
    int		ill_pid;			/* PID of logger */
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
 * Note: int is OK here, because configure ensures int is a 32-bit integer
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
    __pmnsTree	*l_pmns;        /* namespace from meta data */
} __pmLogCtl;

/* l_state values */
#define PM_LOG_STATE_NEW	0
#define PM_LOG_STATE_INIT	1

/*
 * Return the argument if it's a valid filename else return NULL
 * (note: this function could be replaced with a call to access(),
 * but is retained for historical reasons).
 */
extern const char *__pmFindPMDA(const char *);

/*
 * Internal instance profile states 
 */
#define PM_PROFILE_INCLUDE 0	/* include all, exclude some */
#define PM_PROFILE_EXCLUDE 1	/* exclude all, include some */

/* Profile entry (per instance domain) */
typedef struct __pmInDomProfile {
    pmInDom	indom;			/* instance domain */
    int		state;			/* include all or exclude all */
    int		instances_len;		/* length of instances array */
    int		*instances;		/* array of instances */
} __pmInDomProfile;

/* Instance profile for all domains */
typedef struct __pmProfile {
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
typedef struct __pmInResult {
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
 * Version and capabilities information for PDU exchanges
 */

#define UNKNOWN_VERSION	0
#define PDU_VERSION2	2
#define PDU_VERSION	PDU_VERSION2

#define PDU_OVERRIDE2	-1002

typedef struct {
#ifdef HAVE_BITFIELDS_LTOR
    unsigned int	zero     : 1;	/* ensure this is zero for 1.x compatibility */
    unsigned int	version  : 7;	/* PDU_VERSION collector protocol preference */
    unsigned int	licensed : 8;	/* ensure this is one for 2.x compatibility */
    unsigned int	features : 16;	/* advertised (enabled) collector features */
#else
    unsigned int	features : 16;
    unsigned int	licensed : 8;
    unsigned int	version  : 7;
    unsigned int	zero     : 1;
#endif
} __pmPDUInfo;

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
extern int __pmUnparseHostSpec(pmHostSpec *, int, char *, size_t);
extern void __pmFreeHostSpec(pmHostSpec *, int);

typedef enum {
    PCP_ATTR_NONE	= 0,
    PCP_ATTR_PROTOCOL	= 1,	/* either pcp:/pcps: protocol (libssl) */
    PCP_ATTR_SECURE	= 2,	/* relaxed/enforced pcps mode (libssl) */
    PCP_ATTR_COMPRESS	= 3,	/* compression flag, no value (libnss) */
    PCP_ATTR_USERAUTH	= 4,	/* user auth flag, no value (libsasl) */
    PCP_ATTR_USERNAME	= 5,	/* user login identity (libsasl) */
    PCP_ATTR_AUTHNAME	= 6,	/* authentication name (libsasl) */
    PCP_ATTR_PASSWORD	= 7,	/* passphrase-based secret (libsasl) */
    PCP_ATTR_METHOD	= 8,	/* use authentication method (libsasl) */
    PCP_ATTR_REALM	= 9,	/* realm to authenticate in (libsasl) */
    PCP_ATTR_UNIXSOCK	= 10,	/* AF_UNIX socket + SO_PEERCRED (unix) */
    PCP_ATTR_USERID	= 11,	/* uid - user identifier (posix) */
    PCP_ATTR_GROUPID	= 12,	/* gid - group identifier (posix) */
    PCP_ATTR_LOCAL	= 13,	/* AF_UNIX socket with localhost fallback */
    PCP_ATTR_PROCESSID	= 14,	/* pid - process identifier (posix) */
    PCP_ATTR_CONTAINER	= 15,	/* container name (linux) */
} __pmAttrKey;

extern __pmAttrKey __pmLookupAttrKey(const char *, size_t);
extern int __pmAttrKeyStr_r(__pmAttrKey, char *, size_t);
extern int __pmAttrStr_r(__pmAttrKey, const char *, char *, size_t);

extern int __pmParseHostAttrsSpec(
    const char *, pmHostSpec **, int *, __pmHashCtl *, char **);
extern int __pmUnparseHostAttrsSpec(
    pmHostSpec *, int, __pmHashCtl *, char *, size_t);
extern void __pmFreeHostAttrsSpec(pmHostSpec *, int, __pmHashCtl *);
extern void __pmFreeAttrsSpec(__pmHashCtl *);

/*
 * Control for connection to a PMCD
 */
typedef struct {
    __pmMutex		pc_lock;	/* mutex pmcd ipc */
    int			pc_refcnt;	/* number of contexts using this socket */
    int			pc_fd;		/* socket for comm with pmcd */
					/* ... -1 means no connection */
    pmHostSpec		*pc_hosts;	/* pmcd and proxy host specifications */
    int			pc_nhosts;	/* number of pmHostSpec entries */
    int			pc_timeout;	/* set if connect times out */
    int			pc_tout_sec;	/* timeout for __pmGetPDU */
    time_t		pc_again;	/* time to try again */
} __pmPMCDCtl;

extern int __pmConnectPMCD(pmHostSpec *, int, int, __pmHashCtl *);
extern int __pmConnectLocal(__pmHashCtl *);
extern int __pmAuxConnectPMCD(const char *);
extern int __pmAuxConnectPMCDPort(const char *, int);
extern int __pmAuxConnectPMCDUnixSocket(const char *);

extern int __pmAddHostPorts(pmHostSpec *, int *, int);
extern void __pmDropHostPort(pmHostSpec *);
extern void __pmConnectGetPorts(pmHostSpec *);

/*
 * SSL/TLS/IPv6 support via NSS/NSPR.
 */
extern int __pmSecureServerSetup(const char *, const char *);
extern void __pmSecureServerShutdown(void);
extern int __pmSecureServerHandshake(int, int, __pmHashCtl *);
extern int __pmSecureClientHandshake(int, int, const char *, __pmHashCtl *);

typedef fd_set __pmFdSet;
typedef struct __pmSockAddr __pmSockAddr;
typedef struct __pmHostEnt __pmHostEnt;

extern int __pmCreateSocket(void);
extern int __pmCreateIPv6Socket(void);
extern int __pmCreateUnixSocket(void);
extern void __pmCloseSocket(int);

extern int __pmSetSockOpt(int, int, int, const void *, __pmSockLen);
extern int __pmGetSockOpt(int, int, int, void *, __pmSockLen *);
extern int __pmConnect(int, void *, __pmSockLen);
extern int __pmBind(int, void *, __pmSockLen);
extern int __pmListen(int, int);
extern int __pmAccept(int, void *, __pmSockLen *);
extern ssize_t __pmWrite(int, const void *, size_t);
extern ssize_t __pmRead(int, void *, size_t);
extern ssize_t __pmSend(int, const void *, size_t, int);
extern ssize_t __pmRecv(int, void *, size_t, int);
extern int __pmConnectTo(int, const __pmSockAddr *, int);
extern int __pmConnectCheckError(int);
extern int __pmConnectRestoreFlags(int, int);
extern int __pmSocketClosed(void);
extern int __pmGetFileStatusFlags(int);
extern int __pmSetFileStatusFlags(int, int);
extern int __pmGetFileDescriptorFlags(int);
extern int __pmSetFileDescriptorFlags(int, int);

extern int  __pmFD(int);
extern void __pmFD_CLR(int, __pmFdSet *);
extern int  __pmFD_ISSET(int, __pmFdSet *);
extern void __pmFD_SET(int, __pmFdSet *);
extern void __pmFD_ZERO(__pmFdSet *);
extern void __pmFD_COPY(__pmFdSet *, const __pmFdSet *);
extern int __pmSelectRead(int, __pmFdSet *, struct timeval *);
extern int __pmSelectWrite(int, __pmFdSet *, struct timeval *);

extern __pmSockAddr *__pmSockAddrAlloc(void);
extern void	     __pmSockAddrFree(__pmSockAddr *);
extern size_t	     __pmSockAddrSize(void);
extern void	     __pmSockAddrInit(__pmSockAddr *, int, int, int);
extern int	     __pmSockAddrCompare(const __pmSockAddr *, const __pmSockAddr *);
extern __pmSockAddr *__pmSockAddrDup(const __pmSockAddr *);
extern __pmSockAddr *__pmSockAddrMask(__pmSockAddr *, const __pmSockAddr *);
extern void	     __pmSockAddrSetFamily(__pmSockAddr *, int);
extern int	     __pmSockAddrGetFamily(const __pmSockAddr *);
extern void	     __pmSockAddrSetPort(__pmSockAddr *, int);
extern int	     __pmSockAddrGetPort(const __pmSockAddr *);
extern void	     __pmSockAddrSetScope(__pmSockAddr *, int);
extern void	     __pmSockAddrSetPath(__pmSockAddr *, const char *);
extern int	     __pmSockAddrIsLoopBack(const __pmSockAddr *);
extern int	     __pmSockAddrIsInet(const __pmSockAddr *);
extern int	     __pmSockAddrIsIPv6(const __pmSockAddr *);
extern int	     __pmSockAddrIsUnix(const __pmSockAddr *);
extern char *	     __pmSockAddrToString(const __pmSockAddr *);
extern __pmSockAddr *__pmStringToSockAddr(const char *);
extern __pmSockAddr *__pmLoopBackAddress(int);
extern __pmSockAddr *__pmSockAddrFirstSubnetAddr(const __pmSockAddr *, int);
extern __pmSockAddr *__pmSockAddrNextSubnetAddr(__pmSockAddr *, int);

extern __pmHostEnt * __pmHostEntAlloc(void);
extern void	     __pmHostEntFree(__pmHostEnt *);
extern __pmSockAddr *__pmHostEntGetSockAddr(const __pmHostEnt *, void **);
extern char *	     __pmHostEntGetName(__pmHostEnt *);

extern __pmHostEnt * __pmGetAddrInfo(const char *);
extern char *	     __pmGetNameInfo(__pmSockAddr *);

/*
 * Query server features - used for expressing protocol capabilities
 */
typedef enum {
    PM_SERVER_FEATURE_SECURE = 0,
    PM_SERVER_FEATURE_COMPRESS,
    PM_SERVER_FEATURE_IPV6,
    PM_SERVER_FEATURE_AUTH,
    PM_SERVER_FEATURE_CREDS_REQD,
    PM_SERVER_FEATURE_UNIX_DOMAIN,
    PM_SERVER_FEATURE_DISCOVERY,
    PM_SERVER_FEATURE_CONTAINERS,
    PM_SERVER_FEATURES
} __pmServerFeature;

extern int __pmServerHasFeature(__pmServerFeature);
extern int __pmServerSetFeature(__pmServerFeature);
extern int __pmServerClearFeature(__pmServerFeature);
extern int __pmServerCreatePIDFile(const char *, int);
extern int __pmServerAddPorts(const char *);
extern int __pmServerAddInterface(const char *);
extern void __pmServerSetLocalSocket(const char *);
extern int __pmServerSetLocalCreds(int,  __pmHashCtl *);
extern void __pmServerSetServiceSpec(const char *);
typedef void (*__pmServerCallback)(__pmFdSet *, int, int);
extern void __pmServerAddNewClients(__pmFdSet *, __pmServerCallback);
extern int __pmServerAddToClientFdSet(__pmFdSet *, int);
extern int __pmServerOpenRequestPorts(__pmFdSet *, int);
extern void __pmServerCloseRequestPorts(void);
extern void __pmServerDumpRequestPorts(FILE *);
extern char *__pmServerRequestPortString(int, char *, size_t);

/* Service broadcasting, for servers. */
typedef struct __pmServerPresence __pmServerPresence;
extern __pmServerPresence *__pmServerAdvertisePresence(const char *, int);
extern void __pmServerUnadvertisePresence(__pmServerPresence *);

/*
 * Per-context controls for archives and logs
 */
typedef struct {
    __pmLogCtl		*ac_log;	/* global logging and archive control */
    long		ac_offset;	/* fseek ptr for archives */
    int			ac_vol;		/* volume for ac_offset */
    int			ac_serial;	/* serial access pattern for archives */
    __pmHashCtl		ac_pmid_hc;	/* per PMID controls for INTERP */
    double		ac_end;		/* time at end of archive */
    void		*ac_want;	/* used in interp.c */
    void		*ac_unbound;	/* used in interp.c */
    void		*ac_cache;	/* used in interp.c */
    int			ac_cache_idx;	/* used in interp.c */
} __pmArchCtl;

/*
 * PMAPI context. We keep an array of these,
 * one for each context created by the application.
 */
typedef struct {
    __pmMutex		c_lock;		/* mutex for multi-thread access */
    int			c_type;		/* HOST, ARCHIVE, LOCAL or FREE */
    int			c_mode;		/* current mode PM_MODE_* */
    __pmPMCDCtl		*c_pmcd;	/* pmcd control for HOST contexts */
    __pmArchCtl		*c_archctl;	/* log control for ARCHIVE contexts */
    __pmTimeval		c_origin;	/* pmFetch time origin / current time */
    int			c_delta;	/* for updating origin */
    int			c_sent;		/* profile has been sent to pmcd */
    __pmProfile		*c_instprof;	/* instance profile */
    void		*c_dm;		/* derived metrics, if any */
    int			c_flags;	/* ctx flags (set via type/env/attrs) */
    __pmHashCtl		c_attrs;	/* various optional context attributes */
} __pmContext;

#define __PM_MODE_MASK	0xffff

#define PM_CONTEXT_FREE	-1		/* special type */

/*
 * Convert opaque context handle to __pmContext pointer
 */
extern __pmContext *__pmHandleToPtr(int);

/*
 * Dump the current context (source details + instance profile),
 * for a particular instance domain.
 * If indom == PM_INDOM_NULL, then print all all instance domains
 */
extern void __pmDumpContext(FILE *, int, pmInDom);

/*
 * pmFetch helper routines, hooks for derivations and local contexts.
 */
extern int __pmPrepareFetch(__pmContext *, int, const pmID *, pmID **);
extern int __pmFinishResult(__pmContext *, int, pmResult **);
extern int __pmFetchLocal(__pmContext *, int, pmID *, pmResult **);

/*
 * Protocol data unit support
 * Note: int is OK here, because configure ensures int is a 32-bit integer
 */
typedef struct {
    int		len;		/* length of pdu_header + PDU */
    int		type;		/* PDU type */
    int		from;		/* pid of PDU originator */
} __pmPDUHdr;

typedef __uint32_t	__pmPDU;
/*
 * round a size up to the next multiple of a __pmPDU size
 *
 * PM_PDU_SIZE is in units of __pmPDU size
 * PM_PDU_SIZE_BYTES is in units of bytes
 */
#define PM_PDU_SIZE(x) (((x)+sizeof(__pmPDU)-1)/sizeof(__pmPDU))
#define PM_PDU_SIZE_BYTES(x) (sizeof(__pmPDU)*PM_PDU_SIZE(x))

/* Types of credential PDUs (c_type) */
#define CVERSION        0x1

typedef struct {
#ifdef HAVE_BITFIELDS_LTOR
    unsigned int	c_type: 8;	/* Credentials PDU type */
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

/* Flags for CVERSION credential PDUs, and __pmPDUInfo features */
#define PDU_FLAG_SECURE		(1U<<0)
#define PDU_FLAG_COMPRESS	(1U<<1)
#define PDU_FLAG_AUTH		(1U<<2)
#define PDU_FLAG_CREDS_REQD	(1U<<3)
#define PDU_FLAG_SECURE_ACK	(1U<<4)
#define PDU_FLAG_NO_NSS_INIT	(1U<<5)
#define PDU_FLAG_CONTAINER	(1U<<6)

/* Credential CVERSION PDU elements look like this */
typedef struct {
#ifdef HAVE_BITFIELDS_LTOR
    unsigned int	c_type: 8;	/* Credentials PDU type */
    unsigned int	c_version: 8;	/* PCP protocol version */
    unsigned int	c_flags: 16;	/* All feature requests */
#else
    unsigned int	c_flags: 16;
    unsigned int	c_version: 8;
    unsigned int	c_type: 8;
#endif
} __pmVersionCred;

extern void __pmIgnoreSignalPIPE(void);
extern int __pmXmitPDU(int, __pmPDU *);
extern int __pmGetPDU(int, int, int, __pmPDU **);
extern int __pmGetPDUCeiling(void);
extern int __pmSetPDUCeiling(int);

EXTERN unsigned int *__pmPDUCntIn;
EXTERN unsigned int *__pmPDUCntOut;
extern void __pmSetPDUCntBuf(unsigned *, unsigned *);

/* timeout options for PDU handling */
#define TIMEOUT_NEVER	 0
#define TIMEOUT_DEFAULT	-1
/*#define TIMEOUT_ASYNC -2*/
#define TIMEOUT_CONNECT	-3

/* mode options for __pmGetPDU */
#define ANY_SIZE	0	/* replacement for old PDU_BINARY */
#define LIMIT_SIZE	2	/* replacement for old PDU_CLIENT */

extern __pmPDU *__pmFindPDUBuf(int);
extern void __pmPinPDUBuf(void *);
extern int __pmUnpinPDUBuf(void *);
extern void __pmCountPDUBuf(int, int *, int *);

#define PDU_START		0x7000
#define PDU_ERROR		PDU_START
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
#define PDU_CREDS		0x700c
#define PDU_PMNS_IDS		0x700d
#define PDU_PMNS_NAMES		0x700e
#define PDU_PMNS_CHILD		0x700f
#define PDU_PMNS_TRAVERSE	0x7010
#define PDU_ATTR		0x7011
#define PDU_AUTH		PDU_ATTR
#define PDU_FINISH		0x7011
#define PDU_MAX		 	(PDU_FINISH - PDU_START)

/*
 * Unit of space allocation for PDU buffer.
 */
#define PDU_CHUNK		1024

/*
 * PDU encoding formats
 * These have been retired ...
 *  #define PDU_BINARY	0
 *  #define PDU_ASCII	1
 * And this has been replaced by LIMIT_SIZE for __pmGetPDU
 *  #define PDU_CLIENT	2
 */

/*
 * Anonymous PDU sender, when context does not matter, e.g. PDUs from
 * a PMDA sent to PMCD
 */
#define FROM_ANON	0

extern int __pmSendError(int, int, int);
extern int __pmDecodeError(__pmPDU *, int *);
extern int __pmSendXtendError(int, int, int, int);
extern int __pmDecodeXtendError(__pmPDU *, int *, int *);
extern int __pmSendResult(int, int, const pmResult *);
extern int __pmEncodeResult(int, const pmResult *, __pmPDU **);
extern int __pmDecodeResult(__pmPDU *, pmResult **);
extern int __pmSendProfile(int, int, int, __pmProfile *);
extern int __pmDecodeProfile(__pmPDU *, int *, __pmProfile **);
extern int __pmSendFetch(int, int, int, __pmTimeval *, int, pmID *);
extern int __pmDecodeFetch(__pmPDU *, int *, __pmTimeval *, int *, pmID **);
extern int __pmSendDescReq(int, int, pmID);
extern int __pmDecodeDescReq(__pmPDU *, pmID *);
extern int __pmSendDesc(int, int, pmDesc *);
extern int __pmDecodeDesc(__pmPDU *, pmDesc *);
extern int __pmSendInstanceReq(int, int, const __pmTimeval *, pmInDom, int, const char *);
extern int __pmDecodeInstanceReq(__pmPDU *, __pmTimeval *, pmInDom *, int *, char **);
extern int __pmSendInstance(int, int, __pmInResult *);
extern int __pmDecodeInstance(__pmPDU *, __pmInResult **);
extern int __pmSendTextReq(int, int, int, int);
extern int __pmDecodeTextReq(__pmPDU *, int *, int *);
extern int __pmSendText(int, int, int, const char *);
extern int __pmDecodeText(__pmPDU *, int *, char **);
extern int __pmSendCreds(int, int, int, const __pmCred *);
extern int __pmDecodeCreds(__pmPDU *, int *, int *, __pmCred **);
extern int __pmSendIDList(int, int, int, const pmID *, int);
extern int __pmDecodeIDList(__pmPDU *, int, pmID *, int *);
extern int __pmSendNameList(int, int, int, char **, const int *);
extern int __pmDecodeNameList(__pmPDU *, int *, char ***, int **);
extern int __pmSendChildReq(int, int, const char *, int);
extern int __pmDecodeChildReq(__pmPDU *, char **, int *);
extern int __pmSendTraversePMNSReq(int, int, const char *);
extern int __pmDecodeTraversePMNSReq(__pmPDU *, char **);
extern int __pmSendAuth(int, int, int, const char *, int);
extern int __pmDecodeAuth(__pmPDU *, int *, char **, int *);
extern int __pmSendAttr(int, int, int, const char *, int);
extern int __pmDecodeAttr(__pmPDU *, int *, char **, int *);

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
 * no mem today, my love has gone away ....
 */
extern void __pmNoMem(const char *, size_t, int);
#define PM_FATAL_ERR 1
#define PM_RECOV_ERR 0

/*
 * Startup handling:
 * set program name, as used in __pmNotifyErr() ... default is "pcp"
 * set default user for __pmSetProcessIdentity() ... default is "pcp"
 */
EXTERN char *pmProgname;
extern int __pmSetProgname(const char *);
extern int __pmGetUsername(char **);

/*
 * Cleanup handling:
 * shutdown various components in libpcp, releasing all resources
 * (local context PMDAs, any global NSS socket state, etc).
 */
extern int __pmShutdown(void);

/*
 * Map platform error values to PMAPI error codes.
 */
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

extern const char *__pmLogName_r(const char *, int, char *, int);
extern const char *__pmLogName(const char *, int);	/* NOT thread-safe */
extern FILE *__pmLogNewFile(const char *, int);
extern int __pmLogCreate(const char *, const char *, int, __pmLogCtl *);
#define PMLOGREAD_NEXT		0
#define PMLOGREAD_TO_EOF	1
extern int __pmLogRead(__pmLogCtl *, int, FILE *, pmResult **, int);
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
extern int __pmLogPutResult2(__pmLogCtl *, __pmPDU *);
extern int __pmLogFetch(__pmContext *, int, pmID *, pmResult **);
extern int __pmLogFetchInterp(__pmContext *, int, pmID *, pmResult **);
extern void __pmLogSetTime(__pmContext *);
extern void __pmLogResetInterp(__pmContext *);
extern void __pmFreeInterpData(__pmContext *);

extern int __pmLogChangeVol(__pmLogCtl *, int);
extern int __pmLogChkLabel(__pmLogCtl *, FILE *, __pmLogLabel *, int);
extern int __pmGetArchiveEnd(__pmLogCtl *, struct timeval *);

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

extern const char *__pmLogLocalSocketDefault(int, char *buf, size_t bufSize);
extern const char *__pmLogLocalSocketUser(int, char *buf, size_t bufSize);

/* time utils */
extern time_t __pmMktime(struct tm *);

/* reverse ctime and time interval parsing */
extern int __pmParseCtime(const char *, struct tm *, char **);
extern int __pmConvertTime(struct tm *, struct timeval *, struct timeval *);
extern int __pmParseTime(const char *, struct timeval *, struct timeval *,
			 struct timeval *, char **);

/* manipulate internal timestamps */
extern double __pmTimevalSub(const __pmTimeval *, const __pmTimeval *);

/* 32-bit file checksum */
extern __int32_t __pmCheckSum(FILE *);

/* check for localhost */
extern int __pmIsLocalhost(const char *);

/*
 * struct timeval manipulations
 */
extern double __pmtimevalAdd(const struct timeval *, const struct timeval *);
extern void __pmtimevalInc(struct timeval *, const struct timeval *);
extern double __pmtimevalSub(const struct timeval *, const struct timeval *);
extern void __pmtimevalDec(struct timeval *, const struct timeval *);
extern double __pmtimevalToReal(const struct timeval *);
extern void __pmtimevalFromReal(double, struct timeval *);
extern void __pmtimevalSleep(struct timeval);
extern void __pmtimevalPause(struct timeval);
extern void __pmtimevalNow(struct timeval *);

typedef struct {
    char		*label;		/* label to name tz */
    char		*tz;		/* env $TZ */
    int			handle;		/* handle from pmNewZone() */
} pmTimeZone;

/*
 * event tracing for monitoring time between events
 */
extern void __pmEventTrace(const char *);		/* NOT thread-safe */
extern void __pmEventTrace_r(const char *, int *, double *, double *);

/*
 * More IPC protocol stuff
 */

typedef int (*__pmConnectHostType)(int, int);

extern int __pmSetSocketIPC(int);
extern int __pmSetVersionIPC(int, int);
extern int __pmSetDataIPC(int, void *);
extern int __pmDataIPCSize(void);
extern int __pmLastVersionIPC(void);
extern int __pmVersionIPC(int);
extern int __pmSocketIPC(int);
extern int __pmDataIPC(int, void *);
extern void __pmOverrideLastFd(int);
extern void __pmPrintIPC(void);
extern void __pmResetIPC(int);

/* safely insert an atom value into a pmValue */
extern int __pmStuffValue(const pmAtomValue *, pmValue *, int);

/*
 * Optimized fetch bundling ("optfetch") services
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

extern void __pmOptFetchAdd(fetchctl_t **, optreq_t *);
extern int __pmOptFetchDel(fetchctl_t **, optreq_t *);
extern void __pmOptFetchRedo(fetchctl_t **);
extern void __pmOptFetchDump(FILE *, const fetchctl_t *);
extern void __pmOptFetchGetParams(optcost_t *);
extern void __pmOptFetchPutParams(optcost_t *);

/* work out local timezone */
extern char *__pmTimezone(void);			/* NOT thread-safe */
extern char *__pmTimezone_r(char *, int);

/*
 */

/*
 * Generic access control routines
 */
extern int __pmAccAddOp(unsigned int);

extern int __pmAccAddHost(const char *, unsigned int, unsigned int, int);
extern int __pmAccAddUser(const char *, unsigned int, unsigned int, int);
extern int __pmAccAddGroup(const char *, unsigned int, unsigned int, int);

extern int __pmAccAddClient(__pmSockAddr *, unsigned int *);
extern int __pmAccAddAccount(const char *, const char *, unsigned int *);
extern void __pmAccDelClient(__pmSockAddr *);
extern void __pmAccDelAccount(const char *, const char *);

extern void __pmAccDumpHosts(FILE *);
extern void __pmAccDumpUsers(FILE *);
extern void __pmAccDumpGroups(FILE *);
extern void __pmAccDumpLists(FILE *);

extern int __pmAccSaveHosts(void);
extern int __pmAccSaveUsers(void);
extern int __pmAccSaveGroups(void);
extern int __pmAccSaveLists(void);

extern int __pmAccRestoreHosts(void);
extern int __pmAccRestoreUsers(void);
extern int __pmAccRestoreGroups(void);
extern int __pmAccRestoreLists(void);

extern void __pmAccFreeSavedHosts(void);
extern void __pmAccFreeSavedUsers(void);
extern void __pmAccFreeSavedGroups(void);
extern void __pmAccFreeSavedLists(void);

/*
 * platform independent process routines
 */
extern int __pmProcessExists(pid_t);
extern int __pmProcessTerminate(pid_t, int);
extern pid_t __pmProcessCreate(char **, int *, int *);
extern int __pmProcessDataSize(unsigned long *);
extern int __pmProcessRunTimes(double *, double *);
extern int __pmSetProcessIdentity(const char *);

/*
 * platform independent memory mapped file handling
 */
extern void *__pmMemoryMap(int, size_t, int);
extern void __pmMemoryUnmap(void *, size_t);

/*
 * platform independent signal handling
 */
typedef void (*__pmSignalHandler)(int);
extern int __pmSetSignalHandler(int, __pmSignalHandler);

/*
 * platform independent environment and filesystem path access
 */
typedef void (*__pmConfigCallback)(char *, char *, char *);
EXTERN const __pmConfigCallback __pmNativeConfig;
extern void __pmConfig(__pmConfigCallback);
extern char *__pmNativePath(char *);
extern int __pmAbsolutePath(char *);
extern int __pmPathSeparator(void);
extern int __pmMakePath(const char *, mode_t);

/*
 * discover configurable features of the shared libraries
 */
typedef void (*__pmAPIConfigCallback)(const char *, const char *);
extern void __pmAPIConfig(__pmAPIConfigCallback);
extern const char *__pmGetAPIConfig(const char *);

/*
 * internals of argument parsing for special circumstances
 */
extern void __pmStartOptions(pmOptions *);
extern void __pmAddOptArchive(pmOptions *, char *);
extern void __pmAddOptArchiveList(pmOptions *, char *);
extern void __pmAddOptArchiveFolio(pmOptions *, char *);
extern void __pmAddOptHost(pmOptions *, char *);
extern void __pmAddOptHostList(pmOptions *, char *);
extern void __pmEndOptions(pmOptions *);

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

/* logger timeout helper function */
extern int __pmLoggerTimeout(void);

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
        (val) = ((val) & ~0x1) | ((flag) & 0x1)
#define PMLC_GET_ON(val) \
        ((val) & 0x1)
#define PMLC_SET_MAND(val, flag) \
        (val) = ((val) & ~0x2) | (((flag) & 0x1) << 1)
#define PMLC_GET_MAND(val) \
        (((val) & 0x2) >> 1)
#define PMLC_SET_AVAIL(val, flag) \
        (val) = ((val) & ~0x4) | (((flag) & 0x1) << 2)
#define PMLC_GET_AVAIL(val) \
        (((val) & 0x4) >> 2)
#define PMLC_SET_INLOG(val, flag) \
        (val) = ((val) & ~0x8) | (((flag) & 0x1) << 3)
#define PMLC_GET_INLOG(val) \
        (((val) & 0x8) >> 3)

#define PMLC_SET_STATE(val, state) \
        (val) = ((val) & ~0xf) | ((state) & 0xf)
#define PMLC_GET_STATE(val) \
        ((val) & 0xf)

/* 28 bits of delta, 32 bits of state */
#define PMLC_MAX_DELTA  0x0fffffff

#define PMLC_SET_DELTA(val, delta) \
        (val) = ((val) & 0xf) | ((delta) << 4)
#define PMLC_GET_DELTA(val) \
        ((((val) & ~0xf) >> 4) & PMLC_MAX_DELTA)

/*
 * helper functions to register client identity with pmcd for export
 * via pmcd.client.whoami
 */
extern char *__pmGetClientId(int, char **);
extern int __pmSetClientIdArgv(int, char **);
extern int __pmSetClientId(const char *);

/*
 * Adding/deleting/clearing the list of DSO PMDAs supported for
 * PM_CONTEXT_LOCAL contexts
 */
#define PM_LOCAL_ADD	1
#define PM_LOCAL_DEL	2
#define PM_LOCAL_CLEAR	3
extern int __pmLocalPMDA(int, int, const char *, const char *);
extern char *__pmSpecLocalPMDA(const char *);

/*
 * Helper methods for packed arrays of event records
 */
extern int __pmCheckEventRecords(pmValueSet *, int);
extern int __pmCheckHighResEventRecords(pmValueSet *, int);
extern void __pmDumpEventRecords(FILE *, pmValueSet *, int);
extern void __pmDumpHighResEventRecords(FILE *, pmValueSet *, int);

/* Get nanosecond precision timestamp from system clocks */
extern int __pmGetTimespec(struct timespec *);

/* Anonymous metric registration (uses derived metrics support) */
extern int __pmRegisterAnon(const char *, int);

/*
 * Multi-thread support
 * Use PM_MULTI_THREAD_DEBUG for lock debugging with -Dlock[,appl?...]
 */
extern void __pmInitLocks(void);
extern int __pmLock(void *, const char *, int);
extern int __pmUnlock(void *, const char *, int);

/*
 * Each of these scopes defines one or more PMAPI routines that will
 * not allow calls from more than one thread.
 */
#define PM_SCOPE_DSO_PMDA	0
#define PM_SCOPE_ACL		1
#define PM_SCOPE_AF		2
#define PM_SCOPE_LOGPORT	3
#define PM_SCOPE_MAX		3
extern int __pmMultiThreaded(int);

#define PM_INIT_LOCKS()		__pmInitLocks()
#define PM_MULTIPLE_THREADS(x)	__pmMultiThreaded(x)
#define PM_LOCK(lock)		__pmLock(&(lock), __FILE__, __LINE__)
#define PM_UNLOCK(lock)		__pmUnlock(&(lock), __FILE__, __LINE__)

#ifdef HAVE_PTHREAD_MUTEX_T
extern pthread_mutex_t	__pmLock_libpcp;	/* big libpcp lock */
#else
extern void *__pmLock_libpcp;			/* symbol exposure */
#endif

/*
 * Service discovery with options.
 * The 4th argument is a pointer to a mask of flags for boolean options
 * and status. It is set and tested using the following bits.
 */
#define PM_SERVICE_DISCOVERY_INTERRUPTED	0x1
#define PM_SERVICE_DISCOVERY_RESOLVE		0x2

extern int __pmDiscoverServicesWithOptions(const char *,
					   const char *,
					   const char *,
					   const volatile sig_atomic_t *,
					   char ***);


#ifdef __cplusplus
}
#endif

#endif /* PCP_IMPL_H */
