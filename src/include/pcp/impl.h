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

PCP_CALL extern void __pmDumpIDList(FILE *, int, const pmID *);
PCP_CALL extern void __pmDumpNameList(FILE *, int, char **);
PCP_CALL extern void __pmDumpStatusList(FILE *, int, const int *);
PCP_CALL extern void __pmDumpNameAndStatusList(FILE *, int, char **, int *);

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
 * unfortunately, in this version, PCP archives are limited to no
 * more than 2 Gbytes ...
 */
typedef __uint32_t	__pm_off_t;

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
    void	*(*__pmopen)(__pmFILE *, const char *, const char *);
    void        *(*__pmfdopen)(__pmFILE *, int, const char *);
    int         (*__pmseek)(__pmFILE *, off_t, int);
    void        (*__pmrewind)(__pmFILE *);
    off_t       (*__pmtell)(__pmFILE *);
    int         (*__pmfgetc)(__pmFILE *);
    size_t	(*__pmread)(void *, size_t, size_t, __pmFILE *);
    size_t	(*__pmwrite)(void *, size_t, size_t, __pmFILE *);
    int         (*__pmflush)(__pmFILE *);
    int         (*__pmfsync)(__pmFILE *);
    int		(*__pmfileno)(__pmFILE *);
    off_t       (*__pmlseek)(__pmFILE *, off_t, int);
    int         (*__pmfstat)(__pmFILE *, struct stat *);
    int		(*__pmfeof)(__pmFILE *);
    int		(*__pmferror)(__pmFILE *);
    void	(*__pmclearerr)(__pmFILE *);
    int         (*__pmsetvbuf)(__pmFILE *, char *, int, size_t);
    int		(*__pmclose)(__pmFILE *);
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

#define __PM_MODE_MASK	0xffff

/*
 * Dump the current context (source details + instance profile),
 * for a particular instance domain.
 * If indom == PM_INDOM_NULL, then print all instance domains
 */
PCP_CALL extern void __pmDumpContext(FILE *, int, pmInDom);

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

#ifdef __cplusplus
}
#endif

#endif /* PCP_IMPL_H */
