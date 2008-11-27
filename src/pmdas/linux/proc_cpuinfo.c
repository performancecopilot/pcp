/*
 * Linux /proc/cpuinfo metrics cluster
 *
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.  All Rights Reserved.
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
 * Copyright (c) 2001 Gilly Ran (gilly@exanet.com) for the
 * portions of the code supporting the Alpha platform.
 * All rights reserved.
 */

#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include "proc_cpuinfo.h"

static void
map_nodes_to_cnodes(proc_cpuinfo_t *proc_cpuinfo)
{
    int i, j;
    int high_node = 0;
    int high_cnode = 0;
    unsigned int node_module;
    unsigned int node_slot;
    unsigned int node_slab;
    int cnodemap[1024];
    char nodenum[1024];
    char cpunum[1024];
    char nodehwg[1024];
    char cpuhwg[1024];
    cpuinfo_t *c;

    memset(cnodemap, 0, sizeof(cnodemap));
    for (i=0; i < proc_cpuinfo->cpuindom->it_numinst; i++) {
	c = &proc_cpuinfo->cpuinfo[i];
	c->module = -1;
	snprintf(cpunum, sizeof(cpunum), "/hw/cpunum/%d", i);
	if (realpath(cpunum, cpuhwg)) {
	    sscanf(cpuhwg, "/hw/module/%dc%d/slab/%d/node/cpubus/%d/%c",
	    	&c->module, &c->slot, &c->slab, &c->bus, &c->cpu_char);
	    /* now find the matching node number */
	    for (j=0; ; j++) {
	    	snprintf(nodenum, sizeof(nodenum), "/hw/nodenum/%d", j);
		if (access(nodenum, F_OK) !=0 || realpath(nodenum, nodehwg) == NULL)
		    break;
		sscanf(nodehwg, "/hw/module/%dc%d/slab/%d/node",
		    &node_module, &node_slot, &node_slab);
		if (node_module == c->module && node_slot == c->slot && node_slab == c->slab) {
		    proc_cpuinfo->cpuinfo[i].node = j;
		    cnodemap[proc_cpuinfo->cpuinfo[i].node]++;
		    if (proc_cpuinfo->cpuinfo[i].node > high_node)
			high_node = proc_cpuinfo->cpuinfo[i].node;
		    break;
		}
	    }
	}
    }

    /* now map nodes (discontig) to compact nodes (contiguous) */
    for (i=0; i <= high_node; i++) {
    	if (cnodemap[i])
	    cnodemap[i] = high_cnode++;
    }

    for (i=0; i < proc_cpuinfo->cpuindom->it_numinst; i++) {
    	proc_cpuinfo->cpuinfo[i].cnode = cnodemap[proc_cpuinfo->cpuinfo[i].node];
    }
}

char *
cpu_name(proc_cpuinfo_t *proc_cpuinfo, int c)
{
    char name[1024];
    char *s = NULL;
    char *p;
    FILE *f;
    static int started = 0;

    if (!started) {
    	refresh_proc_cpuinfo(proc_cpuinfo);
	map_nodes_to_cnodes(proc_cpuinfo);

	proc_cpuinfo->machine = NULL;
    	if ((f = fopen("/proc/sgi_prominfo/node0/version", "r")) != NULL) {
	    while (fgets(name, sizeof(name), f)) {
		if (strncmp(name, "SGI", 3) == 0) {
		    if ((p = strstr(name, " IP")) != NULL)
			proc_cpuinfo->machine = strndup(p+1, 4);
		    break;
		}
	    }
	    fclose(f);
	}
	if (proc_cpuinfo->machine == NULL)
	    proc_cpuinfo->machine = strdup("linux");

	started = 1;
    }

    if (proc_cpuinfo->cpuinfo[c].module >= 0) {
	/* SGI SNIA CPU names */
	snprintf(name, sizeof(name), "cpu:%d.%d.%d.%c", 
		proc_cpuinfo->cpuinfo[c].module,
		proc_cpuinfo->cpuinfo[c].slot,
		proc_cpuinfo->cpuinfo[c].slab,
		proc_cpuinfo->cpuinfo[c].cpu_char);
	s = name;
    }
    
    if (s == NULL) {
	/* flat namespace for cpu names */
	snprintf(name, sizeof(name), "cpu%d", c);
	s = name;
    }

    return strdup(s);
}

