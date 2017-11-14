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

PCP_CALL extern void __pmPrintDesc(FILE *, const pmDesc *);

/* == end == */

/*
 * This defines the routines, macros and data structures that are used
 * in the Performance Metrics Collection Subsystem (PMCS) below the
 * PMAPI.
 */

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

/* standard log file set up */
PCP_CALL extern FILE *__pmOpenLog(const char *, const char *, FILE *, int *);
PCP_CALL extern FILE *__pmRotateLog(const char *, const char *, FILE *, int *);
/* make __pmNotifyErr also add entries to syslog */
PCP_CALL extern void __pmSyslog(int);
/* standard error, warning and info wrapper for syslog(3) */
PCP_CALL extern void __pmNotifyErr(int, const char *, ...) __PM_PRINTFLIKE(2,3);




/*
 * Return the argument if it's a valid filename else return NULL
 * (note: this function could be replaced with a call to access(),
 * but is retained for historical reasons).
 */
PCP_CALL extern const char *__pmFindPMDA(const char *);



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

PCP_CALL extern void __pmIgnoreSignalPIPE(void);

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
