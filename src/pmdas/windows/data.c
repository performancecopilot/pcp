/*
 * Data structures that define metrics and control the Windows PMDA
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include <ctype.h>

#include "./shm.h"
#include "./domain.h"

shm_hdr_t		*shm;

/*
 * List of instance domains ... we expect *_INDOM macros from shm.h
 * to index into this table.
 * This is the copy used by the PMDA.
 */
pmdaIndom indomtab[] = {
    { DISK_INDOM, 0, NULL },
    { CPU_INDOM, 0, NULL },
    { NETIF_INDOM, 0, NULL },
    { LDISK_INDOM, 0, NULL },
    { SQL_LOCK_INDOM, 0, NULL },
    { SQL_CACHE_INDOM, 0, NULL },
    { SQL_DB_INDOM, 0, NULL },
};
int indomtab_sz = sizeof(indomtab) / sizeof(indomtab[0]);

pmdaMetric *metrictab;

/*
 * all metrics supported in this PMDA - one table entry for each metric
 */
static struct {
    pmDesc	desc;		// PMDA's idea of the semantics
    int		qid;		// PDH query group
    int		flags;		// PDH modifier flags
    char	*pat;		// PDH pattern
} metricdesc[] = {

/* kernel.all.cpu.user */
    { { PMDA_PMID(0,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE, "\\Processor(_Total)\\% User Time"
    },
/* kernel.all.cpu.idle */
    { { PMDA_PMID(0,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE, "\\Processor(_Total)\\% Idle Time"
    },
/* kernel.all.cpu.sys */
    { { PMDA_PMID(0,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE, "\\Processor(_Total)\\% Privileged Time"
    },
/* kernel.percpu.cpu.intr */
    { { PMDA_PMID(0,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE, "\\Processor(_Total)\\% Interrupt Time"
    },
/* kernel.percpu.cpu.user */
    { { PMDA_PMID(0,4), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE, "\\Processor(*/*#*)\\% User Time"
    },
/* kernel.percpu.cpu.idle */
    { { PMDA_PMID(0,5), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE,  "\\Processor(*/*#*)\\% Idle Time"
    },
/* kernel.percpu.cpu.sys */
    { { PMDA_PMID(0,6), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE,  "\\Processor(*/*#*)\\% Privileged Time"
    },
/* kernel.percpu.cpu.intr */
    { { PMDA_PMID(0,7), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE,  "\\Processor(*/*#*)\\% Interrupt Time"
    },
/* kernel.num_processes */
    { { PMDA_PMID(0,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_KERNEL, M_NONE,  "\\System\\Processes"
    },
/* kernel.num_threads */
    { { PMDA_PMID(0,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_KERNEL, M_NONE,  "\\System\\Threads"
    },
/* kernel.all.pswitch */
    { { PMDA_PMID(0,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_KERNEL, M_NONE,  "\\System\\Context Switches/sec"
    },
/* kernel.all.file.read */
    { { PMDA_PMID(0,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_KERNEL, M_NONE,  "\\System\\File Read Operations/sec"
    },
/* kernel.all.file.write */
    { { PMDA_PMID(0,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_KERNEL, M_NONE,  "\\System\\File Write Operations/sec"
    },
/* kernel.all.file.read_bytes */
    { { PMDA_PMID(0,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_KERNEL, M_NONE,  "\\System\\File Read Bytes/sec"
    },
/* kernel.all.file.write_bytes */
    { { PMDA_PMID(0,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_KERNEL, M_NONE,  "\\System\\File Write Bytes/sec"
    },
/* disk.all.read */
    { { PMDA_PMID(0,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_ALL, M_NONE,  "\\PhysicalDisk(_Total)\\Disk Reads/sec"
    },
/* disk.all.write */
    { { PMDA_PMID(0,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_ALL, M_NONE,  "\\PhysicalDisk(_Total)\\Disk Writes/sec"
    },
/* disk.all.total */
    { { PMDA_PMID(0,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_ALL, M_NONE,  "\\PhysicalDisk(_Total)\\Disk Transfers/sec"
    },
/* disk.all.read_bytes */
    { { PMDA_PMID(0,18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_DISK_ALL, M_NONE,  "\\PhysicalDisk(_Total)\\Disk Read Bytes/sec"
    },
/* disk.all.write_bytes */
    { { PMDA_PMID(0,19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_DISK_ALL, M_NONE,  "\\PhysicalDisk(_Total)\\Disk Write Bytes/sec"
    },
/* disk.all.total_bytes */
    { { PMDA_PMID(0,20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_DISK_ALL, M_NONE,  "\\PhysicalDisk(_Total)\\Disk Bytes/sec"
    },
/* disk.dev.read */
    { { PMDA_PMID(0,21), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_DEV, M_NONE,  "\\PhysicalDisk(*/*#*)\\Disk Reads/sec"
    },
/* disk.dev.write */
    { { PMDA_PMID(0,22), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_DEV, M_NONE,  "\\PhysicalDisk(*/*#*)\\Disk Writes/sec"
    },
/* disk.dev.total */
    { { PMDA_PMID(0,23), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_DEV, M_NONE,  "\\PhysicalDisk(*/*#*)\\Disk Transfers/sec"
    },
/* disk.dev.read_bytes */
    { { PMDA_PMID(0,24), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_DISK_DEV, M_NONE,  "\\PhysicalDisk(*/*#*)\\Disk Read Bytes/sec"
    },
/* disk.dev.write_bytes */
    { { PMDA_PMID(0,25), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_DISK_DEV, M_NONE,  "\\PhysicalDisk(*/*#*)\\Disk Write Bytes/sec"
    },
/* disk.dev.total_bytes */
    { { PMDA_PMID(0,26), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_DISK_DEV, M_NONE,  "\\PhysicalDisk(*/*#*)\\Disk Bytes/sec"
    },
/* mem.page_faults */
    { { PMDA_PMID(0,27), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE,  "\\Memory\\Page Faults/sec"
    },
/* mem.available */
    { { PMDA_PMID(0,28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_MBYTE, 0, 0)
      }, Q_MEMORY, M_NONE,  "\\Memory\\Available MBytes"
    },
/* mem.committed_bytes */
    { { PMDA_PMID(0,29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_MEMORY, M_NONE,  "\\Memory\\Committed Bytes"
    },
/* mem.pool.paged_bytes */
    { { PMDA_PMID(0,30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_MEMORY, M_NONE,  "\\Memory\\Pool Paged Bytes"
    },
/* mem.pool.non_paged_bytes */
    { { PMDA_PMID(0,31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_MEMORY, M_NONE,  "\\Memory\\Pool Nonpaged Bytes"
    },
/* mem.cache.lazy_writes */
    { { PMDA_PMID(0,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE,  "\\Cache\\Lazy Write Flushes/sec"
    },
/* mem.cache.lazy_write_pages */
    { { PMDA_PMID(0,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE,  "\\Cache\\Lazy Write Pages/sec"
    },
/* mem.cache.mdl.read */
    { { PMDA_PMID(0,34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE,  "\\Cache\\MDL Reads/sec"
    },
/* mem.cache.read_ahead */
    { { PMDA_PMID(0,35), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE,  "\\Cache\\Read Aheads/sec"
    },
/* mem.cache.mdl.sync_read */
    { { PMDA_PMID(0,36), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE,  "\\Cache\\Sync MDL Reads/sec"
    },
/* mem.cache.mdl.async_read */
    { { PMDA_PMID(0,37), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE,  "\\Cache\\Async MDL Reads/sec"
    },
/* network.interface.in.packets */
    { { PMDA_PMID(0,38), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE,  "\\Network Interface(*/*#*)\\Packets Received/sec"
    },
/* network.interface.in.bytes */
    { { PMDA_PMID(0,39), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_MEMORY, M_NONE,  "\\Network Interface(*/*#*)\\Bytes Received/sec"
    },
/* network.interface.in.errors */
    { { PMDA_PMID(0,40), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE,  "\\Network Interface(*/*#*)\\Packets Received Errors"
    },
/* network.interface.out.packets */
    { { PMDA_PMID(0,41), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE,  "\\Network Interface(*/*#*)\\Packets Sent/sec"
    },
/* network.interface.out.bytes */
    { { PMDA_PMID(0,42), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_MEMORY, M_NONE,  "\\Network Interface(*/*#*)\\Bytes Sent/sec"
    },
/* network.interface.out.errors */
    { { PMDA_PMID(0,43), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE,  "\\Network Interface(*/*#*)\\Packets Outbound Errors"
    },
/* network.interface.total.packets */
    { { PMDA_PMID(0,44), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE,  "\\Network Interface(*/*#*)\\Packets/sec"
    },
/* network.interface.total.bytes */
    { { PMDA_PMID(0,45), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_MEMORY, M_NONE, "\\Network Interface(*/*#*)\\Bytes Total/sec"
    },
/* sqlserver.buf_mgr.cache_hit_ratio */
    { { PMDA_PMID(0,46), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Buffer cache hit ratio"
    },
/* sqlserver.buf_mgr.page_lookups */
    { { PMDA_PMID(0,47), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Page lookups/sec"
    },
/* sqlserver.buf_mgr.free_list_stalls */
    { { PMDA_PMID(0,48), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Free list stalls/sec"
    },
/* sqlserver.buf_mgr.free_pages */
    { { PMDA_PMID(0,49), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Free pages"
    },
/* sqlserver.buf_mgr.total_pages */
    { { PMDA_PMID(0,50), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Total pages"
    },
/* sqlserver.buf_mgr.target_pages */
    { { PMDA_PMID(0,51), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Target pages"
    },
/* sqlserver.buf_mgr.database_pages */
    { { PMDA_PMID(0,52), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Database pages"
    },
/* sqlserver.buf_mgr.reserved_pages */
    { { PMDA_PMID(0,53), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Reserved pages"
    },
/* sqlserver.buf_mgr.stolen_pages */
    { { PMDA_PMID(0,54), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Stolen pages"
    },
/* sqlserver.buf_mgr.lazy_writes */
    { { PMDA_PMID(0,55), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Lazy writes/sec"
    },
/* sqlserver.buf_mgr.readahead_pages */
    { { PMDA_PMID(0,56), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Readahead pages/sec"
    },
/* sqlserver.buf_mgr.procedure_cache_pages */
    { { PMDA_PMID(0,57), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Procedure cache pages"
    },
/* sqlserver.buf_mgr.page_reads */
    { { PMDA_PMID(0,58), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Page reads/sec"
    },
/* sqlserver.buf_mgr.page_writes */
    { { PMDA_PMID(0,59), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Page writes/sec"
    },
/* sqlserver.buf_mgr.checkpoint_pages */
    { { PMDA_PMID(0,60), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Checkpoint pages/sec"
    },
/*  sqlserver.buf_mgr.awe.lookup_maps */
    { { PMDA_PMID(0,61), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\AWE lookup maps/sec"
    },
/* sqlserver.buf_mgr.awe.stolen_maps */
    { { PMDA_PMID(0,62), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\AWE stolen maps/sec"
    },
/* sqlserver.buf_mgr.awe.write_maps */
    { { PMDA_PMID(0,63), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\AWE write maps/sec"
    },
/* sqlserver.buf_mgr.awe.unmap_calls */
    { { PMDA_PMID(0,64), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\AWE unmap calls/sec"
    },
/* sqlserver.buf_mgr.awe.unmap_pages */
    { { PMDA_PMID(0,65), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\AWE unmap pages/sec"
    },
/* sqlserver.buf_mgr.page_life_expectancy */
    { { PMDA_PMID(0,66), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Buffer Manager\\Page life expectancy"
    },
/* filesys.full */
    { { PMDA_PMID(0,67), PM_TYPE_FLOAT, LDISK_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_LDISK, M_NONE, "\\LogicalDisk(*/*#*)\\% Free Space"
    },
/* disk.dev.idle */
    { { PMDA_PMID(0,68), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_DISK_DEV, M_NONE,  "\\PhysicalDisk(*/*#*)\\% Idle Time"
    },
/* sqlserver.locks.all.requests */
    { { PMDA_PMID(0,69), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Locks(_Total)\\Lock Requests/sec"
    },
/* sqlserver.locks.all.waits */
    { { PMDA_PMID(0,70), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Locks(_Total)\\Lock Waits/sec"
    },
/* sqlserver.locks.all.deadlocks */
    { { PMDA_PMID(0,71), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Locks(_Total)\\Number of Deadlocks/sec"
    },
/* sqlserver.locks.all.timeouts */
    { { PMDA_PMID(0,72), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Locks(_Total)\\Lock Timeouts/sec"
    },
/* sqlserver.locks.all.wait_time */
    { { PMDA_PMID(0,73), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Locks(_Total)\\Lock Wait Time (ms)"
    },
#if DELTAV_OVER_DELTAV
/* sqlserver.locks.all.avg_wait */
    { { PMDA_PMID(0,74), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Locks(_Total)\\Average Wait Time (ms)"
    },
#endif
/* sqlserver.locks.region.requests */
    { { PMDA_PMID(0,75), PM_TYPE_U32, SQL_LOCK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Locks(*/*#*)\\Lock Requests/sec"
    },
/* sqlserver.locks.region.waits */
    { { PMDA_PMID(0,76), PM_TYPE_U32, SQL_LOCK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Locks(*/*#*)\\Lock Waits/sec"
    },
/* sqlserver.locks.region.deadlocks */
    { { PMDA_PMID(0,77), PM_TYPE_U32, SQL_LOCK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Locks(*/*#*)\\Number of Deadlocks/sec"
    },
/* sqlserver.locks.region.timeouts */
    { { PMDA_PMID(0,78), PM_TYPE_U32, SQL_LOCK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Locks(*/*#*)\\Lock Timeouts/sec"
    },
/* sqlserver.locks.region.wait_time */
    { { PMDA_PMID(0,79), PM_TYPE_U32, SQL_LOCK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Locks(*/*#*)\\Lock Wait Time (ms)"
    },
#if DELTAV_OVER_DELTAV
/* sqlserver.locks.region.avg_wait */
    { { PMDA_PMID(0,80), PM_TYPE_FLOAT, SQL_LOCK_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Locks(*/*#*)\\Average Wait Time (ms)"
    },
#endif
/* sqlserver.cache_mgr.all.cache_hit_ratio */
    { { PMDA_PMID(0,81), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Cache Manager(_Total)\\Cache Hit Ratio"
    },
/* sqlserver.cache_mgr.cache.cache_hit_ratio */
    { { PMDA_PMID(0,82), PM_TYPE_FLOAT, SQL_CACHE_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Cache Manager(*/*#*)\\Cache Hit Ratio"
    },
/* sqlserver.connections */
    { { PMDA_PMID(0,83), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE,  "\\SQLServer:General Statistics\\User Connections"
    },
/* sqlserver.databases.all.transactions */
    { { PMDA_PMID(0,84), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Databases(_Total)\\Transactions/sec"
    },
/* sqlserver.databases.db.transactions */
    { { PMDA_PMID(0,85), PM_TYPE_U32, SQL_DB_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Databases(*/*#*)\\Transactions/sec"
    },
/* sqlserver.sql.batch_requests */
    { { PMDA_PMID(0,86), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:SQL Statistics\\Batch Requests/sec"
    },
/* sqlserver.latches.waits */
    { { PMDA_PMID(0,87), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Latches\\Latch Waits/sec"
    },
/* sqlserver.latches.wait_time */
    { { PMDA_PMID(0,88), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Latches\\Total Latch Wait Time (ms)"
    },
#if DELTAV_OVER_DELTAV
/* sqlserver.latches.avg_wait_time */
    { { PMDA_PMID(0,89), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Latches\\Average Latch Wait Time (ms)"
    },
#endif
/* sqlserver.databases.all.data_file_size */
    { { PMDA_PMID(0,90), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, Q_SQLSERVER, M_NONE,  "\\SQLServer:Databases(_Total)\\Data File(s) Size (KB)"
    },
/* sqlserver.databases.all.log_file_size */
    { { PMDA_PMID(0,91), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, Q_SQLSERVER, M_NONE,  "\\SQLServer:Databases(_Total)\\Log File(s) Size (KB)"
    },
/* sqlserver.databases.all.log_file_used */
    { { PMDA_PMID(0,92), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, Q_SQLSERVER, M_NONE,  "\\SQLServer:Databases(_Total)\\Log File(s) Used Size (KB)"
    },
/* sqlserver.databases.db.data_file_size */
    { { PMDA_PMID(0,93), PM_TYPE_U32, SQL_DB_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, Q_SQLSERVER, M_NONE,  "\\SQLServer:Databases(*/*#*)\\Data File(s) Size (KB)"
    },
/* sqlserver.databases.db.log_file_size */
    { { PMDA_PMID(0,94), PM_TYPE_U32, SQL_DB_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, Q_SQLSERVER, M_NONE,  "\\SQLServer:Databases(*/*#*)\\Log File(s) Size (KB)"
    },
/* sqlserver.databases.db.log_file_used */
    { { PMDA_PMID(0,95), PM_TYPE_U32, SQL_DB_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, Q_SQLSERVER, M_NONE,  "\\SQLServer:Databases(*/*#*)\\Log File(s) Used Size (KB)"
    },
/* sqlserver.sql.compilations */
    { { PMDA_PMID(0,96), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:SQL Statistics\\SQL Compilations/sec"
    },
/* sqlserver.sql.re_compilations */
    { { PMDA_PMID(0,97), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:SQL Statistics\\SQL Re-Compilations/sec"
    },
/* sqlserver.access.full_scans */
    { { PMDA_PMID(0,98), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Access Methods\\Full Scans/sec"
    },
/* sqlserver.access.pages_allocated */
    { { PMDA_PMID(0,99), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Access Methods\\Pages Allocated/sec"
    },
/* sqlserver.access.table_lock_escalations */
    { { PMDA_PMID(0,100), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Access Methods\\Table Lock Escalations/sec"
    },
/* disk.dev.queuelen */
    { { PMDA_PMID(0,101), PM_TYPE_U32, DISK_INDOM, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_DEV, M_NONE,  "\\PhysicalDisk(*/*#*)\\Current Disk Queue Length"
    },
/* sqlserver.databases.all.log_flushes */
    { { PMDA_PMID(0,102), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Databases(_Total)\\Log Flushes/sec"
    },
/* sqlserver.databases.db.log_flushes */
    { { PMDA_PMID(0,103), PM_TYPE_U32, SQL_DB_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Databases(*/*#*)\\Log Flushes/sec"
    },
/* sqlserver.databases.all.log_bytes_flushed */
    { { PMDA_PMID(0,104), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Databases(_Total)\\Log Bytes Flushed/sec"
    },
/* sqlserver.databases.db.log_bytes_flushed */
    { { PMDA_PMID(0,105), PM_TYPE_U32, SQL_DB_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Databases(*/*#*)\\Log Bytes Flushed/sec"
    },
/* hinv.physmem */
    { { PMDA_PMID(0,106), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_MBYTE, 0, 0)
      }, Q_MEMORY, M_NONE,  ""
    },
/* hinv.ncpu */
    { { PMDA_PMID(0,107), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_KERNEL, M_NONE, ""
    },
/* hinv.ndisk */
    { { PMDA_PMID(0,108), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_KERNEL, M_NONE, ""
    },
/* kernel.uname.distro */
    { { PMDA_PMID(0,109), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_KERNEL, M_NONE, ""
    },
/* kernel.uname.release */
    { { PMDA_PMID(0,110), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_KERNEL, M_NONE, ""
    },
/* kernel.uname.version */
    { { PMDA_PMID(0,111), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0)
      }, Q_KERNEL, M_NONE, ""
    },
/* kernel.uname.sysname */
    { { PMDA_PMID(0,112), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0)
      }, Q_KERNEL, M_NONE, ""
    },
/* kernel.uname.machine */
    { { PMDA_PMID(0,113), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0)
      }, Q_KERNEL, M_NONE, ""
    },
/* kernel.uname.nodename */
    { { PMDA_PMID(0,114), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0)
      }, Q_KERNEL, M_NONE, ""
    },

/* pmda.uname */
    { { PMDA_PMID(0,115), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0)
      }, Q_KERNEL, M_NONE, ""
    },
/* pmda.version */
    { { PMDA_PMID(0,116), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0)
      }, Q_KERNEL, M_NONE, ""
    },

/* filesys.capacity */
    { { PMDA_PMID(0,117), PM_TYPE_U64, LDISK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)
      }, Q_LDISK, M_NONE, ""
    },
/* filesys.used */
    { { PMDA_PMID(0,118), PM_TYPE_U64, LDISK_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)
      }, Q_LDISK, M_NONE, ""
    },
/* filesys.free */
    { { PMDA_PMID(0,119), PM_TYPE_U64, LDISK_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)
      }, Q_LDISK, M_NONE, ""
    },
/* dummy - filesys.free_space */
    { { PMDA_PMID(0,120), PM_TYPE_U32, LDISK_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_MBYTE,0,0)
      }, Q_LDISK, M_NONE, "\\LogicalDisk(*/*#*)\\Free Megabytes"
    },
/* dummy - filesys.free_percent */
    { { PMDA_PMID(0,121), PM_TYPE_FLOAT, LDISK_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0)
      }, Q_LDISK, M_NONE, "\\LogicalDisk(*/*#*)\\% Free Space"
    },
/* sqlserver.access.page_splits */
    { { PMDA_PMID(0,122), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\SQLServer:Access Methods\\Page Splits/sec"
    },
/* network.tcp.activeopens */
    { { PMDA_PMID(0,123), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\TCPv4\\Connections Active"
    },
/* network.tcp.passiveopens */
    { { PMDA_PMID(0,124), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\TCPv4\\Connections Passive"
    },
/* network.tcp.attemptfails */
    { { PMDA_PMID(0,125), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\TCPv4\\Connection Failures"
    },
/* network.tcp.estabresets */
    { { PMDA_PMID(0,126), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\TCPv4\\Connections Reset"
    },
/* network.tcp.currestab */
    { { PMDA_PMID(0,127), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\TCPv4\\Connections Established"
    },
/* network.tcp.insegs */
    { { PMDA_PMID(0,128), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\TCPv4\\Segments Received/sec"
    },
/* network.tcp.outsegs */
    { { PMDA_PMID(0,129), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\TCPv4\\Segments Sent/sec"
    },
/* network.tcp.totalsegs */
    { { PMDA_PMID(0,130), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\TCPv4\\Segments/sec"
    },
/* network.tcp.retranssegs */
    { { PMDA_PMID(0,131), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER, M_NONE, "\\TCPv4\\Segments Retransmitted/sec"
    },

/* disk.all.split_io */
    { { PMDA_PMID(0,132), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_ALL, M_NONE,  "\\PhysicalDisk(_Total)\\Split IO/Sec"
    },
/* disk.dev.split_io */
    { { PMDA_PMID(0,133), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_DEV, M_NONE,  "\\PhysicalDisk(*/*#*)\\Split IO/Sec"
    },

};

int metrictab_sz = sizeof(metricdesc) / sizeof(metricdesc[0]);

void
init_data(int domain)
{
    int			i;
    int			serial;
    __pmID_int		*ip;
    __pmInDom_int	*iip;
    shm_metric_t	*sp;

    /*
     * Create shared memory version of the per-metric table.
     *
     * Also do domain initialization for each pmid and indom element of
     * the shared memory table ... the PMDA table is fixed up in
     * libpcp_pmda
     */
    sp = (shm_metric_t *)&((char *)shm)[shm->segment[SEG_METRICS].base];
    for (i = 0; i < metrictab_sz; i++) {
	sp[i].m_desc = metricdesc[i].desc;
	ip = (__pmID_int *)&sp[i].m_desc.pmid;
	ip->domain = domain;
	if (sp[i].m_desc.indom != PM_INDOM_NULL) {
	    serial = sp[i].m_desc.indom;
	    iip = (__pmInDom_int *)&sp[i].m_desc.indom;
	    iip->serial = serial;
	    iip->pad = 0;
	    iip->domain = domain;
	}
	sp[i].m_qid = metricdesc[i].qid;
	sp[i].m_flags = metricdesc[i].flags;
	if (strlen(metricdesc[i].pat)+1 > MAX_M_PATH_LEN) {
	    fprintf(stderr, "init_data: Warning: pmid %s pattern=\"%s\" too long (exceeds max %d for shm structs)\n",
		pmIDStr(metricdesc[i].desc.pmid), metricdesc[i].pat,
		MAX_M_PATH_LEN);
	    fflush(stderr);
	}
	strncpy(sp[i].m_pat, metricdesc[i].pat, MAX_M_PATH_LEN);
    }

    /*
     * create the PMDA's metrictab[] version of the per-metric table
     */
    if ((metrictab = (pmdaMetric *)malloc(metrictab_sz * sizeof(pmdaMetric))) == NULL) {
	fprintf(stderr, "init_data: Error: malloc metrictab [%d] failed: %s\n",
	    metrictab_sz * sizeof(pmdaMetric), strerror(errno));
	exit(1);
    }
    for (i = 0; i < metrictab_sz; i++) {
	metrictab[i].m_user = NULL;
	metrictab[i].m_desc = metricdesc[i].desc;
    }
}
