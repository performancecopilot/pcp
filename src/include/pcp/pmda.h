/*
 * Copyright (c) 2013-2015 Red Hat.
 * Copyright (c) 1995,2005 Silicon Graphics, Inc.  All Rights Reserved.
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
#define PMDA_INTERFACE_6	6	/* client security attributes in pmda */
#define PMDA_INTERFACE_LATEST	6

/*
 * Type of I/O connection to PMCD (pmdaUnknown defaults to pmdaPipe)
 */
typedef enum {pmdaPipe, pmdaInet, pmdaUnix, pmdaUnknown, pmdaIPv6} pmdaIoType;

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
 * Forward declarations of structures so that inclusion of (internal) impl.h
 * header file is not mandated if this header file is included.
 */
typedef struct __pmnsTree  pmdaNameSpace;
typedef struct __pmHashCtl pmdaHashTable;
typedef struct __pmProfile pmdaInProfile;
typedef struct __pmInResult pmdaInResult;
typedef struct __pmnsNode pmnsNode;

/*
 * libpcp_pmda extension structure.
 *
 * The fields of this structure are initialised using pmdaDaemon() or pmdaDSO()
 * (if the agent is a daemon or a DSO respectively), pmdaGetOpt() and
 * pmdaInit().
 * 
 */
typedef struct pmdaExt {

    unsigned int e_flags;	/* PMDA_EXT_FLAG_* bit field */
    void	*e_ext;		/* used internally within libpcp_pmda */

    char	*e_sockname;	/* socket name to pmcd */
    char	*e_name;	/* name of this pmda */
    char	*e_logfile;	/* path to log file */
    char	*e_helptext;	/* path to help text */		    
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
    pmdaInProfile *e_prof;	/* last received profile */
    pmdaIoType	e_io;		/* connection type to pmcd */
    pmdaIndom	*e_indoms;	/* instance domain table */
    pmdaIndom	*e_idp;		/* used in instance domain expansion */
    pmdaMetric	*e_metrics;	/* metric description table */

    pmdaResultCallBack e_resultCallBack; /* callback to clean up pmResult after fetch */
    pmdaFetchCallBack  e_fetchCallBack;  /* callback to assign metric values in fetch */
    pmdaCheckCallBack  e_checkCallBack;  /* callback on receipt of a PDU */
    pmdaDoneCallBack   e_doneCallBack;   /* callback after PDU has been processed */
    /* added for PMDA_INTERFACE_5 */
    int		e_context;	/* client context id from pmcd */
    pmdaEndContextCallBack	e_endCallBack;	/* callback after client context closed */
} pmdaExt;

#define PMDA_EXT_FLAG_DIRECT	0x01	/* direct mapped PMID metric table */
#define PMDA_EXT_FLAG_HASHED	0x02	/* hashed PMID metric table lookup */
#define PMDA_EXT_SETUPDONE	0x04	/* __pmdaSetup() has been called */
#define PMDA_EXT_CONNECTED	0x08	/* pmdaConnect() done */
#define PMDA_EXT_NOTREADY	0x10	/* pmcd connection marked NOTREADY */

/*
 * Optionally restrict symbol visibility for DSO PMDAs
 *
 * When compiled with -fvisibility=hidden this directive can be used
 * to set up the init routine so that it is the only symbol exported
 * by the DSO PMDA.  This gives the compiler opportunity to generate
 * more optimal code as well as ensuring that just the one symbol is
 * exported (which is a good idea in itself).
 */
