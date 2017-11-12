/*
 * Copyright (c) 2012-2017 Red Hat.
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef PCP_IMPL_H
#define PCP_IMPL_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * functions to be promoted to pmapi.h and the __pm version added
 * to deprecated.*
 * === start ==
 */

/* platform independent set process identity */
PCP_CALL extern int __pmSetProcessIdentity(const char *);

/* filesystem path name separator */
PCP_CALL extern int __pmPathSeparator(void);

/* safely insert an atom value into a pmValue */
PCP_CALL extern int __pmStuffValue(const pmAtomValue *, pmValue *, int);

/* struct timeval/timespec manipulations */
PCP_CALL extern void __pmtimevalNow(struct timeval *);
PCP_CALL extern void __pmtimevalInc(struct timeval *, const struct timeval *);
PCP_CALL extern void __pmtimevalDec(struct timeval *, const struct timeval *);
PCP_CALL extern double __pmtimevalAdd(const struct timeval *, const struct timeval *);
PCP_CALL extern double __pmtimevalSub(const struct timeval *, const struct timeval *);
PCP_CALL extern double __pmtimevalToReal(const struct timeval *);
PCP_CALL extern void __pmtimevalFromReal(double, struct timeval *);
PCP_CALL extern void __pmPrintStamp(FILE *, const struct timeval *);
PCP_CALL extern void __pmPrintHighResStamp(FILE *, const struct timespec *);

/* == end == */


struct __pmnsTree;	/* TODO remove when __pmLogCtl moves to libpcp.h */

/*
 * This defines the routines, macros and data structures that are used
 * in the Performance Metrics Collection Subsystem (PMCS) below the
 * PMAPI.
 */

/* TODO .. move to libpcp.h when all refs to __pmMutex have gone */
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

/* helper routine to print all names of a metric */
PCP_CALL extern void __pmPrintMetricNames(FILE *, int, char **, char *);

/* return true if the named pmns file has changed */
PCP_CALL extern int __pmHasPMNSFileChanged(const char *);

/* standard log file set up */
PCP_CALL extern FILE *__pmOpenLog(const char *, const char *, FILE *, int *);
PCP_CALL extern FILE *__pmRotateLog(const char *, const char *, FILE *, int *);
/* make __pmNotifyErr also add entries to syslog */
PCP_CALL extern void __pmSyslog(int);
/* standard error, warning and info wrapper for syslog(3) */
PCP_CALL extern void __pmNotifyErr(int, const char *, ...) __PM_PRINTFLIKE(2,3);

PCP_CALL extern void __pmDumpResult(FILE *, const pmResult *);
PCP_CALL extern void __pmDumpHighResResult(FILE *, const pmHighResResult *);
PCP_CALL extern void __pmPrintTimespec(FILE *, const __pmTimespec *);
PCP_CALL extern void __pmPrintTimeval(FILE *, const __pmTimeval *);
PCP_CALL extern void __pmPrintDesc(FILE *, const pmDesc *);
PCP_CALL extern void __pmFreeResultValues(pmResult *);
PCP_CALL extern char *__pmPDUTypeStr_r(int, char *, int);
PCP_CALL extern const char *__pmPDUTypeStr(int);	/* NOT thread-safe */
PCP_CALL extern void __pmDumpNameSpace(FILE *, int);
PCP_CALL extern void __pmDumpStack(FILE *);
PCP_DATA extern int __pmLogReads;

PCP_CALL extern void __pmDumpIDList(FILE *, int, const pmID *);
PCP_CALL extern void __pmDumpNameList(FILE *, int, char **);
PCP_CALL extern void __pmDumpStatusList(FILE *, int, const int *);
PCP_CALL extern void __pmDumpNameAndStatusList(FILE *, int, char **, int *);

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

