/*
 * Copyright (c) 2013-2018 Red Hat.
 * Copyright (c) 1995,2005 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef PCP_PMDA_H
#define PCP_PMDA_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * libpcp_pmda interface versions
 */
#define PMDA_INTERFACE_2	2	/* new function arguments */
#define PMDA_INTERFACE_3	3	/* 3-state return from fetch callback */
#define PMDA_INTERFACE_4	4	/* dynamic pmns */
#define PMDA_INTERFACE_5	5	/* client context in pmda and */
					/* 4-state return from fetch callback */
#define PMDA_INTERFACE_6	6	/* client security attributes */
#define PMDA_INTERFACE_7	7	/* metric label metadata */
#define PMDA_INTERFACE_LATEST	7

/*
 * Type of I/O connection to PMCD (pmdaUnknown defaults to pmdaPipe)
 */
typedef enum {pmdaPipe, pmdaInet, pmdaUnix, pmdaUnknown, pmdaIPv6} pmdaIoType;

/*
 * Forward declarations of structures so that inclusion of (internal) libpcp.h
 * header file is not required if this header file is included.
 */
typedef struct __pmnsTree  pmdaNameSpace;
typedef struct __pmHashCtl pmdaHashTable;

/*
 * Instance description: index and name
 */
typedef struct pmdaInstid {
    int		i_inst;		/* internal instance identifier */
    char	*i_name;	/* external instance identifier */
} pmdaInstid;

/*
 * Instance domain description: unique instance id, number of instances in
 * this domain, and the list of instances (not null terminated).
 */
typedef struct pmdaIndom {
    pmInDom	it_indom;	/* indom, filled in */
    int		it_numinst;	/* number of instances */
    pmdaInstid	*it_set;	/* instance identifiers */
} pmdaIndom;

/*
 * Metric description: handle for extending description, and the description.
 */
typedef struct pmdaMetric {
    void	*m_user;	/* for users external use */
    pmDesc	m_desc;		/* metric description */
} pmdaMetric;

/*
 * Type of function call back used by pmdaFetch.
 */
typedef int (*pmdaFetchCallBack)(pmdaMetric *, unsigned int, pmAtomValue *);

/*
 * return values for a pmdaFetchCallBack method
 */
#define PMDA_FETCH_NOVALUES	0
#define PMDA_FETCH_STATIC	1
#define PMDA_FETCH_DYNAMIC	2	/* free avp->vp after __pmStuffValue */

/*
 * Type of function call back used by pmdaMain to clean up a pmResult structure
 * after a fetch.
 */
typedef void (*pmdaResultCallBack)(pmResult *);

/*
 * Type of function call back used by pmdaMain on receipt of each PDU to check
 * availability, etc.
 */
typedef int (*pmdaCheckCallBack)(void);

/*
 * Type of function call back used by pmdaMain after each PDU has been
 * processed.
 */
typedef void (*pmdaDoneCallBack)(void);

/*
 * Type of function call back used by pmdaMain when a client context is
 * closed by PMCD.
 */
typedef void (*pmdaEndContextCallBack)(int);

/*
 * Type of function call back used by pmdaLabel for instance labels.
 */
typedef int (*pmdaLabelCallBack)(pmInDom, unsigned int, pmLabelSet **);


/*
 * libpcp_pmda extension structure.
 *
 * The fields of this structure are initialised using pmdaDaemon() or pmdaDSO()
 * (if the agent is a daemon or a DSO respectively), pmdaGetOpt() and
 * pmdaInit().
 */
typedef struct pmdaExt {

    unsigned int e_flags;	/* PMDA_EXT_FLAG_* bit field */
    void	*e_ext;		/* used internally within libpcp_pmda */

    char	*e_sockname;	/* socket name to pmcd */
    const char	*e_name;	/* name of this pmda */
    const char	*e_logfile;	/* path to log file */
    const char	*e_helptext;	/* path to help text */
    int		e_status;	/* =0 is OK */
    int		e_infd;		/* input file descriptor from pmcd */
    int		e_outfd;	/* output file descriptor to pmcd */
    int		e_port;		/* port to pmcd */
    int		e_singular;	/* =0 for singular values */
    int		e_ordinal;	/* >=0 for non-singular values */
    int		e_direct;	/* =1 if pmid map to meta table */
    int		e_domain;	/* metrics domain */
    int		e_nmetrics;	/* number of metrics */
    int		e_nindoms;	/* number of instance domains */
    int		e_help;		/* help text comes via this handle */
    pmProfile   *e_prof;	/* last received profile */
    pmdaIoType	e_io;		/* connection type to pmcd */
    pmdaIndom	*e_indoms;	/* instance domain table */
    pmdaIndom	*e_idp;		/* used in instance domain expansion */
    pmdaMetric	*e_metrics;	/* metric description table */

    pmdaResultCallBack	e_resultCallBack; /* callback to clean up pmResult after fetch */
    pmdaFetchCallBack	e_fetchCallBack;  /* callback to assign metric values in fetch */
    pmdaCheckCallBack	e_checkCallBack;  /* callback on receipt of a PDU */
    pmdaDoneCallBack	e_doneCallBack;   /* callback after PDU has been processed */

    /* added for PMDA_INTERFACE_5 */
    int		e_context;	/* client context id from pmcd */
    pmdaEndContextCallBack	e_endCallBack;	/* callback after client context closed */

    /* added for PMDA_INTERFACE_7 */
    pmdaLabelCallBack	e_labelCallBack; /* callback to lookup metric instance labels */
} pmdaExt;

