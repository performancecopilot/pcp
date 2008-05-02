/*
 * Instance domain support for shim.exe.
 *
 * Parts of this file contributed by Ken McDonell
 * (kenj At internode DoT on DoT net)
 *
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
 */

#include "./shim.h"
#include <ctype.h>

extern int domain;

#define GIMME_NEXT_INST -99

/*
 * List of instance domains ... we expect *_INDOM macros from shm.h
 * to index into this table.
 * This is the copy used by shim.exe, and needs to match the copy in
 * data.c
 */
static pmInDom indomtab[] = {
    DISK_INDOM,
    CPU_INDOM,
    NETIF_INDOM,
    LDISK_INDOM,
    SQL_LOCK_INDOM,
    SQL_CACHE_INDOM,
    SQL_DB_INDOM,
};
static int indomtab_sz = sizeof(indomtab) / sizeof(indomtab[0]);

static int
update_indom(pmInDom indom, int *instp, char *name)
{
    int		s;
    int		i;
    int		seg;
    int		inst;
    int		numinst;
    shm_inst_t	*ip;

    inst = *instp;

    for (s = 0; s < indomtab_sz; s++) {
	if (indomtab[s] == indom)
	    break;
    }
    if (s == indomtab_sz) {
	/* this is fatal! */
	fprintf(stderr, "update_indom: Fatal: pmInDom %s is not in indomtab[]\n",
	    pmInDomStr(indom));
	for (s = 0; s < indomtab_sz; s++) {
	    fprintf(stderr, "  [%d] %s (0x%x)\n", s, pmInDomStr(indomtab[s]), indomtab[s]);
	}
	exit(1);
    }

    seg = SEG_INDOM + s;
    ip = (shm_inst_t *)&((char *)shm)[shm->segment[seg].base];
    numinst = ip[0].i_inst;	// really numinst hiding here

#ifdef PCP_DEBUG
    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	/* desperate */
	fprintf(stderr, "update_indom(%s, inst=%d, name=\"%s\")\n",
		pmInDomStr(indom), inst, name);
	fprintf(stderr, "  seg=%d numinst=%d\n", seg, numinst);
	fflush(stderr);
    }
#endif

    for (i = 1; i <= numinst; i++) {
	if (strcmp(ip[i].i_name, name) == 0) {
	    if (inst == GIMME_NEXT_INST)
		*instp = inst = ip[i].i_inst;
	    if (ip[i].i_inst == inst) {
		/*
		 * matches and already in the instance domain
		 */
		free(name);
		return 1;
	    }
	    fprintf(stderr, "update_indom: Warning: indom %s inst %d name (%s) differs to previous name (%s)\n",
		pmInDomStr(indom), inst, name, ip[i].i_name);
	    strncpy(ip[i].i_name, name, MAX_INST_NAME_LEN);
	    ip[0].i_name[0] = 'c';		// mark indom changed
	    return 1;
	}
    }

    if (inst == GIMME_NEXT_INST)
	*instp = inst = numinst;

    numinst++;
    memcpy(new_hdr, shm, hdr_size);
    new_hdr->segment[seg].nelt = numinst+1;
    new_hdr->size += sizeof(shm_inst_t);
    shm_reshape(new_hdr);
    ip = (shm_inst_t *)&((char *)shm)[shm->segment[seg].base];

    ip[0].i_inst = numinst;		// real number of instances
    ip[0].i_name[0] = 'c';		// mark indom changed

    ip[numinst].i_inst = inst;
    strncpy(ip[numinst].i_name, name, MAX_INST_NAME_LEN);
    if (strlen(name)+1 > MAX_INST_NAME_LEN) {
	fprintf(stderr, "update_indom: Warning: name=\"%s\" too long (exceeds max %d for shm structs)\n", name, MAX_INST_NAME_LEN);
	fflush(stderr);
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "update_indom: add indom %s numinst=%d inst=%d name=\"%s\"\n",
	    pmInDomStr(indom), numinst, inst, name);
	ip = (shm_inst_t *)&((char *)shm)[shm->segment[SEG_INDOM].base];
	fprintf(stderr, "check disk ctl %d '%c'\n", ip[0].i_inst, ip[0].i_name[0]);
    }