PCP_CALL extern void __pmHashInit(__pmHashCtl *);
PCP_CALL extern int __pmHashPreAlloc(int, __pmHashCtl *);
typedef __pmHashWalkState(*__pmHashWalkCallback)(const __pmHashNode *, void *);
PCP_CALL extern void __pmHashWalkCB(__pmHashWalkCallback, void *, const __pmHashCtl *);
PCP_CALL extern __pmHashNode *__pmHashWalk(__pmHashCtl *, __pmHashWalkState);
PCP_CALL extern __pmHashNode *__pmHashSearch(unsigned int, __pmHashCtl *);
PCP_CALL extern int __pmHashAdd(unsigned int, void *, __pmHashCtl *);
PCP_CALL extern int __pmHashDel(unsigned int, void *, __pmHashCtl *);
PCP_CALL extern void __pmHashClear(__pmHashCtl *);

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
 * PCP file. This abstracts i/o, allowing different handlers,
 * e.g. for stdio pass-thru and transparent decompression (xz, gz, etc).
 * This could conceivably be used for any kind of file within PCP, but
 * is currently used only for archive files.
 */
typedef struct {
    struct __pm_fops *fops;	/* i/o handler, assigned based on file type */
    __pm_off_t	position;	/* current uncompressed file position */
    void	*priv;		/* private data, e.g. for fd, blk cache, etc */
} __pmFILE;

typedef struct __pm_fops {
    void	*(*__pmopen) (__pmFILE *, const char *, const char *);
    void        *(*__pmfdopen)(__pmFILE *, int, const char *);
    int         (*__pmseek) (__pmFILE *, off_t, int);
    void        (*__pmrewind)(__pmFILE *);
    off_t       (*__pmtell)(__pmFILE *);
    int         (*__pmfgetc)(__pmFILE *);
    size_t	(*__pmread)(void *, size_t, size_t, __pmFILE *);
    size_t	(*__pmwrite)(void *, size_t, size_t, __pmFILE *);
    int         (*__pmflush)(__pmFILE *);
    int         (*__pmfsync)(__pmFILE *);
    int		(*__pmfileno) (__pmFILE *);
    off_t       (*__pmlseek)(__pmFILE *, off_t, int);
    int         (*__pmfstat)(__pmFILE *, struct stat *);
    int		(*__pmfeof) (__pmFILE *);
    int		(*__pmferror) (__pmFILE *);
    void	(*__pmclearerr) (__pmFILE *);
    int         (*__pmsetvbuf)(__pmFILE *, char *, int, size_t);
    int		(*__pmclose) (__pmFILE *);
} __pm_fops;

/*
 * Provide a stdio-like API for __pmFILE.
 */
PCP_CALL extern __pmFILE *__pmFopen(const char *, const char *);
PCP_CALL extern __pmFILE *__pmFdopen(int, const char *);
PCP_CALL extern int __pmFseek(__pmFILE *, long, int);
PCP_CALL extern void __pmRewind(__pmFILE *);
PCP_CALL extern long __pmFtell(__pmFILE *);
PCP_CALL extern int __pmFgetc(__pmFILE *);
PCP_CALL extern size_t __pmFread(void *, size_t, size_t, __pmFILE *);
PCP_CALL extern size_t __pmFwrite(void *, size_t, size_t, __pmFILE *);
PCP_CALL extern int __pmFflush(__pmFILE *);
PCP_CALL extern int __pmFsync(__pmFILE *);
PCP_CALL extern off_t __pmLseek(__pmFILE *, off_t, int);
PCP_CALL extern int __pmFstat(__pmFILE *, struct stat *);
PCP_CALL extern int __pmFileno(__pmFILE *);
PCP_CALL extern int __pmFeof(__pmFILE *);
PCP_CALL extern int __pmFerror(__pmFILE *);
PCP_CALL extern void __pmClearerr(__pmFILE *);
PCP_CALL extern int __pmSetvbuf(__pmFILE *, char *, int, size_t);
PCP_CALL extern int __pmFclose(__pmFILE *);

/*
 * Log/Archive Control
 */