#ifdef __GNUC__
# define __PMDA_INIT_CALL __attribute__ ((visibility ("default")))
#else
# define __PMDA_INIT_CALL
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
	    int	    (*profile)(pmdaInProfile *, pmdaExt *);
	    int	    (*fetch)(int, pmID *, pmResult **, pmdaExt *);
	    int	    (*desc)(pmID, pmDesc *, pmdaExt *);
	    int	    (*instance)(pmInDom, int, char *, pmdaInResult **, pmdaExt *);
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
	    int	    (*profile)(pmdaInProfile *, pmdaExt *);
	    int	    (*fetch)(int, pmID *, pmResult **, pmdaExt *);
	    int	    (*desc)(pmID, pmDesc *, pmdaExt *);
	    int	    (*instance)(pmInDom, int, char *, pmdaInResult **, pmdaExt *);
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
	    int	    (*profile)(pmdaInProfile *, pmdaExt *);
	    int	    (*fetch)(int, pmID *, pmResult **, pmdaExt *);
	    int	    (*desc)(pmID, pmDesc *, pmdaExt *);
	    int	    (*instance)(pmInDom, int, char *, pmdaInResult **, pmdaExt *);
	    int	    (*text)(int, int, char **, pmdaExt *);
	    int	    (*store)(pmResult *, pmdaExt *);
	    int     (*pmid)(const char *, pmID *, pmdaExt *);
	    int     (*name)(pmID, char ***, pmdaExt *);
	    int     (*children)(const char *, int, char ***, int **, pmdaExt *);
	    int     (*attribute)(int, int, const char *, int, pmdaExt *);
	} six;

    } version;

} pmdaInterface;

/*
 * PM_CONTEXT_LOCAL support
 */
typedef struct __pmDSO {
    int			domain;
    char		*name;
    char		*init;
    void		*handle;
    pmdaInterface	dispatch;
} __pmDSO;

extern __pmDSO *__pmLookupDSO(int /*domain*/);

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
 * pmdaSetFlags
 *      Allow behaviour flags to be set to enable features, such as to request
 *      libpcp_pmda internally use direct or hashed PMID metric table mapping.
 *      Can be called multiple times - effects are cumulative - no flag can be
 *      cleared, although libpcp_pmda may disable a flag later on if it cannot
 *      enact the requested behaviour.
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
 */

extern int pmdaGetOpt(int, char *const *, const char *, pmdaInterface *, int *);
extern int pmdaGetOptions(int, char *const *, pmdaOptions *, pmdaInterface *);
extern void pmdaUsageMessage(pmdaOptions *);
extern void pmdaDaemon(pmdaInterface *, int, char *, int , char *, char *);
extern void pmdaDSO(pmdaInterface *, int, char *, char *);
extern void pmdaOpenLog(pmdaInterface *);
extern void pmdaSetFlags(pmdaInterface *, int);
extern void pmdaInit(pmdaInterface *, pmdaIndom *, int, pmdaMetric *, int);
extern void pmdaConnect(pmdaInterface *);

extern void pmdaMain(pmdaInterface *);

extern void pmdaSetResultCallBack(pmdaInterface *, pmdaResultCallBack);
extern void pmdaSetFetchCallBack(pmdaInterface *, pmdaFetchCallBack);
extern void pmdaSetCheckCallBack(pmdaInterface *, pmdaCheckCallBack);
extern void pmdaSetDoneCallBack(pmdaInterface *, pmdaDoneCallBack);
extern void pmdaSetEndContextCallBack(pmdaInterface *, pmdaEndContextCallBack);

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
 *	Return the metric desciption.
 *
 * pmdaText
 *	Return the help text for the metric.
 *
 * pmdaStore
 *	Store a value into a metric. This is a no-op.
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
 */

extern int pmdaProfile(pmdaInProfile *, pmdaExt *);
extern int pmdaFetch(int, pmID *, pmResult **, pmdaExt *);
extern int pmdaInstance(pmInDom, int, char *, pmdaInResult **, pmdaExt *);
extern int pmdaDesc(pmID, pmDesc *, pmdaExt *);
extern int pmdaText(int, int, char **, pmdaExt *);
extern int pmdaStore(pmResult *, pmdaExt *);
extern int pmdaPMID(const char *, pmID *, pmdaExt *);
extern int pmdaName(pmID, char ***, pmdaExt *);
extern int pmdaChildren(const char *, int, char ***, int **, pmdaExt *);
extern int pmdaAttribute(int, int, const char *, int, pmdaExt *);