#define PMDA_EXT_FLAG_DIRECT	(1<<0)	/* direct mapped PMID metric table */
#define PMDA_EXT_FLAG_HASHED	(1<<1)	/* hashed PMID metric table lookup */
#define PMDA_EXT_SETUPDONE	(1<<2)	/*  __pmdaSetup() has been called */
#define PMDA_EXT_CONNECTED	(1<<3)	/* pmdaConnect() done */
#define PMDA_EXT_NOTREADY	(1<<4)	/* pmcd connection marked NOTREADY */
#define PMDA_EXT_LABEL_CHANGE	(1<<5)	/* new label metadata notification */
#define PMDA_EXT_NAMES_CHANGE	(1<<6)	/* metric name change notification */

/*
 * Optionally restrict symbol visibility for DSO PMDAs
 *
 * When compiled with -fvisibility=hidden this directive can be used
 * to set up the init routine so that it is the only symbol exported
 * by the DSO PMDA.  This gives the compiler opportunity to generate
 * more optimal code as well as ensuring that just the one symbol is
 * exported (which is a good idea in itself).
 */
#if !defined(IS_MINGW)
# if defined(__GNUC__)
#  define __PMDA_INIT_CALL __attribute__ ((visibility ("default")))
# else
#  define __PMDA_INIT_CALL
# endif
# define PMDA_CALL
#else
# define __PMDA_INIT_CALL
# if defined(PMDA_INTERNAL)
#  define PMDA_CALL __declspec(dllexport)
# else
#  define PMDA_CALL __declspec(dllimport)
# endif
#endif

/*
 * Interface Definitions for PMDA DSO Interface
 * The new interface structure makes use of a union to manage new revisions
 * cleanly.  The structure for each new version must be backward compatible
 * to all of the previous versions (i.e. contain earlier fields unchanged).
 *
 * The domain field is set by pmcd(1) in the case of a DSO PMDA, and by
 * pmdaDaemon and pmdaGetOpt in the case of a Daemon PMDA. It should not be
 * modified.
 */
typedef struct pmdaInterface {
    int	domain;		/* performance metrics domain id */
    struct {
	unsigned int	pmda_interface : 8;	/* PMDA DSO interface version */
	unsigned int	pmapi_version : 8;	/* PMAPI version */
	unsigned int	flags : 16;		/* optional feature flags */
    } comm;		/* set/return communication and version info */
    int	status;		/* return initialization status here */

    union {

/*
 * Interface Version 2 and 3 (PCP 2.0)
 * PMDA_INTERFACE_2 and PMDA_INTERFACE_3
 */

	struct {
	    pmdaExt *ext;
	    int	    (*profile)(pmProfile *, pmdaExt *);
	    int	    (*fetch)(int, pmID *, pmResult **, pmdaExt *);
	    int	    (*desc)(pmID, pmDesc *, pmdaExt *);
	    int	    (*instance)(pmInDom, int, char *, pmInResult **, pmdaExt *);
	    int	    (*text)(int, int, char **, pmdaExt *);
	    int	    (*store)(pmResult *, pmdaExt *);
	} any, two, three;

/*
 * Interface Version 4 (dynamic pmns support) and Version 5 (client context
 * in PMDA).
 * PMDA_INTERFACE_4, PMDA_INTERFACE_5
 */

	struct {
	    pmdaExt *ext;
	    int	    (*profile)(pmProfile *, pmdaExt *);
	    int	    (*fetch)(int, pmID *, pmResult **, pmdaExt *);
	    int	    (*desc)(pmID, pmDesc *, pmdaExt *);
	    int	    (*instance)(pmInDom, int, char *, pmInResult **, pmdaExt *);
	    int	    (*text)(int, int, char **, pmdaExt *);
	    int	    (*store)(pmResult *, pmdaExt *);
	    int     (*pmid)(const char *, pmID *, pmdaExt *);
	    int     (*name)(pmID, char ***, pmdaExt *);
	    int     (*children)(const char *, int, char ***, int **, pmdaExt *);
	} four, five;

/*
 * Interface Version 6 (client context security attributes in PMDA).
 * PMDA_INTERFACE_6
 */
	struct {
	    pmdaExt *ext;
	    int	    (*profile)(pmProfile *, pmdaExt *);
	    int	    (*fetch)(int, pmID *, pmResult **, pmdaExt *);
	    int	    (*desc)(pmID, pmDesc *, pmdaExt *);
	    int	    (*instance)(pmInDom, int, char *, pmInResult **, pmdaExt *);
	    int	    (*text)(int, int, char **, pmdaExt *);
	    int	    (*store)(pmResult *, pmdaExt *);
	    int     (*pmid)(const char *, pmID *, pmdaExt *);
	    int     (*name)(pmID, char ***, pmdaExt *);
	    int     (*children)(const char *, int, char ***, int **, pmdaExt *);
	    int     (*attribute)(int, int, const char *, int, pmdaExt *);
	} six;

/*
 * Interface Version 7 (metric instance name:value labeling in PMDA).
 * PMDA_INTERFACE_7
 */
	struct {
	    pmdaExt *ext;
	    int	    (*profile)(pmProfile *, pmdaExt *);
	    int	    (*fetch)(int, pmID *, pmResult **, pmdaExt *);
	    int	    (*desc)(pmID, pmDesc *, pmdaExt *);
	    int	    (*instance)(pmInDom, int, char *, pmInResult **, pmdaExt *);
	    int	    (*text)(int, int, char **, pmdaExt *);
	    int	    (*store)(pmResult *, pmdaExt *);
	    int     (*pmid)(const char *, pmID *, pmdaExt *);
	    int     (*name)(pmID, char ***, pmdaExt *);
	    int     (*children)(const char *, int, char ***, int **, pmdaExt *);
	    int     (*attribute)(int, int, const char *, int, pmdaExt *);
	    int	    (*label)(int, int, pmLabelSet **, pmdaExt *);
	} seven;

    } version;

} pmdaInterface;