typedef struct {
    int		l_refcnt;	/* number of contexts using this log */
    char	*l_name;	/* external log base name */
    __pmFILE	*l_tifp;	/* temporal index */
    __pmFILE	*l_mdfp;	/* meta data */
    __pmFILE	*l_mfp;		/* current metrics log */
    int		l_curvol;	/* current metrics log volume no. */
    int		l_state;	/* (when writing) log state */
    __pmHashCtl	l_hashpmid;	/* PMID hashed access */
    __pmHashCtl	l_hashindom;	/* instance domain hashed access */
    __pmHashCtl	l_hashrange;	/* ptr to first and last value in log for */
				/* each metric */
    __pmHashCtl	l_hashlabels;	/* maps the various metadata label types */
    __pmHashCtl l_hashtext;	/* maps the various help text types */
    int		l_minvol;	/* (when reading) lowest known volume no. */
    int		l_maxvol;	/* (when reading) highest known volume no. */
    int		l_numseen;	/* (when reading) size of l_seen */
    int		*l_seen;	/* (when reading) volumes opened OK */
    __pmLogLabel l_label;	/* (when reading) log label */
    __pm_off_t	l_physend;	/* (when reading) offset to physical EOF */
				/*                for last volume */
    __pmTimeval	l_endtime;	/* (when reading) timestamp at logical EOF */
    int		l_numti;	/* (when reading) no. temporal index entries */
    __pmLogTI	*l_ti;		/* (when reading) temporal index */
    struct __pmnsTree	*l_pmns;        /* namespace from meta data */
    int		l_multi;	/* part of a multi-archive context */
} __pmLogCtl;

/* l_state values */
#define PM_LOG_STATE_NEW	0
#define PM_LOG_STATE_INIT	1

/*
 * Return the argument if it's a valid filename else return NULL
 * (note: this function could be replaced with a call to access(),
 * but is retained for historical reasons).
 */
PCP_CALL extern const char *__pmFindPMDA(const char *);

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
PCP_CALL extern void __pmDumpProfile(FILE *, int, const __pmProfile *);

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
PCP_CALL extern void __pmDumpInResult(FILE *, const __pmInResult *);

/* instance profile methods */
PCP_CALL extern int __pmProfileSetSent(void);
PCP_CALL extern void __pmFreeProfile(__pmProfile *);
PCP_CALL extern __pmInDomProfile *__pmFindProfile(pmInDom, const __pmProfile *);
PCP_CALL extern int __pmInProfile(pmInDom, const __pmProfile *, int);
PCP_CALL extern void __pmFreeInResult(__pmInResult *);

/*
 * Internal interfaces for metadata labels (name:value pairs).
 */
static inline int
pmlabel_extrinsic(pmLabel *lp)
{
    return (lp->flags & PM_LABEL_OPTIONAL) != 0;
}

static inline int
pmlabel_intrinsic(pmLabel *lp)
{
    return (lp->flags & PM_LABEL_OPTIONAL) == 0;
}

PCP_CALL extern int __pmAddLabels(pmLabelSet **, const char *, int);
PCP_CALL extern int __pmMergeLabels(const char *, const char *, char *, int);
PCP_CALL extern int __pmParseLabels(const char *, int, pmLabel *, int, char *, int *);
PCP_CALL extern int __pmParseLabelSet(const char *, int, int, pmLabelSet **);

PCP_CALL extern int __pmGetContextLabels(pmLabelSet **);
PCP_CALL extern int __pmGetDomainLabels(int, const char *, pmLabelSet **);

PCP_CALL extern void __pmDumpLabelSet(FILE *, const pmLabelSet *);
PCP_CALL extern void __pmDumpLabelSets(FILE *, const pmLabelSet *, int);

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

PCP_CALL extern int __pmParseHostSpec(const char *, pmHostSpec **, int *, char **);
PCP_CALL extern int __pmUnparseHostSpec(pmHostSpec *, int, char *, size_t);
PCP_CALL extern void __pmFreeHostSpec(pmHostSpec *, int);

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
    PCP_ATTR_EXCLUSIVE	= 16,	/* DEPRECATED exclusive socket tied to this context */
} __pmAttrKey;

PCP_CALL extern __pmAttrKey __pmLookupAttrKey(const char *, size_t);
PCP_CALL extern int __pmAttrKeyStr_r(__pmAttrKey, char *, size_t);
PCP_CALL extern int __pmAttrStr_r(__pmAttrKey, const char *, char *, size_t);