/*
 * PMDA "help" text manipulation
 */
extern int pmdaOpenHelp(char *);
extern void pmdaCloseHelp(int);
extern char *pmdaGetHelp(int, pmID, int);
extern char *pmdaGetInDomHelp(int, pmInDom, int);

/*
 * Dynamic metric table manipulation
 *
 * pmdaDynamicPMNS
 *	Register a new dynamic namespace sub-tree associated with one or more
 *	PMID clusters.  Callbacks are passed in to deal with PMDA-specific
 *	components (names, help text, metric duplication, and table sizing).
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
extern void pmdaDynamicPMNS(const char *, int *, int,
                            pmdaUpdatePMNS, pmdaUpdateText,
                            pmdaUpdateMetric, pmdaCountMetrics,
                            pmdaMetric *, int);
extern int pmdaDynamicSetClusterMask(const char *, unsigned int);
extern pmdaNameSpace *pmdaDynamicLookupName(pmdaExt *, const char *);
extern pmdaNameSpace *pmdaDynamicLookupPMID(pmdaExt *, pmID);
extern int pmdaDynamicLookupText(pmID, int, char **, pmdaExt *);
extern void pmdaDynamicMetricTable(pmdaExt *);

extern void pmdaRehash(pmdaExt *, pmdaMetric *, int);

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
extern int pmdaTreePMID(pmdaNameSpace *, const char *, pmID *);
extern int pmdaTreeName(pmdaNameSpace *, pmID, char ***);
extern int pmdaTreeChildren(pmdaNameSpace *, const char *, int, char ***, int **);
extern void pmdaTreeRebuildHash(pmdaNameSpace *, int);
extern int pmdaTreeSize(pmdaNameSpace *);
extern pmnsNode * pmdaNodeLookup(pmnsNode *, const char *);

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
 * pmdaCacheResize
 *	set the maximum instance identifier
 */
extern int pmdaCacheStore(pmInDom, int, const char *, void *);
extern int pmdaCacheStoreKey(pmInDom, int, const char *, int, const void *, void *);
extern int pmdaCacheLookup(pmInDom, int, char **, void **);
extern int pmdaCacheLookupName(pmInDom, const char *, int *, void **);
extern int pmdaCacheLookupKey(pmInDom, const char *, int, const void *, char **, int *, void **);
extern int pmdaCacheOp(pmInDom, int);
extern int pmdaCachePurge(pmInDom, time_t);
extern int pmdaCacheResize(pmInDom, int);

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

extern int __pmdaCntInst(pmInDom, pmdaExt *);
extern void __pmdaStartInst(pmInDom, pmdaExt *);
extern int __pmdaNextInst(int *, pmdaExt *);

extern int __pmdaMainPDU(pmdaInterface *);
extern int __pmdaInFd(pmdaInterface *);

extern void __pmdaCacheDumpAll(FILE *, int);
extern void __pmdaCacheDump(FILE *, pmInDom, int);

/*
 * Client Context support
 */
extern int pmdaGetContext(void);
extern void __pmdaSetContext(int);

/*
 * Event Record support
 */
extern int pmdaEventNewArray(void);
extern int pmdaEventResetArray(int);
extern int pmdaEventReleaseArray(int);
extern int pmdaEventAddRecord(int, struct timeval *, int);
extern int pmdaEventAddMissedRecord(int, struct timeval *, int);
extern int pmdaEventAddParam(int, pmID, int, pmAtomValue *);
extern pmEventArray *pmdaEventGetAddr(int);

/*
 * High Resolution Timer Event Record support
 */
