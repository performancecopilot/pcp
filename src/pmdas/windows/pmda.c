/*
 * Windows PMDA
 *
 * Copyright (c) 2008 Aconex.  All Rights Reserved.
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

static int isDSO = 1;
char windows_uname[32];
char windows_build[32];
unsigned long windows_pagesize;
unsigned long long windows_physmem;

pdh_query_t querydesc[Q_NUMQUERIES];
int querydesc_sz;

/*
 * Array of all metrics - the PMID item field indexes this directly.
 */
pdh_metric_t metricdesc[] = {
/* kernel.all.cpu.user */
    { { PMDA_PMID(0,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL,
      "\\Processor(_Total)\\% User Time"
    },
/* kernel.all.cpu.idle */
    { { PMDA_PMID(0,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL,
      "\\Processor(_Total)\\% Idle Time"
    },
/* kernel.all.cpu.sys */
    { { PMDA_PMID(0,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL,
      "\\Processor(_Total)\\% Privileged Time"
    },
/* kernel.percpu.cpu.intr */
    { { PMDA_PMID(0,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL,
      "\\Processor(_Total)\\% Interrupt Time"
    },
/* kernel.percpu.cpu.user */
    { { PMDA_PMID(0,4), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL,
      "\\Processor(*/*#*)\\% User Time"
    },
/* kernel.percpu.cpu.idle */
    { { PMDA_PMID(0,5), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL,
      "\\Processor(*/*#*)\\% Idle Time"
    },
/* kernel.percpu.cpu.sys */
    { { PMDA_PMID(0,6), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL,
      "\\Processor(*/*#*)\\% Privileged Time"
    },
/* kernel.percpu.cpu.intr */
    { { PMDA_PMID(0,7), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL,
      "\\Processor(*/*#*)\\% Interrupt Time"
    },
/* kernel.num_processes */
    { { PMDA_PMID(0,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_KERNEL, M_NONE, 0, 0, NULL,
      "\\System\\Processes"
    },
/* kernel.num_threads */
    { { PMDA_PMID(0,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_KERNEL, M_NONE, 0, 0, NULL,
      "\\System\\Threads"
    },
/* kernel.all.pswitch */
    { { PMDA_PMID(0,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_KERNEL, M_NONE, 0, 0, NULL,
      "\\System\\Context Switches/sec"
    },
/* kernel.all.file.read */
    { { PMDA_PMID(0,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_KERNEL, M_NONE,  0, 0, NULL,
      "\\System\\File Read Operations/sec"
    },
/* kernel.all.file.write */
    { { PMDA_PMID(0,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_KERNEL, M_NONE, 0, 0, NULL,
      "\\System\\File Write Operations/sec"
    },
/* kernel.all.file.read_bytes */
    { { PMDA_PMID(0,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL,
      "\\System\\File Read Bytes/sec"
    },
/* kernel.all.file.write_bytes */
    { { PMDA_PMID(0,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL,
      "\\System\\File Write Bytes/sec"
    },
/* disk.all.read */
    { { PMDA_PMID(0,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_ALL, M_NONE, 0, 0, NULL,
      "\\PhysicalDisk(_Total)\\Disk Reads/sec"
    },
/* disk.all.write */
    { { PMDA_PMID(0,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_ALL, M_NONE, 0, 0, NULL,
      "\\PhysicalDisk(_Total)\\Disk Writes/sec"
    },
/* disk.all.total */
    { { PMDA_PMID(0,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_ALL, M_NONE, 0, 0, NULL,
      "\\PhysicalDisk(_Total)\\Disk Transfers/sec"
    },
/* disk.all.read_bytes */
    { { PMDA_PMID(0,18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_DISK_ALL, M_NONE, 0, 0, NULL,
      "\\PhysicalDisk(_Total)\\Disk Read Bytes/sec"
    },
/* disk.all.write_bytes */
    { { PMDA_PMID(0,19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_DISK_ALL, M_NONE, 0, 0, NULL,
      "\\PhysicalDisk(_Total)\\Disk Write Bytes/sec"
    },
/* disk.all.total_bytes */
    { { PMDA_PMID(0,20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_DISK_ALL, M_NONE, 0, 0, NULL,
      "\\PhysicalDisk(_Total)\\Disk Bytes/sec"
    },
/* disk.dev.read */
    { { PMDA_PMID(0,21), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_DEV, M_REDO, 0, 0, NULL,
      "\\PhysicalDisk(*/*#*)\\Disk Reads/sec"
    },
/* disk.dev.write */
    { { PMDA_PMID(0,22), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_DEV, M_REDO, 0, 0, NULL,
      "\\PhysicalDisk(*/*#*)\\Disk Writes/sec"
    },
/* disk.dev.total */
    { { PMDA_PMID(0,23), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_DEV, M_REDO, 0, 0, NULL,
      "\\PhysicalDisk(*/*#*)\\Disk Transfers/sec"
    },
/* disk.dev.read_bytes */
    { { PMDA_PMID(0,24), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_DISK_DEV, M_REDO, 0, 0, NULL,
      "\\PhysicalDisk(*/*#*)\\Disk Read Bytes/sec"
    },
/* disk.dev.write_bytes */
    { { PMDA_PMID(0,25), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_DISK_DEV, M_REDO, 0, 0, NULL,
      "\\PhysicalDisk(*/*#*)\\Disk Write Bytes/sec"
    },
/* disk.dev.total_bytes */
    { { PMDA_PMID(0,26), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_DISK_DEV, M_REDO, 0, 0, NULL,
      "\\PhysicalDisk(*/*#*)\\Disk Bytes/sec"
    },
/* mem.page_faults */
    { { PMDA_PMID(0,27), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Page Faults/sec"
    },
/* mem.available */
    { { PMDA_PMID(0,28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_MBYTE, 0, 0)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Available MBytes"
    },
/* mem.committed_bytes */
    { { PMDA_PMID(0,29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Committed Bytes"
    },
/* mem.pool.paged_bytes */
    { { PMDA_PMID(0,30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Pool Paged Bytes"
    },
/* mem.pool.non_paged_bytes */
    { { PMDA_PMID(0,31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Pool Nonpaged Bytes"
    },
/* mem.cache.lazy_writes */
    { { PMDA_PMID(0,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Cache\\Lazy Write Flushes/sec"
    },
/* mem.cache.lazy_write_pages */
    { { PMDA_PMID(0,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Cache\\Lazy Write Pages/sec"
    },
/* mem.cache.mdl.read */
    { { PMDA_PMID(0,34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Cache\\MDL Reads/sec"
    },
/* mem.cache.read_ahead */
    { { PMDA_PMID(0,35), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Cache\\Read Aheads/sec"
    },
/* mem.cache.mdl.sync_read */
    { { PMDA_PMID(0,36), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Cache\\Sync MDL Reads/sec"
    },
/* mem.cache.mdl.async_read */
    { { PMDA_PMID(0,37), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Cache\\Async MDL Reads/sec"
    },
/* network.interface.in.packets */
    { { PMDA_PMID(0,38), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_REDO, 0, 0, NULL,
      "\\Network Interface(*/*#*)\\Packets Received/sec"
    },
/* network.interface.in.bytes */
    { { PMDA_PMID(0,39), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_MEMORY, M_REDO, 0, 0, NULL,
      "\\Network Interface(*/*#*)\\Bytes Received/sec"
    },
/* network.interface.in.errors */
    { { PMDA_PMID(0,40), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_REDO, 0, 0, NULL,
      "\\Network Interface(*/*#*)\\Packets Received Errors"
    },
/* network.interface.out.packets */
    { { PMDA_PMID(0,41), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_REDO, 0, 0, NULL,
      "\\Network Interface(*/*#*)\\Packets Sent/sec"
    },
/* network.interface.out.bytes */
    { { PMDA_PMID(0,42), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_MEMORY, M_REDO, 0, 0, NULL,
      "\\Network Interface(*/*#*)\\Bytes Sent/sec"
    },
/* network.interface.out.errors */
    { { PMDA_PMID(0,43), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_REDO, 0, 0, NULL,
      "\\Network Interface(*/*#*)\\Packets Outbound Errors"
    },
/* network.interface.total.packets */
    { { PMDA_PMID(0,44), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Network Interface(*/*#*)\\Packets/sec"
    },
/* network.interface.total.bytes */
    { { PMDA_PMID(0,45), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Network Interface(*/*#*)\\Bytes Total/sec"
    },
/* sqlserver.buf_mgr.cache_hit_ratio */
    { { PMDA_PMID(0,46), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Buffer cache hit ratio"
    },
/* sqlserver.buf_mgr.page_lookups */
    { { PMDA_PMID(0,47), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Page lookups/sec"
    },
/* sqlserver.buf_mgr.free_list_stalls */
    { { PMDA_PMID(0,48), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Free list stalls/sec"
    },
/* sqlserver.buf_mgr.free_pages */
    { { PMDA_PMID(0,49), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Free pages"
    },
/* sqlserver.buf_mgr.total_pages */
    { { PMDA_PMID(0,50), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Total pages"
    },
/* sqlserver.buf_mgr.target_pages */
    { { PMDA_PMID(0,51), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Target pages"
    },
/* sqlserver.buf_mgr.database_pages */
    { { PMDA_PMID(0,52), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Database pages"
    },
/* sqlserver.buf_mgr.reserved_pages */
    { { PMDA_PMID(0,53), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Reserved pages"
    },
/* sqlserver.buf_mgr.stolen_pages */
    { { PMDA_PMID(0,54), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Stolen pages"
    },
/* sqlserver.buf_mgr.lazy_writes */
    { { PMDA_PMID(0,55), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Lazy writes/sec"
    },
/* sqlserver.buf_mgr.readahead_pages */
    { { PMDA_PMID(0,56), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Readahead pages/sec"
    },
/* sqlserver.buf_mgr.procedure_cache_pages */
    { { PMDA_PMID(0,57), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Procedure cache pages"
    },
/* sqlserver.buf_mgr.page_reads */
    { { PMDA_PMID(0,58), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Page reads/sec"
    },
/* sqlserver.buf_mgr.page_writes */
    { { PMDA_PMID(0,59), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Page writes/sec"
    },
/* sqlserver.buf_mgr.checkpoint_pages */
    { { PMDA_PMID(0,60), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Checkpoint pages/sec"
    },
/*  sqlserver.buf_mgr.awe.lookup_maps */
    { { PMDA_PMID(0,61), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\AWE lookup maps/sec"
    },
/* sqlserver.buf_mgr.awe.stolen_maps */
    { { PMDA_PMID(0,62), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\AWE stolen maps/sec"
    },
/* sqlserver.buf_mgr.awe.write_maps */
    { { PMDA_PMID(0,63), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\AWE write maps/sec"
    },
/* sqlserver.buf_mgr.awe.unmap_calls */
    { { PMDA_PMID(0,64), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\AWE unmap calls/sec"
    },
/* sqlserver.buf_mgr.awe.unmap_pages */
    { { PMDA_PMID(0,65), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\AWE unmap pages/sec"
    },
/* sqlserver.buf_mgr.page_life_expectancy */
    { { PMDA_PMID(0,66), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Buffer Manager\\Page life expectancy"
    },
/* filesys.full */
    { { PMDA_PMID(0,67), PM_TYPE_FLOAT, FILESYS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_LDISK, M_REDO, 0, 0, NULL,
      "\\LogicalDisk(*/*#*)\\% Free Space"
    },
/* disk.dev.idle */
    { { PMDA_PMID(0,68), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0)
      }, Q_DISK_DEV, M_REDO, 0, 0, NULL,
      "\\PhysicalDisk(*/*#*)\\% Idle Time"
    },
/* sqlserver.locks.all.requests */
    { { PMDA_PMID(0,69), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Locks(_Total)\\Lock Requests/sec"
    },
/* sqlserver.locks.all.waits */
    { { PMDA_PMID(0,70), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Locks(_Total)\\Lock Waits/sec"
    },
/* sqlserver.locks.all.deadlocks */
    { { PMDA_PMID(0,71), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Locks(_Total)\\Number of Deadlocks/sec"
    },
/* sqlserver.locks.all.timeouts */
    { { PMDA_PMID(0,72), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Locks(_Total)\\Lock Timeouts/sec"
    },
/* sqlserver.locks.all.wait_time */
    { { PMDA_PMID(0,73), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Locks(_Total)\\Lock Wait Time (ms)"
    },
/* sqlserver.locks.all.avg_wait */
    { { PMDA_PMID(0,74), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Locks(_Total)\\Average Wait Time (ms)"
    },
/* sqlserver.locks.region.requests */
    { { PMDA_PMID(0,75), PM_TYPE_U32, SQL_LOCK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_LOCK, M_NONE, 0, 0, NULL,
      "\\SQLServer:Locks(*/*#*)\\Lock Requests/sec"
    },
/* sqlserver.locks.region.waits */
    { { PMDA_PMID(0,76), PM_TYPE_U32, SQL_LOCK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_LOCK, M_NONE, 0, 0, NULL,
      "\\SQLServer:Locks(*/*#*)\\Lock Waits/sec"
    },
/* sqlserver.locks.region.deadlocks */
    { { PMDA_PMID(0,77), PM_TYPE_U32, SQL_LOCK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_LOCK, M_NONE, 0, 0, NULL,
      "\\SQLServer:Locks(*/*#*)\\Number of Deadlocks/sec"
    },
/* sqlserver.locks.region.timeouts */
    { { PMDA_PMID(0,78), PM_TYPE_U32, SQL_LOCK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_LOCK, M_NONE, 0, 0, NULL,
      "\\SQLServer:Locks(*/*#*)\\Lock Timeouts/sec"
    },
/* sqlserver.locks.region.wait_time */
    { { PMDA_PMID(0,79), PM_TYPE_U32, SQL_LOCK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, Q_SQLSERVER_LOCK, M_NONE, 0, 0, NULL,
      "\\SQLServer:Locks(*/*#*)\\Lock Wait Time (ms)"
    },
/* sqlserver.locks.region.avg_wait */
    { { PMDA_PMID(0,80), PM_TYPE_FLOAT, SQL_LOCK_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, Q_SQLSERVER_LOCK, M_NONE, 0, 0, NULL,
      "\\SQLServer:Locks(*/*#*)\\Average Wait Time (ms)"
    },
/* sqlserver.cache_mgr.all.cache_hit_ratio */
    { { PMDA_PMID(0,81), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Cache Manager(_Total)\\Cache Hit Ratio"
    },
/* sqlserver.cache_mgr.cache.cache_hit_ratio */
    { { PMDA_PMID(0,82), PM_TYPE_FLOAT, SQL_CACHE_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_SQLSERVER_CACHE, M_NONE, 0, 0, NULL,
      "\\SQLServer:Cache Manager(*/*#*)\\Cache Hit Ratio"
    },
/* sqlserver.connections */
    { { PMDA_PMID(0,83), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:General Statistics\\User Connections"
    },
/* sqlserver.databases.all.transactions */
    { { PMDA_PMID(0,84), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Databases(_Total)\\Transactions/sec"
    },
/* sqlserver.databases.db.transactions */
    { { PMDA_PMID(0,85), PM_TYPE_U32, SQL_DB_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_DB, M_NONE, 0, 0, NULL,
      "\\SQLServer:Databases(*/*#*)\\Transactions/sec"
    },
/* sqlserver.sql.batch_requests */
    { { PMDA_PMID(0,86), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:SQL Statistics\\Batch Requests/sec"
    },
/* sqlserver.latches.waits */
    { { PMDA_PMID(0,87), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Latches\\Latch Waits/sec"
    },
/* sqlserver.latches.wait_time */
    { { PMDA_PMID(0,88), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Latches\\Total Latch Wait Time (ms)"
    },
/* sqlserver.latches.avg_wait_time */
    { { PMDA_PMID(0,89), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Latches\\Average Latch Wait Time (ms)"
    },
/* sqlserver.databases.all.data_file_size */
    { { PMDA_PMID(0,90), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Databases(_Total)\\Data File(s) Size (KB)"
    },
/* sqlserver.databases.all.log_file_size */
    { { PMDA_PMID(0,91), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Databases(_Total)\\Log File(s) Size (KB)"
    },
/* sqlserver.databases.all.log_file_used */
    { { PMDA_PMID(0,92), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Databases(_Total)\\Log File(s) Used Size (KB)"
    },
/* sqlserver.databases.db.data_file_size */
    { { PMDA_PMID(0,93), PM_TYPE_U32, SQL_DB_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, Q_SQLSERVER_DB, M_REDO, 0, 0, NULL,
      "\\SQLServer:Databases(*/*#*)\\Data File(s) Size (KB)"
    },
/* sqlserver.databases.db.log_file_size */
    { { PMDA_PMID(0,94), PM_TYPE_U32, SQL_DB_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, Q_SQLSERVER_DB, M_REDO, 0, 0, NULL,
      "\\SQLServer:Databases(*/*#*)\\Log File(s) Size (KB)"
    },
/* sqlserver.databases.db.log_file_used */
    { { PMDA_PMID(0,95), PM_TYPE_U32, SQL_DB_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, Q_SQLSERVER_DB, M_REDO, 0, 0, NULL,
      "\\SQLServer:Databases(*/*#*)\\Log File(s) Used Size (KB)"
    },
/* sqlserver.sql.compilations */
    { { PMDA_PMID(0,96), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:SQL Statistics\\SQL Compilations/sec"
    },
/* sqlserver.sql.re_compilations */
    { { PMDA_PMID(0,97), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:SQL Statistics\\SQL Re-Compilations/sec"
    },
/* sqlserver.access.full_scans */
    { { PMDA_PMID(0,98), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Access Methods\\Full Scans/sec"
    },
/* sqlserver.access.pages_allocated */
    { { PMDA_PMID(0,99), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Access Methods\\Pages Allocated/sec"
    },
/* sqlserver.access.table_lock_escalations */
    { { PMDA_PMID(0,100), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Access Methods\\Table Lock Escalations/sec"
    },
/* disk.dev.queuelen */
    { { PMDA_PMID(0,101), PM_TYPE_U32, DISK_INDOM, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_DEV, M_REDO, 0, 0, NULL,
      "\\PhysicalDisk(*/*#*)\\Current Disk Queue Length"
    },
/* sqlserver.databases.all.log_flushes */
    { { PMDA_PMID(0,102), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Databases(_Total)\\Log Flushes/sec"
    },
/* sqlserver.databases.db.log_flushes */
    { { PMDA_PMID(0,103), PM_TYPE_U32, SQL_DB_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_DB, M_REDO, 0, 0, NULL,
      "\\SQLServer:Databases(*/*#*)\\Log Flushes/sec"
    },
/* sqlserver.databases.all.log_bytes_flushed */
    { { PMDA_PMID(0,104), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Databases(_Total)\\Log Bytes Flushed/sec"
    },
/* sqlserver.databases.db.log_bytes_flushed */
    { { PMDA_PMID(0,105), PM_TYPE_U32, SQL_DB_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, Q_SQLSERVER_DB, M_REDO, 0, 0, NULL,
      "\\SQLServer:Databases(*/*#*)\\Log Bytes Flushed/sec"
    },
/* hinv.physmem */
    { { PMDA_PMID(0,106), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_MBYTE, 0, 0)
      }, Q_MEMORY, M_NONE, 0, 0, NULL, ""
    },
/* hinv.ncpu */
    { { PMDA_PMID(0,107), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_KERNEL, M_NONE, 0, 0, NULL, ""
    },
/* hinv.ndisk */
    { { PMDA_PMID(0,108), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_KERNEL, M_NONE, 0, 0, NULL, ""
    },
/* kernel.uname.distro */
    { { PMDA_PMID(0,109), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL, ""
    },
/* kernel.uname.release */
    { { PMDA_PMID(0,110), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL, ""
    },
/* kernel.uname.version */
    { { PMDA_PMID(0,111), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL, ""
    },
/* kernel.uname.sysname */
    { { PMDA_PMID(0,112), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL, ""
    },
/* kernel.uname.machine */
    { { PMDA_PMID(0,113), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL, ""
    },
/* kernel.uname.nodename */
    { { PMDA_PMID(0,114), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL, ""
    },

/* pmda.uname */
    { { PMDA_PMID(0,115), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL, ""
    },
/* pmda.version */
    { { PMDA_PMID(0,116), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0)
      }, Q_KERNEL, M_NONE, 0, 0, NULL, ""
    },

/* filesys.capacity */
    { { PMDA_PMID(0,117), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)
      }, Q_LDISK, M_REDO, 0, 0, NULL, ""
    },
/* filesys.used */
    { { PMDA_PMID(0,118), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)
      }, Q_LDISK, M_REDO, 0, 0, NULL, ""
    },
/* filesys.free */
    { { PMDA_PMID(0,119), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)
      }, Q_LDISK, M_REDO, 0, 0, NULL, ""
    },
/* dummy - filesys.free_space */
    { { PMDA_PMID(0,120), PM_TYPE_U32, FILESYS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_MBYTE,0,0)
      }, Q_LDISK, M_REDO, 0, 0, NULL,
      "\\LogicalDisk(*/*#*)\\Free Megabytes"
    },
/* dummy - filesys.free_percent */
    { { PMDA_PMID(0,121), PM_TYPE_FLOAT, FILESYS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0)
      }, Q_LDISK, M_REDO, 0, 0, NULL,
      "\\LogicalDisk(*/*#*)\\% Free Space"
    },
/* sqlserver.access.page_splits */
    { { PMDA_PMID(0,122), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Access Methods\\Page Splits/sec"
    },
/* network.tcp.activeopens */
    { { PMDA_PMID(0,123), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\TCPv4\\Connections Active"
    },
/* network.tcp.passiveopens */
    { { PMDA_PMID(0,124), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\TCPv4\\Connections Passive"
    },
/* network.tcp.attemptfails */
    { { PMDA_PMID(0,125), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\TCPv4\\Connection Failures"
    },
/* network.tcp.estabresets */
    { { PMDA_PMID(0,126), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\TCPv4\\Connections Reset"
    },
/* network.tcp.currestab */
    { { PMDA_PMID(0,127), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\TCPv4\\Connections Established"
    },
/* network.tcp.insegs */
    { { PMDA_PMID(0,128), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\TCPv4\\Segments Received/sec"
    },
/* network.tcp.outsegs */
    { { PMDA_PMID(0,129), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\TCPv4\\Segments Sent/sec"
    },
/* network.tcp.totalsegs */
    { { PMDA_PMID(0,130), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\TCPv4\\Segments/sec"
    },
/* network.tcp.retranssegs */
    { { PMDA_PMID(0,131), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\TCPv4\\Segments Retransmitted/sec"
    },

/* disk.all.split_io */
    { { PMDA_PMID(0,132), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_ALL, M_NONE, 0, 0, NULL,
      "\\PhysicalDisk(_Total)\\Split IO/Sec"
    },
/* disk.dev.split_io */
    { { PMDA_PMID(0,133), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_DISK_DEV, M_REDO, 0, 0, NULL,
      "\\PhysicalDisk(*/*#*)\\Split IO/Sec"
    },

/* sqlserver.databases.all.active_transactions */
    { { PMDA_PMID(0,134), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Databases(_Total)\\Active Transactions"
    },
/* sqlserver.databases.db.active_transactions */
    { { PMDA_PMID(0,135), PM_TYPE_U32, SQL_DB_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, Q_SQLSERVER_DB, M_REDO, 0, 0, NULL,
      "\\SQLServer:Databases(*/*#*)\\Active Transactions"
    },

/* mem.commit_limit */
    { { PMDA_PMID(0,136), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Commit Limit"
    },
/* mem.write_copies */
    { { PMDA_PMID(0,137), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Write Copies/sec"
    },
/* mem.transition_faults */
    { { PMDA_PMID(0,138), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Transition Faults/sec"
    },
/* mem.cache.faults */
    { { PMDA_PMID(0,139), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Cache Faults/sec"
    },
/* mem.demand_zero_faults */
    { { PMDA_PMID(0,140), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Demand Zero Faults/sec"
    },
/* mem.pages_total */
    { { PMDA_PMID(0,141), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Pages/sec"
    },
/* mem.page_reads */
    { { PMDA_PMID(0,142), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Page Reads/sec"
    },
/* mem.pages_output */
    { { PMDA_PMID(0,143), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Pages Output/sec"
    },
/* mem.page_writes */
    { { PMDA_PMID(0,144), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Page Writes/sec"
    },
/* mem.pool.paged_allocs */
    { { PMDA_PMID(0,145), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Pool Paged Allocs"
    },
/* mem.pool.nonpaged_allocs */
    { { PMDA_PMID(0,146), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Pool Nonpaged Allocs"
    },
/* mem.system.free_ptes */
    { { PMDA_PMID(0,147), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Free System Page Table Entries"
    },
/* mem.cache.bytes */
    { { PMDA_PMID(0,148), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Page Faults/sec"
    },
/* mem.cache.bytes_peak */
    { { PMDA_PMID(0,149), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Cache Bytes Peak"
    },
/* mem.pool.paged_resident_bytes */
    { { PMDA_PMID(0,150), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\Pool Paged Resident Bytes"
    },
/* mem.system.total_code_bytes */
    { { PMDA_PMID(0,151), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\System Code Total Bytes"
    },
/* mem.system.resident_code_bytes */
    { { PMDA_PMID(0,152), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)
      }, Q_MEMORY, M_NONE, 0, 0, NULL,
      "\\Memory\\System Code Resident Bytes"
    },

/* sqlserver.mem_mgr.connection_memory */
    { { PMDA_PMID(0,153), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(1,0,PM_SPACE_KBYTE,0,0,0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Memory Manager\\Connection Memory (KB)"
    },
/* sqlserver.mem_mgr.granted_workspace */
    { { PMDA_PMID(0,154), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(1,0,PM_SPACE_KBYTE,0,0,0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Memory Manager\\Granted Workspace Memory (KB)"
    },
/* sqlserver.mem_mgr.lock_memory */
    { { PMDA_PMID(0,155), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(1,0,PM_SPACE_KBYTE,0,0,0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Memory Manager\\Lock Memory (KB)"
    },
/* sqlserver.mem_mgr.lock_blocks_allocated */
    { { PMDA_PMID(0,156), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Memory Manager\\Lock Blocks Allocated"
    },
/* sqlserver.mem_mgr.lock_owner_blocks_allocated */
    { { PMDA_PMID(0,157), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Memory Manager\\Lock Owner Blocks Allocated"
    },
/* sqlserver.mem_mgr.lock_blocks */
    { { PMDA_PMID(0,158), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Memory Manager\\Lock Blocks"
    },
/* sqlserver.mem_mgr.lock_owner_blocks */
    { { PMDA_PMID(0,159), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Memory Manager\\Lock Owner Blocks"
    },
/* sqlserver.mem_mgr.maximum_workspace_memory */
    { { PMDA_PMID(0,160), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(1,0,PM_SPACE_KBYTE,0,0,0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Memory Manager\\Maximum Workspace Memory (KB)"
    },
/* sqlserver.mem_mgr.memory_grants_outstanding */
    { { PMDA_PMID(0,161), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Memory Manager\\Memory Grants Outstanding"
    },
/* sqlserver.mem_mgr.memory_grants_pending */
    { { PMDA_PMID(0,162), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Memory Manager\\Memory Grants Pending"
    },
/* sqlserver.mem_mgr.optimizer_memory */
    { { PMDA_PMID(0,163), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(1,0,PM_SPACE_KBYTE,0,0,0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Memory Manager\\Optimizer Memory (KB)"
    },
/* sqlserver.mem_mgr.sql_cache_memory */
    { { PMDA_PMID(0,164), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(1,0,PM_SPACE_KBYTE,0,0,0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Memory Manager\\SQL Cache Memory (KB)"
    },
/* sqlserver.mem_mgr.target_server_memory */
    { { PMDA_PMID(0,165), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(1,0,PM_SPACE_KBYTE,0,0,0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Memory Manager\\Target Server Memory(KB)"
    },
/* sqlserver.mem_mgr.total_server_memory */
    { { PMDA_PMID(0,166), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
	PMDA_PMUNITS(1,0,PM_SPACE_KBYTE,0,0,0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Memory Manager\\Total Server Memory (KB)"
    },
/* sqlserver.cache_mgr.all.cache_pages */
    { { PMDA_PMID(0,167), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Cache Manager(_Total)\\Cache Pages"
    },
/* sqlserver.cache_mgr.all.cache_object_count */
    { { PMDA_PMID(0,168), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Cache Manager(_Total)\\Cache Object Counts"
    },
/* sqlserver.cache_mgr.all.cache_use */
    { { PMDA_PMID(0,169), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_SQLSERVER_ALL, M_NONE, 0, 0, NULL,
      "\\SQLServer:Cache Manager(_Total)\\Cache Use Counts/sec"
    },
/* sqlserver.cache_mgr.cache.cache_pages */
    { { PMDA_PMID(0,170), PM_TYPE_U32, SQL_CACHE_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_SQLSERVER_CACHE, M_NONE, 0, 0, NULL,
      "\\SQLServer:Cache Manager(*/*#*)\\Cache Pages"
    },
/* sqlserver.cache_mgr.cache.cache_object_count */
    { { PMDA_PMID(0,171), PM_TYPE_32, SQL_CACHE_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_SQLSERVER_CACHE, M_NONE, 0, 0, NULL,
      "\\SQLServer:Cache Manager(*/*#*)\\Cache Object Counts"
    },
/* sqlserver.cache_mgr.cache.cache_use */
    { { PMDA_PMID(0,172), PM_TYPE_U32, SQL_CACHE_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)
      }, Q_SQLSERVER_CACHE, M_NONE, 0, 0, NULL,
      "\\SQLServer:Cache Manager(*/*#*)\\Cache Use Counts/sec"
    },
#if 0
/* process.count */
    { { PMDA_PMID(0,173), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, Q_PROCESS, M_REDO, 0, 0, NULL,
      "\\Objects\Processes"
    },
/* process.psinfo.pid */
    { { PMDA_PMID(0,174), 
/* process.psinfo.ppid */
    { { PMDA_PMID(0,175), 
/* process.psinfo.cpu_time */
    { { PMDA_PMID(0,176), 
/* process.psinfo.elapsed_time */
    { { PMDA_PMID(0,177), 
/* process.psinfo.utime */
    { { PMDA_PMID(0,178), 
/* process.psinfo.stime */
    { { PMDA_PMID(0,179), 
/* process.psinfo.nthreads */
    { { PMDA_PMID(0,180), 
/* process.psinfo.priority_base */
    { { PMDA_PMID(0,181), 
/* process.psinfo.nhandles */
    { { PMDA_PMID(0,182), 
/* process.memory.size */
    { { PMDA_PMID(0,183), 
/* process.memory.rss */
    { { PMDA_PMID(0,184), 
/* process.memory.page_file */
    { { PMDA_PMID(0,185), 
/* process.memory.private */
    { { PMDA_PMID(0,186), 
/* process.memory.pool_paged */
    { { PMDA_PMID(0,187), 
/* process.memory.pool_nonpages */
    { { PMDA_PMID(0,188), 
/* process.io.reads */
    { { PMDA_PMID(0,189), 
/* process.io.writes */
    { { PMDA_PMID(0,190), 
/* process.io.data */
    { { PMDA_PMID(0,191), 
/* process.io.other */
    { { PMDA_PMID(0,192), 
/* process.io.read_bytes */
    { { PMDA_PMID(0,193), 
/* process.io.write_bytes */
    { { PMDA_PMID(0,194), 
/* process.io.data_bytes */
    { { PMDA_PMID(0,195), 
/* process.io.other_bytes */
    { { PMDA_PMID(0,196), 
/* process.thread.context_switches */
    { { PMDA_PMID(0,197), 
/* process.thread.cpu_time */
    { { PMDA_PMID(0,198), 
/* process.thread.utime */
    { { PMDA_PMID(0,199), 
/* process.thread.stime */
    { { PMDA_PMID(0,200), 
/* process.thread.elapsed_time */
    { { PMDA_PMID(0,201), 
/* process.thread.priority */
    { { PMDA_PMID(0,202), 
/* process.thread.priority_base */
    { { PMDA_PMID(0,203), 
/* process.thread.start_address */
    { { PMDA_PMID(0,204), 
/* process.thread.state */
    { { PMDA_PMID(0,205), 
/* process.thread.wait_reason_s */
    { { PMDA_PMID(0,206), 
/* process.thread.process_id */
    { { PMDA_PMID(0,207), 
/* process.thread.thread_id */
    { { PMDA_PMID(0,208), 
#endif
};
int metricdesc_sz = sizeof(metricdesc) / sizeof(metricdesc[0]);

static int
windows_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    windows_instance_refresh(indom);
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
windows_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    windows_fetch_refresh(numpmid, pmidlist);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
windows_fetch_callback(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*pmidp = (__pmID_int *)&mdesc->m_desc.pmid;
    pdh_metric_t	*mp;
    int			i;

    if (pmidp->cluster != 0 || pmidp->item > metricdesc_sz)
	return PM_ERR_PMID;

    /*
     * Check if its one of the derived metrics, or one that doen't use PDH
     */
    switch (pmidp->item) {
    case 106:	/* hinv.physmem */
	atom->ul = windows_physmem;
	return 1;
    case 107:	/* hinv.ncpu */
	atom->ul = pmdaCacheOp(CPU_INDOM, PMDA_CACHE_SIZE_ACTIVE);
	return 1;
    case 108:	/* hinv.ndisk */
	atom->ul = pmdaCacheOp(DISK_INDOM, PMDA_CACHE_SIZE_ACTIVE);
	return 1;
    case 109:	/* kernel.uname.distro */
	atom->cp = windows_uname;
	return 1;
    case 110:	/* kernel.uname.release */
	atom->cp = windows_build;
	return 1;
    case 111:	/* kernel.uname.version */
	atom->cp = windows_build;
	return 1;
    case 112:	/* kernel.uname.sysname */
	atom->cp = "Windows";
	return 1;
    case 113:	/* kernel.uname.machine */
	atom->cp = "?";
	return 1;
    case 114:	/* kernel.uname.nodename */
	atom->cp = "?";
	return 1;
    case 115:	/* pmda.uname */
	atom->cp = windows_uname;
	return 1;
    case 116:	/* pmda.version */
	atom->cp = pmGetConfig("PCP_VERSION");
	return 1;
    }

#if 0	/* TODO ... */
    /*
     * special case the filesystem metrics at this point -
     * mapping the PDH services semantics for these to the
     * saner metrics from other platforms is not pretty...
     * depends on pmids for these metrics
     *		 67 filesys.full
     * 		117 filesys.capacity
     *		118 filesys.used
     *		119 filesys.free
     *		120 dummy metric, atomtab holds FreeMB
     *		121 dummy metric, atomtab holds %Free
     */
    if (pmidp->item == 67 || (pmidp->item >= 117 && pmidp->item <= 119)) {
	float used_space, free_space, free_percent;
	unsigned long long used, avail, capacity;
	int item;

	/* TODO: fix this up ... probably needs to move in with the */
	/* actual fetch code - need to flag if extra values needed, */
	/* do the fetch, then stitch up the atom for these metrics. */

	for (count = 0, i = 0; i < atomtab_sz; i++) {
	    if (atomtab[i].inst != inst)
		continue;
	    if (pmidp->item == 67) {	/* filesys.full, atomtab holds %Free */
		atom->f = (1.0 - atomtab[i].atom.f) * 100.0;
		return 1;
	    }
	    item = ((__pmID_int*)&atomtab[i].pmid)->item;
	    if (item == 120) {		/* dummy metric, atomtab holds FreeMB */
		free_space = ((float)atomtab[i].atom.ul);
		count++;
	    } else if (item == 121) {	/* dummy metric, atomtab holds %Free */
		free_percent = atomtab[i].atom.f;
		count++;
	    }
	}
	if (count != 2)	/* we need both "dummy" metric values below */
	    return 0;

	used_space = (free_space / free_percent) - free_space;
	used = 1024 * (unsigned long long)used_space;	/* MB to KB */
	avail = 1024 * (unsigned long long)free_space;	/* MB to KB */
	capacity = used + avail;

	if (pmidp->item == 117)		/* filesys.capacity */
	    atom->ull = capacity;
	else if (pmidp->item == 118)	/* filesys.used */
	    atom->ull = used;
	else if (pmidp->item == 119)	/* filesys.free */
	    atom->ull = avail;
	return 1;
    }
#endif

    mp = &metricdesc[pmidp->item];
    for (i = 0; i < mp->num_vals; i++) {
	if (mp->vals[i].inst != inst)
	    continue;
	if (!(mp->vals[i].flags & V_COLLECTED))
	    return 1;
	*atom = mp->vals[i].atom;
	return 0;
    }
    return 1;
}

static int
windows_help(int ident, int type, char **buf, pmdaExt *pmda)
{
    /* TODO */
    return 0;
}

/*
 * Initialise the agent.
 */
void 
windows_init(pmdaInterface *dp)
{
    static pmdaMetric	*metrictab;
    int			metrictab_sz = metricdesc_sz;
    int			i;

    if (isDSO)
	pmdaDSO(dp, PMDA_INTERFACE_3, "windows DSO", NULL);

    if (dp->status != 0)
	return;

    windows_globals();

    /* Create the PMDA's metrictab[] version of the per-metric table */
    metrictab = (pmdaMetric *)malloc(metrictab_sz * sizeof(pmdaMetric));
    if (metrictab == NULL) {
	fprintf(stderr, "Error: malloc metrictab [%d] failed: %s\n",
		metrictab_sz * sizeof(pmdaMetric), strerror(errno));
	return;
    }
    for (i = 0; i < metrictab_sz; i++) {
	/* rewrite indom, now that we know what the domain number is */
	metricdesc[i].desc.indom = windows_indom(metricdesc[i].qid, dp->domain);

	/* write the metrictab entry for this metric */
	metrictab[i].m_desc = metricdesc[i].desc;
	metrictab[i].m_user = NULL;
    }

    dp->version.two.fetch = windows_fetch;
    dp->version.two.instance = windows_instance;
    dp->version.two.text = windows_help;
    pmdaSetFetchCallBack(dp, windows_fetch_callback);
    pmdaInit(dp, NULL, 0, metrictab, metrictab_sz);
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile   write log into logfile rather than using default log name\n",
	      stderr);		
    exit(1);
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			err = 0;
    pmdaInterface	d;
    int			c;

    isDSO = 0;
    pmProgname = basename(argv[0]);
    pmdaDaemon(&d, PMDA_INTERFACE_3, pmProgname, WINDOWS, "windows.log", NULL);

    while ((c = pmdaGetOpt(argc, argv, "D:d:l:?", &d, &err)) != EOF)
	err++;
    if (err)
    	usage();

    pmdaOpenLog(&d);
    windows_init(&d);
    pmdaConnect(&d);
    pmdaMain(&d);
    exit(0);
}