PCP_CALL extern int __pmParseHostAttrsSpec(
    const char *, pmHostSpec **, int *, __pmHashCtl *, char **);
PCP_CALL extern int __pmUnparseHostAttrsSpec(
    pmHostSpec *, int, __pmHashCtl *, char *, size_t);
PCP_CALL extern void __pmFreeHostAttrsSpec(pmHostSpec *, int, __pmHashCtl *);
PCP_CALL extern void __pmFreeAttrsSpec(__pmHashCtl *);

/*
 * Control for connection to a PMCD
 */
typedef struct {
    int			pc_fd;		/* socket for comm with pmcd */
					/* ... -1 means no connection */
    pmHostSpec		*pc_hosts;	/* pmcd and proxy host specifications */
    int			pc_nhosts;	/* number of pmHostSpec entries */
    int			pc_timeout;	/* set if connect times out */
    int			pc_tout_sec;	/* timeout for __pmGetPDU */
    time_t		pc_again;	/* time to try again */
} __pmPMCDCtl;

PCP_CALL extern int __pmConnectPMCD(pmHostSpec *, int, int, __pmHashCtl *);
PCP_CALL extern int __pmConnectLocal(__pmHashCtl *);
PCP_CALL extern int __pmAuxConnectPMCD(const char *);
PCP_CALL extern int __pmAuxConnectPMCDPort(const char *, int);
PCP_CALL extern int __pmAuxConnectPMCDUnixSocket(const char *);

PCP_CALL extern int __pmAddHostPorts(pmHostSpec *, int *, int);
PCP_CALL extern void __pmDropHostPort(pmHostSpec *);
PCP_CALL extern void __pmConnectGetPorts(pmHostSpec *);

/*
 * SSL/TLS/IPv6 support via NSS/NSPR.
 */
PCP_CALL extern int __pmSecureServerSetup(const char *, const char *);
PCP_CALL extern int __pmSecureServerCertificateSetup(const char *, const char *, const char *);
PCP_CALL extern void __pmSecureServerShutdown(void);
PCP_CALL extern int __pmSecureServerHandshake(int, int, __pmHashCtl *);
PCP_CALL extern int __pmSecureClientHandshake(int, int, const char *, __pmHashCtl *);

typedef fd_set __pmFdSet;
typedef struct __pmSockAddr __pmSockAddr;
typedef struct __pmHostEnt __pmHostEnt;

PCP_CALL extern int __pmCreateSocket(void);
PCP_CALL extern int __pmCreateIPv6Socket(void);
PCP_CALL extern int __pmCreateUnixSocket(void);
PCP_CALL extern void __pmCloseSocket(int);

PCP_CALL extern int __pmSetSockOpt(int, int, int, const void *, __pmSockLen);
PCP_CALL extern int __pmGetSockOpt(int, int, int, void *, __pmSockLen *);
PCP_CALL extern int __pmConnect(int, void *, __pmSockLen);
PCP_CALL extern int __pmBind(int, void *, __pmSockLen);
PCP_CALL extern int __pmListen(int, int);
PCP_CALL extern int __pmAccept(int, void *, __pmSockLen *);
PCP_CALL extern ssize_t __pmWrite(int, const void *, size_t);
PCP_CALL extern ssize_t __pmRead(int, void *, size_t);
PCP_CALL extern ssize_t __pmSend(int, const void *, size_t, int);
PCP_CALL extern ssize_t __pmRecv(int, void *, size_t, int);
PCP_CALL extern int __pmConnectTo(int, const __pmSockAddr *, int);
PCP_CALL extern int __pmConnectCheckError(int);
PCP_CALL extern int __pmConnectRestoreFlags(int, int);
PCP_CALL extern int __pmSocketClosed(void);
PCP_CALL extern int __pmGetFileStatusFlags(int);
PCP_CALL extern int __pmSetFileStatusFlags(int, int);
PCP_CALL extern int __pmGetFileDescriptorFlags(int);
PCP_CALL extern int __pmSetFileDescriptorFlags(int, int);