/* comm(unication) flags */
#define PMDA_FLAG_AUTHORIZE	(1<<2)	/* authentication support */
#define PMDA_FLAG_CONTAINER	(1<<6)	/* container name support */

/* communication attributes (mirrored from libpcp.h) */
#define PMDA_ATTR_USERNAME   5  /* username (sasl) */
#define PMDA_ATTR_USERID	11	/* uid - user identifier (posix) */
#define PMDA_ATTR_GROUPID	12	/* gid - group identifier (posix) */
#define PMDA_ATTR_PROCESSID	14	/* pid - process identifier (posix) */
#define PMDA_ATTR_CONTAINER	15	/* container name */

/*
 * PM_CONTEXT_LOCAL support
 */
typedef struct __pmDSO {
    int			domain;
    char		*name;
    char		*init;
    void		*handle;
    int			ctx_last_prof;	/* ctx that sent last profile */
    pmdaInterface	dispatch;
} __pmDSO;

/*
 * Macro that can be used to create each metrics' PMID.
 * cluster has a maximum value of 2^12-1
 * item has a maximum value of 2^10-1
 */
#define PMDA_PMID(cluster,item) ((((cluster)&0xfff)<<10)|((item)&0x3ff))

/* Macro for pmUnits bitmap in a pmDesc declaration */
#ifdef HAVE_BITFIELDS_LTOR
#define PMDA_PMUNITS(a,b,c,d,e,f) {a,b,c,d,e,f,0}
#else
#define PMDA_PMUNITS(a,b,c,d,e,f) {0,f,e,d,c,b,a}
#endif

/* Command line option processing macros and data structures */

#define PMDA_OPTIONS "D:d:h:i:l:pu:U:"
#define PMDA_OPTIONS_HEADER(s)	{ "", 0, '-', 0, (s) }
#define PMDA_OPTIONS_TEXT(s)	{ "", 0, '|', 0, (s) }
#define PMDA_OPTIONS_END	{ NULL, 0, 0, 0, NULL }

#define PMDAOPT_DEBUG	{ "debug",      1, 'D', "DBG", \
			NULL }
#define PMDAOPT_DOMAIN	{ "domain",	1, 'd', "NUM", \
			"use domain (numeric) for metrics domain of PMDA" }
#define PMDAOPT_HELPTEXT { "helpfile",	1, 'h', "FILE", \
			"path to PMDA metrics help text file" }
#define PMDAOPT_INET	{ "inet",	1, 'i', "PORT", \
			"pmcd IPv4 connection service name or numeric port" }
#define PMDAOPT_IPV6	{ "ipv6",	1, '6', "PORT", \
			"pmcd IPv6 connection service name or numeric port" }
#define PMDAOPT_LOGFILE	{ "log",	1, 'l', "FILE", \
			"write log to FILE rather than using default log name" }
