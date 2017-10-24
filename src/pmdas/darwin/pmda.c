/*
 * MacOS X kernel PMDA
 * "darwin" is easier to type than "macosx",  especially for Aussies. ;-)
 *
 * Copyright (c) 2012 Red Hat.
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
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/utsname.h>
#include <mach/mach.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "domain.h"

#include "darwin.h"
#include "disk.h"
#include "network.h"


#define page_count_to_kb(x) (((__uint64_t)(x) << mach_page_shift) >> 10)
#define page_count_to_mb(x) (((__uint64_t)(x) << mach_page_shift) >> 20)

static pmdaInterface		dispatch;
static int			_isDSO = 1;	/* =0 I am a daemon */
static char			*username;

mach_port_t		mach_host = 0;
vm_size_t		mach_page_size = 0;
unsigned int		mach_page_shift = 0;

unsigned int		mach_hertz = 0;
extern int refresh_hertz(unsigned int *);

int			mach_uname_error = 0;
struct utsname		mach_uname = { { 0 } };
extern int refresh_uname(struct utsname *);

int			mach_loadavg_error = 0;
float			mach_loadavg[3] = { 0,0,0 };
extern int refresh_loadavg(float *);

int			mach_cpuload_error = 0;
struct host_cpu_load_info	mach_cpuload = { { 0 } };
extern int refresh_cpuload(struct host_cpu_load_info *);

int			mach_vmstat_error = 0;
struct vm_statistics	mach_vmstat = { 0 };
extern int refresh_vmstat(struct vm_statistics *);

int			mach_fs_error = 0;
struct statfs		*mach_fs = NULL;
extern int refresh_filesys(struct statfs **, pmdaIndom *);

int			mach_disk_error = 0;
struct diskstats	mach_disk = { 0 };
extern int refresh_disks(struct diskstats *, pmdaIndom *);

int			mach_cpu_error = 0;
struct processor_cpu_load_info	*mach_cpu = NULL;
extern int refresh_cpus(struct processor_cpu_load_info **, pmdaIndom *);

int			mach_uptime_error = 0;
unsigned int		mach_uptime = 0;
extern int refresh_uptime(unsigned int *);

int			mach_net_error = 0;
struct netstats		mach_net = { 0 };
extern int refresh_network(struct netstats *, pmdaIndom *);
extern void init_network(void);

int			mach_nfs_error = 0;
struct nfsstats		mach_nfs = { 0 };
extern int refresh_nfs(struct nfsstats *);

char			hw_model[MODEL_SIZE];
extern int refresh_hinv(void);

/*
 * Metric Instance Domains (statically initialized ones only)
 */
static pmdaInstid loadavg_indom_id[] = {
    { 1, "1 minute" },	{ 5, "5 minute" },	{ 15, "15 minute" }
};
#define LOADAVG_COUNT	(sizeof(loadavg_indom_id)/sizeof(pmdaInstid))

static pmdaInstid nfs3_indom_id[] = {
    { 0, "null" },	{ 1, "getattr" },	{ 2, "setattr" },
    { 3, "lookup" },	{ 4, "access" },	{ 5, "readlink" },
    { 6, "read" },	{ 7, "write" },		{ 8, "create" },
    { 9, "mkdir" },	{ 10, "symlink" },	{ 11, "mknod" },
    { 12, "remove" },	{ 13, "rmdir" },	{ 14, "rename" },
    { 15, "link" },	{ 16, "readdir" },	{ 17, "readdir+" },
    { 18, "statfs" },	{ 19, "fsinfo" },	{ 20, "pathconf" },
    { 21, "commit" },	{ 22, "getlease" },	{ 23, "vacate" },
    { 24, "evict" }
};
#define NFS3_RPC_COUNT	(sizeof(nfs3_indom_id)/sizeof(pmdaInstid))

/*
 * Metric Instance Domain table
 */
enum {
    LOADAVG_INDOM,		/* 0 - 1, 5, 15 minute run queue averages */
    FILESYS_INDOM,		/* 1 - set of all mounted filesystems */
    DISK_INDOM,			/* 2 - set of all disk devices */
    CPU_INDOM,			/* 3 - set of all processors */
    NETWORK_INDOM,		/* 4 - set of all network interfaces */
    NFS3_INDOM,			/* 5 - nfs v3 operations */
    NUM_INDOMS			/* total number of instance domains */
};