PCP_CALL extern int  __pmFD(int);
PCP_CALL extern void __pmFD_CLR(int, __pmFdSet *);
PCP_CALL extern int  __pmFD_ISSET(int, __pmFdSet *);
PCP_CALL extern void __pmFD_SET(int, __pmFdSet *);
PCP_CALL extern void __pmFD_ZERO(__pmFdSet *);
PCP_CALL extern void __pmFD_COPY(__pmFdSet *, const __pmFdSet *);
PCP_CALL extern int __pmSelectRead(int, __pmFdSet *, struct timeval *);
PCP_CALL extern int __pmSelectWrite(int, __pmFdSet *, struct timeval *);

PCP_CALL extern __pmSockAddr *__pmSockAddrAlloc(void);
PCP_CALL extern void	     __pmSockAddrFree(__pmSockAddr *);
PCP_CALL extern size_t	     __pmSockAddrSize(void);
PCP_CALL extern void	     __pmSockAddrInit(__pmSockAddr *, int, int, int);
PCP_CALL extern int	     __pmSockAddrCompare(const __pmSockAddr *, const __pmSockAddr *);
PCP_CALL extern __pmSockAddr *__pmSockAddrDup(const __pmSockAddr *);
PCP_CALL extern __pmSockAddr *__pmSockAddrMask(__pmSockAddr *, const __pmSockAddr *);
PCP_CALL extern void	     __pmSockAddrSetFamily(__pmSockAddr *, int);
PCP_CALL extern int	     __pmSockAddrGetFamily(const __pmSockAddr *);
PCP_CALL extern void	     __pmSockAddrSetPort(__pmSockAddr *, int);
PCP_CALL extern int	     __pmSockAddrGetPort(const __pmSockAddr *);
PCP_CALL extern void	     __pmSockAddrSetScope(__pmSockAddr *, int);
PCP_CALL extern void	     __pmSockAddrSetPath(__pmSockAddr *, const char *);
PCP_CALL extern int	     __pmSockAddrIsLoopBack(const __pmSockAddr *);
PCP_CALL extern int	     __pmSockAddrIsInet(const __pmSockAddr *);
PCP_CALL extern int	     __pmSockAddrIsIPv6(const __pmSockAddr *);
PCP_CALL extern int	     __pmSockAddrIsUnix(const __pmSockAddr *);
PCP_CALL extern char *	     __pmSockAddrToString(const __pmSockAddr *);
PCP_CALL extern __pmSockAddr *__pmStringToSockAddr(const char *);
PCP_CALL extern __pmSockAddr *__pmLoopBackAddress(int);
PCP_CALL extern __pmSockAddr *__pmSockAddrFirstSubnetAddr(const __pmSockAddr *, int);
PCP_CALL extern __pmSockAddr *__pmSockAddrNextSubnetAddr(__pmSockAddr *, int);

PCP_CALL extern __pmHostEnt * __pmHostEntAlloc(void);
PCP_CALL extern void	     __pmHostEntFree(__pmHostEnt *);
PCP_CALL extern __pmSockAddr *__pmHostEntGetSockAddr(const __pmHostEnt *, void **);
PCP_CALL extern char *	     __pmHostEntGetName(__pmHostEnt *);

PCP_CALL extern __pmHostEnt * __pmGetAddrInfo(const char *);
PCP_CALL extern char *	     __pmGetNameInfo(__pmSockAddr *);

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
    PM_SERVER_FEATURE_LOCAL,
    PM_SERVER_FEATURE_CERT_REQD,
    PM_SERVER_FEATURES
} __pmServerFeature;

