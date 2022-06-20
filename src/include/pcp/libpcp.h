/*
 * CAVEAT
 *	The interfaces and data structures defined in this header are
 *	intended for internal Performance Co-Pilot (PCP) developer use.
 *
 *	They are not part of the PCP APIs that are guaranteed to
 *	remain fixed across releases, and they may not work, or may
 *	provide different semantics at some point in the future.
 *
 * Copyright (c) 2012-2022 Red Hat.
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
#ifndef PCP_LIBPCP_H
#define PCP_LIBPCP_H

#ifdef __cplusplus
extern "C" {
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
 * HTTP[S] connections come here by default, over-ride with $WEBAPI_PORT
 * in environment
 */
#define WEBAPI_PORT 44323
#define WEBAPI_PROTOCOL "http"

/*
 * internal libpcp state ... PM_STATE_APPL means we are at or above the
 * PMAPI in a state where PMAPI calls can safely be made ... PM_STATE_PMCS
 * means we are in the PMCD, or a PMDA, or low-level PDU code, and
 * PMAPI calls are a bad idea.
 */
#define PM_STATE_APPL	0
#define PM_STATE_PMCS	1
PCP_CALL extern void __pmSetInternalState(int);

/*
 * Thread-safe support ... #define to enable thread-safe protection of
 * global data structures and mutual exclusion when required.
 *
 * We require pthread.h and working mutex, the rest can be faked
 * by the libpcp itself.
 *
 */
#if defined(HAVE_PTHREAD_H) && defined(HAVE_PTHREAD_MUTEX_T)
#define PM_MULTI_THREAD 1
#include <pthread.h>
typedef pthread_mutex_t __pmMutex;
#else
typedef void * __pmMutex;
#endif

/*
 * Multi-thread support
 * Use PM_MULTI_THREAD_DEBUG for lock debugging with -Dlock[,appl?...]
 */
PCP_CALL extern void __pmInitLocks(void);
PCP_CALL extern int __pmLock(void *, const char *, int);
PCP_CALL extern int __pmUnlock(void *, const char *, int);

/*
 * Each of these scopes defines one or more PMAPI routines that will
 * not allow calls from more than one thread.
 */
#define PM_SCOPE_DSO_PMDA	0
#define PM_SCOPE_ACL		1
#define PM_SCOPE_AF		2
#define PM_SCOPE_LOGPORT	3
#define PM_SCOPE_MAX		3
PCP_CALL extern int __pmMultiThreaded(int);

#define PM_INIT_LOCKS()		__pmInitLocks()
#define PM_MULTIPLE_THREADS(x)	__pmMultiThreaded(x)
#define PM_LOCK(lock)		__pmLock(&(lock), __FILE__, __LINE__)
#define PM_UNLOCK(lock)		__pmUnlock(&(lock), __FILE__, __LINE__)

#ifdef HAVE_PTHREAD_MUTEX_T
/* the big libpcp lock */
PCP_DATA extern pthread_mutex_t	__pmLock_libpcp;
#else
PCP_DATA extern void *__pmLock_libpcp;			/* symbol exposure */
#endif

/*
 * Internally, this is how to decode a PMID!
 * - flag is to denote state internally in some operations
 * - domain is usually the unique domain number of a PMDA, but see
 *   below for some special cases
 * - cluster and item together uniquely identify a metric within a domain
 */
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

/*
 * Internally, this is how to decode an Instance Domain Identifier
 * - flag is to denote state internally in some operations
 * - domain is usually the unique domain number of a PMDA, but DYNAMIC_PMID
 *   (number 511) is reserved (see above for PMID encoding rules)
 * - serial uniquely identifies an InDom within a domain
 */
typedef struct {
#ifdef HAVE_BITFIELDS_LTOR
	unsigned int	flag : 1;
	unsigned int	domain : 9;
	unsigned int	serial : 22;
#else
	unsigned int	serial : 22;
	unsigned int	domain : 9;
	unsigned int	flag : 1;
#endif
} __pmInDom_int;

/*
 * Internal structure of a PMNS node
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
 * Internal structure of a PMNS tree
 */
typedef struct __pmnsTree {
    __pmnsNode		*root;  /* root of tree structure */
    __pmnsNode		**htab; /* hash table of nodes keyed on pmid */
    int			htabsize;     /* number of nodes in the table */
    int			mark_state;   /* the total mark value for trimming */
} __pmnsTree;

/* used by pmnsmerge/pmnsdel */
PCP_CALL extern __pmnsTree *__pmExportPMNS(void); 

/* for PMNS in archives and PMDA use */
PCP_CALL extern int __pmNewPMNS(__pmnsTree **);
PCP_CALL extern void __pmUsePMNS(__pmnsTree *); /* for debugging */
PCP_CALL extern int __pmFixPMNSHashTab(__pmnsTree *, int, int);
PCP_CALL extern int __pmAddPMNSNode(__pmnsTree *, int, const char *);

/* return true if the named pmns file has changed */
PCP_CALL extern int __pmHasPMNSFileChanged(const char *);

/* PDU types */
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
#define PDU_LABEL_REQ		0x7012
#define PDU_LABEL		0x7013
#define PDU_HIGHRES_FETCH	0x7014
#define PDU_HIGHRES_RESULT	0x7015
#define PDU_DESC_IDS		0x7016
#define PDU_DESCS		0x7017
#define PDU_FINISH		0x7017
#define PDU_MAX		 	(PDU_FINISH - PDU_START)

typedef __uint32_t	__pmPDU;
/*
 * round a size up to the next multiple of a __pmPDU size
 *
 * PM_PDU_SIZE is in units of __pmPDU size
 * PM_PDU_SIZE_BYTES is in units of bytes
 */
#define PM_PDU_SIZE(x) (((x)+sizeof(__pmPDU)-1)/sizeof(__pmPDU))
#define PM_PDU_SIZE_BYTES(x) (sizeof(__pmPDU)*PM_PDU_SIZE(x))

/* timeout options for PDU handling */
#define TIMEOUT_NEVER	 0
#define TIMEOUT_DEFAULT	-1
/* deprecated #define TIMEOUT_ASYNC -2 */
#define TIMEOUT_CONNECT	-3

/* Version and capabilities information for PDU exchanges */
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
 * Protocol data unit support
 */
typedef struct {
    __int32_t	len;		/* length of pdu_header + PDU */
    __int32_t	type;		/* PDU type */
    __int32_t	from;		/* pid of PDU originator */
} __pmPDUHdr;

/* credentials stuff */
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
#define PDU_FLAG_CERT_REQD	(1U<<7)
#define PDU_FLAG_BAD_LABEL	(1U<<8)	/* bad, encoding issues */
#define PDU_FLAG_LABELS		(1U<<9)
#define PDU_FLAG_HIGHRES	(1U<<10)
#define PDU_FLAG_DESCS		(1U<<11)
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

/*
 * Universal timestamp ... this is like a struct timespec, but we
 * control the size of the fields and choose different field names
 * to give the compiler the maximum chance of spotting misuses.
 * All time of day fields internally within libpcp use this format
 * which is Y2038 safe.
 */
typedef struct {
    __int64_t	sec;
    __int32_t	nsec;
} __pmTimestamp;

/* Internal version of a pmResult */
typedef struct __pmResult {
    __pmTimestamp	timestamp;	/* time stamped by collector */
    int                 numpmid;	/* number of PMIDs */
    pmValueSet		*vset[1];	/* set of value sets, one per PMID */
} __pmResult;

