/*
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 *
 * This is a specially crafted subset of the union of
 *	pmapi.h
 *	impl.h
 *	pmda.h
 *	platform_defs.h
 * to be used with the Microsoft C/C++ compiler on Intel i386 platforms
 */

/* $Id: shim_pcp.h,v 1.1 2004/07/12 02:43:23 kenmcd Exp $ */

#ifndef _SHIM_PCP_H
#define _SHIM_PCP_H

#include <stdio.h>

/*
 * don't know where to get this from for a Windows compilation,
 * but we need the Cygwin-compatible version, so no WinSock confusion
 * please!
 */
struct mytimeval {
    long tv_sec;	/* seconds */
    long tv_usec;	/* microseconds */
};

/* #undef HAVE_BITFIELDS_LTOR */

/* long and pointer must be either 32 bit or 64 bit */
/* #undef HAVE_64BIT_LONG */
#define HAVE_32BIT_LONG 1
#define HAVE_32BIT_PTR 1
/* #undef HAVE_64BIT_PTR */

typedef unsigned int	pmID;		/* Metric Identifier */
#define PM_ID_NULL	0xffffffff

/*
 * Internally, this is how to decode a PMID!
 */
typedef struct {
#ifdef HAVE_BITFIELDS_LTOR
	int		pad : 2;
	unsigned int	domain : 8;
	unsigned int	cluster : 12;
	unsigned int	item : 10;
#else
	unsigned int	item : 10;
	unsigned int	cluster : 12;
	unsigned int	domain : 8;
	int		pad : 2;
#endif
} __pmID_int;

typedef unsigned int	pmInDom;	/* Instance-Domain */
#define PM_INDOM_NULL	0xffffffff
#define PM_IN_NULL	0xffffffff

/*
 * Internally, this is how to decode an Instance Domain Identifier
 */
typedef struct {
#ifdef HAVE_BITFIELDS_LTOR
	int		pad : 2;
	unsigned int	domain : 8;		/* the administrative PMD */
	unsigned int	serial : 22;		/* unique within PMD */
#else
	unsigned int	serial : 22;		/* unique within PMD */
	unsigned int	domain : 8;		/* the administrative PMD */
	int		pad : 2;
#endif
} __pmInDom_int;

/*
 * Encoding for the units (dimensions Time and Space) and scale
 * for Performance Metric Values
 *
 * For example, a pmUnits struct of
 *	{ 1, -1, 0, PM_SPACE_MBYTE, PM_TIME_SEC, 0 }
 * represents Mbytes/sec, while 
 *	{ 0, 1, -1, 0, PM_TIME_HOUR, 6 }
 * represents hours/million-events
 */
typedef struct {
#ifdef HAVE_BITFIELDS_LTOR
    int	dimSpace : 4;	/* space dimension */
    int	dimTime : 4;	/* time dimension */
    int	dimCount : 4;	/* event dimension */
    int	scaleSpace : 4;	/* one of PM_SPACE_* below */
    int	scaleTime : 4;	/* one of PM_TIME_* below */
    int	scaleCount : 4;	/* one of PM_COUNT_* below */
    int pad : 8;
#else
    int pad : 8;
    int	scaleCount : 4;	/* one of PM_COUNT_* below */
    int	scaleTime : 4;	/* one of PM_TIME_* below */
    int	scaleSpace : 4;	/* one of PM_SPACE_* below */
    int	dimCount : 4;	/* event dimension */
    int	dimTime : 4;	/* time dimension */
    int	dimSpace : 4;	/* space dimension */
#endif
} pmUnits;			/* dimensional units and scale of value */