#define PMDAOPT_PIPE	{ "pipe",	0, 'p', 0, \
			"use a pipe for communication with pmcd" }
#define PMDAOPT_UNIX	{ "unix",	1, 'u', "FILE", \
			"use a unix domain socket for communication with pmcd" }
#define PMDAOPT_USERNAME { "username",	1, 'U', "USER", \
			"run the PMDA using the named user account" }

struct pmdaOptions;
#define __pmdaOptions pmdaOptions /* backward-compatibility */
typedef int (*pmdaOptionOverride)(int, struct pmdaOptions *);

typedef struct pmdaOptions {
    int			version;
    int			flags;
    const char *	short_options;
    pmLongOptions *	long_options;
    const char *	short_usage;
    pmdaOptionOverride	override;

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
    int			errors;

    /* out: PMDA options (non-pmdaInterface options) */
    char *		username;
} pmdaOptions;


/*
 * PMDA Setup Routines.
 *
 * pmdaGetOpt
 * pmdaGetOptions
 * pmdaUsageMessage
 *	Replacements for pmgetopt_r(3) which strip out the standard PMDA flags
 *	before returning the next command line option.  The standard PMDA
 *	flags are PMDA_OPTIONS which will initialise the pmdaExt structure
 *	with the IPC details, path to the log and help file, domain number,
 *	and the user account under which the PMDA should run.
 *	An error counter will be incremented if there was an error parsing any
 *	of these options.
 *
 * pmdaDaemon
 *      Setup the pmdaInterface structure for a daemon and initialise
 *	the pmdaExt structure with the PMDA's name, domain and path to
 *	the log file and help file. The libpcp internal state is also
 *	initialised.
 *
 * pmdaDSO
 *      Setup the pmdaInterface structure for a DSO and initialise the
 *	pmdaExt structure with the PMDA's name and help file.
 *
 * pmdaOpenLog
 *	Redirects stderr to the logfile.
 *
 * pmdaSetFlags / pmdaExtSetFlags / pmdaSetCommFlags
 *      Allow behaviour flags to be set to enable features, such as to request
 *      libpcp_pmda internally use direct or hashed PMID metric table mapping.
 *      Can be called multiple times - effects are cumulative - no flag can be
 *      cleared, although libpcp_pmda may disable a flag later on if it cannot
 *      enact the requested behaviour.  Must be called before pmdaInit for any
 *      flags involving early setup (such as metric table hashing), otherwise
 *      can be called at any time (such as for namespace change notification).
 *
 * pmdaSetData / pmdaExtSetData / pmdaExtGetData
 *	Private data hook get/set for (esp. DSO) PMDAs that is accessible via
 *	the callbacks through the pmdaExt structure.
 *
 * pmdaInit
 *      Further initialises the pmdaExt structure with the instance domain and
 *      metric structures. Unique identifiers are applied to each instance 
 *	domain and metric. Also open the help text file and checks whether the 
 *	metrics can be directly mapped.
 *
 * pmdaConnect
 *      Connect to the PMCD process using the method set in the pmdaExt e_io
 *      field.
 *
 * pmdaMain
 *	Loop which receives PDUs and dispatches the callbacks. Must be called
 *	by a daemon PMDA.
 *
 * pmdaSendError
 *	Used to inform PMCD the PMDA is ready/notready to process requests.
 *	See pmcd(1) for details, in particular the protocol entry for the
 *	PMDA in pmcd.conf can specify "notready", in which case the PMDA
 *	must call pmdaSendError(dispatch, PM_ERR_PMDAREADY) when it is
 *	ready after starting up.
 *
 * pmdaSetResultCallBack
 *      Allows an application specific routine to be specified for cleaning up
 *      a pmResult after a fetch. Most PMDAs should not use this.
 *
 * pmdaSetFetchCallBack
 *      Allows an application specific routine to be specified for completing a
 *      pmAtom structure with a metrics value. This must be set if pmdaFetch is
 *      used as the fetch callback.
 *
 * pmdaSetCheckCallBack
 *      Allows an application specific routine to be called upon receipt of any
 *      PDU. For all PDUs except PDU_PROFILE, a result less than zero
 *      indicates an error. If set to zero (which is also the default),
 *      the callback is ignored.
 *
 * pmdaSetDoneCallBack
 *      Allows an application specific routine to be called after each PDU is
 *      processed. The result is ignored. If set to zero (which is also
 *      the default), the callback is ignored.
 *
 * pmdaSetEndContextCallBack
 *      Allows an application specific routine to be called when a
 *      PMCD context is closed, so any per-context state can be cleaned
 *      up.  If set to zero (which is also the default), the callback
 *      is ignored.
 *
 * pmdaSetLabelCallBack
 *	Lookup any metadata labels associated with metric instances.
 *	Passed in a metric table entry and instance identifier and expects
 *      the callback to fill the given labelset structure.
 */