static pmdaIndom indomtab[] = {
    { LOADAVG_INDOM,	3, loadavg_indom_id },
    { FILESYS_INDOM,	0, NULL },
    { DISK_INDOM,	0, NULL },
    { CPU_INDOM,	0, NULL },
    { NETWORK_INDOM,	0, NULL },
    { NFS3_INDOM,	NFS3_RPC_COUNT, nfs3_indom_id },
};

/*
 * Fetch clusters and metric table
 */
enum {
    CLUSTER_INIT = 0,		/*  0 = values we know at startup */
    CLUSTER_VMSTAT,		/*  1 = mach memory statistics */
    CLUSTER_KERNEL_UNAME,	/*  2 = utsname information */
    CLUSTER_LOADAVG,		/*  3 = run queue averages */
    CLUSTER_HINV,		/*  4 = hardware inventory */
    CLUSTER_FILESYS,		/*  5 = mounted filesystems */
    CLUSTER_CPULOAD,		/*  6 = number of ticks in state */
    CLUSTER_DISK,		/*  7 = disk device statistics */
    CLUSTER_CPU,		/*  8 = per-cpu statistics */
    CLUSTER_UPTIME,		/*  9 = system uptime in seconds */
    CLUSTER_NETWORK,		/* 10 = networking statistics */
    CLUSTER_NFS,		/* 11 = nfs filesystem statistics */
    NUM_CLUSTERS		/* total number of clusters */
};

