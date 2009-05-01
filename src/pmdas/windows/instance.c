/*
 * Copyright (c) 2008 Aconex.  All Rights Reserved.
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Parts of this file contributed by Ken McDonell
 * (kenj At internode DoT on DoT net)
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
#include "hypnotoad.h"
#include <ctype.h>

pmInDom
windows_indom(int qid, int domain)
{
    int		serial;

    switch (qid) {
    case Q_DISK_DEV:		serial = DISK_INDOM; break;
    case Q_PERCPU:		serial = CPU_INDOM; break;
    case Q_NETWORK_IF:		serial = NETIF_INDOM; break;
    case Q_LDISK:		serial = FILESYS_INDOM; break;
    case Q_SQLSERVER_LOCK:	serial = SQL_LOCK_INDOM; break;
    case Q_SQLSERVER_CACHE:	serial = SQL_CACHE_INDOM; break;
    case Q_SQLSERVER_DB:	serial = SQL_DB_INDOM; break;
    case Q_PROCESSES:		serial = PROCESS_INDOM; break;
    case Q_THREADS:		serial = THREAD_INDOM; break;
    default:			return PM_INDOM_NULL;
    }
    return INDOM(domain, serial);
}

void
windows_instance_refresh(pmInDom indom)
{
    int		i;

    for (i = 0; i < metricdesc_sz; i++) {
	pdh_metric_t *mp = &metricdesc[i];

	if (indom != mp->desc.indom || mp->pat[0] == '\0')
	    continue;
	if (mp->flags & M_REDO) {
	    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
	    windows_check_metric(mp);
	}
	break;
    }
}

int
windows_check_instance(char *path, pdh_metric_t *mp)
{
    __pmInDom_int	*ip;
    char		*p, *q, *name;
    int			ok = 0;

    if (mp->desc.indom == PM_INDOM_NULL)
	return PM_IN_NULL;

    ip = (__pmInDom_int *)&mp->desc.indom;
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
		    return -1;
		}
		// inst = atoi(p);
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
			    __pmNotifyErr(LOG_ERR, "windows_check_instance: "
					"Error: DISK_INDOM malloc[%d] failed "
					"path=%s\n", q - p + 1, path);
			    return -1;
			}
			/*
			 * If more than one drive letter maps to the same
			 * logical disk (e.g. mirrored root),, the name
			 * contains spaces, e.g.
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
		__pmNotifyErr(LOG_ERR, "windows_check_instance: Error: "
				"unrecognized disk instance: %s\n", path);
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
		    return -1;
		}
		int inst = atoi(p);
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
		__pmNotifyErr(LOG_ERR, "windows_check_instance: Error: "
				"unrecognized cpu instance: %s\n", path);
		return -1;
	    }
	    break;

	/*
	 * Examples:
	 * \\WINNT\Network Interface(MS TCP Loopback interface)\Bytes Total/sec
	 */
	case NETIF_INDOM:
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
			__pmNotifyErr(LOG_ERR, "windows_check_instance: Error: "
					"malloc[%d] failed for NETIF_INDOM "
					"path=%s\n", q - p + 1, path);
			return -1;
		    }
		    /*
		     * The network interface names have many spaces and are
		     * not unique up to the first space by any means.  So,
		     * replace ' 's to play by the PCP instance name rules.
		     */
		    for (p = name; *p; p++) {
			if (*p == ' ') *p = '_';
		    }
		}
	    }
	    /*
	     * expecting something like ...\Network Interface(...)...
	     * don't know what to do with this one!
	     */
	    if (!ok) {
		__pmNotifyErr(LOG_ERR, "windows_check_instance: Error: "
			"unrecognized network interface instance: %s\n", path);
		return -1;
	    }
	    break;

	/*
	 * Examples:
	 * \\TOWER\LogicalDisk(C:)\% Free Space
	 */
	case FILESYS_INDOM:
	    p = strchr(path, '(');	// skip hostname and metric name
	    if (p != NULL) {
		p++;
		if (strncmp(p, "_Total)", 7) == 0) {
		    /*
		     * The totals value makes no semantic sense,
		     * just skip it here
		     */
		    return -1;
		}
		while (isascii(*p) && isdigit(*p))
		    p++;
		if (*p == ' ')
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
			__pmNotifyErr(LOG_ERR, "windows_check_instance: Error: "
				"malloc[%d] failed for LDISK_INDOM path=%s\n",
				q - p + 1, path);
			return -1;
		    }
		}
	    }
	    /*
	     * expecting something like ...\LogicalDisk(C:)...
	     * don't know what to do with this one!
	     */
	    if (!ok) {
		__pmNotifyErr(LOG_ERR, "windows_check_instance: Error: "
			"unrecognized logical disk instance: %s\n", path);
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
	    p = strchr(path, '(');	// skip hostname and metric name
	    if (p != NULL) {
		p++;
		if (strncmp(p, "_Total)", 7) == 0) {
		    /*
		     * The totals are done as independent metrics,
		     * just skip them here
		     */
		    return -1;
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
			__pmNotifyErr(LOG_ERR, "windows_check_instance: Error: "
				"malloc[%d] failed, SQL_INDOM path=%s\n",
				q - p + 1, path);
			return -1;
		    }
		}
	    }
	    /*
	     * expecting something like ... \SQLServer:...(...)\...
	     * don't know what to do with this one!
	     */
	    if (!ok) {
		__pmNotifyErr(LOG_ERR, "windows_check_instance: Error: "
				"unrecognized SQLServer instance: %s\n", path);
		return -1;
	    }
	    break;

	/*
	 * Per-process and per-thread instance domain
	 *
	 * Examples:
	 * \\TOWER\Process(svchost#6)\% Processor Time
	 * \\TOWER\Thread(svchost/1#1)\% Processor Time
	 * \\TOWER\Thread(Idle/0)\ID Process
	 * \\TOWER\Thread(Idle/0)\ID Thread
	 */
	case PROCESS_INDOM:
	case THREAD_INDOM:
	    p = strchr(path, '(');	// skip hostname and Process/Thread
	    if (p != NULL) {
		p++;
		if ((strncmp(p, "_Total)", 7) == 0 ||
		    (strncmp(p, "_Total/", 7) == 0)) {
		    /*
		     * The totals are done as independent metrics,
		     * just skip them here
		     */
		    return -1;
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
			__pmNotifyErr(LOG_ERR, "windows_check_instance: Error: "
				"malloc[%d] failed, process/thread path=%s\n",
				q - p + 1, path);
			return -1;
		    }
		}
	    }
	    /*
	     * expecting something like ... \Process(...)\...
	     * don't know what to do with this one!
	     */
	    if (!ok) {
		__pmNotifyErr(LOG_ERR, "windows_check_instance: Error: "
				"unrecognized process/thread name: %s\n", path);
		return -1;
	    }
	    break;

	default:
	    __pmNotifyErr(LOG_ERR, "windows_check_instance: Error: "
				   "pmInDom %s is unknown for metric %s\n",
			pmInDomStr(mp->desc.indom), pmIDStr(mp->desc.pmid));
	    return -1;
    }

    return pmdaCacheStore(mp->desc.indom, PMDA_CACHE_ADD, name, NULL);
}
