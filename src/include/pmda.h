#ifndef _PMDA_H
#define _PMDA_H

/*
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * libpcp_pmda interface versions ... _must_ be bit field values
 */
#define PMDA_INTERFACE_1	1	/* initial argument style */
#define PMDA_INTERFACE_2	2	/* new function arguments */
#define PMDA_INTERFACE_3	3	/* 3-state return from fetch callback */
#define PMDA_INTERFACE_4	4	/* dynamic pmns */
#define PMDA_INTERFACE_LATEST	4

/*
 * Type of I/O connection to PMCD (pmdaUnknown defaults to pmdaPipe)
 */
typedef enum {pmdaPipe, pmdaInet, pmdaUnix, pmdaUnknown} pmdaIoType;

/*
 * Instance description: index and name
 */
typedef struct {
    int		i_inst;		/* internal instance identifier */
    char	*i_name;	/* external instance identifier */
} pmdaInstid;

/*
 * Instance domain description: unique instance id, number of instances in
 * this domain, and the list of instances (not null terminated).
 */
typedef struct {
    pmInDom	it_indom;	/* indom, filled in */
    int		it_numinst;	/* number of instances */
    pmdaInstid	*it_set;	/* instance identifiers */
} pmdaIndom;

/*
 * Metric description: handle for extending description, and the description.
 */
typedef struct {
    void	*m_user;	/* for users external use */
    pmDesc	m_desc;		/* metric description */
} pmdaMetric;

/*
 * Type of function call back used by pmdaFetch.
 */
typedef int (*pmdaFetchCallBack)(pmdaMetric *, unsigned int, pmAtomValue *);

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
 * libpcp_pmda extension structure.
 *
 * The fields of this structure are initialised using pmdaDaemon() or pmdaDSO()
 * (if the agent is a daemon or a DSO respectively), pmdaGetOpt() and
 * pmdaInit().
 * 
 */
typedef struct {

    unsigned int e_flags;	/* usage TBD */
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
    __pmProfile	*e_prof;	/* last received profile */
    pmdaIoType	e_io;		/* connection type to pmcd */
    pmdaIndom	*e_indoms;	/* instance domain table */
    pmdaIndom	*e_idp;		/* used in instance domain expansion */
    pmdaMetric	*e_metrics;	/* metric description table */

    pmdaResultCallBack e_resultCallBack; /* callback to clean up pmResult after fetch */
    pmdaFetchCallBack  e_fetchCallBack;  /* callback to assign metric values in fetch */
    pmdaCheckCallBack  e_checkCallBack;  /* callback on receipt of a PDU */
    pmdaDoneCallBack   e_doneCallBack;   /* callback after PDU has been processed */
} pmdaExt;

/*
 * Interface Definitions for PMDA DSO Interface
 * The new interface structure differs significantly from the original version
 * (_pmPMDA that used to be in impl.h) with the use of a union to
 * manage new revisions cleanly.
 *
 * The domain field is set by pmcd(1) in the case of a DSO PMDA, and by
 * pmdaDaemon and pmdaGetOpt in the case of a Daemon PMDA. It should not be
 * modified.
 */