PMDA_CALL extern int pmdaGetOpt(int, char *const *, const char *, pmdaInterface *, int *);
PMDA_CALL extern int pmdaGetOptions(int, char *const *, pmdaOptions *, pmdaInterface *);
PMDA_CALL extern void pmdaUsageMessage(pmdaOptions *);
PMDA_CALL extern void pmdaDaemon(pmdaInterface *, int, const char *, int , const char *, const char *);
PMDA_CALL extern void pmdaDSO(pmdaInterface *, int, char *, char *);
PMDA_CALL extern void pmdaOpenLog(pmdaInterface *);
PMDA_CALL extern void *pmdaExtGetData(pmdaExt *);
PMDA_CALL extern void pmdaExtSetData(pmdaExt *, void *);
PMDA_CALL extern void pmdaSetData(pmdaInterface *, void *);
PMDA_CALL extern void pmdaExtSetFlags(pmdaExt *, int);
PMDA_CALL extern void pmdaSetFlags(pmdaInterface *, int);
PMDA_CALL extern void pmdaSetCommFlags(pmdaInterface *, int);
PMDA_CALL extern void pmdaInit(pmdaInterface *, pmdaIndom *, int, pmdaMetric *, int);
PMDA_CALL extern void pmdaConnect(pmdaInterface *);

PMDA_CALL extern void pmdaMain(pmdaInterface *);
PMDA_CALL extern void pmdaSendError(pmdaInterface *, int);

PMDA_CALL extern void pmdaSetResultCallBack(pmdaInterface *, pmdaResultCallBack);
PMDA_CALL extern void pmdaSetFetchCallBack(pmdaInterface *, pmdaFetchCallBack);
PMDA_CALL extern void pmdaSetCheckCallBack(pmdaInterface *, pmdaCheckCallBack);
PMDA_CALL extern void pmdaSetDoneCallBack(pmdaInterface *, pmdaDoneCallBack);
PMDA_CALL extern void pmdaSetEndContextCallBack(pmdaInterface *, pmdaEndContextCallBack);
PMDA_CALL extern void pmdaSetLabelCallBack(pmdaInterface *, pmdaLabelCallBack);

/*
 * Callbacks to PMCD which should be adequate for most PMDAs.
 * NOTE: if pmdaFetch is used, e_callback must be specified in the pmdaExt
 *       structure.
 *
 * pmdaProfile
 *	Store the instance profile away for the next fetch.
 *
 * pmdaFetch
 *	Resize the pmResult and call e_callback in the pmdaExt structure
 *	for each metric instance required by the profile.
 *
 * pmdaInstance
 *	Return description of instances and instance domains.
 *
 * pmdaDesc
 *	Return the metric description.
 *
 * pmdaText
 *	Return the help text for the metric or instance domain.
 *
 * pmdaStore
 *	Store a value into a metric. This sets the context number.
 *
 * pmdaPMID
 *	Return the PMID for a named metric within a dynamic subtree
 *	of the PMNS.
 *
 * pmdaName
 *	Given a PMID, return the names of all matching metrics within a
 *	dynamic subtree of the PMNS.
 *
 * pmdaChildren
 *	If traverse == 0, return the names of all the descendent children
 *      and their status, given a named metric in a dynamic subtree of
 *	the PMNS (this is the pmGetChildren or pmGetChildrenStatus variant).
 *	If traverse == 1, return the full names of all descendent metrics
 *	(this is the pmTraversePMNS variant, with the status added)
 *
 * pmdaAttribute
 *	Inform the agent about security aspects of a client connection,
 *	such as the authenticated userid.  Passed in a client identifier,
 *	numeric PCP_ATTR, pointer to the associated value, and the length
 *	of the value.
 *
 * pmdaLabel
 *	Return labels for contexts, domains, metrics, indoms or instances.
 *	Passed in a type (PM_LABEL) and identifier (domain, metric, indom);
 *      fills in labelsets, returns a count thereof or negative error code.
 */

PMDA_CALL extern int pmdaProfile(pmProfile *, pmdaExt *);
PMDA_CALL extern int pmdaFetch(int, pmID *, pmResult **, pmdaExt *);
PMDA_CALL extern int pmdaInstance(pmInDom, int, char *, pmInResult **, pmdaExt *);
PMDA_CALL extern int pmdaDesc(pmID, pmDesc *, pmdaExt *);
PMDA_CALL extern int pmdaText(int, int, char **, pmdaExt *);
PMDA_CALL extern int pmdaStore(pmResult *, pmdaExt *);
PMDA_CALL extern int pmdaPMID(const char *, pmID *, pmdaExt *);
PMDA_CALL extern int pmdaName(pmID, char ***, pmdaExt *);
PMDA_CALL extern int pmdaChildren(const char *, int, char ***, int **, pmdaExt *);
PMDA_CALL extern int pmdaAttribute(int, int, const char *, int, pmdaExt *);
PMDA_CALL extern int pmdaLabel(int, int, pmLabelSet **, pmdaExt *);

