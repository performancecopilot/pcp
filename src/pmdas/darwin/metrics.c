/*
 * Darwin PMDA metric table
 *
 * Copyright (c) 2026 Red Hat.
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

#include <sys/utsname.h>
#include <mach/mach.h>
#include "pmapi.h"
#include "pmda.h"
#include "darwin.h"
#include "metrics.h"
#include "network.h"
#include "vfs.h"
#include "vmstat.h"
#include "udp.h"
#include "sockstat.h"
#include "tcpconn.h"
#include "tcp.h"

/*
 * External declarations for global data referenced in metrictab.
 * These variables are defined in pmda.c and represent the actual
 * data storage for metrics.
 */
extern vm_size_t mach_page_size;
extern unsigned int mach_hertz;
extern char hw_model[];
extern struct vm_statistics64 mach_vmstat;
extern struct compressor_stats mach_compressor;
extern struct utsname mach_uname;
extern unsigned int mach_uptime;
extern struct nfsstats mach_nfs;
extern vfsstats_t mach_vfs;
extern udpstats_t mach_udp;
extern sockstats_t mach_sockstat;
extern tcpconn_stats_t mach_tcpconn;
extern tcpstats_t mach_tcp;

pmdaMetric metrictab[] = {

/* hinv.pagesize */
  { &mach_page_size,
    { PMDA_PMID(CLUSTER_INIT,0), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* kernel.all.hz */
  { &mach_hertz,
    { PMDA_PMID(CLUSTER_INIT,1), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,-1,1,0,PM_TIME_SEC,PM_COUNT_ONE) }, },
/* hinv.machine */
  { &hw_model,
    { PMDA_PMID(CLUSTER_INIT,2), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* hinv.physmem */
  { NULL,
    { PMDA_PMID(CLUSTER_VMSTAT,2), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(1,0,0,PM_SPACE_MBYTE,0,0) }, },
/* mem.physmem */
  { NULL,
    { PMDA_PMID(CLUSTER_VMSTAT,3), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* mem.freemem */
  { NULL,
    { PMDA_PMID(CLUSTER_VMSTAT,4), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* mem.active */
  { NULL,
    { PMDA_PMID(CLUSTER_VMSTAT,5), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* mem.inactive */
  { NULL,
    { PMDA_PMID(CLUSTER_VMSTAT,6), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* mem.pages.free */
  { &mach_vmstat.free_count,
    { PMDA_PMID(CLUSTER_VMSTAT,7), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.pages.active */
  { &mach_vmstat.active_count,
    { PMDA_PMID(CLUSTER_VMSTAT,8), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.pages.inactive */
  { &mach_vmstat.inactive_count,
    { PMDA_PMID(CLUSTER_VMSTAT,9), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.pages.reactivated */
  { &mach_vmstat.reactivations,
    { PMDA_PMID(CLUSTER_VMSTAT,10), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.pages.wired */
  { &mach_vmstat.wire_count,
    { PMDA_PMID(CLUSTER_VMSTAT,11), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.pages.faults */
  { &mach_vmstat.faults,
    { PMDA_PMID(CLUSTER_VMSTAT,12), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.pages.cow_faults */
  { &mach_vmstat.cow_faults,
    { PMDA_PMID(CLUSTER_VMSTAT,13), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.pages.zero_filled */
  { &mach_vmstat.zero_fill_count,
    { PMDA_PMID(CLUSTER_VMSTAT,14), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.pageins */
  { &mach_vmstat.pageins,
    { PMDA_PMID(CLUSTER_VMSTAT,15), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.pageouts */
  { &mach_vmstat.pageouts,
    { PMDA_PMID(CLUSTER_VMSTAT,16), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/*
 * REMOVED: mem.cache_hits and mem.cache_lookups (CLUSTER_VMSTAT items 17, 18)
 * These correspond to vm_statistics64.hits and vm_statistics64.lookups fields
 * which exist in the kernel API but are not populated on modern macOS (always 0).
 * Verified on macOS 15.x (Sequoia) - fields present but unused by kernel.
 * Item numbers 17 and 18 are reserved for these deprecated metrics.
 */
/* mem.util.wired */
  { NULL,
    { PMDA_PMID(CLUSTER_VMSTAT,19), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* mem.util.active */
  { NULL,
    { PMDA_PMID(CLUSTER_VMSTAT,20), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* mem.util.inactive */
  { NULL,
    { PMDA_PMID(CLUSTER_VMSTAT,21), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* mem.util.free */
  { NULL,
    { PMDA_PMID(CLUSTER_VMSTAT,22), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* mem.util.used */
  { NULL,
    { PMDA_PMID(CLUSTER_VMSTAT,23), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* swap.length */
  { NULL,
    { PMDA_PMID(CLUSTER_VMSTAT,24), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* swap.used */
  { NULL,
    { PMDA_PMID(CLUSTER_VMSTAT,25), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* swap.free */
  { NULL,
    { PMDA_PMID(CLUSTER_VMSTAT,26), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* swap.pagesin */
  { &mach_vmstat.pageins,
    { PMDA_PMID(CLUSTER_VMSTAT,27), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* swap.pagesout */
  { &mach_vmstat.pageouts,
    { PMDA_PMID(CLUSTER_VMSTAT,28), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* kernel.uname.release */
  { mach_uname.release,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 23), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* kernel.uname.version */
  { mach_uname.version,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 24), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* kernel.uname.sysname */
  { mach_uname.sysname,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 25), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* kernel.uname.machine */
  { mach_uname.machine,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 26), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* kernel.uname.nodename */
  { mach_uname.nodename,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 27), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* kernel.uname.distro */
  { NULL,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 30), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* pmda.uname */
  { NULL,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 28), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* pmda.version */
  { NULL,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 29), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* kernel.all.load */
  { NULL,
    { PMDA_PMID(CLUSTER_LOADAVG,30), PM_TYPE_FLOAT, LOADAVG_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* hinv.nfilesys */
  { NULL,
    { PMDA_PMID(CLUSTER_FILESYS,31), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* filesys.capacity */
  { NULL,
    { PMDA_PMID(CLUSTER_FILESYS,32), PM_TYPE_U64, FILESYS_INDOM,
      PM_SEM_DISCRETE, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* filesys.used */
  { NULL,
    { PMDA_PMID(CLUSTER_FILESYS,33), PM_TYPE_U64, FILESYS_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* filesys.free */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,34), PM_TYPE_U64, FILESYS_INDOM,
       PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* filesys.usedfiles */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,35), PM_TYPE_U32, FILESYS_INDOM,
       PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* filesys.freefiles */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,36), PM_TYPE_U32, FILESYS_INDOM,
       PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* filesys.mountdir */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,37), PM_TYPE_STRING, FILESYS_INDOM,
       PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* filesys.full */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,38), PM_TYPE_DOUBLE, FILESYS_INDOM,
       PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* filesys.blocksize */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,39), PM_TYPE_U32, FILESYS_INDOM,
       PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* filesys.avail */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,40), PM_TYPE_U64, FILESYS_INDOM,
       PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* filesys.type */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,41), PM_TYPE_STRING, FILESYS_INDOM,
       PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* kernel.all.cpu.user */
  { NULL,
    { PMDA_PMID(CLUSTER_CPULOAD,42), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* kernel.all.cpu.nice */
  { NULL,
    { PMDA_PMID(CLUSTER_CPULOAD,43), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* kernel.all.cpu.sys */
  { NULL,
    { PMDA_PMID(CLUSTER_CPULOAD,44), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* kernel.all.cpu.idle */
  { NULL,
    { PMDA_PMID(CLUSTER_CPULOAD,45), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* hinv.ndisk */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,46), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* disk.dev.read */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,47), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* disk.dev.write */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,48), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* disk.dev.total */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,49), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* disk.dev.read_bytes */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,50), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* disk.dev.write_bytes */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,51), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* disk.dev.total_bytes */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,52), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* disk.dev.blkread */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,53), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* disk.dev.blkwrite */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,54), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* disk.dev.blktotal */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,55), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* disk.dev.read_time */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,56), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
/* disk.dev.write_time */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,57), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
/* disk.dev.total_time */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,58), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
/* disk.all.read */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,59), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* disk.all.write */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* disk.all.total */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,61), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* disk.all.read_bytes */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,62), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* disk.all.write_bytes */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,63), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* disk.all.total_bytes */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,64), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* disk.all.blkread */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,65), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* disk.all.blkwrite */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,66), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* disk.all.blktotal */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,67), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* disk.all.read_time */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,68), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
/* disk.all.write_time */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,69), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
/* disk.all.total_time */
  { NULL,
    { PMDA_PMID(CLUSTER_DISK,70), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },

/* hinv.ncpu */
  { NULL,
    { PMDA_PMID(CLUSTER_CPU,71), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* kernel.percpu.cpu.user */
  { NULL,
    { PMDA_PMID(CLUSTER_CPU,72), PM_TYPE_U64, CPU_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* kernel.percpu.cpu.nice */
  { NULL,
    { PMDA_PMID(CLUSTER_CPU,73), PM_TYPE_U64, CPU_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* kernel.percpu.cpu.sys */
  { NULL,
    { PMDA_PMID(CLUSTER_CPU,74), PM_TYPE_U64, CPU_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
/* kernel.percpu.cpu.idle */
  { NULL,
    { PMDA_PMID(CLUSTER_CPU,75), PM_TYPE_U64, CPU_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.all.uptime */
  { &mach_uptime,
    { PMDA_PMID(CLUSTER_UPTIME,76), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,1,1,0,PM_TIME_SEC,PM_COUNT_ONE) }, },

/* network.interface.in.bytes */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,77), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* network.interface.in.packets */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,78), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.interface.in.errors */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,79), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.interface.in.drops */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,80), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.interface.in.mcasts */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,81), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.interface.out.bytes */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,82), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,0,PM_SPACE_BYTE,0) }, },
/* network.interface.out.packets */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,83), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.interface.out.errors */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,84), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.interface.out.mcasts */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,85), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.interface.collisions */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,86), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.interface.mtu */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,87), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,0,PM_SPACE_BYTE,0) }, },
/* network.interface.baudrate */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,88), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,0,PM_SPACE_BYTE,0) }, },
/* network.interface.total.bytes */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,89), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,0,PM_SPACE_BYTE,0) }, },
/* network.interface.total.packets */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,90), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.interface.total.errors */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,91), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.interface.total.drops */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,92), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.interface.total.mcasts */
  { NULL,
    { PMDA_PMID(CLUSTER_NETWORK,93), PM_TYPE_U64, NETWORK_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* nfs3.client.calls */
  { NULL,
    { PMDA_PMID(CLUSTER_NFS,94), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* nfs3.client.reqs */
  { NULL,
    { PMDA_PMID(CLUSTER_NFS,95), PM_TYPE_U64, NFS3_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* nfs3.server.calls */
  { NULL,
    { PMDA_PMID(CLUSTER_NFS,96), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* nfs3.server.reqs */
  { NULL,
    { PMDA_PMID(CLUSTER_NFS,97), PM_TYPE_U64, NFS3_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.rpccnt */
  { &mach_nfs.client.rpcrequests,
    { PMDA_PMID(CLUSTER_NFS,98), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.rpcretrans */
  { &mach_nfs.client.rpcretries,
    { PMDA_PMID(CLUSTER_NFS,99), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.rpctimeouts */
  { &mach_nfs.client.rpctimeouts,
    { PMDA_PMID(CLUSTER_NFS,100), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.rpcinvalid */
  { &mach_nfs.client.rpcinvalid,
    { PMDA_PMID(CLUSTER_NFS,101), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.rpcunexpected */
  { &mach_nfs.client.rpcunexpected,
    { PMDA_PMID(CLUSTER_NFS,102), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.attrcache.hits */
  { &mach_nfs.client.attrcache_hits,
    { PMDA_PMID(CLUSTER_NFS,103), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.attrcache.misses */
  { &mach_nfs.client.attrcache_misses,
    { PMDA_PMID(CLUSTER_NFS,104), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.lookupcache.hits */
  { &mach_nfs.client.lookupcache_hits,
    { PMDA_PMID(CLUSTER_NFS,105), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.lookupcache.misses */
  { &mach_nfs.client.lookupcache_misses,
    { PMDA_PMID(CLUSTER_NFS,106), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.read.hits */
  { &mach_nfs.client.read_bios,
    { PMDA_PMID(CLUSTER_NFS,107), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.read.misses */
  { &mach_nfs.client.biocache_reads,
    { PMDA_PMID(CLUSTER_NFS,108), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.write.hits */
  { &mach_nfs.client.write_bios,
    { PMDA_PMID(CLUSTER_NFS,109), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.write.misses */
  { &mach_nfs.client.biocache_writes,
    { PMDA_PMID(CLUSTER_NFS,110), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.readlink.hits */
  { &mach_nfs.client.readlink_bios,
    { PMDA_PMID(CLUSTER_NFS,111), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.readlink.misses */
  { &mach_nfs.client.biocache_readlinks,
    { PMDA_PMID(CLUSTER_NFS,112), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.readdir.hits */
  { &mach_nfs.client.readdir_bios,
    { PMDA_PMID(CLUSTER_NFS,113), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.readdir.misses */
  { &mach_nfs.client.biocache_readdirs,
    { PMDA_PMID(CLUSTER_NFS,114), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.direofcache.hits */
  { &mach_nfs.client.direofcache_hits,
    { PMDA_PMID(CLUSTER_NFS,115), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.direofcache.misses */
  { &mach_nfs.client.direofcache_misses,
    { PMDA_PMID(CLUSTER_NFS,116), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.retfailed */
  { &mach_nfs.server.srvrpc_errs,
    { PMDA_PMID(CLUSTER_NFS,117), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.faults */
  { &mach_nfs.server.srvrpc_errs,
    { PMDA_PMID(CLUSTER_NFS,118), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.cache.inprog */
  { &mach_nfs.server.srvcache_inproghits,
    { PMDA_PMID(CLUSTER_NFS,119), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.cache.nonidem */
  { &mach_nfs.server.srvcache_nonidemdonehits,
    { PMDA_PMID(CLUSTER_NFS,120), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.cache.idem */
  { &mach_nfs.server.srvcache_idemdonehits,
    { PMDA_PMID(CLUSTER_NFS,121), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.cache.misses */
  { &mach_nfs.server.srvcache_misses,
    { PMDA_PMID(CLUSTER_NFS,122), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.nqnfs.leases -- deprecated */
  { NULL,
    { PMDA_PMID(CLUSTER_NFS,123), PM_TYPE_NOSUPPORT, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.nqnfs.maxleases -- deprecated */
  { NULL,
    { PMDA_PMID(CLUSTER_NFS,124), PM_TYPE_NOSUPPORT, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.nqnfs.getleases -- deprecated */
  { NULL,
    { PMDA_PMID(CLUSTER_NFS,125), PM_TYPE_NOSUPPORT, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.vopwrites */
  { &mach_nfs.server.srvvop_writes,
    { PMDA_PMID(CLUSTER_NFS,126), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.pageins */
  { &mach_nfs.client.pageins,
    { PMDA_PMID(CLUSTER_NFS,127), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.pageouts */
  { &mach_nfs.client.pageouts,
    { PMDA_PMID(CLUSTER_NFS,128), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* filesys.maxfiles */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,129), PM_TYPE_U32, FILESYS_INDOM,
       PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* mem.util.compressed */
  { NULL,
    { PMDA_PMID(CLUSTER_VMSTAT,130), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* mem.compressions */
  { &mach_vmstat.compressions,
    { PMDA_PMID(CLUSTER_VMSTAT,131), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.decompressions */
  { &mach_vmstat.decompressions,
    { PMDA_PMID(CLUSTER_VMSTAT,132), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.compressor.pages */
  { &mach_vmstat.compressor_page_count,
    { PMDA_PMID(CLUSTER_VMSTAT,133), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.compressor.uncompressed_pages */
  { &mach_vmstat.total_uncompressed_pages_in_compressor,
    { PMDA_PMID(CLUSTER_VMSTAT,134), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.compressor.swapouts_under_30s */
  { &mach_compressor.swapouts_under_30s,
    { PMDA_PMID(CLUSTER_VMSTAT,135), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.compressor.swapouts_under_60s */
  { &mach_compressor.swapouts_under_60s,
    { PMDA_PMID(CLUSTER_VMSTAT,136), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.compressor.swapouts_under_300s */
  { &mach_compressor.swapouts_under_300s,
    { PMDA_PMID(CLUSTER_VMSTAT,137), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.compressor.thrashing_detected */
  { &mach_compressor.thrashing_detected,
    { PMDA_PMID(CLUSTER_VMSTAT,138), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.compressor.major_compactions */
  { &mach_compressor.major_compactions,
    { PMDA_PMID(CLUSTER_VMSTAT,139), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.compressor.lz4_compressions */
  { &mach_compressor.lz4_compressions,
    { PMDA_PMID(CLUSTER_VMSTAT,140), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* vfs.files.count */
  { &mach_vfs.num_files,
    { PMDA_PMID(CLUSTER_VFS,135), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* vfs.files.max */
  { &mach_vfs.max_files,
    { PMDA_PMID(CLUSTER_VFS,136), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* vfs.files.free */
  { NULL,
    { PMDA_PMID(CLUSTER_VFS,137), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* vfs.vnodes.count */
  { &mach_vfs.num_vnodes,
    { PMDA_PMID(CLUSTER_VFS,138), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* vfs.vnodes.max */
  { &mach_vfs.max_vnodes,
    { PMDA_PMID(CLUSTER_VFS,139), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* kernel.all.nprocs */
  { &mach_vfs.num_tasks,
    { PMDA_PMID(CLUSTER_VFS,140), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* kernel.all.nthreads */
  { &mach_vfs.num_threads,
    { PMDA_PMID(CLUSTER_VFS,141), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.udp.indatagrams */
  { &mach_udp.ipackets,
    { PMDA_PMID(CLUSTER_UDP,142), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.udp.outdatagrams */
  { &mach_udp.opackets,
    { PMDA_PMID(CLUSTER_UDP,143), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.udp.noports */
  { &mach_udp.noport,
    { PMDA_PMID(CLUSTER_UDP,144), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.udp.inerrors.total */
  { NULL,
    { PMDA_PMID(CLUSTER_UDP,145), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.udp.rcvbuferrors */
  { &mach_udp.fullsock,
    { PMDA_PMID(CLUSTER_UDP,146), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.udp.inerrors.hdrops */
  { &mach_udp.hdrops,
    { PMDA_PMID(CLUSTER_UDP,147), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.udp.inerrors.badsum */
  { &mach_udp.badsum,
    { PMDA_PMID(CLUSTER_UDP,148), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.udp.inerrors.badlen */
  { &mach_udp.badlen,
    { PMDA_PMID(CLUSTER_UDP,149), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.icmp.inmsgs */
  { NULL,
    { PMDA_PMID(CLUSTER_ICMP,147), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.icmp.outmsgs */
  { NULL,
    { PMDA_PMID(CLUSTER_ICMP,148), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.icmp.inerrors */
  { NULL,
    { PMDA_PMID(CLUSTER_ICMP,149), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.icmp.indestunreachs */
  { NULL,
    { PMDA_PMID(CLUSTER_ICMP,150), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.icmp.inechos */
  { NULL,
    { PMDA_PMID(CLUSTER_ICMP,151), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.icmp.inechoreps */
  { NULL,
    { PMDA_PMID(CLUSTER_ICMP,152), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.icmp.outechos */
  { NULL,
    { PMDA_PMID(CLUSTER_ICMP,153), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.icmp.outechoreps */
  { NULL,
    { PMDA_PMID(CLUSTER_ICMP,154), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.sockstat.tcp.inuse */
  { &mach_sockstat.tcp_inuse,
    { PMDA_PMID(CLUSTER_SOCKSTAT,155), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* network.sockstat.udp.inuse */
  { &mach_sockstat.udp_inuse,
    { PMDA_PMID(CLUSTER_SOCKSTAT,156), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.tcpconn.established */
  { &mach_tcpconn.state[TCPS_ESTABLISHED],
    { PMDA_PMID(CLUSTER_TCPCONN,157), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* network.tcpconn.syn_sent */
  { &mach_tcpconn.state[TCPS_SYN_SENT],
    { PMDA_PMID(CLUSTER_TCPCONN,158), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* network.tcpconn.syn_recv */
  { &mach_tcpconn.state[TCPS_SYN_RECEIVED],
    { PMDA_PMID(CLUSTER_TCPCONN,159), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* network.tcpconn.fin_wait1 */
  { &mach_tcpconn.state[TCPS_FIN_WAIT_1],
    { PMDA_PMID(CLUSTER_TCPCONN,160), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* network.tcpconn.fin_wait2 */
  { &mach_tcpconn.state[TCPS_FIN_WAIT_2],
    { PMDA_PMID(CLUSTER_TCPCONN,161), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* network.tcpconn.time_wait */
  { &mach_tcpconn.state[TCPS_TIME_WAIT],
    { PMDA_PMID(CLUSTER_TCPCONN,162), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* network.tcpconn.close */
  { &mach_tcpconn.state[TCPS_CLOSED],
    { PMDA_PMID(CLUSTER_TCPCONN,163), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* network.tcpconn.close_wait */
  { &mach_tcpconn.state[TCPS_CLOSE_WAIT],
    { PMDA_PMID(CLUSTER_TCPCONN,164), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* network.tcpconn.last_ack */
  { &mach_tcpconn.state[TCPS_LAST_ACK],
    { PMDA_PMID(CLUSTER_TCPCONN,165), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* network.tcpconn.listen */
  { &mach_tcpconn.state[TCPS_LISTEN],
    { PMDA_PMID(CLUSTER_TCPCONN,166), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* network.tcpconn.closing */
  { &mach_tcpconn.state[TCPS_CLOSING],
    { PMDA_PMID(CLUSTER_TCPCONN,167), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.tcp.activeopens */
  { &mach_tcp.stats.tcps_connattempt,
    { PMDA_PMID(CLUSTER_TCP,168), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.tcp.passiveopens */
  { &mach_tcp.stats.tcps_accepts,
    { PMDA_PMID(CLUSTER_TCP,169), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.tcp.attemptfails */
  { &mach_tcp.stats.tcps_conndrops,
    { PMDA_PMID(CLUSTER_TCP,170), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.tcp.estabresets */
  { &mach_tcp.stats.tcps_drops,
    { PMDA_PMID(CLUSTER_TCP,171), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.tcp.currestab */
  { NULL,
    { PMDA_PMID(CLUSTER_TCP,172), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* network.tcp.insegs */
  { &mach_tcp.stats.tcps_rcvtotal,
    { PMDA_PMID(CLUSTER_TCP,173), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.tcp.outsegs */
  { &mach_tcp.stats.tcps_sndtotal,
    { PMDA_PMID(CLUSTER_TCP,174), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.tcp.retranssegs */
  { &mach_tcp.stats.tcps_sndrexmitpack,
    { PMDA_PMID(CLUSTER_TCP,175), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.tcp.inerrs.total */
  { NULL,
    { PMDA_PMID(CLUSTER_TCP,176), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.tcp.outrsts */
  { NULL,
    { PMDA_PMID(CLUSTER_TCP,177), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.tcp.incsumerrors */
  { &mach_tcp.stats.tcps_rcvbadsum,
    { PMDA_PMID(CLUSTER_TCP,178), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.tcp.rtoalgorithm */
  { NULL,
    { PMDA_PMID(CLUSTER_TCP,179), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* network.tcp.rtomin */
  { NULL,
    { PMDA_PMID(CLUSTER_TCP,180), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,-1,0,0,PM_TIME_MSEC,0) }, },
/* network.tcp.rtomax */
  { NULL,
    { PMDA_PMID(CLUSTER_TCP,181), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,-1,0,0,PM_TIME_MSEC,0) }, },
/* network.tcp.maxconn */
  { NULL,
    { PMDA_PMID(CLUSTER_TCP,182), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* network.tcp.inerrs.badsum */
  { &mach_tcp.stats.tcps_rcvbadsum,
    { PMDA_PMID(CLUSTER_TCP,183), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.tcp.inerrs.badoff */
  { &mach_tcp.stats.tcps_rcvbadoff,
    { PMDA_PMID(CLUSTER_TCP,184), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.tcp.inerrs.short */
  { &mach_tcp.stats.tcps_rcvshort,
    { PMDA_PMID(CLUSTER_TCP,185), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* network.tcp.inerrs.memdrop */
  { &mach_tcp.stats.tcps_rcvmemdrop,
    { PMDA_PMID(CLUSTER_TCP,186), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* kernel.limits.maxproc */
  { &mach_vfs.maxproc,
    { PMDA_PMID(CLUSTER_LIMITS,0), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* kernel.limits.maxprocperuid */
  { &mach_vfs.maxprocperuid,
    { PMDA_PMID(CLUSTER_LIMITS,1), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* kernel.limits.maxfiles */
  { &mach_vfs.maxfiles,
    { PMDA_PMID(CLUSTER_LIMITS,2), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* kernel.limits.maxfilesperproc */
  { &mach_vfs.maxfilesperproc,
    { PMDA_PMID(CLUSTER_LIMITS,3), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* vfs.vnodes.recycled */
  { &mach_vfs.recycled_vnodes,
    { PMDA_PMID(CLUSTER_LIMITS,4), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* hinv.ngpu */
  { NULL,
    { PMDA_PMID(CLUSTER_GPU,99), PM_TYPE_U32, PM_INDOM_NULL,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* darwin.gpu.util */
  { NULL,
    { PMDA_PMID(CLUSTER_GPU,0), PM_TYPE_U32, GPU_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* darwin.gpu.memory.used */
  { NULL,
    { PMDA_PMID(CLUSTER_GPU,1), PM_TYPE_U64, GPU_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* darwin.gpu.memory.free */
  { NULL,
    { PMDA_PMID(CLUSTER_GPU,2), PM_TYPE_U64, GPU_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

};

int metrictab_sz = sizeof(metrictab) / sizeof(metrictab[0]);