/* pmUnits.scaleSpace */
#define PM_SPACE_BYTE	0	/* bytes */
#define PM_SPACE_KBYTE	1	/* Kilobytes (1024) */
#define PM_SPACE_MBYTE	2	/* Megabytes (1024^2) */
#define PM_SPACE_GBYTE	3	/* Gigabytes (1024^3) */
#define PM_SPACE_TBYTE	4	/* Terabytes (1024^4) */
#define PM_SPACE_PBYTE	5	/* Petabytes (1024^5) */
#define PM_SPACE_EBYTE	6	/* Exabytes  (1024^6) */
/* pmUnits.scaleTime */
#define PM_TIME_NSEC	0	/* nanoseconds */
#define PM_TIME_USEC	1	/* microseconds */
#define PM_TIME_MSEC	2	/* milliseconds */
#define PM_TIME_SEC	3	/* seconds */
#define PM_TIME_MIN	4	/* minutes */
#define PM_TIME_HOUR	5	/* hours */
/*
 * pmUnits.scaleCount (e.g. count events, syscalls, interrupts, etc.)
 * -- these are simply powers of 10, and not enumerated here,
 *    e.g. 6 for 10^6, or -3 for 10^-3
 */
#define PM_COUNT_ONE	0	/* 1 */

/* Performance Metric Descriptor */
typedef struct {
    pmID	pmid;		/* unique identifier */
    int		type;		/* base data type (see below) */
    pmInDom	indom;		/* instance domain */
    int		sem;		/* semantics of value (see below) */
    pmUnits	units;		/* dimension and units */
} pmDesc;

/* pmDesc.type -- data type of metric values */
#define PM_TYPE_NOSUPPORT	-1	/* not implemented in this version */
#define PM_TYPE_32		0	/* 32-bit signed integer */
#define PM_TYPE_U32		1	/* 32-bit unsigned integer */
#define PM_TYPE_64		2	/* 64-bit signed integer */
#define PM_TYPE_U64		3	/* 64-bit unsigned integer */
#define PM_TYPE_FLOAT		4	/* 32-bit floating point */
#define PM_TYPE_DOUBLE		5	/* 64-bit floating point */
#define PM_TYPE_STRING		6	/* array of char */
#define PM_TYPE_AGGREGATE	7	/* arbitrary binary data (aggregate) */
#define PM_TYPE_AGGREGATE_STATIC 8	/* static pointer to aggregate */
#define PM_TYPE_UNKNOWN		255	/* used in pmValueBlock, not pmDesc */

/* pmDesc.sem -- semantics/interpretation of metric values */
#define PM_SEM_COUNTER	1	/* cumulative counter (monotonic increasing) */
				/* was PM_SEM_RATE, no longer used now */
#define PM_SEM_INSTANT	3	/* instantaneous value, continuous domain */
#define PM_SEM_DISCRETE	4	/* instantaneous value, discrete domain */

typedef int __int32_t;
typedef unsigned int __uint32_t;
typedef long long __int64_t;
typedef unsigned long long __uint64_t;

/* Generic Union for Value-Type conversions */
typedef union {
    __int32_t	l;	/* 32-bit signed */
    __uint32_t	ul;	/* 32-bit unsigned */
    __int64_t	ll;	/* 64-bit signed */
    __uint64_t	ull;	/* 64-bit unsigned */
    float	f;	/* 32-bit floating point */
    double	d;	/* 64-bit floating point */
    char	*cp;	/* char ptr */
    void	*vp;	/* void ptr */
} pmAtomValue;


/* PMAPI Error Conditions */

#define PM_ERR_BASE2 12345
#define PM_ERR_BASE  PM_ERR_BASE2