/*
 * PMDA "help" text manipulation
 */
PMDA_CALL extern int pmdaOpenHelp(const char *);
PMDA_CALL extern void pmdaCloseHelp(int);
PMDA_CALL extern char *pmdaGetHelp(int, pmID, int);
PMDA_CALL extern char *pmdaGetInDomHelp(int, pmInDom, int);

/*
 * PMDA "label" metadata (name:value pairs) manipulation
 */
PMDA_CALL extern int pmdaAddLabels(pmLabelSet **, const char *, ...) __PM_PRINTFLIKE(2,3);
PMDA_CALL extern int pmdaAddNotes(pmLabelSet **, const char *, ...) __PM_PRINTFLIKE(2,3);
PMDA_CALL extern int pmdaAddLabelFlags(pmLabelSet *, int);

/*
 * Dynamic metric table manipulation
 *
 * pmdaExtDynamicPMNS / pmdaDynamicPMNS
 *	Register a new dynamic namespace sub-tree associated with one or more
 *	PMID clusters.  Callbacks are passed in to deal with PMDA-specific
 *	components (names, help text, metric duplication, and table sizing).
 *	pmdaExtDynamicPMNS is preferred as it works in both DSO/daemon modes;
 *	however pmdaDynamicPMNS is maintained for backward compatibility.
 *
 * pmdaDynamicLookupName
 *	Perform PMDA name lookup operations for the name callback, for dynamic
 *	metrics.
 *
 * pmdaDynamicLookupPMID
 *	Perform PMDA reverse name lookup operations for the PMID callback, for
 *	dynamic metrics.
 *
 * pmdaDynamicLookupText
 *	Perform PMDA help text lookup operations for dynamic metrics.
 *
 * pmdaDynamicMetricTable
 *	Install a new metric table for the PMDA, after changes to the set of
 *	metrics which the PMDA must export (IOW, dynamic metrics are in use).
 *
 * pmdaRehash
 *      Update the metrictable within the pmdaExt structure with new (dynamic)
 *      metrics and recompute the hash table used for optimised lookup.  Aids
 *      PMDAs with large numbers of metrics to get closer to directly mapped
 *      PMID lookup time, rather than multiple linear table scans per fetch.
 *      [NOTE: can be used by any interface version, not only dynamic metrics]
 */
typedef int  (*pmdaUpdatePMNS)(pmdaExt *, pmdaNameSpace **);
typedef int  (*pmdaUpdateText)(pmdaExt *, pmID, int, char **);
typedef void (*pmdaUpdateMetric)(pmdaMetric *, pmdaMetric *, int);
typedef void (*pmdaCountMetrics)(int *, int *);
PMDA_CALL extern void pmdaExtDynamicPMNS(const char *, int *, int,
                                     pmdaUpdatePMNS, pmdaUpdateText,
                                     pmdaUpdateMetric, pmdaCountMetrics,
                                     pmdaMetric *, int, pmdaExt *);
PMDA_CALL extern void pmdaDynamicPMNS(const char *, int *, int,
                                     pmdaUpdatePMNS, pmdaUpdateText,
                                     pmdaUpdateMetric, pmdaCountMetrics,
                                     pmdaMetric *, int);
PMDA_CALL extern int pmdaDynamicSetClusterMask(const char *, unsigned int);
PMDA_CALL extern pmdaNameSpace *pmdaDynamicLookupName(pmdaExt *, const char *);
PMDA_CALL extern pmdaNameSpace *pmdaDynamicLookupPMID(pmdaExt *, pmID);
PMDA_CALL extern int pmdaDynamicLookupText(pmID, int, char **, pmdaExt *);
PMDA_CALL extern void pmdaDynamicMetricTable(pmdaExt *);

PMDA_CALL extern void pmdaRehash(pmdaExt *, pmdaMetric *, int);

/*
 * Dynamic names interface (version 4) helper routines.
 *
 * pmdaTreePMID
 *	when a pmdaNameSpace is being used, this provides
 *	an implementation for the four.pmid() interface.
 *
 * pmdaTreeName
 *	when a pmdaNameSpace is being used, this provides
 *	an implementation for the four.name() interface.
 *
 * pmdaTreeChildren
 *	when a pmdaNameSpace is being used, this provides
 *	an implementation for the four.children() interface.
 *
 * pmdaTreeRebuildHash
 *	iterate over a pmdaNameSpace and (re)build the hash
 *	for any subsequent PMID -> name (reverse) lookups
 *
 * pmdaTreeSize
 *	returns the numbers of entries in a pmdaNameSpace.
 */
