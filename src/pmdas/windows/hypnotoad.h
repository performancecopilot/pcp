/*
 * Copyright (c) 2008-2010 Aconex.  All Rights Reserved.
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
 */
#ifndef HYPNOTOAD_H
#define HYPNOTOAD_H

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "pdh.h"
#include "pdhmsg.h"
#include "domain.h"

#define MAX_M_PATH_LEN	80	/* pattern passed to PdhExpandCounterPath */
#define MAX_M_TEXT_LEN	512	/* longest long-text string that we allow */
#define INDOM(x,y)	(((x)<<22)|(y))	/* pmdaCache interfaces use indom */

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
    SQL_USER_INDOM,
    NUMINDOMS
};

typedef enum {
    V_NONE,
    V_ERROR_SEEN = 0x1,
    V_COLLECTED	= 0x2,	/* if PdhGetRawCounterValue was successful */
} pdh_valueflags_t;
 
typedef struct {
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
    M_VERIFIED	= 0x10,		/* has this metrics semantics been checked */
    M_AUTO64	= 0x20,		/* allow auto-modification on 64/32bit type */
} pdh_metricflag_t;

typedef struct {
    pmDesc		desc;		/* metric descriptor */
    pdh_metricflag_t	flags;		/* state of this metric */
    int			ctype;		/* PDH counter type */
    int			num_alloc;	/* high water allocation mark */
    int			num_vals;	/* one or more metric values */
    pdh_value_t		*vals;
    char		pat[MAX_M_PATH_LEN];	/* for PdhExpandCounterPath */
} pdh_metric_t;

extern pdh_metric_t metricdesc[];
extern int metricdesc_sz;

extern char *windows_uname;
extern char *windows_build;
extern char *windows_machine;
extern int windows_indom_setup[];
extern int windows_indom_reset[];
extern unsigned long windows_pagesize;
extern MEMORYSTATUSEX windows_memstat;
extern void windows_fetch_memstat(void);

extern void windows_open(int);
extern int windows_indom_fixed(int);
extern char *pdherrstr(int);

typedef void (*pdh_metric_inform_t)(pdh_metric_t *, PDH_COUNTER_INFO_A *);
typedef void (*pdh_metric_visitor_t)(pdh_metric_t *, LPSTR, pdh_value_t *);
extern int windows_visit_metric(pdh_metric_t *, pdh_metric_visitor_t);
extern int windows_inform_metric(pdh_metric_t *, LPTSTR, pdh_value_t *,
				 BOOLEAN, pdh_metric_inform_t);

extern void windows_instance_refresh(pmInDom);
extern int windows_lookup_instance(char *, pdh_metric_t *);
extern void windows_fetch_refresh(int numpmid, pmID pmidlist[], pmdaExt *);
extern void windows_verify_callback(pdh_metric_t *, LPSTR, pdh_value_t *);

extern int windows_help(int, int, char **, pmdaExt *);

#endif	/* HYPNOTOAD_H */