#endif

    return 1;
}

int
check_instance(char *path, shm_metric_t *sp, int *instp)
{
    __pmInDom_int	*ip;
    char		*p;
    char		*q;
    int			inst;
    char		*name;
    int			ok = 0;
    static int		first = 1;

    if (first) {
	int	s;
	int	serial;
	/*
	 * copied from libpcp_pmda (open.c) ... need to be endian
	 * safe here
	 */
	for (s = 0; s < indomtab_sz; s++) {
	    serial = indomtab[s];
	    ip = (__pmInDom_int *)&indomtab[s];
	    ip->serial = serial;
	    ip->pad = 0;
	    ip->domain = domain;
	}
	first = 0;
    }

    if (sp->m_desc.indom == PM_INDOM_NULL) {
	return PM_IN_NULL;
    }

    ip = (__pmInDom_int *)&sp->m_desc.indom;
    switch (ip->serial) {
	/*
	 * Examples:
	 * \\WINBUILD\PhysicalDisk(0 C:)\Disk Reads/sec
	 * \\SOMEHOST\PhysicalDisk(0 C: D: E:)\Disk Transfers/sec
	 * \\WINBUILD\PhysicalDisk(_Total)\Disk Write Bytes/sec
	 */
	case DISK_INDOM:
	    p = strchr(path, '(');	// skip hostname and metric name
	    if (p != NULL) {
		p++;
		if (strncmp(p, "_Total)", 7) == 0) {
		    /*
		     * The totals get enumerated in the per disk path
		     * expansion, just skip 'em here
		     */
		    return 0;
		}
		inst = atoi(p);
		while (isascii(*p) && isdigit(*p)) p++;
		if (*p == ' ') {
		    p++;
		    q = strchr(p, ')');
		    if (q != NULL) {
			name = (char *)malloc(q - p + 1);
			if (name != NULL) {
			    strncpy(name, p, q - p);
			    name[q - p] = '\0';
			    ok = 1;
			}
			else {
			    fprintf(stderr, "check_instance: malloc[%d] failed for DISK_INDOM path=%s\n", q - p + 1, path);
			    exit(1);
			}
			/*
			 * If more than one drive letter maps to the same
			 * physical disk, the name contains spaces, e.g.
			 * "C: D:" ... replace ' ' by '_' to play by the
			 * PCP rules for instance names
			 */
			for (p = name; *p; p++) {
			    if (*p == ' ') *p = '_';
			}
		    }
		}
	    }
	    /*
	     * expecting something like ...\PhysicalDisk(N name)...
	     * don't know what to do with this one!
	     */
	    if (!ok) {
		fprintf(stderr, "check_instance: unrecognized disk instance: %s\n", path);
		return -1;
	    }
	    break;

	/*
	 * Examples:
	 * \\WINBUILD\Processor(0)\% User Time
	 * \\WINBUILD\Processor(_Total)\Interrupts/sec
	 */
	case CPU_INDOM:
	    p = strchr(path, '(');	// skip hostname and metric name
	    if (p != NULL) {
		p++;
		if (strncmp(p, "_Total)", 7) == 0) {
		    /*
		     * The totals get enumerated in the per cpu path
		     * expansion, just skip 'em here
		     */
		    return 0;
		}
		inst = atoi(p);
		name = (char *)malloc(6);	// "cpuNN"
		if (name != NULL) {
		    sprintf(name, "cpu%d", inst);
		}
		ok = 1;
	    }
	    /*
	     * expecting something like ...\Processor(N)...
	     * don't know what to do with this one!
	     */
	    if (!ok) {
		fprintf(stderr, "check_instance: unrecognized cpu instance: %s\n", path);
		return -1;
	    }
	    break;

	/*
	 * Examples:
	 * \\WINBUILD\Network Interface(MS TCP Loopback interface)\Bytes Total/sec
	 */
	case NETIF_INDOM:
	    inst = GIMME_NEXT_INST;
	    p = strchr(path, '(');	// skip hostname and metric name
	    if (p != NULL) {
		p++;
		q = strchr(p, ')');
		if (q != NULL) {
		    name = (char *)malloc(q - p + 1);
		    if (name != NULL) {
			strncpy(name, p, q - p);
			name[q - p] = '\0';
			ok = 1;
		    }
		    else {
			fprintf(stderr, "check_instance: malloc[%d] failed for NETIF_INDOM path=%s\n", q - p + 1, path);
			exit(1);
		    }
		}
	    }
	    /*
	     * expecting something like ...\Network Interface(...)...
	     * don't know what to do with this one!
	     */
	    if (!ok) {
		fprintf(stderr, "check_instance: unrecognized network interface instance: %s\n", path);
		return -1;
	    }
	    break;

	/*
	 * Examples:
	 * \\TOWER\LogicalDisk(C:)\% Free Space
	 */
	case LDISK_INDOM:
	    inst = GIMME_NEXT_INST;
	    p = strchr(path, '(');	// skip hostname and metric name
	    if (p != NULL) {
		p++;
		if (strncmp(p, "_Total)", 7) == 0) {
		    /*
		     * The totals value makes no semantic sense,
		     * just skip it here
		     */
		    return 0;
		}
		q = strchr(p, ')');
		if (q != NULL) {
		    name = (char *)malloc(q - p + 1);
		    if (name != NULL) {
			strncpy(name, p, q - p);
			name[q - p] = '\0';
			ok = 1;
		    }
		    else {
			fprintf(stderr, "check_instance: malloc[%d] failed for LDISK_INDOM path=%s\n", q - p + 1, path);
			exit(1);
		    }
		}
	    }
	    /*
	     * expecting something like ...\LogicalDisk(C:)...
	     * don't know what to do with this one!
	     */
	    if (!ok) {
		fprintf(stderr, "check_instance: unrecognized logical disk instance: %s\n", path);
		return -1;
	    }
	    break;

	/*
	 * SQLServer instance domains all have similar syntax
	 *
	 * Examples:
	 * \\TOWER\SQLServer:Locks(Table)\Average Wait Time (ms)
	 * \\TOWER\SQLServer:Cache Manager(Cursors)\Cache Hit Ratio
	 * \\TOWER\SQLServer:Databases(ACONEX_SYS)\Transactions/sec
	 */
	case SQL_LOCK_INDOM:
	case SQL_CACHE_INDOM:
	case SQL_DB_INDOM:
	    inst = GIMME_NEXT_INST;
	    p = strchr(path, '(');	// skip hostname and metric name
	    if (p != NULL) {
		p++;
		if (strncmp(p, "_Total)", 7) == 0) {
		    /*
		     * The totals are done as independent metrics,
		     * just skip them here
		     */
		    return 0;
		}
		q = strchr(p, ')');
		if (q != NULL) {
		    name = (char *)malloc(q - p + 1);
		    if (name != NULL) {
			strncpy(name, p, q - p);
			name[q - p] = '\0';
			ok = 1;
		    }
		    else {
			fprintf(stderr, "check_instance: malloc[%d] failed for SQL_LOCK_INDOM path=%s\n", q - p + 1, path);
			exit(1);
		    }
		}
	    }
	    /*
	     * expecting something like ... \SQLServer:...(...)\...
	     * don't know what to do with this one!
	     */
	    if (!ok) {
		fprintf(stderr, "check_instance: unrecognized SQLServer instance: %s\n", path);
		return -1;
	    }
	    break;

	default:
	    /* this is fatal! */
	    fprintf(stderr, "check_instance: Fatal: pmInDom %s is unknown for metric %s\n",
		pmInDomStr(sp->m_desc.indom), pmIDStr(sp->m_desc.pmid));
	    exit(1);
    }

    /*
     * check and update indom ... may free name if not needed
     */
    if (update_indom(sp->m_desc.indom, &inst, name)) {
	*instp = inst;
	return 1;
    }
    else
	return 0;
}