static pmdaMetric metrictab[] = {

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
/* mem.cache_hits */
  { &mach_vmstat.hits,
    { PMDA_PMID(CLUSTER_VMSTAT,17), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* mem.cache_lookups */
  { &mach_vmstat.lookups,
    { PMDA_PMID(CLUSTER_VMSTAT,18), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
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

/* kernel.uname.release */
  { mach_uname.release,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 23), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* kernel.uname.version */
  { mach_uname.version,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 24), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* kernel.uname.sysname */
  { mach_uname.sysname,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 25), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* kernel.uname.machine */
  { mach_uname.machine,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 26), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* kernel.uname.nodename */
  { mach_uname.nodename,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 27), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* pmda.uname */
  { NULL,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 28), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* pmda.version */
  { NULL,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 29), PM_TYPE_STRING, PM_INDOM_NULL,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

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
    { PMDA_PMID(CLUSTER_NFS,94), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* nfs3.client.reqs */
  { NULL,
    { PMDA_PMID(CLUSTER_NFS,95), PM_TYPE_32, NFS3_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* nfs3.server.calls */
  { NULL,
    { PMDA_PMID(CLUSTER_NFS,96), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* nfs3.server.reqs */
  { NULL,
    { PMDA_PMID(CLUSTER_NFS,97), PM_TYPE_32, NFS3_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.rpccnt */
  { &mach_nfs.rpcrequests,
    { PMDA_PMID(CLUSTER_NFS,98), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.rpcretrans */
  { &mach_nfs.rpcretries,
    { PMDA_PMID(CLUSTER_NFS,99), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.rpctimeouts */
  { &mach_nfs.rpctimeouts,
    { PMDA_PMID(CLUSTER_NFS,100), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.rpcinvalid */
  { &mach_nfs.rpcinvalid,
    { PMDA_PMID(CLUSTER_NFS,101), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.rpcunexpected */
  { &mach_nfs.rpcunexpected,
    { PMDA_PMID(CLUSTER_NFS,102), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.attrcache.hits */
  { &mach_nfs.attrcache_hits,
    { PMDA_PMID(CLUSTER_NFS,103), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.attrcache.misses */
  { &mach_nfs.attrcache_misses,
    { PMDA_PMID(CLUSTER_NFS,104), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.lookupcache.hits */
  { &mach_nfs.lookupcache_hits,
    { PMDA_PMID(CLUSTER_NFS,105), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.lookupcache.misses */
  { &mach_nfs.lookupcache_misses,
    { PMDA_PMID(CLUSTER_NFS,106), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.read.hits */
  { &mach_nfs.read_bios,
    { PMDA_PMID(CLUSTER_NFS,107), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.read.misses */
  { &mach_nfs.biocache_reads,
    { PMDA_PMID(CLUSTER_NFS,108), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.write.hits */
  { &mach_nfs.write_bios,
    { PMDA_PMID(CLUSTER_NFS,109), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.write.misses */
  { &mach_nfs.biocache_writes,
    { PMDA_PMID(CLUSTER_NFS,110), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.readlink.hits */
  { &mach_nfs.readlink_bios,
    { PMDA_PMID(CLUSTER_NFS,111), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.readlink.misses */
  { &mach_nfs.biocache_readlinks,
    { PMDA_PMID(CLUSTER_NFS,112), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.readdir.hits */
  { &mach_nfs.readdir_bios,
    { PMDA_PMID(CLUSTER_NFS,113), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.biocache.readdir.misses */
  { &mach_nfs.biocache_readdirs,
    { PMDA_PMID(CLUSTER_NFS,114), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.direofcache.hits */
  { &mach_nfs.direofcache_hits,
    { PMDA_PMID(CLUSTER_NFS,115), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.client.direofcache.misses */
  { &mach_nfs.direofcache_misses,
    { PMDA_PMID(CLUSTER_NFS,116), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.retfailed */
  { &mach_nfs.srvrpc_errs,
    { PMDA_PMID(CLUSTER_NFS,117), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.faults */
  { &mach_nfs.srvrpc_errs,
    { PMDA_PMID(CLUSTER_NFS,118), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.cache.inprog */
  { &mach_nfs.srvcache_inproghits,
    { PMDA_PMID(CLUSTER_NFS,119), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.cache.nonidem */
  { &mach_nfs.srvcache_nonidemdonehits,
    { PMDA_PMID(CLUSTER_NFS,120), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.cache.idem */
  { &mach_nfs.srvcache_idemdonehits,
    { PMDA_PMID(CLUSTER_NFS,121), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.cache.misses */
  { &mach_nfs.srvcache_misses,
    { PMDA_PMID(CLUSTER_NFS,122), PM_TYPE_32, PM_INDOM_NULL,
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
  { &mach_nfs.srvvop_writes,
    { PMDA_PMID(CLUSTER_NFS,126), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.pageins */
  { &mach_nfs.pageins,
    { PMDA_PMID(CLUSTER_NFS,127), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* rpc.server.pageouts */
  { &mach_nfs.pageouts,
    { PMDA_PMID(CLUSTER_NFS,128), PM_TYPE_32, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* filesys.maxfiles */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,129), PM_TYPE_U32, FILESYS_INDOM,
       PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

};

static void
darwin_refresh(int *need_refresh)
{
    if (need_refresh[CLUSTER_LOADAVG])
	mach_loadavg_error = refresh_loadavg(mach_loadavg);
    if (need_refresh[CLUSTER_CPULOAD])
	mach_cpuload_error = refresh_cpuload(&mach_cpuload);
    if (need_refresh[CLUSTER_VMSTAT])
	mach_vmstat_error = refresh_vmstat(&mach_vmstat);
    if (need_refresh[CLUSTER_KERNEL_UNAME])
	mach_uname_error = refresh_uname(&mach_uname);
    if (need_refresh[CLUSTER_FILESYS])
	mach_fs_error = refresh_filesys(&mach_fs, &indomtab[FILESYS_INDOM]);
    if (need_refresh[CLUSTER_DISK])
	mach_disk_error = refresh_disks(&mach_disk, &indomtab[DISK_INDOM]);
    if (need_refresh[CLUSTER_CPU])
	mach_cpu_error = refresh_cpus(&mach_cpu, &indomtab[CPU_INDOM]);
    if (need_refresh[CLUSTER_UPTIME])
	mach_uptime_error = refresh_uptime(&mach_uptime);
    if (need_refresh[CLUSTER_NETWORK])
	mach_net_error = refresh_network(&mach_net, &indomtab[NETWORK_INDOM]);
    if (need_refresh[CLUSTER_NFS])
	mach_nfs_error = refresh_nfs(&mach_nfs);
}

static inline int
fetch_loadavg(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    if (mach_loadavg_error)
	return mach_loadavg_error;
    switch (item) {
    case 30:  /* kernel.all.load */
	if (inst == 1)
	    atom->f = mach_loadavg[0];
	else if (inst == 5)
	    atom->f = mach_loadavg[1];
	else if (inst == 15)
	    atom->f = mach_loadavg[2];
	else
	    return PM_ERR_INST; 
	return 1;
    }
    return PM_ERR_PMID;
}

static inline int
fetch_cpuload(unsigned int item, pmAtomValue *atom)
{
    if (mach_cpuload_error)
	return mach_cpuload_error;
    switch (item) {
    case 42: /* kernel.all.cpu.user */
	atom->ull = LOAD_SCALE * (double)
		mach_cpuload.cpu_ticks[CPU_STATE_USER] / mach_hertz;
        return 1;
    case 43: /* kernel.all.cpu.nice */
	atom->ull = LOAD_SCALE * (double)
		mach_cpuload.cpu_ticks[CPU_STATE_NICE] / mach_hertz;
        return 1;
    case 44: /* kernel.all.cpu.sys */
	atom->ull = LOAD_SCALE * (double)
		mach_cpuload.cpu_ticks[CPU_STATE_SYSTEM] / mach_hertz;
        return 1;
    case 45: /* kernel.all.cpu.idle */
	atom->ull = LOAD_SCALE * (double)
		mach_cpuload.cpu_ticks[CPU_STATE_IDLE] / mach_hertz;
        return 1;
    }
    return PM_ERR_PMID;
}

static inline int
fetch_vmstat(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    if (mach_vmstat_error)
	return mach_vmstat_error;
    switch (item) {
    case 2: /* hinv.physmem */
	atom->ul = (__uint32_t)page_count_to_mb(
			mach_vmstat.free_count + mach_vmstat.wire_count +
			mach_vmstat.active_count + mach_vmstat.inactive_count);
	return 1;
    case 3: /* mem.physmem */
	atom->ull = page_count_to_kb(
			mach_vmstat.free_count + mach_vmstat.wire_count +
			mach_vmstat.active_count + mach_vmstat.inactive_count);
	return 1;
    case 4: /* mem.freemem */
	atom->ull = page_count_to_kb(mach_vmstat.free_count);
	return 1;
    case 5: /* mem.active */
	atom->ull = page_count_to_kb(mach_vmstat.active_count);
	return 1;
    case 6: /* mem.inactive */
	atom->ull = page_count_to_kb(mach_vmstat.inactive_count);
	return 1;
    case 19: /* mem.util.wired */
	atom->ull = page_count_to_kb(mach_vmstat.wire_count);
	return 1;
    case 20: /* mem.util.active */
	atom->ull = page_count_to_kb(mach_vmstat.active_count);
	return 1;
    case 21: /* mem.util.inactive */
	atom->ull = page_count_to_kb(mach_vmstat.inactive_count);
	return 1;
    case 22: /* mem.util.free */
	atom->ull = page_count_to_kb(mach_vmstat.free_count);
	return 1;
    case 23: /* mem.util.used */
	atom->ull = page_count_to_kb(mach_vmstat.wire_count+mach_vmstat.active_count+mach_vmstat.inactive_count);
	return 1;
    }
    return PM_ERR_PMID;
}

static inline int
fetch_uname(unsigned int item, pmAtomValue *atom)
{
    static char mach_uname_all[(_SYS_NAMELEN*5)+8];

    if (mach_uname_error)
	return mach_uname_error;
    switch (item) {
    case 28: /* pmda.uname */
	pmsprintf(mach_uname_all, sizeof(mach_uname_all), "%s %s %s %s %s",
		mach_uname.sysname, mach_uname.nodename,
		mach_uname.release, mach_uname.version,
		mach_uname.machine);
	atom->cp = mach_uname_all;
	return 1;
    case 29: /* pmda.version */
	atom->cp = pmGetConfig("PCP_VERSION");
	return 1;
    }
    return PM_ERR_PMID;
}

static inline int
fetch_filesys(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    __uint64_t	ull, used;

    if (mach_fs_error)
	return mach_fs_error;
    if (item == 31) {	/* hinv.nfilesys */
	atom->ul = indomtab[FILESYS_INDOM].it_numinst;
	return 1;
    }
    if (indomtab[FILESYS_INDOM].it_numinst == 0)
	return 0;	/* no values available */
    if (inst < 0 || inst >= indomtab[FILESYS_INDOM].it_numinst)
	return PM_ERR_INST;
    switch (item) {
    case 32: /* filesys.capacity */
	ull = (__uint64_t)mach_fs[inst].f_blocks;
	atom->ull = ull * mach_fs[inst].f_bsize >> 10;
	return 1;
    case 33: /* filesys.used */
	used = (__uint64_t)(mach_fs[inst].f_blocks - mach_fs[inst].f_bfree);
	atom->ull = used * mach_fs[inst].f_bsize >> 10;
	return 1;
    case 34: /* filesys.free */
	ull = (__uint64_t)mach_fs[inst].f_bfree;
	atom->ull = ull * mach_fs[inst].f_bsize >> 10;
	return 1;
    case 129: /* filesys.maxfiles */
	atom->ul = mach_fs[inst].f_files;
	return 1;
    case 35: /* filesys.usedfiles */
	atom->ul = mach_fs[inst].f_files - mach_fs[inst].f_ffree;
	return 1;
    case 36: /* filesys.freefiles */
	atom->ul = mach_fs[inst].f_ffree;
	return 1;
    case 37: /* filesys.mountdir */
	atom->cp = mach_fs[inst].f_mntonname;
	return 1;
    case 38: /* filesys.full */
	used = (__uint64_t)(mach_fs[inst].f_blocks - mach_fs[inst].f_bfree);
	ull = used + (__uint64_t)mach_fs[inst].f_bavail;
	atom->d = (100.0 * (double)used) / (double)ull;
	return 1;
    case 39: /* filesys.blocksize */
	atom->ul = mach_fs[inst].f_bsize;
	return 1;
    case 40: /* filesys.avail */
	ull = (__uint64_t)mach_fs[inst].f_bavail;
	atom->ull = ull * mach_fs[inst].f_bsize >> 10;
	return 1;
    case 41: /* filesys.type */
	atom->cp = mach_fs[inst].f_fstypename;
	return 1;
    }
    return PM_ERR_PMID;
}

static inline int
fetch_disk(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    if (mach_disk_error)
	return mach_disk_error;
    if (item == 46) {	/* hinv.ndisk */
	atom->ul = indomtab[DISK_INDOM].it_numinst;
	return 1;
    }
    if (indomtab[DISK_INDOM].it_numinst == 0)
	return 0;	/* no values available */
    if (item < 59 && (inst < 0 || inst >= indomtab[DISK_INDOM].it_numinst))
	return PM_ERR_INST;
    switch (item) {
    case 47: /* disk.dev.read */
	atom->ull = mach_disk.disks[inst].read;
	return 1;
    case 48: /* disk.dev.write */
	atom->ull = mach_disk.disks[inst].write;
	return 1;
    case 49: /* disk.dev.total */
	atom->ull = mach_disk.disks[inst].read + mach_disk.disks[inst].write;
	return 1;
    case 50: /* disk.dev.read_bytes */
	atom->ull = mach_disk.disks[inst].read_bytes >> 10;
	return 1;
    case 51: /* disk.dev.write_bytes */
	atom->ull = mach_disk.disks[inst].write_bytes >> 10;
	return 1;
    case 52: /* disk.dev.total_bytes */
	atom->ull = (mach_disk.disks[inst].read_bytes +
			mach_disk.disks[inst].write_bytes) >> 10;
	return 1;
    case 53: /* disk.dev.blkread */
	atom->ull = mach_disk.disks[inst].read_bytes /
			mach_disk.disks[inst].blocksize;
	return 1;
    case 54: /* disk.dev.blkwrite */
	atom->ull = mach_disk.disks[inst].write_bytes /
			mach_disk.disks[inst].blocksize;
	return 1;
    case 55: /* disk.dev.blktotal */
	atom->ull = (mach_disk.disks[inst].read_bytes +
			 mach_disk.disks[inst].write_bytes) /
				mach_disk.disks[inst].blocksize;
	return 1;
    case 56: /* disk.dev.read_time */
	atom->ull = mach_disk.disks[inst].read_time;
	return 1;
    case 57: /* disk.dev.write_time */
	atom->ull = mach_disk.disks[inst].write_time;
	return 1;
    case 58: /* disk.dev.total_time */
	atom->ull = mach_disk.disks[inst].read_time +
				mach_disk.disks[inst].write_time;
	return 1;
    case 59: /* disk.all.read */
	atom->ull = mach_disk.read;
	return 1;
    case 60: /* disk.all.write */
	atom->ull = mach_disk.write;
	return 1;
    case 61: /* disk.all.total */
	atom->ull = mach_disk.read + mach_disk.write;
	return 1;
    case 62: /* disk.all.read_bytes */
	atom->ull = mach_disk.read_bytes >> 10;
	return 1;
    case 63: /* disk.all.write_bytes */
	atom->ull = mach_disk.write_bytes >> 10;
	return 1;
    case 64: /* disk.all.total_bytes */
	atom->ull = (mach_disk.read_bytes + mach_disk.write_bytes) >> 10;
	return 1;
    case 65: /* disk.all.blkread */
	atom->ull = mach_disk.blkread;
	return 1;
    case 66: /* disk.all.blkwrite */
	atom->ull = mach_disk.blkwrite;
	return 1;
    case 67: /* disk.all.blktotal */
	atom->ull = mach_disk.blkread + mach_disk.blkwrite;
	return 1;
    case 68: /* disk.all.read_time */
	atom->ull = mach_disk.read_time;
	return 1;
    case 69: /* disk.all.write_time */
	atom->ull = mach_disk.write_time;
	return 1;
    case 70: /* disk.all.total_time */
	atom->ull = mach_disk.read_time + mach_disk.write_time;
	return 1;
    }
    return PM_ERR_PMID;
}

static inline int
fetch_cpu(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    if (mach_cpu_error)
	return mach_cpu_error;
    if (item == 71) {	/* hinv.ncpu */
	atom->ul = indomtab[CPU_INDOM].it_numinst;
	return 1;
    }
    if (indomtab[CPU_INDOM].it_numinst == 0)	/* uh-huh. */
	return 0;	/* no values available */
    if (inst < 0 || inst >= indomtab[CPU_INDOM].it_numinst)
	return PM_ERR_INST;
    switch (item) {
    case 72: /* kernel.percpu.cpu.user */
	atom->ull = LOAD_SCALE * (double)
			mach_cpu[inst].cpu_ticks[CPU_STATE_USER] / mach_hertz;
	return 1;
    case 73: /* kernel.percpu.cpu.nice */
	atom->ull = LOAD_SCALE * (double)
			mach_cpu[inst].cpu_ticks[CPU_STATE_NICE] / mach_hertz;
	return 1;
    case 74: /* kernel.percpu.cpu.sys */
	atom->ull = LOAD_SCALE * (double)
			mach_cpu[inst].cpu_ticks[CPU_STATE_SYSTEM] / mach_hertz;
	return 1;
    case 75: /* kernel.percpu.cpu.idle */
	atom->ull = LOAD_SCALE * (double)
			mach_cpu[inst].cpu_ticks[CPU_STATE_IDLE] / mach_hertz;
	return 1;
    }
    return PM_ERR_PMID;
}

static inline int
fetch_network(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    if (mach_net_error)
	return mach_net_error;
    if (indomtab[NETWORK_INDOM].it_numinst == 0)
	return 0;	/* no values available */
    if (inst < 0 || inst >= indomtab[NETWORK_INDOM].it_numinst)
	return PM_ERR_INST;
    switch (item) {
    case 77: /* network.interface.in.bytes */
	atom->ull = mach_net.interfaces[inst].ibytes;
	return 1;
    case 78: /* network.interface.in.packets */
	atom->ull = mach_net.interfaces[inst].ipackets;
	return 1;
    case 79: /* network.interface.in.errors */
	atom->ull = mach_net.interfaces[inst].ierrors;
	return 1;
    case 80: /* network.interface.in.drops */
	atom->ull = mach_net.interfaces[inst].iqdrops;
	return 1;
    case 81: /* network.interface.in.mcasts */
	atom->ull = mach_net.interfaces[inst].imcasts;
	return 1;
    case 82: /* network.interface.out.bytes */
	atom->ull = mach_net.interfaces[inst].obytes;
	return 1;
    case 83: /* network.interface.out.packets */
	atom->ull = mach_net.interfaces[inst].opackets;
	return 1;
    case 84: /* network.interface.out.errors */
	atom->ull = mach_net.interfaces[inst].oerrors;
	return 1;
    case 85: /* network.interface.out.mcasts */
	atom->ull = mach_net.interfaces[inst].omcasts;
	return 1;
    case 86: /* network.interface.collisions */
	atom->ull = mach_net.interfaces[inst].collisions;
	return 1;
    case 87: /* network.interface.mtu */
	atom->ull = mach_net.interfaces[inst].mtu;
	return 1;
    case 88: /* network.interface.baudrate */
	atom->ull = mach_net.interfaces[inst].baudrate;
	return 1;
    case 89: /* network.interface.total.bytes */
	atom->ull = mach_net.interfaces[inst].ibytes +
		    mach_net.interfaces[inst].obytes;
	return 1;
    case 90: /* network.interface.total.packets */
	atom->ull = mach_net.interfaces[inst].ipackets +
		    mach_net.interfaces[inst].opackets;
	return 1;
    case 91: /* network.interface.total.errors */
	atom->ull = mach_net.interfaces[inst].ierrors +
		    mach_net.interfaces[inst].oerrors;
	return 1;
    case 92: /* network.interface.total.drops */
	atom->ull = mach_net.interfaces[inst].iqdrops;
	return 1;
    case 93: /* network.interface.total.mcasts */
	atom->ull = mach_net.interfaces[inst].imcasts +
		    mach_net.interfaces[inst].omcasts;
	return 1;
    }
    return PM_ERR_PMID;
}

static inline int
fetch_nfs(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    if (mach_net_error)
	return mach_net_error;
    switch (item) {
    case 94: /* nfs3.client.calls */
	for (atom->l = 0, inst = 0; inst < NFS3_RPC_COUNT; inst++)
	    atom->l += mach_nfs.rpccnt[inst];
	return 1;
    case 95: /* nfs3.client.reqs */
	if (inst < 0 || inst >= NFS3_RPC_COUNT)
	    return PM_ERR_INST;
	atom->l = mach_nfs.rpccnt[inst];
	return 1;
    case 96: /* nfs3.server.calls */
	for (atom->l = 0, inst = 0; inst < NFS3_RPC_COUNT; inst++)
	    atom->l += mach_nfs.srvrpccnt[inst];
	return 1;
    case 97: /* nfs3.server.reqs */
	if (inst < 0 || inst >= NFS3_RPC_COUNT)
	    return PM_ERR_INST;
	atom->l = mach_nfs.srvrpccnt[inst];
	return 1;
    case 123:	/* rpc.server.nqnfs.leases    -- deprecated */
    case 124:	/* rpc.server.nqnfs.maxleases -- deprecated */
    case 125:	/* rpc.server.nqnfs.getleases -- deprecated */
	return PM_ERR_APPVERSION;
    }
    return PM_ERR_PMID;
}


static int
darwin_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int	*idp = (__pmID_int *)&(mdesc->m_desc.pmid);

    if (mdesc->m_user) {
	/*   
	 * The metric value is extracted directly via the address specified
	 * in metrictab.  Note: not all metrics support this - those that
	 * don't have NULL for the m_user field in their respective
	 * metrictab slot.
	 */
	switch (mdesc->m_desc.type) {
        case PM_TYPE_32:	atom->l = *(__int32_t *)mdesc->m_user; break;
	case PM_TYPE_U32:	atom->ul = *(__uint32_t *)mdesc->m_user; break;
	case PM_TYPE_64:	atom->ll = *(__int64_t *)mdesc->m_user; break;
	case PM_TYPE_U64:	atom->ull = *(__uint64_t *)mdesc->m_user; break;
	case PM_TYPE_FLOAT:	atom->f = *(float *)mdesc->m_user; break;
	case PM_TYPE_DOUBLE:	atom->d = *(double *)mdesc->m_user; break;
	case PM_TYPE_STRING:	atom->cp = (char *)mdesc->m_user; break;
	case PM_TYPE_NOSUPPORT: return 0;
	default:		fprintf(stderr,
			"Error in fetchCallBack: unsupported metric type %s\n",
					pmTypeStr(mdesc->m_desc.type));
				return 0;
	}
	return 1;
    }

    switch (idp->cluster) {
    case CLUSTER_LOADAVG:	return fetch_loadavg(idp->item, inst, atom);
    case CLUSTER_CPULOAD:	return fetch_cpuload(idp->item, atom);
    case CLUSTER_VMSTAT:	return fetch_vmstat(idp->item, inst, atom);
    case CLUSTER_KERNEL_UNAME:	return fetch_uname(idp->item, atom);
    case CLUSTER_FILESYS:	return fetch_filesys(idp->item, inst, atom);
    case CLUSTER_DISK:		return fetch_disk(idp->item, inst, atom);
    case CLUSTER_CPU:		return fetch_cpu(idp->item, inst, atom);
    case CLUSTER_NETWORK:	return fetch_network(idp->item, inst, atom);
    case CLUSTER_NFS:		return fetch_nfs(idp->item, inst, atom);
    }
    return 0;
}

static int
darwin_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    __pmInDom_int	*indomp = (__pmInDom_int *)&indom;
    int			need_refresh[NUM_CLUSTERS] = { 0 };

    switch (indomp->serial) {
    case FILESYS_INDOM: need_refresh[CLUSTER_FILESYS]++; break;
    case DISK_INDOM:	need_refresh[CLUSTER_DISK]++; break;
    case CPU_INDOM:	need_refresh[CLUSTER_CPU]++; break;
    case NETWORK_INDOM:	need_refresh[CLUSTER_NETWORK]++; break;
    }
    darwin_refresh(need_refresh);
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
darwin_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int	i, need_refresh[NUM_CLUSTERS] = { 0 };

    for (i = 0; i < numpmid; i++) {
	__pmID_int *idp = (__pmID_int *)&(pmidlist[i]);
	if (idp->cluster >= 0 && idp->cluster < NUM_CLUSTERS)
	    need_refresh[idp->cluster]++;
    }
    darwin_refresh(need_refresh);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

void 
darwin_init(pmdaInterface *dp)
{
    int		sts;

    if (_isDSO) {
	int sep = __pmPathSeparator();
	char helppath[MAXPATHLEN];
	pmsprintf(helppath, MAXPATHLEN, "%s%c" "darwin" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_3, "darwin DSO", helppath);
    } else {
	__pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    dp->version.two.instance = darwin_instance;
    dp->version.two.fetch = darwin_fetch;
    pmdaSetFetchCallBack(dp, darwin_fetchCallBack);

    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]),
		metrictab, sizeof(metrictab)/sizeof(metrictab[0]));

    mach_host = mach_host_self();
    host_page_size(mach_host, &mach_page_size);
    mach_page_shift = ffs(mach_page_size) - 1;
    if (refresh_hertz(&mach_hertz) != 0)
	mach_hertz = 100;
    if ((sts = refresh_hinv()) != 0)
	fprintf(stderr, "darwin_init: refresh_hinv failed: %s\n", pmErrStr(sts));
    init_network();
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
"  -d domain    use domain (numeric) for metrics domain of PMDA\n"
"  -l logfile   write log into logfile rather than using default log name\n"
"  -U username  user account to run under (default \"pcp\")\n"
"\nExactly one of the following options may appear:\n"
"  -i port      expect PMCD to connect on given inet port (number or name)\n"
"  -p           expect PMCD to supply stdin/stdout (pipe)\n"
"  -u socket    expect PMCD to connect on given unix domain socket\n"
"  -6 port      expect PMCD to connect on given ipv6 port (number or name)\n",
	  stderr);		
    exit(1);
}

int
main(int argc, char **argv)
{
    int			c, sep = __pmPathSeparator();
    int			errflag = 0;
    char		helppath[MAXPATHLEN];

    _isDSO = 0;
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    pmsprintf(helppath, MAXPATHLEN, "%s%c" "darwin" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_3, pmProgname, DARWIN, "darwin.log",
		helppath);

    while ((c = pmdaGetOpt(argc, argv, "D:d:i:l:pu:U:6:?", &dispatch, &errflag)) != EOF) {
	switch(c) {
	case 'U':
	    username = optarg;
	    break;
	default:
	    errflag++;
	}
    }
    if (errflag)
	usage();

    pmdaOpenLog(&dispatch);
    darwin_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
