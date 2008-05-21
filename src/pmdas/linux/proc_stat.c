/*
 * Linux /proc/stat metrics cluster
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
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include "proc_cpuinfo.h"
#include "proc_stat.h"

static int started;
static char *statbuf;
static int maxstatbuf;
static char **bufindex;
static int nbufindex;
static int maxbufindex;

/*
 * real time difference, *ap minus *bp
 */
double
tv_sub(struct timeval *ap, struct timeval *bp)
{
     return ap->tv_sec - bp->tv_sec + (double)(ap->tv_usec - bp->tv_usec)/1000000.0;
}

int
refresh_proc_stat(proc_cpuinfo_t *proc_cpuinfo, proc_stat_t *proc_stat)
{
    char fmt[64];
    int fd;
    int n;
    int i;
    int j;

    if ((fd = open("/proc/stat", O_RDONLY)) < 0) {
    	return -errno;
    }

    for (n=0;;) {
	if (n >= maxstatbuf) {
	    maxstatbuf += 512;
	    statbuf = (char *)realloc(statbuf, maxstatbuf * sizeof(char));
	}
	if ((i = read(fd, statbuf+n, 512)) > 0)
	    n += i;
	else
	    break;
    }
    statbuf[n] = '\0';
    close(fd);

    if (bufindex == NULL) {
	maxbufindex = 4;
    	bufindex = (char **)malloc(maxbufindex * sizeof(char *));
    }

    nbufindex = 0;
    bufindex[nbufindex++] = statbuf;
    for (i=0; i < n; i++) {
        if (statbuf[i] == '\n') {
            statbuf[i] = '\0';
	    if (nbufindex >= maxbufindex) {
	    	maxbufindex += 4;
		bufindex = (char **)realloc(bufindex, maxbufindex * sizeof(char *));
	    }
            bufindex[nbufindex++] = statbuf + i + 1;
        }
    }

    if (!started) {
    	started = 1;
	memset(proc_stat, 0, sizeof(proc_stat));

	/* hz of running kernel */
	proc_stat->hz = sysconf(_SC_CLK_TCK);

	/* scan ncpus */
	for (i=0; i < nbufindex; i++) {
	    if (strncmp("cpu", bufindex[i], 3) == 0 && isdigit(bufindex[i][3]))
	    	proc_stat->ncpu++;
	}
	if (proc_stat->ncpu == 0)
	    proc_stat->ncpu = 1; /* non-SMP kernel? */
	proc_stat->cpu_indom->it_numinst = proc_stat->ncpu;
	proc_stat->cpu_indom->it_set = (pmdaInstid *)malloc(
		proc_stat->ncpu * sizeof(pmdaInstid));
	/*
	 * Map out the CPU instance domain.
	 * If we have "sapic" in /proc/cpuinfo,
	 * then use cpu:M.S.C naming (Module, Slot, Cpu)
	 * else just use cpu[0-9]* naming
	 */
	for (i=0; i < proc_stat->ncpu; i++) {
	    proc_stat->cpu_indom->it_set[i].i_inst = i;
	    proc_stat->cpu_indom->it_set[i].i_name = cpu_name(proc_cpuinfo, i);
	}

	n = proc_stat->ncpu * sizeof(unsigned long long);
	proc_stat->p_user = (unsigned long long *)malloc(n);
	proc_stat->p_nice = (unsigned long long *)malloc(n);
	proc_stat->p_sys = (unsigned long long *)malloc(n);
	proc_stat->p_idle = (unsigned long long *)malloc(n);
	proc_stat->p_wait = (unsigned long long *)malloc(n);
	proc_stat->p_irq = (unsigned long long *)malloc(n);
	proc_stat->p_sirq = (unsigned long long *)malloc(n);
	memset(proc_stat->p_user, 0, n);
	memset(proc_stat->p_nice, 0, n);
	memset(proc_stat->p_sys, 0, n);
	memset(proc_stat->p_idle, 0, n);
	memset(proc_stat->p_wait, 0, n);
	memset(proc_stat->p_irq, 0, n);
	memset(proc_stat->p_sirq, 0, n);
    }

    /*
     * cpu  95379 4 20053 6502503
     * 2.6 kernels have 3 additional fields
     * for wait, irq and soft_irq.
     */
    strcpy(fmt, "cpu %llu %llu %llu %llu %llu %llu %llu");
    n = sscanf((const char *)bufindex[0], fmt,
	&proc_stat->user, &proc_stat->nice,
	&proc_stat->sys, &proc_stat->idle,
	&proc_stat->wait, &proc_stat->irq,
	&proc_stat->sirq);
    if (n == 4)
    	proc_stat->wait = proc_stat->irq = proc_stat->sirq = 0;

    /*
     * per-cpu stats
     * e.g. cpu0 95379 4 20053 6502503
     * 2.6 kernels have 3 additional fields
     * for wait, irq and soft_irq.
     */
    if (proc_stat->ncpu == 1) {
	/*
	 * Don't bother scanning - the counters are the same
	 * as for "all" cpus, as already scanned above.
	 * This also handles the non-SMP code where
	 * there is no line starting with "cpu0".
	 */
	proc_stat->p_user[0] = proc_stat->user;
	proc_stat->p_nice[0] = proc_stat->nice;
	proc_stat->p_sys[0] = proc_stat->sys;
	proc_stat->p_idle[0] = proc_stat->idle;
	proc_stat->p_wait[0] = proc_stat->wait;
	proc_stat->p_irq[0] = proc_stat->irq;
	proc_stat->p_sirq[0] = proc_stat->sirq;
    }
    else {
	strcpy(fmt, "cpu%d %llu %llu %llu %llu %llu %llu %llu");
	for (i=0; i < proc_stat->ncpu; i++) {
	    for (j=0; j < nbufindex; j++) {
		if (strncmp("cpu", bufindex[j], 3) == 0 && isdigit(bufindex[j][3])) {
		    int c;
		    int cpunum = atoi(&bufindex[j][3]);
		    if (cpunum >= 0 && cpunum < proc_stat->ncpu) {
			n = sscanf(bufindex[j], fmt, &c,
			    &proc_stat->p_user[cpunum],
			    &proc_stat->p_nice[cpunum],
			    &proc_stat->p_sys[cpunum],
			    &proc_stat->p_idle[cpunum],
			    &proc_stat->p_wait[cpunum],
			    &proc_stat->p_irq[cpunum],
			    &proc_stat->p_sirq[cpunum]);
			if (n == 4) {
			    proc_stat->p_wait[cpunum] =
			    proc_stat->p_irq[cpunum] =
			    proc_stat->p_sirq[cpunum] = 0;
			}
		    }
		}
	    }
	    if (j == nbufindex) {
		break;
	    }
	}
    }

    /*
     * page 59739 34786
     * Note: this has moved to /proc/vmstat in 2.6 kernels
     */
    strcpy(fmt, "page %u %u");
    for (j=0; j < nbufindex; j++) {
    	if (strncmp(fmt, bufindex[j], 5) == 0) {
	    sscanf((const char *)bufindex[j], fmt,
		&proc_stat->page[0], &proc_stat->page[1]);
	    break;
	}
    }

    /*
     * swap 0 1
     * Note: this has moved to /proc/vmstat in 2.6 kernels
     */
    strcpy(fmt, "swap %u %u");
    for (j=0; j < nbufindex; j++) {
    	if (strncmp(fmt, bufindex[j], 5) == 0) {
	    sscanf((const char *)bufindex[j], fmt,
		&proc_stat->swap[0], &proc_stat->swap[1]);
	    break;
	}
    }

    /*
     * intr 32845463 24099228 2049 0 2 ....
     * (just export the first number, which is total interrupts)
     */
    strcpy(fmt, "intr %llu");
    for (j=0; j < nbufindex; j++) {
    	if (strncmp(fmt, bufindex[j], 5) == 0) {
	    sscanf((const char *)bufindex[j], fmt, &proc_stat->intr);
	    break;
	}
    }

    /*
     * ctxt 1733480
     */
    strcpy(fmt, "ctxt %llu");
    for (j=0; j < nbufindex; j++) {
    	if (strncmp(fmt, bufindex[j], 5) == 0) {
	    sscanf((const char *)bufindex[j], fmt, &proc_stat->ctxt);
	    break;
	}
    }

    /*
     * btime 1733480
     */
    strcpy(fmt, "btime %lu");
    for (j=0; j < nbufindex; j++) {
    	if (strncmp(fmt, bufindex[j], 6) == 0) {
	    sscanf((const char *)bufindex[j], fmt, &proc_stat->btime);
	    break;
	}
    }

    /*
     * processes 2213
     */
    strcpy(fmt, "processes %lu");
    for (j=0; j < nbufindex; j++) {
    	if (strncmp(fmt, bufindex[j], 10) == 0) {
	    sscanf((const char *)bufindex[j], fmt, &proc_stat->processes);
	    break;
	}
    }

    /* success */
    return 0;
}
