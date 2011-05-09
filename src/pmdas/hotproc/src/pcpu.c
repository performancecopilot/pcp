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
#include "pcpu.h"
#include "hotproc.h"

static pmDesc	desctab[] = {
    /* hotproc.cpuburn */
    { PMID(CLUSTER_CPU,ITEM_CPUBURN), 
      PM_TYPE_FLOAT, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

};

static int      ndesc = (sizeof(desctab)/sizeof(desctab[0]));

static float *cpuburn_buf = NULL;

void
pcpu_init(int dom)
{
  init_table(ndesc, desctab, dom);
}

int 
pcpu_getdesc(pmID pmid, pmDesc *desc)
{
    return getdesc(ndesc, desctab, pmid, desc);
}

int
pcpu_setatom(int item, pmAtomValue *atom, int j)
{
    switch (item) {
	case ITEM_CPUBURN:
	    atom->f = cpuburn_buf[j];
	    break;
    }
    return 0;
}

int
pcpu_getinfo(pid_t pid, int j)
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
		
    cpuburn_buf[j] = node->r_cpuburn;
    return 0;
}


int
pcpu_allocbuf(int size)
{
    static int max_size = 0;
    float *cpub;

    if (size > max_size) {
	cpub = realloc(cpuburn_buf, size * sizeof(float));  
	if (cpub == NULL)
	    return -oserror();
	cpuburn_buf = cpub;
	max_size = size;
    }

    return 0;
}