#if defined(HAVE_64BIT_PTR)
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
#elif defined(HAVE_32BIT_PTR)
/* In the 32-bit world, structures may be used in PDUs as defined */
typedef pmValue		__pmValue_PDU;
typedef pmValueSet	__pmValueSet_PDU;
#else
bozo - unknown size of pointer !!!
#endif

/* mode options for __pmGetPDU */
#define ANY_SIZE	0	/* replacement for old PDU_BINARY */
#define LIMIT_SIZE	2	/* replacement for old PDU_CLIENT */

/*
 * PDU encoding formats
 * These have been retired ...
 *  #define PDU_BINARY	0
 *  #define PDU_ASCII	1
 * And this has been replaced by LIMIT_SIZE for __pmGetPDU
 *  #define PDU_CLIENT	2
 */

/* Unit of space allocation for PDU buffer */
#define PDU_CHUNK		1024

/*
 * Anonymous PDU sender, when context does not matter, e.g. PDUs from
 * a PMDA sent to PMCD
 */
#define FROM_ANON	0

/* PDU type independent send-receive routines */
PCP_CALL extern int __pmXmitPDU(int, __pmPDU *);
PCP_CALL extern int __pmGetPDU(int, int, int, __pmPDU **);
PCP_CALL extern int __pmSetPDUCeiling(int);

/* PDU type specfic send-encode-decode routines */
PCP_CALL extern int __pmSendError(int, int, int);
PCP_CALL extern int __pmDecodeError(__pmPDU *, int *);
PCP_CALL extern int __pmSendXtendError(int, int, int, int);
PCP_CALL extern int __pmDecodeXtendError(__pmPDU *, int *, int *);
PCP_CALL extern int __pmSendResult(int, int, const __pmResult *);
/* see below for __pmEncodeResult() */
PCP_CALL extern int __pmSendHighResResult(int, int, const __pmResult *);
PCP_CALL extern int __pmEncodeHighResResult(const __pmResult *, __pmPDU **);
PCP_CALL extern int __pmDecodeResult(__pmPDU *, __pmResult **);
PCP_CALL extern int __pmDecodeHighResResult(__pmPDU *, __pmResult **);
PCP_CALL extern int __pmDecodeValueSet(__pmPDU *, int, __pmPDU *, char *, int, int, int, pmValueSet **);
PCP_CALL extern int __pmSendProfile(int, int, int, pmProfile *);
PCP_CALL extern int __pmDecodeProfile(__pmPDU *, int *, pmProfile **);
PCP_CALL extern int __pmSendFetchPDU(int, int, int, int, pmID *, int);
PCP_CALL extern int __pmSendFetch(int, int, int, int, pmID *);
PCP_CALL extern int __pmDecodeFetch(__pmPDU *, int *, void *, int *, pmID **);
PCP_CALL extern int __pmSendHighResFetch(int, int, int, int, pmID *);
PCP_CALL extern int __pmDecodeHighResFetch(__pmPDU *, int *, int *, pmID **);
PCP_CALL extern int __pmSendDescReq(int, int, pmID);
PCP_CALL extern int __pmDecodeDescReq(__pmPDU *, pmID *);
PCP_CALL extern int __pmSendDesc(int, int, pmDesc *);
PCP_CALL extern int __pmDecodeDesc(__pmPDU *, pmDesc *);
PCP_CALL extern int __pmSendDescs(int, int, int, pmDesc *);
PCP_CALL extern int __pmDecodeDescs(__pmPDU *, int, pmDesc *);
PCP_CALL extern int __pmDecodeDescs2(__pmPDU *, int *, pmDesc **);
PCP_CALL extern int __pmSendInstanceReq(int, int, pmInDom, int, const char *);
PCP_CALL extern int __pmDecodeInstanceReq(__pmPDU *, pmInDom *, int *, char **);
PCP_CALL extern int __pmSendInstance(int, int, pmInResult *);
PCP_CALL extern int __pmDecodeInstance(__pmPDU *, pmInResult **);
PCP_CALL extern int __pmSendTextReq(int, int, int, int);
PCP_CALL extern int __pmDecodeTextReq(__pmPDU *, int *, int *);
PCP_CALL extern int __pmSendText(int, int, int, const char *);
PCP_CALL extern int __pmDecodeText(__pmPDU *, int *, char **);
PCP_CALL extern int __pmSendCreds(int, int, int, const __pmCred *);
PCP_CALL extern int __pmDecodeCreds(__pmPDU *, int *, int *, __pmCred **);
PCP_CALL extern int __pmSendIDList(int, int, int, const pmID *, int);
PCP_CALL extern int __pmDecodeIDList(__pmPDU *, int, pmID *, int *);
PCP_CALL extern int __pmDecodeIDList2(__pmPDU *, int *, pmID **);
PCP_CALL extern int __pmSendNameList(int, int, int, const char **, const int *);
PCP_CALL extern int __pmDecodeNameList(__pmPDU *, int *, char ***, int **);
PCP_CALL extern int __pmSendChildReq(int, int, const char *, int);
PCP_CALL extern int __pmDecodeChildReq(__pmPDU *, char **, int *);
PCP_CALL extern int __pmSendTraversePMNSReq(int, int, const char *);
PCP_CALL extern int __pmDecodeTraversePMNSReq(__pmPDU *, char **);
PCP_CALL extern int __pmSendAuth(int, int, int, const char *, int);
PCP_CALL extern int __pmDecodeAuth(__pmPDU *, int *, char **, int *);
PCP_CALL extern int __pmSendAttr(int, int, int, const char *, int);
PCP_CALL extern int __pmDecodeAttr(__pmPDU *, int *, char **, int *);
PCP_CALL extern int __pmSendLabelReq(int, int, int, int);
PCP_CALL extern int __pmDecodeLabelReq(__pmPDU *, int *, int *);
PCP_CALL extern int __pmSendLabel(int, int, int, int, pmLabelSet *, int);
PCP_CALL extern int __pmDecodeLabel(__pmPDU *, int *, int *, pmLabelSet **, int *);
PCP_CALL unsigned int __pmServerGetFeaturesFromPDU(__pmPDU *);
PCP_CALL extern int __pmFeaturesIPC(int);

/* PDU buffer services */
PCP_CALL extern __pmPDU *__pmFindPDUBuf(int);
PCP_CALL extern void __pmPinPDUBuf(void *);
PCP_CALL extern int __pmUnpinPDUBuf(void *);
PCP_CALL extern void __pmCountPDUBuf(int, int *, int *);

/* PDU counting services */
PCP_DATA extern unsigned int *__pmPDUCntIn;
PCP_DATA extern unsigned int *__pmPDUCntOut;
PCP_CALL extern void __pmSetPDUCntBuf(unsigned *, unsigned *);
PCP_CALL extern void __pmDumpPDUCnt(FILE *);
PCP_CALL extern void __pmDumpPDUTrace(FILE *);

/* internal IPC protocol stuff */
typedef int (*__pmConnectHostType)(int, int);
PCP_CALL extern int __pmSetSocketIPC(int);
PCP_CALL extern int __pmSetVersionIPC(int, int);
PCP_CALL extern int __pmVersionIPC(int);
PCP_CALL extern int __pmSocketIPC(int);
PCP_CALL extern void __pmOverrideLastFd(int);
PCP_CALL extern void __pmResetIPC(int);