extern int pmdaEventNewHighResArray(void);
extern int pmdaEventResetHighResArray(int);
extern int pmdaEventReleaseHighResArray(int);
extern int pmdaEventAddHighResRecord(int, struct timespec *, int);
extern int pmdaEventAddHighResMissedRecord(int, struct timespec *, int);
extern int pmdaEventHighResAddParam(int, pmID, int, pmAtomValue *);
extern pmHighResEventArray *pmdaEventHighResGetAddr(int);

/*
 * Event Queue support
 */
extern int pmdaEventNewQueue(const char *, size_t);
extern int pmdaEventNewActiveQueue(const char *, size_t, unsigned int);
extern int pmdaEventQueueHandle(const char *);
extern int pmdaEventQueueAppend(int, void *, size_t, struct timeval *);
extern int pmdaEventQueueClients(int, pmAtomValue *);
extern int pmdaEventQueueCounter(int, pmAtomValue *);
extern int pmdaEventQueueBytes(int, pmAtomValue *);
extern int pmdaEventQueueMemory(int, pmAtomValue *);

typedef int (*pmdaEventDecodeCallBack)(int,
		void *, size_t, struct timeval *, void *);
extern int pmdaEventQueueRecords(int, pmAtomValue *, int,
		pmdaEventDecodeCallBack, void *);

extern int pmdaEventNewClient(int);
extern int pmdaEventEndClient(int);
extern int pmdaEventClients(pmAtomValue *);

typedef int (*pmdaEventApplyFilterCallBack)(void *, void *, size_t);
typedef void (*pmdaEventReleaseFilterCallBack)(void *);
extern int pmdaEventSetFilter(int, int, void *,
		pmdaEventApplyFilterCallBack, pmdaEventReleaseFilterCallBack);
extern int pmdaEventSetAccess(int, int, int);

extern char *__pmdaEventPrint(const char *, int, char *, int);

extern void pmdaInterfaceMoved(pmdaInterface *);

/*
 * Privileged PMDA services, as offered by pmdaroot(1).
 */
extern int pmdaRootConnect(const char *);
extern void pmdaRootShutdown(int);
extern int pmdaRootContainerHostName(int, const char *, int, char *, int);
extern int pmdaRootContainerProcessID(int, const char *, int);
extern int pmdaRootContainerCGroupName(int, const char *, int, char *, int);

/*
 * Local PDU exchange details for elevated privilege operations.
 * Only the PMDAs and pmcd need to know about this.
 */
#define ROOT_PDU_VERSION1	1
#define ROOT_PDU_VERSION	ROOT_PDU_VERSION1

#define PDUROOT_INFO		0x9000
#define PDUROOT_HOSTNAME_REQ	0x9001
#define PDUROOT_HOSTNAME	0x9002
#define PDUROOT_PROCESSID_REQ	0x9003
#define PDUROOT_PROCESSID	0x9004
#define PDUROOT_CGROUPNAME_REQ	0x9005
#define PDUROOT_CGROUPNAME	0x9006
/*#define PDUROOT_STARTPMDA_REQ	0x9007*/
/*#define PDUROOT_STARTPMDA	0x9008*/
/*#define PDUROOT_SASLAUTH_REQ	0x9009*/
/*#define PDUROOT_SASLAUTH	0x900a*/

typedef enum {
    PDUROOT_FLAG_HOSTNAME	= (1<<0),
    PDUROOT_FLAG_PROCESSID	= (1<<1),
    PDUROOT_FLAG_CGROUPNAME	= (1<<2),
} __pmdaRootServerFeature;

typedef struct {
    int		type;
    int		length;
    int		status;
    int		version;
} __pmdaRootPDUHdr;

extern int __pmdaSendRootPDUInfo(int, int, int);
extern int __pmdaRecvRootPDUInfo(int, int *, int *);
extern int __pmdaSendRootPDUContainer(int, int, int, const char *, int, int);
extern int __pmdaRecvRootPDUContainer(int, int, void *, int);
extern int __pmdaDecodeRootPDUContainer(void *, int, int *, char *, int);

#ifdef __cplusplus
}
#endif

#endif /* PCP_PMDA_H */