PCP_CALL extern int __pmServerHasFeature(__pmServerFeature);
PCP_CALL extern int __pmServerSetFeature(__pmServerFeature);
PCP_CALL extern int __pmServerClearFeature(__pmServerFeature);
PCP_CALL extern int __pmServerCreatePIDFile(const char *, int);
PCP_CALL extern void __pmServerStart(int, char **, int);
PCP_CALL extern int __pmServerAddPorts(const char *);
PCP_CALL extern int __pmServerAddInterface(const char *);
PCP_CALL extern void __pmServerSetLocalSocket(const char *);
PCP_CALL extern int __pmServerSetLocalCreds(int,  __pmHashCtl *);
PCP_CALL extern void __pmServerSetServiceSpec(const char *);
typedef void (*__pmServerCallback)(__pmFdSet *, int, int);
PCP_CALL extern void __pmServerAddNewClients(__pmFdSet *, __pmServerCallback);
PCP_CALL extern int __pmServerAddToClientFdSet(__pmFdSet *, int);
PCP_CALL extern int __pmServerOpenRequestPorts(__pmFdSet *, int);
PCP_CALL extern void __pmServerCloseRequestPorts(void);
PCP_CALL extern void __pmServerDumpRequestPorts(FILE *);
PCP_CALL extern char *__pmServerRequestPortString(int, char *, size_t);

/* Service broadcasting, for servers. */
typedef struct __pmServerPresence __pmServerPresence;
PCP_CALL extern __pmServerPresence *__pmServerAdvertisePresence(const char *, int);
PCP_CALL extern void __pmServerUnadvertisePresence(__pmServerPresence *);

/*
 * Minimal information to retain for each archive in a multi-archive context
 */
typedef struct {
    char		*ml_name;	/* external log base name */
    __pmTimeval		ml_starttime;	/* start time of the archive */
    char		*ml_hostname;	/* name of collection host */
    char		*ml_tz;		/* $TZ at collection host */
} __pmMultiLogCtl;

/*
 * Per-context controls for archives and logs
 */
typedef struct {
    __pmLogCtl		*ac_log;	/* Current global logging and archive
					   control */
    long		ac_offset;	/* fseek ptr for archives */
    int			ac_vol;		/* volume for ac_offset */
    int			ac_serial;	/* serial access pattern for archives */
    __pmHashCtl		ac_pmid_hc;	/* per PMID controls for INTERP */
    double		ac_end;		/* time at end of archive */
    void		*ac_want;	/* used in interp.c */
    void		*ac_unbound;	/* used in interp.c */
    void		*ac_cache;	/* used in interp.c */
    int			ac_cache_idx;	/* used in interp.c */
    /*
     * These were added to the ABI in order to support multiple archives
     * in a single context. In order to maintain ABI compatibility they must
     * be at the end of this structure.
     */
    int			ac_mark_done;	/* mark record between archives */
					/*   has been generated */
    int			ac_num_logs;	/* The number of archives */
    int			ac_cur_log;	/* The currently open archive */
    __pmMultiLogCtl	**ac_log_list;	/* Current set of archives */
} __pmArchCtl;

/*
 * PMAPI context. We keep an array of these,
 * one for each context created by the application.
 */
typedef struct {
    __pmMutex		c_lock;		/* mutex for multi-thread access */
    int			c_type;		/* HOST, ARCHIVE, LOCAL or INIT or FREE */
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
    int			c_handle;	/* context number above PMAPI */
    int			c_slot;		/* index to contexts[] below PMAPI */
} __pmContext;

#define __PM_MODE_MASK	0xffff

#define PM_CONTEXT_INIT	-2		/* special type: being initialized, do not use */

/*
 * Convert opaque context handle to __pmContext pointer
 */
PCP_CALL extern __pmContext *__pmHandleToPtr(int);

/*
 * Like __pmHandleToPtr(pmWhichContext()), but with no locking
 */
PCP_CALL __pmContext *__pmCurrentContext(void);

/*
 * Dump the current context (source details + instance profile),
 * for a particular instance domain.
 * If indom == PM_INDOM_NULL, then print all instance domains
 */
PCP_CALL extern void __pmDumpContext(FILE *, int, pmInDom);

/*
 * pmFetch helper routines, hooks for derivations and local contexts.
 */
PCP_CALL extern int __pmPrepareFetch(__pmContext *, int, const pmID *, pmID **);
PCP_CALL extern int __pmFinishResult(__pmContext *, int, pmResult **);
PCP_CALL extern int __pmFetchLocal(__pmContext *, int, pmID *, pmResult **);