typedef struct {
    int	domain;		/* performance metrics domain id */
    struct {
	unsigned int	pmda_interface : 8;	/* PMDA DSO interface version */
	unsigned int	pmapi_version : 8;	/* PMAPI version */
	unsigned int	flags : 16;		/* usage TBD */
    } comm;		/* set/return communication and version info */
    int	status;		/* return initialization status here */

    union {

/*
 * Interface Version 1 (PCP 1.0 & PCP 1.1)
 * PMDA_INTERFACE_1
 */

    	struct {
	    int	    (*profile)(__pmProfile *);
	    int	    (*fetch)(int, pmID *, pmResult **);
	    int	    (*desc)(pmID, pmDesc *);
	    int	    (*instance)(pmInDom, int, char *, __pmInResult **);
	    int	    (*text)(int, int, char **);
	    int	    (*control)(pmResult *, int, int, int);
	    int	    (*store)(pmResult *);
	} one;

/*
 * Interface Version 2 (PCP 2.0)
 * PMDA_INTERFACE_2, PMDA_INTERFACE_3
 */

	struct {
	    pmdaExt *ext;
	    int	    (*profile)(__pmProfile *, pmdaExt *);
	    int	    (*fetch)(int, pmID *, pmResult **, pmdaExt *);
	    int	    (*desc)(pmID, pmDesc *, pmdaExt *);
	    int	    (*instance)(pmInDom, int, char *, __pmInResult **, pmdaExt *);
	    int	    (*text)(int, int, char **, pmdaExt *);
	    int	    (*store)(pmResult *, pmdaExt *);
	} two;

/*
 * Interface Version 3 (dynamic pmns support)
 * PMDA_INTERFACE_4
 */

	struct {
	    pmdaExt *ext;
	    int	    (*profile)(__pmProfile *, pmdaExt *);
	    int	    (*fetch)(int, pmID *, pmResult **, pmdaExt *);
	    int	    (*desc)(pmID, pmDesc *, pmdaExt *);
	    int	    (*instance)(pmInDom, int, char *, __pmInResult **, pmdaExt *);
	    int	    (*text)(int, int, char **, pmdaExt *);
	    int	    (*store)(pmResult *, pmdaExt *);
	    int     (*pmid)(char *, pmID *, pmdaExt *);
	    int     (*name)(pmID, char ***, pmdaExt *);
	    int     (*children)(char *, char ***, int **, pmdaExt *);
	} three;

    } version;

} pmdaInterface;

/*
 * PM_CONTEXT_LOCAL support
 */
typedef struct {
    int			domain;
    char		*name;
    char		*init;
    void		*handle;
    pmdaInterface	dispatch;
} __pmDSO;

extern __pmDSO *__pmLookupDSO(int /*domain*/);

/* Macro that can be used to create each metrics' PMID. */
#define PMDA_PMID(x,y) 	((x<<10)|y)

/* macro for pmUnits bitmap in a pmDesc declaration */
#ifdef HAVE_BITFIELDS_LTOR
#define PMDA_PMUNITS(a,b,c,d,e,f) {a,b,c,d,e,f,0}
#else
#define PMDA_PMUNITS(a,b,c,d,e,f) {0,f,e,d,c,b,a}
#endif


/*
 * PMDA Setup Routines.
 *
 * pmdaGetOpt
 *	Replacement for getopt(3) which strips out the standard PMDA flags
 *	before returning the next command line option. The standard PMDA
 *	flags are "D:d:i:l:pu:" which will initialise the pmdaExt structure
 *	with the IPC details, path to the log file and domain number.
 *	err will be incremented if there was an error parsing these options.
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
 * pmdaInit
 *      Further initialises the pmdaExt structure with the instance domain and
 *      metric structures. Unique identifiers are applied to each instance 
 *	domain and metric. Also open the help text file and checks that the 
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
 *      PDU. For all PDUs except PDU_PROFILE, a result less than zero indicates an
 *      error. If set to zero (which is also the default), the callback is ignored.
 *
 * pmdaSetDoneCallBack
 *      Allows an application specific routine to be called after each PDU is
 *      processed. The result is ignored. If set to zero (which is also the default),
 *      the callback is ignored.
 */

extern int pmdaGetOpt(int, char *const *, const char *, pmdaInterface *, int *);
extern void pmdaDaemon(pmdaInterface *, int, char *, int , char *, char *);
extern void pmdaDSO(pmdaInterface *, int, char *, char *);
extern void pmdaOpenLog(pmdaInterface *);
extern void pmdaInit(pmdaInterface *, pmdaIndom *, int, pmdaMetric *, int);
extern void pmdaConnect(pmdaInterface *);

extern void pmdaMain(pmdaInterface *);

extern void pmdaSetResultCallBack(pmdaInterface *, pmdaResultCallBack);
extern void pmdaSetFetchCallBack(pmdaInterface *, pmdaFetchCallBack);
extern void pmdaSetCheckCallBack(pmdaInterface *, pmdaCheckCallBack);
extern void pmdaSetDoneCallBack(pmdaInterface *, pmdaDoneCallBack);