/* platform independent socket services */
typedef fd_set __pmFdSet;
typedef struct __pmSockAddr __pmSockAddr;
typedef struct __pmHostEnt __pmHostEnt;
PCP_CALL extern int __pmCreateSocket(void);
PCP_CALL extern int __pmCreateIPv6Socket(void);
PCP_CALL extern int __pmCreateUnixSocket(void);
PCP_CALL extern int __pmBind(int, void *, __pmSockLen);
PCP_CALL extern int __pmListen(int, int);
PCP_CALL extern int __pmConnect(int, void *, __pmSockLen);
PCP_CALL extern int __pmAccept(int, void *, __pmSockLen *);
PCP_CALL extern void __pmCloseSocket(int);
PCP_CALL extern int __pmSetSockOpt(int, int, int, const void *, __pmSockLen);
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
PCP_CALL extern void	     __pmSockAddrSetFamily(__pmSockAddr *, int);
PCP_CALL extern int	     __pmSockAddrGetFamily(const __pmSockAddr *);
PCP_CALL extern void	     __pmSockAddrSetPort(__pmSockAddr *, int);
PCP_CALL extern int	     __pmSockAddrGetPort(const __pmSockAddr *);
PCP_CALL extern void	     __pmSockAddrSetPath(__pmSockAddr *, const char *);
PCP_CALL extern int	     __pmSockAddrIsLoopBack(const __pmSockAddr *);
PCP_CALL extern int	     __pmSockAddrIsInet(const __pmSockAddr *);
PCP_CALL extern int	     __pmSockAddrIsIPv6(const __pmSockAddr *);
PCP_CALL extern int	     __pmSockAddrIsUnix(const __pmSockAddr *);
PCP_CALL extern char *	     __pmSockAddrToString(const __pmSockAddr *);
PCP_CALL extern char *	     __pmGetNameInfo(__pmSockAddr *);
PCP_CALL extern __pmSockAddr *__pmStringToSockAddr(const char *);
PCP_CALL extern __pmSockAddr *__pmLoopBackAddress(int);
PCP_CALL extern __pmSockAddr *__pmHostEntGetSockAddr(const __pmHostEnt *, void **);
PCP_CALL extern __pmHostEnt * __pmGetAddrInfo(const char *);
PCP_CALL extern void	     __pmHostEntFree(__pmHostEnt *);
PCP_CALL extern char *	     __pmHostEntGetName(__pmHostEnt *);

/* Hashed Data Structures for the Processing of Logs and Archives */
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
PCP_CALL extern void __pmHashFree(__pmHashCtl *);


/*
 * Host specification allowing one or more pmproxy host, and port numbers
 * within the one string, i.e. pmcd host specifications of the form:
 *		host:port,port@proxy:port,port 
 */
typedef struct {
    char	*name;			/* hostname (always valid) */
    int		*ports;			/* array of host port numbers */
    int		nports;			/* number of ports in host port array */
} __pmHostSpec;
PCP_CALL extern int __pmParseHostSpec(const char *, __pmHostSpec **, int *, char **);
PCP_CALL extern int __pmUnparseHostSpec(__pmHostSpec *, int, char *, size_t);

/*
 * Version 2 PCP archives are limited to no more than 2 Gbytes.
 * Version 3 PCP archives are limited to no more than 2 Ebytes.
 */
typedef __uint32_t	__pmoff32_t;
typedef __uint64_t	__pmoff64_t;

/*
 * PCP file. This abstracts i/o, allowing different handlers, e.g.
 * for stdio pass-thru and transparent decompression (xz, gz, etc).
 */
typedef struct {
    struct __pm_fops *fops;	/* i/o handler, assigned based on file type */
    off_t	position;	/* current uncompressed file position */
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
    int         (*__pmstat)(const char *, struct stat *);
    int         (*__pmfstat)(__pmFILE *, struct stat *);
    int		(*__pmfeof)(__pmFILE *);
    int		(*__pmferror)(__pmFILE *);
    void	(*__pmclearerr)(__pmFILE *);
    int         (*__pmsetvbuf)(__pmFILE *, char *, int, size_t);
    int		(*__pmclose)(__pmFILE *);
} __pm_fops;

/* Provide a stdio-like API for __pmFILE */
PCP_CALL extern int __pmAccess(const char *, int);
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
PCP_CALL extern int __pmStat(const char *, struct stat *);
PCP_CALL extern int __pmFstat(__pmFILE *, struct stat *);
PCP_CALL extern int __pmFileno(__pmFILE *);
PCP_CALL extern int __pmFeof(__pmFILE *);
PCP_CALL extern int __pmFerror(__pmFILE *);
PCP_CALL extern void __pmClearerr(__pmFILE *);
PCP_CALL extern int __pmSetvbuf(__pmFILE *, char *, int, size_t);
PCP_CALL extern int __pmFclose(__pmFILE *);

PCP_CALL extern int __pmCompressedFileIndex(char *, size_t);

/*
 * st_size within struct stat is set by __pmStat() to this value to indicate
 * that the size could not be obtained. This happens when the file is compressed.
 * In order to get the uncompressed size, the file must be opened using
 * __pmFopen() and then __pmFstat() must be used.
 */
#define PM_ST_SIZE_INVALID (-1) /* st_size is of type off_t, which is signed */

/* Control for connection to a PMCD */
typedef struct {
    int			pc_fd;		/* socket for comm with pmcd */
					/* ... -1 means no connection */
    __pmHostSpec	*pc_hosts;	/* pmcd and proxy host specifications */
    int			pc_nhosts;	/* number of hostspec entries */
    int			pc_timeout;	/* set if connect times out */
    int			pc_tout_sec;	/* timeout for __pmGetPDU */
    time_t		pc_again;	/* time to try again */
} __pmPMCDCtl;
PCP_CALL extern int __pmAuxConnectPMCDPort(const char *, int);

/* Internal interfaces for metadata labels (name:value pairs) */
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
PCP_CALL extern pmLabelSet *__pmDupLabelSets(pmLabelSet *, int);
PCP_CALL extern int __pmParseLabelSet(const char *, int, int, pmLabelSet **);
PCP_CALL extern int __pmGetContextLabels(pmLabelSet **);
PCP_CALL extern int __pmGetDomainLabels(int, const char *, pmLabelSet **);

/* internal archive data structures */
/*
 * record header in the metadata log file ... len (by itself) also is
 * used as a trailer
 */
typedef struct __pmLogHdr {
    __int32_t		len;	/* record length, includes header and trailer */
    __int32_t		type;	/* see TYPE_* #defines below */
} __pmLogHdr;

#define TYPE_DESC		1	/* header, pmDesc, trailer          */
#define TYPE_INDOM_V2		2	/* header, __pmInDom_v2, trailer    */
					/*         (with pmTimeval)         */
#define TYPE_LABEL_V2		3	/* header, __pmLogLabelSet, trailer */
					/*         (with pmTimeval)         */
#define TYPE_TEXT		4	/* header, __pmLogText, trailer     */
#define TYPE_INDOM		5	/* header, __pmInDom_v3, trailer    */
					/*         ("full" indom variant,   */
					/*          with __pmTimestamp)     */
#define TYPE_INDOM_DELTA	6	/* header, __pmInDom_v3, trailer    */
					/*         ("delta" indom variant,  */
					/*          with __pmTimestamp)     */
#define TYPE_LABEL		7	/* header, __pmLogLabelSet, trailer */
					/*         (with __pmTimestamp)     */