#define PM_ERR_GENERIC		(-PM_ERR_BASE-0)    /* Generic error, already reported above */
#define PM_ERR_PMNS		(-PM_ERR_BASE-1)    /* Problems parsing PMNS definitions */
#define PM_ERR_NOPMNS		(-PM_ERR_BASE-2)    /* PMNS not accessible */
#define PM_ERR_DUPPMNS		(-PM_ERR_BASE-3)    /* Attempt to reload the PMNS */
#define PM_ERR_TEXT		(-PM_ERR_BASE-4)    /* Oneline or help text is not available */
#define PM_ERR_APPVERSION	(-PM_ERR_BASE-5)    /* Metric not supported by this version of monitored application */
#define PM_ERR_VALUE		(-PM_ERR_BASE-6)    /* Missing metric value(s) */
#define PM_ERR_LICENSE		(-PM_ERR_BASE-7)    /* Current PCP license does not permit this operation */
#define PM_ERR_TIMEOUT		(-PM_ERR_BASE-8)    /* Timeout waiting for a response from PMCD */
#define PM_ERR_NODATA		(-PM_ERR_BASE-9)    /* Empty archive log file */
#define PM_ERR_RESET		(-PM_ERR_BASE-10)   /* pmcd reset or configuration changed */
#define PM_ERR_FILE		(-PM_ERR_BASE-11)   /* Cannot locate a file */
#define PM_ERR_NAME		(-PM_ERR_BASE-12)   /* Unknown metric name */
#define PM_ERR_PMID		(-PM_ERR_BASE-13)   /* Unknown or illegal metric identifier */
#define PM_ERR_INDOM		(-PM_ERR_BASE-14)   /* Unknown or illegal instance domain identifier */
#define PM_ERR_INST		(-PM_ERR_BASE-15)   /* Unknown or illegal instance identifier */
#define PM_ERR_UNIT		(-PM_ERR_BASE-16)   /* Illegal pmUnits specification */
#define PM_ERR_CONV		(-PM_ERR_BASE-17)   /* Impossible value or scale conversion */
#define PM_ERR_TRUNC		(-PM_ERR_BASE-18)   /* Truncation in value conversion */
#define PM_ERR_SIGN		(-PM_ERR_BASE-19)   /* Negative value in conversion to unsigned */
#define PM_ERR_PROFILE		(-PM_ERR_BASE-20)   /* Explicit instance identifier(s) required */
#define PM_ERR_IPC		(-PM_ERR_BASE-21)   /* IPC protocol failure */
#define PM_ERR_NOASCII		(-PM_ERR_BASE-22)   /* ASCII format not supported for this PDU */
#define PM_ERR_EOF		(-PM_ERR_BASE-23)   /* IPC channel closed */
#define PM_ERR_NOTHOST		(-PM_ERR_BASE-24)   /* Operation requires context with host source of metrics */
#define PM_ERR_EOL		(-PM_ERR_BASE-25)   /* End of PCP archive log */
#define PM_ERR_MODE		(-PM_ERR_BASE-26)   /* Illegal mode specification */
#define PM_ERR_LABEL		(-PM_ERR_BASE-27)   /* Illegal label record at start of a PCP archive log file */
#define PM_ERR_LOGREC		(-PM_ERR_BASE-28)   /* Corrupted record in a PCP archive log */
#define PM_ERR_NOTARCHIVE	(-PM_ERR_BASE-29)   /* Operation requires context with archive source of metrics */
#define PM_ERR_LOGFILE          (-PM_ERR_BASE-30)   /* Missing archive file */
#define PM_ERR_NOCONTEXT	(-PM_ERR_BASE-31)   /* Attempt to use an illegal context */
#define PM_ERR_PROFILESPEC	(-PM_ERR_BASE-32)   /* NULL pmInDom with non-NULL instlist */
#define PM_ERR_PMID_LOG		(-PM_ERR_BASE-33)   /* Metric not defined in the PCP archive log */
#define PM_ERR_INDOM_LOG	(-PM_ERR_BASE-34)   /* Instance domain identifier not defined in the PCP archive log */
#define PM_ERR_INST_LOG		(-PM_ERR_BASE-35)   /* Instance identifier not defined in the PCP archive log */
#define PM_ERR_NOPROFILE	(-PM_ERR_BASE-36)   /* Missing profile - protocol botch */
#define	PM_ERR_NOAGENT		(-PM_ERR_BASE-41)   /* No pmcd agent for domain of request */
#define PM_ERR_PERMISSION	(-PM_ERR_BASE-42)   /* No permission to perform requested operation */
#define PM_ERR_CONNLIMIT	(-PM_ERR_BASE-43)   /* PMCD connection limit for this host exceeded */
#define PM_ERR_AGAIN		(-PM_ERR_BASE-44)   /* try again. Info not currently available */

#define PM_ERR_ISCONN		(-PM_ERR_BASE-45)   /* already connected */
#define PM_ERR_NOTCONN		(-PM_ERR_BASE-46)   /* not connected */
#define PM_ERR_NEEDPORT		(-PM_ERR_BASE-47)   /* port name required */
#define PM_ERR_WANTACK		(-PM_ERR_BASE-48)   /* can not send due to pending acks */
#define PM_ERR_NONLEAF		(-PM_ERR_BASE-49)   /* PMNS node is not a leaf node */
#define PM_ERR_OBJSTYLE		(-PM_ERR_BASE-50)   /* user/kernel object style mismatch */
#define PM_ERR_PMCDLICENSE	(-PM_ERR_BASE-51)   /* PMCD is not licensed to accept connections */