/* Archive context helper. */
int __pmFindOrOpenArchive(__pmContext *, const char *, int);

PCP_CALL extern void __pmIgnoreSignalPIPE(void);

PCP_CALL extern double __pmConnectTimeout(void);
PCP_CALL extern int __pmSetConnectTimeout(double);
PCP_CALL extern double __pmRequestTimeout(void);
PCP_CALL extern int __pmSetRequestTimeout(double);

/*
 * no mem today, my love has gone away ....
 */
PCP_CALL extern void __pmNoMem(const char *, size_t, int);
#define PM_FATAL_ERR 1
#define PM_RECOV_ERR 0

/*
 * Startup handling:
 * set default user for __pmSetProcessIdentity() ... default is "pcp"
 */
PCP_CALL extern int __pmGetUsername(char **);

/*
 * Cleanup handling:
 * shutdown various components in libpcp, releasing all resources
 * (local context PMDAs, any global NSS socket state, etc).
 */
PCP_CALL extern int __pmShutdown(void);

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
typedef struct __pmLogInDom {
    struct __pmLogInDom	*next;
    __pmTimeval		stamp;
    int			numinst;
    int			*instlist;
    char		**namelist;
    int			*buf; 
    int			allinbuf; 
} __pmLogInDom;

/*
 * __pmLogLabelSet is used to hold the sets of labels for the label
 * hierarchy in memory.  Only in the case of instances will it have
 * multiple labelsets.  For all other (higher) hierarchy levels, a
 * single labelset suffices (nsets == 1, and nlabels >= 0).  Also,
 * in memory labelsets are linked together in reverse chronological
 * order (just like the __pmLogInDom structure above).
 * -- externally we write these as
 *	timestamp
 *	type (int - PM_LABEL_* types)
 *	ident (int - PM_IN_NULL, domain, indom, pmid)
 *	nsets (int - usually 1, except for instances)
 *	jsonb offset (int - offset to jsonb start)
 *	labelset[0] ... labelset[numsets-1]
 *	jsonb table (strings, concatenated)
 *
 * -- with each labelset array entry as
 *	inst (int)
 *	nlabels (int)
 *	jsonb offset (int)
 *	jsonb length (int)
 *	label[0] ... label[nlabels-1] (struct pmLabel)
 */

typedef struct __pmLogLabelSet {
    struct __pmLogLabelSet *next;
    __pmTimeval		stamp;
    int			type;
    int			ident;
    int			nsets;
    pmLabelSet		*labelsets;
} __pmLogLabelSet;

/*
 * __pmLogText is used to hold the metric and instance domain help text
 * internally, for help text associated with an archive context.
 */
typedef struct __pmLogText {
    int			type;	/* oneline/full and pmid/indom */
    int			ident;	/* metric or indom identifier */
    char		*text;
} __pmLogText;

/*
 * record header in the metadata log file ... len (by itself) also is
 * used as a trailer
 */
typedef struct __pmLogHdr {
    int		len;	/* record length, includes header and trailer */
    int		type;	/* see TYPE_* #defines below */
} __pmLogHdr;

#define TYPE_DESC	1	/* header, pmDesc, trailer */
#define TYPE_INDOM	2	/* header, __pmLogInDom, trailer */
#define TYPE_LABEL	3	/* header, __pmLogLabelSet, trailer */
#define TYPE_TEXT	4	/* header, __pmLogText, trailer */

PCP_CALL extern void __pmLogPutIndex(const __pmLogCtl *, const __pmTimeval *);