#define TYPE_MAX TYPE_LABEL

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
 * NOTE: timestamp is pmTimeVal for V2 archives and pmTimespec for
 *       V3 archives
 *
 * NOTE: the same structure is used for TYPE_INDOM_DELTA records, except
 *       the semantics of nameindex[] is changed so that a value of -1
 *       means the corresponding instance has been deleted from the
 *       instance domain (similarly namelist[] becomes NULL once the
 *       record is loaded), else the corresponding instance has been added
 *       to the instance domain
 */

#define PMLID_SELF	1		/* __pmLogInDom is malloc'd */
#define PMLID_INSTLIST	2		/* instlist[] is malloc'd */
#define PMLID_NAMELIST	4		/* namelist[] is malloc'd */
#define PMLID_NAMES	8		/* namelist[i] strings are malloc'd */

typedef struct __pmLogInDom {
    struct __pmLogInDom	*next;			/* backwards in time */
    struct __pmLogInDom	*prior;			/* forwards in time */
    pmInDom		indom;
    __pmTimestamp	stamp;
    int			isdelta;		/* 1 => delta indom semantics */
    int			numinst;
    int			alloc;			/* PMLID_... allocation flag bits */
    int			*instlist;		/* may point into buf[] */
    char		**namelist;		/* may point into buf[] */
    __int32_t		*buf;			/* on-disk buffer */
} __pmLogInDom;

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
 * __pmLogLabelSet is used to hold the sets of labels for the label
 * hierarchy in memory.  Only in the case of instances will it have
 * multiple labelsets.  For all other (higher) hierarchy levels, a
 * single labelset suffices (nsets == 1, and nlabels >= 0).  Also,
 * in memory labelsets are linked together in reverse chronological
 * order (just like the __pmLogInDom structure above).
 * -- externally we write these as
 *	timestamp (pmTimeval for TYPE_LABEL_V2 records, __pmTimestamp
 *	           for TYPE_LABEL records)
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
    __pmTimestamp	stamp;
    int			type;
    int			ident;
    int			nsets;
    pmLabelSet		*labelsets;
} __pmLogLabelSet;

/*
 * Internal archive label (below PMAPI)
 */
typedef struct {
    int			magic;		/* PM_LOG_MAGIC|PM_LOG_VERS?? */
    int			pid;		/* PID of logger */
    __pmTimestamp	start;		/* start of this log */
    int			vol;		/* current log volume no. */
    __uint32_t		features;	/* current enabled features */
    char		*hostname;	/* hostname at collection host */
    char		*timezone;	/* squashed $TZ at collection host */
    char		*zoneinfo;	/* detailed $TZ at collection host */
} __pmLogLabel;

/*
 * Internal Temporal Index Record
 */
typedef struct {
    __pmTimestamp	stamp;	/* now */
    int			vol;		/* current log volume no. */
    off_t		off_meta;	/* end of metadata file */
    off_t		off_data;	/* end of data file */
} __pmLogTI;

/*
 * Log/Archive Control
 */
typedef struct {
    __pmMutex	lc_lock;	/* mutex for multi-thread access */
    int		refcnt;		/* number of contexts using this log */
    char	*name;		/* external log base name */
    __pmFILE	*tifp;		/* temporal index */
    __pmFILE	*mdfp;		/* meta data */
    int		state;		/* (when writing) log state */
    __pmHashCtl	hashpmid;	/* PMID hashed access */
    __pmHashCtl	hashrange;	/* ptr to first and last value in log for */
				/* each metric */
    __pmHashCtl	hashindom;	/* instance domain hashed access */
    __pmHashCtl	trimindom;	/* timestamps for first and last value per */
    				/* instance per indom (nested hashing, lazy */
				/* loading) */
    __pmHashCtl	hashlabels;	/* maps the various metadata label types */
    __pmHashCtl hashtext;	/* maps the various help text types */
    int		minvol;		/* (when reading) lowest known volume no. */
    int		maxvol;		/* (when reading) highest known volume no. */
    int		numseen;	/* (when reading) size of seen */
    int		*seen;		/* (when reading) volumes opened OK */
    __pmLogLabel label;		/* (when reading) log label */
    off_t	physend;	/* (when reading) offset to physical EOF */
				/*                for last volume */
    __pmTimestamp endtime;	/* (when reading) timestamp at logical EOF */
    int		numti;		/* (when reading) no. temporal index entries */
    __pmLogTI	*ti;		/* (when reading) temporal index */
    struct __pmnsTree *pmns;	/* namespace from meta data */
    int		multi;		/* part of a multi-archive context */
} __pmLogCtl;

/* state values */
#define PM_LOG_STATE_NEW	0
#define PM_LOG_STATE_INIT	1

PCP_CALL extern int __pmEncodeResult(const __pmLogCtl *, const __pmResult *, __pmPDU **);

/*
 * Minimal information to retain for each archive in a multi-archive context
 */
typedef struct {
    char		*name;	/* external log base name */
    __pmTimestamp	starttime;	/* start time of the archive */
    char		*hostname;	/* name of collection host */
    char		*timezone;	/* squashed $TZ at collection host */
    char		*zoneinfo;	/* detailed $TZ at collection host */
} __pmMultiLogCtl;

/*
 * Per-context controls for archives and logs
 */
typedef struct {
    __pmLogCtl		*ac_log;	/* Current global logging and archive
					   control */
    __pmFILE		*ac_mfp;	/* current metrics log */
    int			ac_curvol;	/* current metrics log volume no. */
    long		ac_offset;	/* fseek ptr for archives */
    int			ac_vol;		/* volume for ac_offset */
    int			ac_serial;	/* serial access pattern for archives */
    int			ac_chkfeatures;	/* 1 => check featutre bits */
    __pmHashCtl		ac_pmid_hc;	/* per PMID controls for INTERP */
    double		ac_end;		/* time at end of archive */
    void		*ac_want;	/* used in interp.c */
    void		*ac_unbound;	/* used in interp.c */
    void		*ac_cache;	/* used in interp.c */
    int			ac_cache_idx;	/* used in interp.c */
    /*
     * These were added to the ABI in order to support multiple archives
     * in a single context.
     */
    int			ac_mark_done;	/* mark record between archives */
					/*   has been generated */
    int			ac_num_logs;	/* The number of archives */
    int			ac_cur_log;	/* The currently open archive */
    __pmMultiLogCtl	**ac_log_list;	/* Current set of archives */
} __pmArchCtl;

/*
 * Instance trimming control structures for archive replay ...
 * we use the timestamps from the indom metadata to establish time
 * boundaries (calipers) for individual instances in specific indoms.
 *
 * These structures are only built if required, and then only for "large"
 * indoms, see the HASH_THRESHOLD #define before time_caliper() in interp.c
 *
 * Top-level per-indom trimming control, which is accessed as a hash
 * using the indom as the key from trimindom.
 */
typedef struct {
    __pmHashCtl	hashinst;		/* nested hash on inst for this indom */
} __pmLogTrimInDom;

PCP_CALL extern void __pmFreeLogInDom(__pmLogInDom *);
PCP_CALL extern __pmLogInDom *__pmDupLogInDom(__pmLogInDom *);

/*
 * Nested per-instance trimming control (potentially one of these for
 * _every_ instance in _every_ "large" indom), accessed as a hash using
 * the internal instance identifier as the key from hashinst.
 *
 * Note: times are relative to the start of the archive, so the same as
 *       those used in interp.c for t_req et al
 */