PMDA_CALL extern int pmdaTreePMID(pmdaNameSpace *, const char *, pmID *);
PMDA_CALL extern int pmdaTreeName(pmdaNameSpace *, pmID, char ***);
PMDA_CALL extern int pmdaTreeChildren(pmdaNameSpace *, const char *, int, char ***, int **);
PMDA_CALL extern void pmdaTreeRebuildHash(pmdaNameSpace *, int);
PMDA_CALL extern int pmdaTreeSize(pmdaNameSpace *);

PMDA_CALL extern int pmdaTreeCreate(pmdaNameSpace **);
PMDA_CALL extern int pmdaTreeInsert(pmdaNameSpace *, pmID, const char *);
PMDA_CALL extern void pmdaTreeRelease(pmdaNameSpace *);

/*
 * PMDA instance domain cache support
 *
 * pmdaCacheStore
 * 	add entry into the cache, or change state, assigns internal
 * 	instance identifier
 *
 * pmdaCacheStoreKey
 * 	add entry into the cache, or change state, caller provides "hint"
 * 	for internal instance identifier
 *
 * pmdaCacheLookup
 *	fetch entry based on internal instance identifier
 *
 * pmdaCacheLookupName
 *	fetch entry based on external instance name
 *
 * pmdaCacheLookupKey
 *	fetch entry based on key as "hint", like pmdaCacheStoreKey()
 *
 * pmdaCacheOp
 *	service routines to load, unload, mark as write-thru, purge,
 *	count entries, etc
 * 
 * pmdaCachePurge
 *	cull inactive entries
 *
 * pmdaCachePurgeCallback
 *	cull inactive entries and invoke callback for private data
 *
 * pmdaCacheResize
 *	set the maximum instance identifier
 */
PMDA_CALL extern int pmdaCacheStore(pmInDom, int, const char *, void *);
PMDA_CALL extern int pmdaCacheStoreKey(pmInDom, int, const char *, int, const void *, void *);
PMDA_CALL extern int pmdaCacheLookup(pmInDom, int, char **, void **);
PMDA_CALL extern int pmdaCacheLookupName(pmInDom, const char *, int *, void **);
PMDA_CALL extern int pmdaCacheLookupKey(pmInDom, const char *, int, const void *, char **, int *, void **);
PMDA_CALL extern int pmdaCacheOp(pmInDom, int);
PMDA_CALL extern int pmdaCachePurge(pmInDom, time_t);
PMDA_CALL extern int pmdaCachePurgeCallback(pmInDom, time_t, void (*)(void *));
PMDA_CALL extern int pmdaCacheResize(pmInDom, int);

#define PMDA_CACHE_LOAD			1
#define PMDA_CACHE_ADD			2
#define PMDA_CACHE_HIDE			3
#define PMDA_CACHE_CULL			4
#define PMDA_CACHE_EMPTY		5
#define PMDA_CACHE_SAVE			6
#define PMDA_CACHE_STRINGS		7
#define PMDA_CACHE_ACTIVE		8
#define PMDA_CACHE_INACTIVE		9
#define PMDA_CACHE_SIZE			10
#define PMDA_CACHE_SIZE_ACTIVE		11
#define PMDA_CACHE_SIZE_INACTIVE	12
#define PMDA_CACHE_REUSE		13
#define PMDA_CACHE_WALK_REWIND		14
#define PMDA_CACHE_WALK_NEXT		15
#define PMDA_CACHE_CHECK		16
#define PMDA_CACHE_REORG		17
#define PMDA_CACHE_SYNC			18
#define PMDA_CACHE_DUMP			19
#define PMDA_CACHE_DUMP_ALL		20
#define PMDA_CACHE_WRITE		21

/*
 * Internal libpcp_pmda routines.
 *
 * __pmdaCntInst
 *	Returns the number of instances for an entry in the instance domain
 *	table.
 *
 * __pmdaStartInst
 *	Setup for __pmdaNextInst to iterate over an instance domain.
 *
 * __pmdaNextInst
 *	Step through an instance domain, returning instances one at a
 *	time.
 *
 * __pmdaMainPDU
 *	Use this when you need to override pmdaMain and construct
 *      your own loop.
 *	Call this function in the _body_ of your loop.
 *	See pmdaMain code for an example.
 *	Returns negative int on failure, 0 otherwise.
 *
 * __pmdaInFd
 *	This returns the file descriptor that is used to get the
 *	PDU from pmcd.	
 *	One may use the fd to do a select call in a custom main loop.
 *	Returns negative int on failure, file descriptor otherwise.
 *
 * __pmdaCacheDumpAll and __pmdaCacheDump
 *	print out cache contents
 */

PMDA_CALL extern int __pmdaCntInst(pmInDom, pmdaExt *);
PMDA_CALL extern void __pmdaStartInst(pmInDom, pmdaExt *);
PMDA_CALL extern int __pmdaNextInst(int *, pmdaExt *);

