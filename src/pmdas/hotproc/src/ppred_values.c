/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <unistd.h>
#include <stdio.h>
#include <syslog.h>
#include <fcntl.h>
#include <ctype.h>
#include <pwd.h>

#include "pmapi.h"
#include "impl.h"
#include "proc.h"
#include "proc_aux.h"
#include "cluster.h"
#include "ppred_values.h"
#include "hotproc.h"


static pmDesc	desctab[] = {
    /* hotproc.predicate.syscalls */
    { PMID(CLUSTER_PRED,ITEM_SYSCALLS), 
      PM_TYPE_FLOAT, PM_INDOM_PROC, PM_SEM_INSTANT, {0,-1,1, 0,PM_TIME_SEC,0} },
    /* hotproc.predicate.ctxswitch */
    { PMID(CLUSTER_PRED,ITEM_CTXSWITCH), 
      PM_TYPE_FLOAT, PM_INDOM_PROC, PM_SEM_INSTANT, {0,-1,1, 0,PM_TIME_SEC,0} },
    /* hotproc.predicate.virtualsize */
    { PMID(CLUSTER_PRED,ITEM_VIRTUALSIZE), 
      PM_TYPE_FLOAT, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0, PM_SPACE_KBYTE,0,0} },
    /* hotproc.predicate.residentsize */
    { PMID(CLUSTER_PRED,ITEM_RESIDENTSIZE), 
      PM_TYPE_FLOAT, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0, PM_SPACE_KBYTE,0,0} },
    /* hotproc.predicate.iodemand */
    { PMID(CLUSTER_PRED,ITEM_IODEMAND), 
      PM_TYPE_FLOAT, PM_INDOM_PROC, PM_SEM_INSTANT, {1,-1,0, PM_SPACE_KBYTE,PM_TIME_SEC,0} },

    /*
     * NOTE
     * iowait and schedwait really have units of sec/sec, i.e.  utilization
     */
    /* hotproc.predicate.iowait */
    { PMID(CLUSTER_PRED,ITEM_IOWAIT), 
      PM_TYPE_FLOAT, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0, 0,0,0} },
    /* hotproc.predicate.schedwait */
    { PMID(CLUSTER_PRED,ITEM_SCHEDWAIT), 
      PM_TYPE_FLOAT, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0, 0,0,0} },
};

static int      ndesc = (sizeof(desctab)/sizeof(desctab[0]));

static derived_pred_t *pred_buf = NULL;

void
ppred_init(int dom)
{
    init_table(ndesc, desctab, dom);
}

int 
ppred_getdesc(pmID pmid, pmDesc *desc)
{
    return getdesc(ndesc, desctab, pmid, desc);
}

int
ppred_available(int item)
{
    extern int need_psusage;        /* is psusage buffer needed or not */
    extern int need_accounting;     /* is pracinfo buffer needed or not */

    switch (item) {
	case ITEM_SYSCALLS:
	case ITEM_CTXSWITCH:
	case ITEM_IODEMAND:
	    return need_psusage;
	case ITEM_IOWAIT:
	case ITEM_SCHEDWAIT:
	    return need_accounting;
	case ITEM_VIRTUALSIZE:
	case ITEM_RESIDENTSIZE:
	    /* need_psinfo - always have it */
	    return 1;
    }
    return 1;
}

int
ppred_setatom(int item, pmAtomValue *atom, int j)
{
    switch (item) {
	case ITEM_SYSCALLS:
	    atom->f = pred_buf[j].syscalls;
	    break;
	case ITEM_CTXSWITCH:
	    atom->f = pred_buf[j].ctxswitch;
	    break;
	case ITEM_VIRTUALSIZE:
	    atom->f = pred_buf[j].virtualsize;
	    break;
	case ITEM_RESIDENTSIZE:
	    atom->f = pred_buf[j].residentsize;
	    break;
	case ITEM_IODEMAND:
	    atom->f = pred_buf[j].iodemand;
	    break;
	case ITEM_IOWAIT:
	    atom->f = pred_buf[j].iowait;
	    break;
	case ITEM_SCHEDWAIT:
	    atom->f = pred_buf[j].schedwait;
	    break;
    }
    return 0;
}

int
ppred_getinfo(pid_t pid, int j)
{
    process_t *node;
    char *path;

    node = lookup_curr_node(pid);
    if (node == NULL) {
	/* node should be there if it's in active list ! */
	(void)fprintf(stderr, "%s: Internal error for lookup_node()", pmProgname);
	exit(1);
    }
    proc_pid_to_path(pid, NULL, &path, PINFO_PATH);
    if (access(path, R_OK) < 0)
	return -oserror(); 
		
    pred_buf[j] = node->preds;
    return 0;
}


int
ppred_allocbuf(int size)
{
    static int max_size = 0;
    derived_pred_t *predb;

    if (size > max_size) {
	predb = realloc(pred_buf, size * sizeof(derived_pred_t));  
	if (predb == NULL)
	    return -oserror();
	pred_buf = predb;
	max_size = size;
    }

    return 0;
}