int
refresh_proc_cpuinfo(proc_cpuinfo_t *proc_cpuinfo)
{
    char buf[4096];
    FILE *fp;
    int cpunum;
    cpuinfo_t *info;
    char *val;
    char *p;
    static int started = 0;

    if (!started) {
	int need;
	if (proc_cpuinfo->cpuindom == NULL || proc_cpuinfo->cpuindom->it_numinst == 0)
	    abort();
	need = proc_cpuinfo->cpuindom->it_numinst * sizeof(cpuinfo_t);
	proc_cpuinfo->cpuinfo = (cpuinfo_t *)malloc(need);
	memset(proc_cpuinfo->cpuinfo, 0, need);
    	started = 1;
    }

    if ((fp = fopen("/proc/cpuinfo", "r")) == (FILE *)NULL)
    	return -errno;

#if defined(HAVE_ALPHA_LINUX)
    cpunum = 0;
#else	//intel
    cpunum = -1;
#endif
    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((val = strrchr(buf, '\n')) != NULL)
	    *val = '\0';
	if ((val = strchr(buf, ':')) == NULL)
	    continue;
	val += 2;

#if !defined(HAVE_ALPHA_LINUX)
	if (strncmp(buf, "processor", 9) == 0) {
	    cpunum++;
	    proc_cpuinfo->cpuinfo[cpunum].cpu_num = atoi(val);
	    continue;
	}
#endif

	info = &proc_cpuinfo->cpuinfo[cpunum];

	if (info->sapic == NULL && strncasecmp(buf, "sapic", 5) == 0)
	    info->sapic = strdup(val);
	if (info->model == NULL && strncasecmp(buf, "model name", 10) == 0)
	    info->model = strdup(val);
	if (info->model == NULL && strncasecmp(buf, "model", 5) == 0)
	    info->model = strdup(val);
	if (info->model == NULL && strncasecmp(buf, "cpu model", 9) == 0)
	    info->model = strdup(val);
	if (info->vendor == NULL && strncasecmp(buf, "vendor", 6) == 0)
	    info->vendor = strdup(val);
	if (info->stepping == NULL && strncasecmp(buf, "step", 4) == 0)
	    info->stepping = strdup(val);
	if (info->stepping == NULL && strncasecmp(buf, "revision", 8) == 0)
	    info->stepping = strdup(val);
	if (info->stepping == NULL && strncasecmp(buf, "cpu revision", 12) == 0)
	    info->stepping = strdup(val);
	if (info->clock == 0.0 && strncasecmp(buf, "cpu MHz", 7) == 0)
	    info->clock = atof(val);
	if (info->clock == 0.0 && strncasecmp(buf, "cycle frequency", 15) == 0) {
	    if ((p = strchr(val, ' ')) != NULL)
		*p = '\0';
	    info->clock = (atof(val))/1000000;
	}
	if (info->cache == 0 && strncasecmp(buf, "cache", 5) == 0)
	    info->cache = atoi(val);
	if (info->bogomips == 0.0 && strncasecmp(buf, "bogo", 4) == 0)
	    info->bogomips = atof(val);
	if (info->bogomips == 0.0 && strncasecmp(buf, "BogoMIPS", 8) == 0)
	    info->bogomips = atof(val);
    }
    fclose(fp);

#if defined(HAVE_ALPHA_LINUX)
    /* all processors are identical, therefore duplicate it to all the instances */
    for (cpunum=1; cpunum < proc_cpuinfo->cpuindom->it_numinst; cpunum++)
	memcpy(&proc_cpuinfo->cpuinfo[cpunum], info, sizeof(cpuinfo_t));
#endif

    /* success */
    return 0;
}
