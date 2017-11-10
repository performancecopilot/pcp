/*
 * CAVEAT
 *	The interfaces and data structures defined in this header are
 *	intended for internal Performance Co-Pilot (PCP) developer use.
 *
 *	They are not part of the PCP APIs that are guaranteed to
 *	remain fixed across releases, and they may not work, or may
 *	provide different semantics at some point in the future.
 *
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
#ifndef PCP_LIBPCP_H
#define PCP_LIBPCP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * internal libpcp state ... PM_STATE_APPL means we are at or above the
 * PMAPI in a state where PMAPI calls can safely be made ... PM_STATE_PMCS
 * means we are in the PMCD, or a PMDA, or low-level PDU code, and
 * PMAPI calls are a bad idea.
 */
#define PM_STATE_APPL	0
#define PM_STATE_PMCS	1
PCP_CALL extern void __pmSetInternalState(int);
PCP_CALL extern int __pmGetInternalState(void);

/*
 * Multi-thread support
 * Use PM_MULTI_THREAD_DEBUG for lock debugging with -Dlock[,appl?...]
 */
PCP_CALL extern void __pmInitLocks(void);
PCP_CALL extern int __pmLock(void *, const char *, int);
PCP_CALL extern int __pmUnlock(void *, const char *, int);
PCP_CALL extern int __pmIsLocked(void *);
#ifdef BUILD_WITH_LOCK_ASSERTS
PCP_CALL extern void __pmCheckIsUnlocked(void *, char *, int);
#endif /* BUILD_WITH_LOCK_ASSERTS */

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
#define PM_IS_LOCKED(lock) 	__pmIsLocked(&(lock))

#ifdef HAVE_PTHREAD_MUTEX_T
/* the big libpcp lock */
PCP_CALL extern pthread_mutex_t	__pmLock_libpcp;
/* mutex for calls to external routines that are not thread-safe */
PCP_CALL extern pthread_mutex_t	__pmLock_extcall;
#else
PCP_CALL extern void *__pmLock_libpcp;			/* symbol exposure */
PCP_CALL extern void *__pmLock_extcall;			/* symbol exposure */
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
	int		flag : 1;
	unsigned int	domain : 9;
	unsigned int	serial : 22;
#else
	unsigned int	serial : 22;
	unsigned int	domain : 9;
	int		flag : 1;
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

/* used by pmnsmerge... */
PCP_CALL extern __pmnsTree *__pmExportPMNS(void); 

/* for PMNS in archives */
PCP_CALL extern int __pmNewPMNS(__pmnsTree **);
PCP_CALL extern void __pmFreePMNS(__pmnsTree *);
PCP_CALL extern void __pmUsePMNS(__pmnsTree *); /* for debugging */
PCP_CALL extern int __pmFixPMNSHashTab(__pmnsTree *, int, int);
PCP_CALL extern int __pmAddPMNSNode(__pmnsTree *, int, const char *);
PCP_CALL extern void __pmDumpNameNode(FILE *, __pmnsNode *, int);

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
#define PDU_FINISH		0x7013
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
 * Note: int is OK here, because configure ensures int is a 32-bit integer
 */
