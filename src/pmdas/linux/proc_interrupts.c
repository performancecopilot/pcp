/*
 * Linux /proc/interrupts metrics cluster
 *
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include <ctype.h>
#include <sys/stat.h>
#include "proc_interrupts.h"

static int started = 0;

int
refresh_proc_interrupts(proc_interrupts_t *proc_interrupts)
{
    char buf[1024];
    FILE *fp;
    unsigned int cpu;
    unsigned int intr;
    unsigned int inst;
    unsigned int id;
    unsigned int count;
    int i;
    int n;
    int free_entry;
    int is_syscall;
    char *s, *p;
    pmdaIndom *indomp = proc_interrupts->indom;
    
    if (!started) {
    	started = 1;
	memset(proc_interrupts, 0, sizeof(proc_interrupts));

	proc_interrupts->nstats = 0;
	proc_interrupts->maxstats = 16;
	proc_interrupts->stats = (proc_interrupt_counter_t *)malloc(
	    proc_interrupts->maxstats * sizeof(proc_interrupt_counter_t));

	proc_interrupts->ncpus = 0;
	proc_interrupts->maxcpus = 2;
	proc_interrupts->syscall = (unsigned int *)malloc(
	    proc_interrupts->maxcpus * sizeof(unsigned int));
	memset(proc_interrupts->syscall, 0,
	    proc_interrupts->maxcpus * sizeof(unsigned int));

	indomp->it_set = (pmdaInstid *)malloc(sizeof(pmdaInstid));
	indomp->it_numinst = 0;
    }

    if ((fp = fopen("/proc/interrupts", "r")) < 0) {
    	return -errno;
    }

    for (i=0; i < proc_interrupts->nstats; i++) {
    	proc_interrupts->stats[i].seen = 0;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (buf[3] != ':')
	    continue;
	s = buf + 3;
	is_syscall = 0;

	if (sscanf(buf, "%u:", &intr) != 1) {
	    if (strncmp(buf, "SYS:", 4) != 0)
		continue;
	    else {
		is_syscall = 1;
	    }
	}

	for (s++, cpu=0; ; cpu++) { 
	    while (isspace(*s))
		s++;
	    if (!isdigit(*s)) {
		/* s is name of interrupt, extends to end of line */
		break;
	    }

	    /* count for this interrupt or syscall on this cpu */
	    sscanf(s, "%u", &count);
	    while (isdigit(*s)) {
		s++;
	    }

	    if (is_syscall) {
		/* set the system call counter for this cpu */
		if (cpu >= proc_interrupts->maxcpus) {
		    proc_interrupts->maxcpus += 2;
		    proc_interrupts->syscall = (unsigned int *)realloc(
			proc_interrupts->syscall,
			proc_interrupts->maxcpus * sizeof(unsigned int));

		}
		if (cpu >= proc_interrupts->ncpus)
		    proc_interrupts->ncpus = cpu + 1;
		proc_interrupts->syscall[cpu] = count;
		continue;
	    }

	    /*
	     * otherwise set the counter for this {cpu X irq}
	     */
	    id = (cpu << 16) | intr;

	    free_entry = -1;
	    for (inst=0; inst < proc_interrupts->nstats; inst++) {
	    	if (proc_interrupts->stats[inst].valid == 0) {
		    free_entry = inst;
		    continue;
		}
	    	if (proc_interrupts->stats[inst].id == id)
		    break;
	    }

	    if (inst == proc_interrupts->nstats) {
		if (free_entry >= 0)
		    inst = free_entry;
		else {
		    proc_interrupts->nstats++;
		    if (proc_interrupts->nstats >= proc_interrupts->maxstats) {
			proc_interrupts->maxstats += 16;
			proc_interrupts->stats = (proc_interrupt_counter_t *)realloc(
			    proc_interrupts->stats, proc_interrupts->maxstats *
			    sizeof(proc_interrupt_counter_t));
		    }
		}
		memset(&proc_interrupts->stats[inst], 0, sizeof(proc_interrupt_counter_t));
	    	proc_interrupts->stats[inst].id = id;
	    	proc_interrupts->stats[inst].valid = 1;
	    }

	    proc_interrupts->stats[inst].count = count;
	    proc_interrupts->stats[inst].seen = 1;
	}

	if (s == NULL)
	    s = "unknown";
	else
	if ((p = strrchr(s, '\n')) != NULL)
	    *p = '\0';

	for (inst=0; inst < proc_interrupts->nstats; inst++) {
	    char tmp[sizeof(buf)];
	    if (proc_interrupts->stats[inst].valid && proc_interrupts->stats[inst].name == NULL) {
	    	sprintf(tmp, "cpu%d_intr%d %s", 
		    proc_interrupts->stats[inst].id >> 16,
		    proc_interrupts->stats[inst].id & 0xffff,
		    s);
		proc_interrupts->stats[inst].name = strdup(tmp);
	    }
	}
    }

    /* check for interrupts that have gone away */
    for (n=0, i=0; i < proc_interrupts->nstats; i++) {
	if (proc_interrupts->stats[i].valid) {
	    if (proc_interrupts->stats[i].seen == 0) {
		free(proc_interrupts->stats[i].name);
		proc_interrupts->stats[i].name = NULL;
		proc_interrupts->stats[i].valid = 0;
	    }
	    else
		n++;
    	}
    }

    /* refresh indom */
    if (indomp->it_numinst != n) {
        indomp->it_numinst = n;
        indomp->it_set = (pmdaInstid *)realloc(indomp->it_set, n * sizeof(pmdaInstid));
        memset(indomp->it_set, 0, n * sizeof(pmdaInstid));
    }
    for (n=0, i=0; i < proc_interrupts->nstats; i++) {
        if (proc_interrupts->stats[i].valid) {
            if (proc_interrupts->stats[i].id != indomp->it_set[n].i_inst ||
		indomp->it_set[n].i_name == NULL) {
                indomp->it_set[n].i_inst = proc_interrupts->stats[i].id;
                indomp->it_set[n].i_name = proc_interrupts->stats[i].name;
            }
            n++;
        }
    }

    /* success */
    fclose(fp);
    return 0;
}