typedef struct {
    double	t_birth;		/* instance first present */
    double	t_death;		/* instance no longer present */
} __pmLogTrimInst;

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
    __pmTimestamp	c_origin;	/* pmFetch time origin / current time */
    __pmTimestamp	c_delta;	/* for updating origin */
    int			c_direction;	/* signedness of delta (-1/0/1) */
    int			c_sent;		/* profile has been sent to pmcd */
    pmProfile		*c_instprof;	/* instance profile */
    void		*c_dm;		/* derived metrics, if any */
    int			c_flags;	/* ctx flags (set via type/env/attrs) */
    __pmHashCtl		c_attrs;	/* various optional context attributes */
    int			c_handle;	/* context number above PMAPI */
    int			c_slot;		/* index to contexts[] below PMAPI */
} __pmContext;

#define PM_CONTEXT_INIT	-2		/* special type: being initialized, do not use */

/* mask for (archive) directional parts of c_mode */
#define __PM_MODE_MASK	0xffff

/* internal archive routines */
PCP_CALL extern int __pmLogVersion(const __pmLogCtl *);
PCP_CALL extern size_t __pmLogLabelSize(const __pmLogCtl *);
PCP_CALL extern int __pmLogChkLabel(__pmArchCtl *, __pmFILE *, __pmLogLabel *, int);
PCP_CALL extern int __pmLogCreate(const char *, const char *, int, __pmArchCtl *);
PCP_CALL extern __pmFILE *__pmLogNewFile(const char *, int);
PCP_CALL extern void __pmLogClose(__pmArchCtl *);
PCP_CALL extern int __pmLogPutDesc(__pmArchCtl *, const pmDesc *, int, char **);
PCP_CALL extern int __pmLogPutInDom(__pmArchCtl *, int, const __pmLogInDom * const);
PCP_CALL extern int __pmLogPutResult(__pmArchCtl *, __pmPDU *);
PCP_CALL extern int __pmLogPutResult2(__pmArchCtl *, __pmPDU *);
PCP_CALL extern int __pmLogPutResult3(__pmArchCtl *, __pmPDU *);
PCP_CALL extern int __pmLogPutIndex(const __pmArchCtl *, const __pmTimestamp *);
PCP_CALL extern int __pmLogLoadIndex(__pmLogCtl *);
PCP_CALL extern int __pmLogEncodeLabels(__pmLogCtl *, unsigned int, unsigned int, int, pmLabelSet *, const __pmTimestamp *, __int32_t **);
PCP_CALL extern int __pmLogPutLabels(__pmArchCtl *, unsigned int, unsigned int, int, pmLabelSet *, const __pmTimestamp *);
PCP_CALL extern int __pmLogPutText(__pmArchCtl *, unsigned int, unsigned int, char *, int);
PCP_CALL extern int __pmLogWriteLabel(__pmFILE *, const __pmLogLabel *);
PCP_CALL extern int __pmLogWriteMark(__pmArchCtl *, const __pmTimestamp *, const __pmTimestamp *);
PCP_CALL extern int __pmLogLoadMeta(__pmArchCtl *);
PCP_CALL extern int __pmLogAddDesc(__pmArchCtl *, const pmDesc *);
PCP_CALL extern int __pmLogAddInDom(__pmArchCtl *, int, const __pmLogInDom *, __int32_t *);
PCP_CALL extern int __pmLogEncodeInDom(__pmLogCtl *, int, const __pmLogInDom * const, __int32_t **);
PCP_CALL extern __pmLogInDom *__pmLogSearchInDom(__pmLogCtl *, pmInDom, __pmTimestamp *);
PCP_CALL extern void __pmLogUndeltaInDom(pmInDom, __pmLogInDom *);
PCP_CALL extern int __pmLogAddPMNSNode(__pmArchCtl *, pmID, const char *);
PCP_CALL extern int __pmLogAddLabelSets(__pmArchCtl *, const __pmTimestamp *, unsigned int, unsigned int, int, pmLabelSet *);
PCP_CALL extern int __pmLogAddText(__pmArchCtl *, unsigned int, unsigned int, const char *);
PCP_CALL extern int __pmLogAddVolume(__pmArchCtl *, unsigned int);
#define PMLOGREAD_NEXT		0
#define PMLOGREAD_TO_EOF	1
PCP_CALL extern int __pmLogRead(__pmArchCtl *, int, __pmFILE *, __pmResult **, int);
PCP_CALL extern int __pmLogRead_ctx(__pmContext *, int, __pmFILE *, __pmResult **, int);
PCP_CALL extern int __pmLogChangeVol(__pmArchCtl *, int);
PCP_CALL extern int __pmLogFetch(__pmContext *, int, pmID *, __pmResult **);
PCP_CALL extern int __pmLogGetInDom(__pmArchCtl *, pmInDom, __pmTimestamp *, int **, char ***);
PCP_CALL extern int __pmGetArchiveEnd(__pmArchCtl *, __pmTimestamp *);
PCP_CALL extern int __pmLogLookupDesc(__pmArchCtl *, pmID, pmDesc *);
#define PMLOGPUTINDOM_DUP       1
PCP_CALL extern int __pmLogLookupInDom(__pmArchCtl *, pmInDom, __pmTimestamp *, const char *);
PCP_CALL extern int __pmLogLookupLabel(__pmArchCtl *, unsigned int, unsigned int, pmLabelSet **, const __pmTimestamp *);
PCP_CALL extern int __pmLogLookupText(__pmArchCtl *, unsigned int, unsigned int, char **);
PCP_CALL extern int __pmLogNameInDom(__pmArchCtl *, pmInDom, __pmTimestamp *, int, char **);
PCP_CALL extern const char *__pmLogMetaTypeStr(int);
PCP_CALL extern char *__pmLogMetaTypeStr_r(int, char *, int);
PCP_CALL extern const char *__pmLogLocalSocketDefault(int, char *buf, size_t bufSize);
PCP_CALL extern const char *__pmLogLocalSocketUser(int, char *buf, size_t bufSize);
PCP_CALL extern int __pmLogCompressedSuffix(const char *);
PCP_CALL extern char *__pmLogBaseName(char *);
PCP_CALL extern char *__pmLogBaseNameVol(char *, int *);
PCP_DATA extern int __pmLogReads;

/* Convert opaque context handle to __pmContext pointer */
PCP_CALL extern __pmContext *__pmHandleToPtr(int);

/*
 * Dump the current context (source details + instance profile),
 * for a particular instance domain.
 * If indom == PM_INDOM_NULL, then print all instance domains
 */

/* pmFetch helper routines, hooks for derivations and local contexts */
PCP_CALL extern int __pmFetch(__pmContext *, int, pmID *, __pmResult **);
PCP_CALL extern int __pmFetchLocal(__pmContext *, int, pmID *, __pmResult **);
PCP_CALL extern int __pmFetchArchive(__pmContext *, __pmResult **);
PCP_CALL extern int __pmPrepareFetch(__pmContext *, int, const pmID *, pmID **);
PCP_CALL extern int __pmFinishResult(__pmContext *, int, __pmResult **);
PCP_CALL extern int __pmFinishHighResResult(__pmContext *, int, pmHighResResult **);
PCP_CALL extern int __pmFetchLocal(__pmContext *, int, pmID *, __pmResult **);
PCP_CALL extern int __pmFetchHighResLocal(__pmContext *, int, pmID *, __pmResult **);
PCP_CALL extern int __pmDecodeResult_ctx(__pmContext *, __pmPDU *, __pmResult **);
PCP_CALL extern int __pmDecodeHighResResult_ctx(__pmContext *, __pmPDU *, __pmResult **);
PCP_CALL extern void __pmGetResultSize(int, int, pmValueSet * const *, size_t *, size_t *);
PCP_CALL extern void __pmSortInstances(__pmResult *);