typedef struct {
    int		len;		/* length of pdu_header + PDU */
    int		type;		/* PDU type */
    int		from;		/* pid of PDU originator */
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
#define PDU_FLAG_LABEL		(1U<<8)
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
PCP_CALL extern int __pmGetPDUCeiling(void);
PCP_CALL extern int __pmSetPDUCeiling(int);

/* PDU type specfic send-encode-decode routines */
PCP_CALL extern int __pmSendError(int, int, int);
PCP_CALL extern int __pmDecodeError(__pmPDU *, int *);
PCP_CALL extern int __pmSendXtendError(int, int, int, int);
PCP_CALL extern int __pmDecodeXtendError(__pmPDU *, int *, int *);
PCP_CALL extern int __pmSendResult(int, int, const pmResult *);
PCP_CALL extern int __pmEncodeResult(int, const pmResult *, __pmPDU **);
PCP_CALL extern int __pmDecodeResult(__pmPDU *, pmResult **);
PCP_CALL extern int __pmSendProfile(int, int, int, __pmProfile *);
PCP_CALL extern int __pmDecodeProfile(__pmPDU *, int *, __pmProfile **);
PCP_CALL extern int __pmSendFetch(int, int, int, __pmTimeval *, int, pmID *);
PCP_CALL extern int __pmDecodeFetch(__pmPDU *, int *, __pmTimeval *, int *, pmID **);
PCP_CALL extern int __pmSendDescReq(int, int, pmID);
PCP_CALL extern int __pmDecodeDescReq(__pmPDU *, pmID *);
PCP_CALL extern int __pmSendDesc(int, int, pmDesc *);
PCP_CALL extern int __pmDecodeDesc(__pmPDU *, pmDesc *);
PCP_CALL extern int __pmSendInstanceReq(int, int, const __pmTimeval *, pmInDom, int, const char *);
PCP_CALL extern int __pmDecodeInstanceReq(__pmPDU *, __pmTimeval *, pmInDom *, int *, char **);
PCP_CALL extern int __pmSendInstance(int, int, __pmInResult *);
PCP_CALL extern int __pmDecodeInstance(__pmPDU *, __pmInResult **);
PCP_CALL extern int __pmSendTextReq(int, int, int, int);
PCP_CALL extern int __pmDecodeTextReq(__pmPDU *, int *, int *);
PCP_CALL extern int __pmSendText(int, int, int, const char *);
PCP_CALL extern int __pmDecodeText(__pmPDU *, int *, char **);
PCP_CALL extern int __pmSendCreds(int, int, int, const __pmCred *);
PCP_CALL extern int __pmDecodeCreds(__pmPDU *, int *, int *, __pmCred **);
PCP_CALL extern int __pmSendIDList(int, int, int, const pmID *, int);
PCP_CALL extern int __pmDecodeIDList(__pmPDU *, int, pmID *, int *);
PCP_CALL extern int __pmSendNameList(int, int, int, char **, const int *);
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

/* PDU buffer services */
PCP_CALL extern __pmPDU *__pmFindPDUBuf(int);
PCP_CALL extern void __pmPinPDUBuf(void *);
PCP_CALL extern int __pmUnpinPDUBuf(void *);
PCP_CALL extern void __pmCountPDUBuf(int, int *, int *);
PCP_DATA extern unsigned int *__pmPDUCntIn;
PCP_DATA extern unsigned int *__pmPDUCntOut;
PCP_CALL extern void __pmSetPDUCntBuf(unsigned *, unsigned *);

/* internal IPC protocol stuff */
typedef int (*__pmConnectHostType)(int, int);
PCP_CALL extern int __pmSetSocketIPC(int);
PCP_CALL extern int __pmSetVersionIPC(int, int);
PCP_CALL extern int __pmSetFeaturesIPC(int, int, int);
PCP_CALL extern int __pmSetDataIPC(int, void *);
PCP_CALL extern int __pmDataIPCSize(void);
PCP_CALL extern int __pmLastVersionIPC(void);
PCP_CALL extern int __pmVersionIPC(int);
PCP_CALL extern int __pmSocketIPC(int);
PCP_CALL extern int __pmFeaturesIPC(int);
PCP_CALL extern int __pmDataIPC(int, void *);
PCP_CALL extern void __pmOverrideLastFd(int);
PCP_CALL extern void __pmPrintIPC(void);
PCP_CALL extern void __pmResetIPC(int);

/* TODO - more achive stuff here */
PCP_CALL extern int __pmLogPutResult(__pmLogCtl *, __pmPDU *);
PCP_CALL extern int __pmLogPutResult2(__pmLogCtl *, __pmPDU *);

/* Generic access control routines */
PCP_CALL extern int __pmAccAddOp(unsigned int);
PCP_CALL extern int __pmAccAddHost(const char *, unsigned int, unsigned int, int);
PCP_CALL extern int __pmAccAddUser(const char *, unsigned int, unsigned int, int);
PCP_CALL extern int __pmAccAddGroup(const char *, unsigned int, unsigned int, int);
PCP_CALL extern int __pmAccAddClient(__pmSockAddr *, unsigned int *);
PCP_CALL extern int __pmAccAddAccount(const char *, const char *, unsigned int *);
PCP_CALL extern void __pmAccDelClient(__pmSockAddr *);
PCP_CALL extern void __pmAccDelAccount(const char *, const char *);
PCP_CALL extern void __pmAccDumpHosts(FILE *);
PCP_CALL extern void __pmAccDumpUsers(FILE *);
PCP_CALL extern void __pmAccDumpGroups(FILE *);
PCP_CALL extern void __pmAccDumpLists(FILE *);
PCP_CALL extern int __pmAccSaveHosts(void);
PCP_CALL extern int __pmAccSaveUsers(void);
PCP_CALL extern int __pmAccSaveGroups(void);
PCP_CALL extern int __pmAccSaveLists(void);
PCP_CALL extern int __pmAccRestoreHosts(void);
PCP_CALL extern int __pmAccRestoreUsers(void);
PCP_CALL extern int __pmAccRestoreGroups(void);
PCP_CALL extern int __pmAccRestoreLists(void);
PCP_CALL extern void __pmAccFreeSavedHosts(void);
PCP_CALL extern void __pmAccFreeSavedUsers(void);
PCP_CALL extern void __pmAccFreeSavedGroups(void);
PCP_CALL extern void __pmAccFreeSavedLists(void);

/* AF - general purpose asynchronous event management routines */
PCP_CALL extern int __pmAFsetup(const struct timeval *, const struct timeval *, void *, void (*)(int, void *));
PCP_CALL extern int __pmAFregister(const struct timeval *, void *, void (*)(int, void *));
PCP_CALL extern int __pmAFunregister(int);
PCP_CALL extern void __pmAFblock(void);
PCP_CALL extern void __pmAFunblock(void);
PCP_CALL extern int __pmAFisempty(void);

/* private PDU protocol between pmlc and pmlogger */
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
PCP_CALL extern int __pmConnectLogger(const char *, int *, int *);
PCP_CALL extern int __pmSendLogControl(int, const pmResult *, int, int, int);
PCP_CALL extern int __pmDecodeLogControl(const __pmPDU *, pmResult **, int *, int *, int *);
PCP_CALL extern int __pmSendLogRequest(int, int);
PCP_CALL extern int __pmDecodeLogRequest(const __pmPDU *, int *);
PCP_CALL extern int __pmSendLogStatus(int, __pmLoggerStatus *);
PCP_CALL extern int __pmDecodeLogStatus(__pmPDU *, __pmLoggerStatus **);

/* logger timeout helper function */
PCP_CALL extern int __pmLoggerTimeout(void);

/* other interfaces shared by pmlc and pmlogger */
PCP_CALL extern int __pmControlLog(int, const pmResult *, int, int, int, pmResult **);
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
PCP_CALL extern void __pmDumpErrTab(FILE *);

/* Anonymous metric registration (uses derived metrics support) */
PCP_CALL extern int __pmRegisterAnon(const char *, int);

/* Get nanosecond precision timestamp from system clocks */
PCP_CALL extern int __pmGetTimespec(struct timespec *);

/*
 * discover configurable features of the shared libraries
 */
typedef void (*__pmAPIConfigCallback)(const char *, const char *);
PCP_CALL extern void __pmAPIConfig(__pmAPIConfigCallback);
PCP_CALL extern const char *__pmGetAPIConfig(const char *);

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
PCP_CALL extern int __pmCheckHighResEventRecords(pmValueSet *, int);
PCP_CALL extern void __pmDumpEventRecords(FILE *, pmValueSet *, int);
PCP_CALL extern void __pmDumpHighResEventRecords(FILE *, pmValueSet *, int);

/* event tracing for monitoring time between events */
PCP_CALL extern void __pmEventTrace(const char *);		/* NOT thread-safe */
PCP_CALL extern void __pmEventTrace_r(const char *, int *, double *, double *);

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

/* 32-bit file checksum */
PCP_CALL extern __int32_t __pmCheckSum(FILE *);

/* for QA apps ...  */
PCP_CALL extern void __pmDumpDebug(FILE *);

/* DSO PMDA helpers */
struct __pmDSO;			/* opaque, real definition in pmda.h */
PCP_CALL extern struct __pmDSO *__pmLookupDSO(int);
/*
 * Adding/deleting/clearing the list of DSO PMDAs supported for
 * PM_CONTEXT_LOCAL contexts
 */
#define PM_LOCAL_ADD	1
#define PM_LOCAL_DEL	2
#define PM_LOCAL_CLEAR	3
PCP_CALL extern int __pmLocalPMDA(int, int, const char *, const char *);
PCP_CALL extern char *__pmSpecLocalPMDA(const char *);

/* internals of argument parsing for special circumstances */
PCP_CALL extern void __pmStartOptions(pmOptions *);
PCP_CALL extern int  __pmGetLongOptions(pmOptions *);
PCP_CALL extern void __pmAddOptArchive(pmOptions *, char *);
PCP_CALL extern void __pmAddOptArchiveList(pmOptions *, char *);
PCP_CALL extern void __pmAddOptArchiveFolio(pmOptions *, char *);
PCP_CALL extern void __pmAddOptContainer(pmOptions *, char *);
PCP_CALL extern void __pmAddOptHost(pmOptions *, char *);
PCP_CALL extern void __pmAddOptHostList(pmOptions *, char *);
PCP_CALL extern void __pmSetLocalContextFlag(pmOptions *);
PCP_CALL extern void __pmSetLocalContextTable(pmOptions *, char *);
PCP_CALL extern void __pmEndOptions(pmOptions *);

/* real pmTimeZone declaration */
typedef struct {
    char		*label;		/* label to name tz */
    char		*tz;		/* env $TZ */
    int			handle;		/* handle from pmNewZone() */
} pmTimeZone;

/* work out local timezone */
PCP_CALL extern char *__pmTimezone(void);		/* NOT thread-safe */
PCP_CALL extern char *__pmTimezone_r(char *, int);

/* string conversion to value of given type, suitable for pmStore */
PCP_CALL extern int __pmStringValue(const char *, pmAtomValue *, int);

/* timeval-based delays */
PCP_CALL extern void __pmtimevalSleep(struct timeval);
PCP_CALL extern void __pmtimevalPause(struct timeval);

/* manipulate internal timestamps */
PCP_CALL extern int __pmTimevalCmp(const __pmTimeval *, const __pmTimeval *);
PCP_CALL extern double __pmTimevalSub(const __pmTimeval *, const __pmTimeval *);

/* reverse ctime, time interval parsing, time conversions */
PCP_CALL extern int __pmParseCtime(const char *, struct tm *, char **);
PCP_CALL extern int __pmParseTime(const char *, struct timeval *, struct timeval *,
			 struct timeval *, char **);
PCP_CALL extern int __pmConvertTime(struct tm *, struct timeval *, struct timeval *);
PCP_CALL extern time_t __pmMktime(struct tm *);

#ifdef __cplusplus
}
#endif

#endif /* PCP_LIBPCP_H */