#define PM_ERR_TOOSMALL		(-PM_ERR_BASE-98)   /* Insufficient elements in list */
#define PM_ERR_TOOBIG		(-PM_ERR_BASE-99)   /* Result size exceeded */

#define PM_ERR_PMDAREADY	(-PM_ERR_BASE-1048) /* now ready to respond */
#define PM_ERR_PMDANOTREADY	(-PM_ERR_BASE-1049) /* not yet ready to respond */
#define PM_ERR_NYI		(-PM_ERR_BASE-8999) /* Functionality not yet implemented */


/*
 * some handy formatting routines for messages, and other output
 */
extern const char *pmIDStr(pmID);
extern const char *pmInDomStr(pmInDom);

/*
 * These ones are only for debugging and may not appear in the shipped
 * libpmapi ...
 */
extern int	pmDebug;
/* debug control bit fields */
/* debug control bit fields */
#define  DBG_TRACE_PDU		1	/* PDU send and receive */
#define  DBG_TRACE_FETCH	2	/* dump pmFetch results */
#define  DBG_TRACE_PROFILE	4	/* trace profile changes */
#define  DBG_TRACE_VALUE	8	/* metric value conversions */
#define  DBG_TRACE_CONTEXT	16	/* trace PMAPI context changes */
#define  DBG_TRACE_INDOM	32	/* instance domain operations */
#define  DBG_TRACE_PDUBUF	64	/* PDU buffer management */
#define  DBG_TRACE_LOG		128	/* generic archive log operations */
#define  DBG_TRACE_LOGMETA	(1<<8)	/* meta data in archives */
#define  DBG_TRACE_OPTFETCH	(1<<9)	/* optFetch tracing */
#define  DBG_TRACE_AF		(1<<10)	/* trace async timer events */
#define  DBG_TRACE_APPL0	(1<<11)	/* reserved for applications */
#define  DBG_TRACE_APPL1	(1<<12)	/* reserved for applications */
#define  DBG_TRACE_APPL2	(1<<13)	/* reserved for applications */
#define  DBG_TRACE_PMNS		(1<<14)	/* PMNS operations */
#define  DBG_TRACE_LIBPMDA	(1<<15)	/* libpcp_pmda */
#define  DBG_TRACE_TIMECONTROL	(1<<16)	/* time control api */
#define  DBG_TRACE_PMC		(1<<17)	/* metrics class */
#define  DBG_TRACE_INTERP	(1<<20)	/* interpolate mode for archives */
#define  DBG_TRACE_CONFIG	(1<<21) /* configuration parameters */
#define  DBG_TRACE_LOOP		(1<<22) /* pmLoop tracing */

extern int __pmParseDebug(const char *);

/*
 * For the help text PDUs, the type (PM_TEXT_ONELINE or PM_TEXT_HELP)
 * is 'or'd with the following to encode the request for a PMID or
 * a pmInDom ...
 * Note the values must therefore be (a) bit fields and (b) different
 *	to the public macros PM_TEXT_* in pmapi.h 
 */
#define PM_TEXT_PMID	4
#define PM_TEXT_INDOM	8

/* Get help and descriptive text */
#define PM_TEXT_ONELINE 1
#define PM_TEXT_HELP	2

/*
 * program name, as used in __pmNotifyErr() ... default is "pcp"
 */
extern char	*pmProgname;


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

/* Macro that can be used to create each metrics' PMID. */
#define PMDA_PMID(x,y) 	((x<<10)|y)

/* macro for pmUnits bitmap in a pmDesc declaration */
#ifdef HAVE_BITFIELDS_LTOR
#define PMDA_PMUNITS(a,b,c,d,e,f) {a,b,c,d,e,f,0}
#else
#define PMDA_PMUNITS(a,b,c,d,e,f) {0,f,e,d,c,b,a}
#endif

#endif /* _SHIM_PCP_H */