/* safely insert an atom value into a pmValue */
PCP_CALL extern int __pmStuffValue(const pmAtomValue *, pmValue *, int);

/* Archive context helper. */
PCP_CALL extern int __pmFindOrOpenArchive(__pmContext *, const char *, int);
PCP_CALL extern int __pmLogFindOpen(__pmArchCtl *, const char *);

/* Generic access control routines */
PCP_CALL extern int __pmAccAddOp(unsigned int);
PCP_CALL extern int __pmAccAddHost(const char *, unsigned int, unsigned int, int);
PCP_CALL extern int __pmAccAddUser(const char *, unsigned int, unsigned int, int);
PCP_CALL extern int __pmAccAddGroup(const char *, unsigned int, unsigned int, int);
PCP_CALL extern int __pmAccAddClient(__pmSockAddr *, unsigned int *);
PCP_CALL extern int __pmAccAddAccount(const char *, const char *, unsigned int *);
PCP_CALL extern void __pmAccDelClient(__pmSockAddr *);
PCP_CALL extern void __pmAccDumpHosts(FILE *);
PCP_CALL extern void __pmAccDumpUsers(FILE *);
PCP_CALL extern void __pmAccDumpGroups(FILE *);
PCP_CALL extern void __pmAccDumpLists(FILE *);
PCP_CALL extern int __pmAccSaveHosts(void);
PCP_CALL extern int __pmAccSaveLists(void);
PCP_CALL extern int __pmAccRestoreHosts(void);
PCP_CALL extern int __pmAccRestoreLists(void);
PCP_CALL extern void __pmAccFreeSavedLists(void);

/* AF - general purpose asynchronous event management routines */
PCP_CALL extern int __pmAFsetup(const struct timeval *, const struct timeval *, void *, void (*)(int, void *));
PCP_CALL extern int __pmAFregister(const struct timeval *, void *, void (*)(int, void *));
PCP_CALL extern int __pmAFunregister(int);
PCP_CALL extern void __pmAFblock(void);
PCP_CALL extern void __pmAFunblock(void);
PCP_CALL extern int __pmAFisempty(void);

/* private PDU protocol between pmlc and pmlogger */
#define LOG_PDU_VERSION3	3	/* __pmTimestamp */
#define LOG_PDU_VERSION2	2	/* private pdus & PCP 2.0 error codes */
#define LOG_PDU_VERSION		LOG_PDU_VERSION3
#define LOG_REQUEST_NEWVOLUME	1
#define LOG_REQUEST_STATUS	2
#define LOG_REQUEST_SYNC	3
typedef struct {
    __pmTimestamp	start;		/* start time for log */
    __pmTimestamp	last;		/* last time log written */
    __pmTimestamp	now;		/* current time */
    int			state;		/* state of log (from __pmLogCtl) */
    int			vol;		/* current volume number of log */
    __int64_t		size;		/* size of current volume */
    struct {
	char		*hostname;	/* name of pmcd host */
	char		*fqdn;		/* fully qualified domain name of pmcd host */
	char		*timezone; 	/* squashed $TZ at collection host */
	char		*zoneinfo; 	/* detailed $TZ at collection host */
    } pmcd;
    struct {
	char		*timezone; 	/* squashed $TZ at pmlogger host */
	char		*zoneinfo; 	/* detailed $TZ at pmlogger host */
    } pmlogger;
} __pmLoggerStatus;
#define PDU_LOG_CONTROL		0x8000
#define PDU_LOG_STATUS_V2	0x8001
#define PDU_LOG_REQUEST		0x8002
#define PDU_LOG_STATUS		0x8003
PCP_CALL extern int __pmConnectLogger(const char *, int *, int *);
PCP_CALL extern int __pmSendLogControl(int, const __pmResult *, int, int, int);
PCP_CALL extern int __pmDecodeLogControl(const __pmPDU *, __pmResult **, int *, int *, int *);
PCP_CALL extern int __pmSendLogRequest(int, int);
PCP_CALL extern int __pmDecodeLogRequest(const __pmPDU *, int *);
PCP_CALL extern int __pmSendLogStatus(int, __pmLoggerStatus *);
PCP_CALL extern int __pmDecodeLogStatus(__pmPDU *, __pmLoggerStatus **);
PCP_CALL extern void __pmFreeLogStatus(__pmLoggerStatus *, int);

/* logger timeout helper function */
PCP_CALL extern int __pmLoggerTimeout(void);

/* other interfaces shared by pmlc and pmlogger */
PCP_CALL extern int __pmControlLog(int, const __pmResult *, int, int, int, __pmResult **);
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

/* Optimized fetch bundling ("optfetch") services */
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
/* Objective function parameters */
typedef struct {
    int		c_pmid;		/* cost per PMD for PMIDs in a fetch */
    int		c_indom;	/* cost per PMD for indoms in a fetch */
    int		c_fetch;	/* cost of a new fetch group */
    int		c_indomsize;	/* expected numer of instances for an indom */
    int		c_xtrainst;	/* cost of retrieving an unwanted metric inst */
    int		c_scope;	/* cost opt., 0 for incremental, 1 for global */
} optcost_t;
#define OPT_COST_INFINITY	0x7fffffff
PCP_CALL extern void __pmOptFetchAdd(fetchctl_t **, optreq_t *);
PCP_CALL extern int __pmOptFetchDel(fetchctl_t **, optreq_t *);
PCP_CALL extern void __pmOptFetchRedo(fetchctl_t **);
PCP_CALL extern void __pmOptFetchDump(FILE *, const fetchctl_t *);
PCP_CALL extern void __pmOptFetchGetParams(optcost_t *);
PCP_CALL extern void __pmOptFetchPutParams(optcost_t *);

/* __pmProcessExec and friends ... replacementes for system(3) and popen(3) */
typedef struct __pmExecCtl __pmExecCtl_t;		/* opaque handle */
PCP_CALL extern int __pmProcessAddArg(__pmExecCtl_t **, const char *);
PCP_CALL extern int __pmProcessUnpickArgs(__pmExecCtl_t **, const char *);
#define PM_EXEC_TOSS_NONE	0
#define PM_EXEC_TOSS_STDIN	1
#define PM_EXEC_TOSS_STDOUT	2
#define PM_EXEC_TOSS_STDERR	4
#define PM_EXEC_TOSS_ALL	7
#define PM_EXEC_NOWAIT		0
#define PM_EXEC_WAIT		1
PCP_CALL extern int __pmProcessExec(__pmExecCtl_t **, int, int);
PCP_CALL extern int __pmProcessPipe(__pmExecCtl_t **, const char *, int, FILE **);
PCP_CALL extern int __pmProcessPipeClose(FILE *);

/* platform independent environment and filesystem path access */
typedef void (*__pmConfigCallback)(char *, char *, char *);
PCP_DATA extern const __pmConfigCallback __pmNativeConfig;
PCP_CALL extern void __pmConfig(__pmConfigCallback);
PCP_CALL extern char *__pmNativePath(char *);
PCP_CALL extern int __pmAbsolutePath(char *);
PCP_CALL extern int __pmMakePath(const char *, mode_t);

/* platform independent signal handling */
typedef void (*__pmSignalHandler)(int);
PCP_CALL extern int __pmSetSignalHandler(int, __pmSignalHandler);