/*
 * Callbacks to PMCD which should be adequate for most PMDAs.
 * NOTE: if pmdaFetch is used, e_callback must be specified in the pmdaExt
 *       structure.
 *
 * pmdaProfile
 *	Store the __pmProfile away for the next fetch.
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
 *	Return the PMID for a named dynamic metric.
 *
 * pmdaName
 *	Return the name of alll matching dynamic metrics, given a PMID.
 *
 * pmdaChildren
 *	Return the names of all the descendent children and their
 *	status, given a named metric in a dynamic subtree of the PMNS.
 *
 */

extern int pmdaProfile(__pmProfile *, pmdaExt *);
extern int pmdaFetch(int , pmID *, pmResult **, pmdaExt *);
extern int pmdaInstance(pmInDom, int, char *, __pmInResult **, pmdaExt *);
extern int pmdaDesc(pmID, pmDesc *, pmdaExt *);
extern int pmdaText(int, int, char **, pmdaExt *);
extern int pmdaStore(pmResult *, pmdaExt *);
extern int pmdaPMID(char *, pmID *, pmdaExt *);
extern int pmdaName(pmID, char ***, pmdaExt *);
extern int pmdaChildren(char *, char ***, int **, pmdaExt *);

/*
 * PMDA "help" text manipulation
 */
extern int pmdaOpenHelp(char *);
extern void pmdaCloseHelp(int);
extern char *pmdaGetHelp(int, pmID, int);
extern char *pmdaGetInDomHelp(int, pmInDom, int);

/*
 * PMDA instance domain cache support
 *
 * pmdaCacheStore
 * 	add entry into the cache, or change state, assigns internal
 * 	instance identifier
 *
 * pmdaCacheLookup
 *	fetch entry based on internal instance identifier
 *
 * pmdaCacheLookupName
 *	fetch entry based on external instance name
 *
 * pmdaCacheOp
 *	service routines to load, unload, mark as write-thru, purge,
 *	count entries, etc
 * 
 * pmdaCachePurge
 *	cull inactive entries
 */
extern int pmdaCacheStore(pmInDom, int, char *, void *);
extern int pmdaCacheLookup(pmInDom, int, char **, void **);
extern int pmdaCacheLookupName(pmInDom, char *, int *, void **);
extern int pmdaCacheOp(pmInDom, int);
extern int pmdaCachePurge(pmInDom, time_t);

#define PMDA_CACHE_LOAD			1
#define PMDA_CACHE_ADD			2
#define PMDA_CACHE_HIDE			3
#define PMDA_CACHE_CULL			4
#define PMDA_CACHE_EMPTY		5
#define PMDA_CACHE_SAVE			6
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
 * __pmdaSetup
 *      Setup the PMDA's pmdaInterface and pmdaExt structures which are common 
 *      to both Daemon and DSO PMDAs.
 *
 * __pmdaSetupPDU
 *	Exchange version information with pmcd.
 *
 * __pmdaOpenInet
 *	Open an inet port to PMCD.
 *
 * __pmdaOpenUnix
 *	Open a unix port to PMCD.
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

extern void __pmdaSetup(pmdaInterface *, int, char *);
extern int __pmdaSetupPDU(int, int, char *);

extern void __pmdaOpenInet(char *, int, int *, int *);
extern void __pmdaOpenUnix(char *, int *, int *);

extern int __pmdaMainPDU(pmdaInterface *);
extern int __pmdaInFd(pmdaInterface *);

extern void __pmdaCacheDumpAll(FILE *, int);
extern void __pmdaCacheDump(FILE *, pmInDom, int);

/*
 * Outdated routines
 *
 * pmdaMainLoopFreeResultCallback
 *      Was provided for setting the callback in pmdaMainLoop for cleaning
 *	the pmResult structure.
 *	Do not use this function as this is now supported by 
 *      pmdaSetResultCallBack().
 *
 * extern void pmdaMainLoopFreeResultCallback(void (*)(pmResult *));
 */

#ifdef __cplusplus
}
#endif

#endif /* _PMDA_H */
