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
#include <sys/stat.h>
#include <dirent.h>
#include <sys/procfs.h>
#include <sys/immu.h>
#include <sys/sysmacros.h>
#include <pwd.h>
#include <grp.h>

#include "pmapi.h"

#include "./proc.h"
#include "./proc_aux.h"
#include "./pglobal.h"

static pmDesc	desctab[] = {
    { PMID(CLUSTER_GLOBAL,ITEM_NPROCS), 
      PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} },
    { PMID(CLUSTER_GLOBAL,ITEM_REFRESH), 
      PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} },
    { PMID(CLUSTER_GLOBAL,ITEM_CONFIG), 
      PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} },
    { PMID(CLUSTER_GLOBAL,ITEM_CONFIG_GEN), 
      PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} },
    { PMID(CLUSTER_GLOBAL,ITEM_CPUIDLE), 
      PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} },
    { PMID(CLUSTER_GLOBAL,ITEM_TOTAL_CPUBURN), 
      PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} },
    { PMID(CLUSTER_GLOBAL,ITEM_TRANSIENT), 
      PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} },
    { PMID(CLUSTER_GLOBAL,ITEM_TOTAL_NOTCPUBURN), 
      PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} },
    { PMID(CLUSTER_GLOBAL,ITEM_OTHER_TOTAL), 
      PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} },
    { PMID(CLUSTER_GLOBAL,ITEM_OTHER_PERCENT), 
      PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} }
};

static int      ndesc = (sizeof(desctab)/sizeof(desctab[0]));


void
pglobal_init(int dom)
{
    init_table(ndesc, desctab, dom);
}

int 
pglobal_getdesc(pmID pmid, pmDesc *desc)
{
    return getdesc(ndesc, desctab, pmid, desc);
}

int
pglobal_setatom(int item, pmAtomValue *atom, int j)
{
    /* noop */
    return 0;
}


int
pglobal_getinfo(pid_t pid, int j)
{
    /* noop */
    return 0;
}


int
pglobal_allocbuf(int size)
{
    /* noop */
    return 0;
}