/* platform independent process management routines */
PCP_CALL extern int __pmProcessExists(pid_t);
PCP_CALL extern int __pmProcessTerminate(pid_t, int);
PCP_CALL extern pid_t __pmProcessCreate(char **, int *, int *);
PCP_CALL extern int __pmProcessDataSize(unsigned long *);
PCP_CALL extern int __pmProcessRunTimes(double *, double *);

/* platform independent memory mapped file handling */
PCP_CALL extern void *__pmMemoryMap(int, size_t, int);
PCP_CALL extern void __pmMemoryUnmap(void *, size_t);

/* map platform error values to PMAPI error codes.  */
PCP_CALL extern int __pmMapErrno(int);

/* Anonymous metric registration (uses derived metrics support) */
PCP_CALL extern int __pmRegisterAnon(const char *, int);

/* Get nanosecond precision timestamp from system clocks */
PCP_CALL extern int __pmGetTimespec(struct timespec *);

/*
 * discover configurable features of the shared libraries
 */
typedef void (*__pmAPIConfigCallback)(const char *, const char *);
PCP_CALL extern void __pmAPIConfig(__pmAPIConfigCallback);

/*
 * Service discovery with options.
 * The 4th argument is a pointer to a mask of flags for boolean options
 * and status. It is set and tested using the following bits.
 */
#define PM_SERVICE_DISCOVERY_INTERRUPTED	0x1
#define PM_SERVICE_DISCOVERY_RESOLVE		0x2
PCP_CALL extern int __pmDiscoverServicesWithOptions(const char *,
					   const char *,
					   const char *,
					   const volatile sig_atomic_t *,
					   char ***);

/* Helper methods for packed arrays of event records */
PCP_CALL extern int __pmCheckEventRecords(pmValueSet *, int);

/*
 * helper functions to register client identity with pmcd for export
 * via pmcd.client.whoami
 */
PCP_CALL extern char *__pmGetClientId(int, char **);
PCP_CALL extern int __pmSetClientIdArgv(int, char **);
PCP_CALL extern int __pmSetClientId(const char *);

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
PCP_CALL extern int __pmLogFindPort(const char *, int, __pmLogPort **);

#define PM_LOG_PRIMARY_PID	0	/* symbolic pid for primary logger */
#define PM_LOG_PRIMARY_PORT	0	/* symbolic port for primary pmlogger */
#define PM_LOG_ALL_PIDS		-1	/* symbolic pid for all pmloggers */
#define PM_LOG_NO_PID		-2	/* not a valid pid for pmlogger */
#define PM_LOG_NO_PORT		-2	/* not a valid port for pmlogger */

/* check for localhost */
PCP_CALL extern int __pmIsLocalhost(const char *);

/* DSO PMDA helpers */
PCP_CALL extern int __pmLocalPMDA(int, int, const char *, const char *);

/* internals of argument parsing for special circumstances */
PCP_CALL extern void __pmStartOptions(pmOptions *);
PCP_CALL extern int  __pmGetLongOptions(pmOptions *);
PCP_CALL extern void __pmAddOptArchive(pmOptions *, char *);
PCP_CALL extern void __pmAddOptArchivePath(pmOptions *);
PCP_CALL extern void __pmAddOptArchiveList(pmOptions *, char *);
PCP_CALL extern void __pmAddOptArchiveFolio(pmOptions *, char *);
PCP_CALL extern void __pmAddOptContainer(pmOptions *, char *);
PCP_CALL extern void __pmAddOptHost(pmOptions *, char *);
PCP_CALL extern void __pmAddOptHostList(pmOptions *, char *);
PCP_CALL extern void __pmSetLocalContextFlag(pmOptions *);
PCP_CALL extern void __pmSetLocalContextTable(pmOptions *, char *);
PCP_CALL extern void __pmEndOptions(pmOptions *);

/* work out local timezone */
PCP_CALL extern char *__pmTimezone(void);		/* NOT thread-safe */
PCP_CALL extern char *__pmTimezone_r(char *, int);
PCP_CALL extern char *__pmZoneinfo(void);

/* string conversion to value of given type, suitable for pmStore */
PCP_CALL extern int __pmStringValue(const char *, pmAtomValue *, int);

/* time-based delays */
PCP_CALL extern void __pmtimespecSleep(struct timespec);
PCP_CALL extern void __pmtimespecPause(struct timespec);
PCP_CALL extern void __pmtimevalSleep(struct timeval);
PCP_CALL extern void __pmtimevalPause(struct timeval);

/* manipulate internal timestamps */
PCP_CALL extern int __pmGetTimestamp(__pmTimestamp *);
PCP_CALL extern double __pmTimevalSub(const pmTimeval *, const pmTimeval *);
PCP_CALL extern double __pmTimespecSub(const pmTimespec *, const pmTimespec *);
PCP_CALL extern double __pmTimestampSub(const __pmTimestamp *, const __pmTimestamp *);
PCP_CALL extern void __pmTimestampInc(__pmTimestamp *, const __pmTimestamp *);
PCP_CALL extern void __pmTimestampDec(__pmTimestamp *, const __pmTimestamp *);
PCP_CALL extern int __pmTimestampCmp(const __pmTimestamp *, const __pmTimestamp *);

/* reverse ctime, time interval parsing, time conversions */
PCP_CALL extern int __pmParseCtime(const char *, struct tm *, char **);
PCP_CALL extern int __pmParseTime(const char *, struct timeval *,
				struct timeval *, struct timeval *, char **);
PCP_CALL extern int __pmParseHighResTime(const char *, struct timespec *,
				struct timespec *, struct timespec *, char **);
PCP_CALL extern int __pmConvertTime(struct tm *, struct timeval *,
				struct timeval *);
PCP_CALL extern int __pmConvertHighResTime(struct tm *, struct timespec *,
				struct timespec *);
PCP_CALL extern time_t __pmMktime(struct tm *);

/* Query server features - used for expressing protocol capabilities */
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
PCP_CALL extern int __pmServerOpenRequestPorts(__pmFdSet *, int);
PCP_CALL extern int __pmServerGetRequestPort(int, const char **, int *);
PCP_CALL extern int __pmServerSetupRequestPorts(void);
PCP_CALL extern void __pmServerCloseRequestPorts(void);
PCP_CALL extern void __pmServerDumpRequestPorts(FILE *);
PCP_CALL extern char *__pmServerRequestPortString(int, char *, size_t);

/* service manager notifications */
PCP_CALL extern int __pmServerNotifyServiceManagerReady(pid_t);
PCP_CALL extern int __pmServerNotifyServiceManagerStopping(pid_t);

/* Service broadcasting, for servers. */
typedef struct __pmServerPresence __pmServerPresence;
PCP_CALL extern __pmServerPresence *__pmServerAdvertisePresence(const char *, int);
PCP_CALL extern void __pmServerUnadvertisePresence(__pmServerPresence *);

/* Attributes stuff */
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
PCP_CALL extern int __pmParseHostAttrsSpec(
    const char *, __pmHostSpec **, int *, __pmHashCtl *, char **);
PCP_CALL extern int __pmUnparseHostAttrsSpec(
    __pmHostSpec *, int, __pmHashCtl *, char *, size_t);
PCP_CALL extern int __pmUrlEncode(const char *inp, const size_t len, char **outp);
PCP_CALL extern int __pmUrlDecode(const char *inp, const size_t len, char **outp);

