/*
 * Copyright (c) 2008 Aconex.  All Rights Reserved.
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
 */
#ifndef HYPNOTOAD_H
#define HYPNOTOAD_H

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "libpdh.h"
#include "domain.h"

#define MAX_M_PATH_LEN 80	/* pattern passed to PdhExpandCounterPath */

enum {
    DISK_INDOM,
    CPU_INDOM,
    NETIF_INDOM,
    FILESYS_INDOM,
    SQL_LOCK_INDOM,
    SQL_CACHE_INDOM,
    SQL_DB_INDOM,
    PROCESS_INDOM,
    THREAD_INDOM,
    NUMINDOMS
};

/*
 * The Query Descriptor table is shared for both instance and value
 * refreshes.  It contains handles that can be used to obtain the
 * current values/instances for a group of metrics, or one instance
 * domain.  It does _not_ have a one-to-one mapping with indoms.
 */
typedef enum {
    Q_DISK_ALL,
    Q_DISK_DEV,
    Q_PERCPU,
    Q_KERNEL,
    Q_MEMORY,
    Q_NETWORK_ALL,
    Q_NETWORK_IF,
    Q_SQLSERVER_ALL,
    Q_SQLSERVER_DB,
    Q_SQLSERVER_LOCK,
    Q_SQLSERVER_CACHE,
    Q_LDISK,
    Q_PROCESSES,
    Q_THREADS,
    Q_NUMQUERIES
} pdh_querytypes_t;

typedef enum {
    Q_NONE,
    Q_COLLECTED	= 0x1,	/* if PdhCollectQueryData has been called */
    Q_ERR_SEEN	= 0x2,	/* if PdhCollectQueryData error reported */
} pdh_queryflags_t;

typedef struct {
    PDH_HQUERY		hdl;		/* from PdhOpenQuery */
    pdh_queryflags_t	flags;
} pdh_query_t;

extern pdh_query_t querydesc[];		/* refreshes batched via this table */
extern int querydesc_sz;


typedef enum {
    V_NONE,
    V_VERIFIED	= 0x1,	/* if Pdh{AddCounterA,GetCounterInfoA} called */
    V_COLLECTED	= 0x2,	/* if PdhGetRawCounterValue was successful */
} pdh_valueflags_t;

typedef struct {
    PDH_HCOUNTER	hdl;		/* from PdhAddCounter */
    int			inst;		/* PM_IN_NULL or instance identifier */
    pdh_valueflags_t	flags;
    pmAtomValue		atom;
} pdh_value_t;

typedef enum {
    M_NONE,
    M_EXPANDED	= 0x1,		/* pattern has been expanded */
    M_REDO	= 0x2,		/* redo pattern expansion on each fetch */
    M_NOVALUES	= 0x4,		/* setup failed, don't bother with the fetch */
    M_OPTIONAL	= 0x8,		/* optional component, no values is expected */
} pdh_metricflag_t;

typedef struct {
    pmDesc		desc;		/* metric descriptor */
    int			qid;		/* index into query group table */
    pdh_metricflag_t	flags;	
    int			ctype;		/* PDH counter type */
    int			num_vals;	/* one or more metric values */
    pdh_value_t		*vals;
    char		pat[MAX_M_PATH_LEN];	/* for PdhExpandCounterPath */
} pdh_metric_t;

extern pdh_metric_t metricdesc[];
extern int metricdesc_sz;

extern void windows_open();
extern char windows_uname[];
extern char windows_build[];
extern unsigned long windows_pagesize;
extern unsigned long long windows_physmem;

extern void errmsg();
extern char *pdherrstr(int);
extern char *decode_ctype(DWORD);
extern pmInDom windows_indom(int, int);

extern int windows_check_metric(pdh_metric_t *, int);
extern int windows_check_instance(char *, pdh_metric_t *);

extern void windows_instance_refresh(pmInDom);
extern void windows_fetch_refresh(int numpmid, pmID pmidlist[]);

extern int windows_help(int, int, char **, pmdaExt *);

#endif	/* HYPNOTOAD_H */