PCP_CALL extern const char *__pmLogName_r(const char *, int, char *, int);
PCP_CALL extern const char *__pmLogName(const char *, int);	/* NOT thread-safe */
PCP_CALL extern __pmFILE *__pmLogNewFile(const char *, int);
PCP_CALL extern int __pmLogCreate(const char *, const char *, int, __pmLogCtl *);
#define PMLOGREAD_NEXT		0
#define PMLOGREAD_TO_EOF	1
PCP_CALL extern int __pmLogRead(__pmLogCtl *, int, __pmFILE *, pmResult **, int);
PCP_CALL extern int __pmLogRead_ctx(__pmContext *, int, __pmFILE *, pmResult **, int);
PCP_CALL extern int __pmLogWriteLabel(__pmFILE *, const __pmLogLabel *);
PCP_CALL extern int __pmLogOpen(const char *, __pmContext *);
PCP_CALL extern int __pmLogLoadLabel(__pmLogCtl *, const char *);
PCP_CALL extern int __pmLogLoadIndex(__pmLogCtl *);
PCP_CALL extern int __pmLogLoadMeta(__pmLogCtl *);
PCP_CALL extern void __pmLogClose(__pmLogCtl *);
PCP_CALL extern void __pmLogCacheClear(__pmFILE *);
PCP_CALL extern char *__pmLogBaseName(char *);

PCP_CALL extern __pmTimeval *__pmLogStartTime(__pmArchCtl *);
PCP_CALL extern int __pmLogChangeArchive(__pmContext *, int);
PCP_CALL extern int __pmLogCheckForNextArchive(__pmLogCtl *, int, pmResult **);
PCP_CALL extern int __pmLogGenerateMark(__pmLogCtl *, int, pmResult **);

PCP_CALL extern int __pmLogChangeToNextArchive(__pmLogCtl **);
PCP_CALL extern int __pmLogChangeToPreviousArchive(__pmLogCtl **);
PCP_CALL extern void __pmArchCtlFree (__pmArchCtl *);

PCP_CALL extern int __pmLogPutDesc(__pmLogCtl *, const pmDesc *, int, char **);
PCP_CALL extern int __pmLogLookupDesc(__pmLogCtl *, pmID, pmDesc *);
#define PMLOGPUTINDOM_DUP       1
PCP_CALL extern int __pmLogPutInDom(__pmLogCtl *, pmInDom, const __pmTimeval *, int, int *, char **);
PCP_CALL extern int __pmLogGetInDom(__pmLogCtl *, pmInDom, __pmTimeval *, int **, char ***);
PCP_CALL extern int __pmLogLookupInDom(__pmLogCtl *, pmInDom, __pmTimeval *, const char *);
PCP_CALL extern int __pmLogNameInDom(__pmLogCtl *, pmInDom, __pmTimeval *, int, char **);

PCP_CALL extern int __pmLogLookupLabel(__pmLogCtl *, unsigned int, unsigned int, pmLabelSet **, const __pmTimeval *);
PCP_CALL extern int __pmLogPutLabel(__pmLogCtl *, unsigned int, unsigned int, int, pmLabelSet *, const __pmTimeval *);
PCP_CALL extern int __pmLogLookupText(__pmLogCtl *, unsigned int , unsigned int, char **);
PCP_CALL extern int __pmLogPutText(__pmLogCtl *, unsigned int , unsigned int, char *, int);

PCP_CALL extern int __pmLogFetch(__pmContext *, int, pmID *, pmResult **);
PCP_CALL extern int __pmLogFetchInterp(__pmContext *, int, pmID *, pmResult **);
PCP_CALL extern void __pmLogSetTime(__pmContext *);
PCP_CALL extern void __pmLogResetInterp(__pmContext *);
PCP_CALL extern void __pmFreeInterpData(__pmContext *);

PCP_CALL extern int __pmLogChangeVol(__pmLogCtl *, int);
PCP_CALL extern int __pmLogChkLabel(__pmLogCtl *, __pmFILE *, __pmLogLabel *, int);
PCP_CALL extern int __pmGetArchiveLabel(__pmLogCtl *, pmLogLabel *);
PCP_CALL extern int __pmGetArchiveEnd(__pmLogCtl *, struct timeval *);

PCP_CALL extern const char *__pmLogLocalSocketDefault(int, char *buf, size_t bufSize);
PCP_CALL extern const char *__pmLogLocalSocketUser(int, char *buf, size_t bufSize);

#ifdef __cplusplus
}
#endif

#endif /* PCP_IMPL_H */
