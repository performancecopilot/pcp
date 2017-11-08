/*
 * CAVEAT
 * 	The interfaces defined in this header file are provided by
 * 	libpcp, but unlike those in pmapi.h, these ones are not
 * 	guaranteed to remain fixed across PCP releases.
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

/*
 * Generic access control routines
 */
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

/*
 * AF - general purpose asynchronous event management routines
 */
PCP_CALL extern int __pmAFsetup(const struct timeval *, const struct timeval *, void *, void (*)(int, void *));
PCP_CALL extern int __pmAFregister(const struct timeval *, void *, void (*)(int, void *));
PCP_CALL extern int __pmAFunregister(int);
PCP_CALL extern void __pmAFblock(void);
PCP_CALL extern void __pmAFunblock(void);
PCP_CALL extern int __pmAFisempty(void);

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

PCP_CALL extern int __pmConnectLogger(const char *, int *, int *);
PCP_CALL extern int __pmSendLogControl(int, const pmResult *, int, int, int);
PCP_CALL extern int __pmDecodeLogControl(const __pmPDU *, pmResult **, int *, int *, int *);
PCP_CALL extern int __pmSendLogRequest(int, int);
PCP_CALL extern int __pmDecodeLogRequest(const __pmPDU *, int *);
PCP_CALL extern int __pmSendLogStatus(int, __pmLoggerStatus *);
PCP_CALL extern int __pmDecodeLogStatus(__pmPDU *, __pmLoggerStatus **);

/* logger timeout helper function */
PCP_CALL extern int __pmLoggerTimeout(void);

/*
 * other interfaces shared by pmlc and pmlogger
 */

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

/*
 * For QA apps ...
 */
PCP_CALL extern void __pmDumpDebug(FILE *);

#endif /* PCP_LIBPCP_H */