PMDA_CALL extern int __pmdaMainPDU(pmdaInterface *);
PMDA_CALL extern int __pmdaInFd(pmdaInterface *);

PMDA_CALL extern void __pmdaCacheDumpAll(FILE *, int);
PMDA_CALL extern void __pmdaCacheDump(FILE *, pmInDom, int);

/*
 * Client Context support
 */
PMDA_CALL extern int pmdaGetContext(void);
PMDA_CALL extern void __pmdaSetContext(int);

/*
 * Event Record support
 */
PMDA_CALL extern int pmdaEventNewArray(void);
PMDA_CALL extern int pmdaEventResetArray(int);
PMDA_CALL extern int pmdaEventReleaseArray(int);
PMDA_CALL extern int pmdaEventAddRecord(int, struct timeval *, int);
PMDA_CALL extern int pmdaEventAddMissedRecord(int, struct timeval *, int);
PMDA_CALL extern int pmdaEventAddParam(int, pmID, int, pmAtomValue *);
PMDA_CALL extern pmEventArray *pmdaEventGetAddr(int);

/*
 * High Resolution Timer Event Record support
 */
PMDA_CALL extern int pmdaEventNewHighResArray(void);
PMDA_CALL extern int pmdaEventResetHighResArray(int);
PMDA_CALL extern int pmdaEventReleaseHighResArray(int);
PMDA_CALL extern int pmdaEventAddHighResRecord(int, struct timespec *, int);
PMDA_CALL extern int pmdaEventAddHighResMissedRecord(int, struct timespec *, int);
PMDA_CALL extern int pmdaEventAddHighResParam(int, pmID, int, pmAtomValue *);
PMDA_CALL extern pmHighResEventArray *pmdaEventGetHighResAddr(int);
/* old names retained for backwards compatibility */
PMDA_CALL extern int pmdaEventHighResAddParam(int, pmID, int, pmAtomValue *);
PMDA_CALL extern pmHighResEventArray *pmdaEventHighResGetAddr(int);

/*
 * Event Queue support
 */
PMDA_CALL extern int pmdaEventNewQueue(const char *, size_t);
PMDA_CALL extern int pmdaEventNewActiveQueue(const char *, size_t, unsigned int);
PMDA_CALL extern int pmdaEventQueueShutdown(int);
PMDA_CALL extern int pmdaEventQueueHandle(const char *);
PMDA_CALL extern int pmdaEventQueueAppend(int, void *, size_t, struct timeval *);
PMDA_CALL extern int pmdaEventQueueClients(int, pmAtomValue *);
PMDA_CALL extern int pmdaEventQueueCounter(int, pmAtomValue *);
PMDA_CALL extern int pmdaEventQueueBytes(int, pmAtomValue *);
PMDA_CALL extern int pmdaEventQueueMemory(int, pmAtomValue *);

typedef int (*pmdaEventDecodeCallBack)(int,
		void *, size_t, struct timeval *, void *);
PMDA_CALL extern int pmdaEventQueueRecords(int, pmAtomValue *, int,
		pmdaEventDecodeCallBack, void *);

PMDA_CALL extern int pmdaEventNewClient(int);
PMDA_CALL extern int pmdaEventEndClient(int);
PMDA_CALL extern int pmdaEventClients(pmAtomValue *);

typedef int (*pmdaEventApplyFilterCallBack)(void *, void *, size_t);
typedef void (*pmdaEventReleaseFilterCallBack)(void *);
PMDA_CALL extern int pmdaEventSetFilter(int, int, void *,
		pmdaEventApplyFilterCallBack, pmdaEventReleaseFilterCallBack);
PMDA_CALL extern int pmdaEventSetAccess(int, int, int);

PMDA_CALL extern char *__pmdaEventPrint(const char *, int, char *, int);

PMDA_CALL extern void pmdaInterfaceMoved(pmdaInterface *);

/*
 * Privileged PMDA services, as offered by pmdaroot(1).
 */
PMDA_CALL extern int pmdaRootConnect(const char *);
PMDA_CALL extern void pmdaRootShutdown(int);
PMDA_CALL extern int pmdaRootContainerHostName(int, const char *, int, char *, int);
PMDA_CALL extern int pmdaRootContainerProcessID(int, const char *, int);
PMDA_CALL extern int pmdaRootContainerCGroupName(int, const char *, int, char *, int);
PMDA_CALL extern int pmdaRootProcessStart(int, int, const char *, int labellen,
			const char *, int, int *, int *, int *);
PMDA_CALL extern int pmdaRootProcessWait(int, int, int *);
PMDA_CALL extern int pmdaRootProcessTerminate(int, int);

#ifdef __cplusplus
}
#endif

#endif /* PCP_PMDA_H */