/* SSL/TLS/IPv6 support via NSS/NSPR */
PCP_CALL extern int __pmSecureServerCertificateSetup(const char *, const char *, const char *);
PCP_CALL extern void __pmSecureServerShutdown(void);
PCP_CALL extern int __pmSecureServerHandshake(int, int, __pmHashCtl *);
PCP_CALL extern int __pmSecureClientHandshake(int, int, const char *, __pmHashCtl *);

/* PDU or connection timeouts */
PCP_CALL extern double __pmConnectTimeout(void);
PCP_CALL extern int __pmSetConnectTimeout(double);
PCP_CALL extern double __pmRequestTimeout(void);
PCP_CALL extern int __pmSetRequestTimeout(double);

/* instance profile methods */
PCP_CALL extern int __pmInProfile(pmInDom, const pmProfile *, int);

/* __pmResult alloc/offsets */
PCP_CALL extern __pmResult *__pmAllocResult(int);
/*
 * __pmResult and pmResult are identical after the timestamp.
 * If we're exporting a pmResult from a __pmResult, this function
 * returns the correct address for the pmResult at, or shortly
 * after, the start of the __pmResult (rp)
 */
static inline pmResult *
__pmOffsetResult(const __pmResult *rp)
{
   return (pmResult *)(&((char *)rp)[offsetof(__pmResult,numpmid) - offsetof(pmResult,numpmid)]);
}

/* same for pmHighResResult ... */
static inline pmHighResResult *
__pmOffsetHighResResult(const __pmResult *rp)
{
   return (pmHighResResult *)(&((char *)rp)[offsetof(__pmResult,numpmid) - offsetof(pmHighResResult,numpmid)]);
}

/* free malloc'd data structures */
PCP_CALL extern void __pmFreeAttrsSpec(__pmHashCtl *);
PCP_CALL extern void __pmFreeHostAttrsSpec(__pmHostSpec *, int, __pmHashCtl *);
PCP_CALL extern void __pmFreeHostSpec(__pmHostSpec *, int);
PCP_CALL extern void __pmFreeInResult(pmInResult *);
PCP_CALL extern void __pmFreePMNS(__pmnsTree *);
PCP_CALL extern void __pmFreeProfile(pmProfile *);
PCP_CALL extern void __pmFreeResultValues(pmResult *);
PCP_CALL extern void __pmFreeHighResResultValues(pmHighResResult *);
PCP_CALL extern void __pmFreeDerived(__pmContext *);
PCP_CALL extern void __pmFreeResult(__pmResult *);

/* diagnostics for formatting or printing miscellaneous data structures */
PCP_CALL extern void __pmDumpContext(FILE *, int, pmInDom);
PCP_CALL extern void __pmDumpDebug(FILE *);
PCP_CALL extern void __pmDumpErrTab(FILE *);
PCP_CALL extern void __pmDumpEventRecords(FILE *, pmValueSet *, int);
PCP_CALL extern void __pmDumpHighResEventRecords(FILE *, pmValueSet *, int);
PCP_CALL extern void __pmDumpHighResResult(FILE *, const pmHighResResult *);
PCP_CALL extern void __pmDumpIDList(FILE *, int, const pmID *);
PCP_CALL extern void __pmDumpInResult(FILE *, const pmInResult *);
PCP_CALL extern void __pmDumpLabelSets(FILE *, const pmLabelSet *, int);
PCP_CALL extern void __pmDumpNameList(FILE *, int, const char **);
PCP_CALL extern void __pmDumpNameNode(FILE *, const __pmnsNode *, int);
PCP_CALL extern void __pmDumpNameSpace(FILE *, int);
PCP_CALL extern void __pmDumpResult(FILE *, const pmResult *);
PCP_CALL extern void __pmPrintResult(FILE *, const __pmResult *);
PCP_CALL extern void __pmDumpStack(void);
PCP_CALL extern void __pmDumpStackInit(void *);
PCP_CALL extern void __pmDumpStatusList(FILE *, int, const int *);
PCP_CALL extern void __pmPrintTimeval(FILE *, const pmTimeval *);
PCP_CALL extern void __pmPrintTimespec(FILE *, const pmTimespec *);
PCP_CALL extern void __pmPrintTimestamp(FILE *, const __pmTimestamp *);
PCP_CALL extern void __pmPrintIPC(void);
PCP_CALL extern char *__pmPDUTypeStr_r(int, char *, int);
PCP_CALL extern const char *__pmPDUTypeStr(int);	/* NOT thread-safe */
PCP_CALL extern int __pmAttrKeyStr_r(__pmAttrKey, char *, size_t);
PCP_CALL extern int __pmAttrStr_r(__pmAttrKey, const char *, char *, size_t);
PCP_CALL extern char *__pmLabelIdentString(int, int, char *, size_t);
PCP_CALL extern const char *__pmLabelTypeString(int);
PCP_CALL extern const char *__pmGetLabelConfigHostName(char *, size_t);
PCP_CALL extern const char *__pmGetLabelConfigMachineID(char *, size_t);
PCP_CALL extern const char *__pmGetLabelConfigDomainName(char *, size_t);
PCP_CALL extern char *__pmLogFeaturesStr(__uint32_t);

/* log file rotation */
PCP_CALL extern FILE *__pmRotateLog(const char *, const char *, FILE *, int *);

/*
 * Dump the instance profile, for a particular instance domain
 * If indom == PM_INDOM_NULL, then print all instance domains
 */
PCP_CALL extern void __pmDumpProfile(FILE *, int, const pmProfile *);

/* helper routine to print all names of a metric */
PCP_CALL extern void __pmPrintMetricNames(FILE *, int, char **, char *);

/*
 * Return the argument if it's a valid filename else return NULL
 * Note: this function could be replaced with a call to access(),
 * but is retained for historical use in __pmConnectLocal()
 */
PCP_CALL extern const char *__pmFindPMDA(const char *);

/*
 * Cleanup handling:
 * shutdown various components in libpcp, releasing all resources
 * (local context PMDAs, any global NSS socket state, etc).
 */
PCP_CALL extern int __pmShutdown(void);

PCP_CALL extern void __pmIgnoreSignalPIPE(void);

/* free high resolution timestamp variant of pmResult */
PCP_CALL extern void __pmFreeHighResResult(pmHighResResult *);

/*
 * Loading archive records from disk ...
 */
PCP_CALL extern int __pmLogLoadInDom(__pmArchCtl *, int, int, __pmLogInDom *, __int32_t **);
PCP_CALL extern int __pmLogLoadLabel(__pmFILE *, __pmLogLabel *);
PCP_CALL extern void __pmLogFreeLabel(__pmLogLabel *);
PCP_CALL extern int __pmLogLoadLabelSet(char *, int, int, __pmTimestamp *,
					int *, int *, int *, pmLabelSet **);

/*
 * Routines to pack (Put) and unpack (Load) each timestamp format
 * to/from a __pmTimestamp ... used to dink with PDU buffer and
 * on-disk records for archives.
 * Does endian-safe "hton" for Put, and "ntoh" for Load functions.
 */
PCP_CALL extern void __pmLoadTimestamp(const __int32_t *, __pmTimestamp *);
PCP_CALL extern void __pmLoadTimeval(const __int32_t *, __pmTimestamp *);
PCP_CALL extern void __pmPutTimestamp(const __pmTimestamp *, __int32_t *);
PCP_CALL extern void __pmPutTimeval(const __pmTimestamp *, __int32_t *);

#ifdef __cplusplus
}
#endif

#endif /* PCP_LIBPCP_H */
