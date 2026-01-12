/*
 * Darwin kernel (macOS) PMDA
 *
 * Copyright (c) 2012,2025 Red Hat.
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
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <mach/mach.h>
#include "pmapi.h"
#include "pmda.h"
#include "domain.h"

#include "darwin.h"
#include "disk.h"
#include "network.h"
#include "vfs.h"
#include "udp.h"
#include "icmp.h"
#include "sockstat.h"
#include "tcpconn.h"
#include "tcp.h"
#include "metrics.h"


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
struct vm_statistics64	mach_vmstat = { 0 };
extern int refresh_vmstat(struct vm_statistics64 *);

int			mach_swap_error = 0;
struct xsw_usage	mach_swap = { 0 };
extern int refresh_swap(struct xsw_usage *);

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

int			mach_vfs_error = 0;
vfsstats_t		mach_vfs = { 0 };

int			mach_udp_error = 0;
udpstats_t		mach_udp = { 0 };

int			mach_icmp_error = 0;
icmpstats_t		mach_icmp = { 0 };

int			mach_sockstat_error = 0;
sockstats_t		mach_sockstat = { 0 };

int			mach_tcpconn_error = 0;
tcpconn_stats_t		mach_tcpconn = { 0 };

int			mach_tcp_error = 0;
tcpstats_t		mach_tcp = { 0 };

char			hw_model[MODEL_SIZE];
extern int refresh_hinv(void);

extern char *macos_version(void);

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
 * (enum now defined in darwin.h for use by refactored modules)
 */
pmdaIndom indomtab[] = {
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
    CLUSTER_VFS,		/* 12 = vfs statistics */
    CLUSTER_UDP,		/* 13 = udp protocol statistics */
    CLUSTER_ICMP,		/* 14 = icmp protocol statistics */
    CLUSTER_SOCKSTAT,		/* 15 = socket statistics */
    CLUSTER_TCPCONN,		/* 16 = tcp connection states */
    CLUSTER_TCP,		/* 17 = tcp protocol statistics */
    NUM_CLUSTERS		/* total number of clusters */
};


static void
darwin_refresh(int *need_refresh)
{
    if (need_refresh[CLUSTER_LOADAVG])
	mach_loadavg_error = refresh_loadavg(mach_loadavg);
    if (need_refresh[CLUSTER_CPULOAD])
	mach_cpuload_error = refresh_cpuload(&mach_cpuload);
    if (need_refresh[CLUSTER_VMSTAT]) {
	mach_vmstat_error = refresh_vmstat(&mach_vmstat);
	mach_swap_error = refresh_swap(&mach_swap);
    }
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
    if (need_refresh[CLUSTER_VFS])
	mach_vfs_error = refresh_vfs(&mach_vfs);
    if (need_refresh[CLUSTER_UDP])
	mach_udp_error = refresh_udp(&mach_udp);
    if (need_refresh[CLUSTER_ICMP])
	mach_icmp_error = refresh_icmp(&mach_icmp);
    if (need_refresh[CLUSTER_SOCKSTAT])
	mach_sockstat_error = refresh_sockstat(&mach_sockstat);
    if (need_refresh[CLUSTER_TCPCONN])
	mach_tcpconn_error = refresh_tcpconn(&mach_tcpconn);
    if (need_refresh[CLUSTER_TCP])
	mach_tcp_error = refresh_tcp(&mach_tcp);
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
    case 24: /* swap.length */
	if (mach_swap_error)
	    return mach_swap_error;
	atom->ull = mach_swap.xsu_total >> 10;  // bytes to KB
	return 1;
    case 25: /* swap.used */
	if (mach_swap_error)
	    return mach_swap_error;
	atom->ull = mach_swap.xsu_used >> 10;   // bytes to KB
	return 1;
    case 26: /* swap.free */
	if (mach_swap_error)
	    return mach_swap_error;
	atom->ull = mach_swap.xsu_avail >> 10;  // bytes to KB
	return 1;
    case 130: /* mem.util.compressed */
	atom->ull = page_count_to_kb(mach_vmstat.compressor_page_count);
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
    case 30: /* kernel.all.distro */
	atom->cp = macos_version();
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
fetch_nfs(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    if (mach_nfs_error)
	return mach_nfs_error;
    switch (item) {
    case 94: /* nfs3.client.calls */
	for (atom->ull = 0, inst = 0; inst < NFS3_RPC_COUNT; inst++)
	    atom->ull += mach_nfs.client.rpccntv3[inst];
	return 1;
    case 95: /* nfs3.client.reqs */
	if (inst < 0 || inst >= NFS3_RPC_COUNT)
	    return PM_ERR_INST;
	atom->ull = mach_nfs.client.rpccntv3[inst];
	return 1;
    case 96: /* nfs3.server.calls */
	for (atom->ull = 0, inst = 0; inst < NFS3_RPC_COUNT; inst++)
	    atom->ull += mach_nfs.server.srvrpccntv3[inst];
	return 1;
    case 97: /* nfs3.server.reqs */
	if (inst < 0 || inst >= NFS3_RPC_COUNT)
	    return PM_ERR_INST;
	atom->ull = mach_nfs.server.srvrpccntv3[inst];
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
    unsigned int	item;

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

    item = pmID_item(mdesc->m_desc.pmid);
    switch (pmID_cluster(mdesc->m_desc.pmid)) {
    case CLUSTER_LOADAVG:	return fetch_loadavg(item, inst, atom);
    case CLUSTER_CPULOAD:	return fetch_cpuload(item, atom);
    case CLUSTER_VMSTAT:	return fetch_vmstat(item, inst, atom);
    case CLUSTER_KERNEL_UNAME:	return fetch_uname(item, atom);
    case CLUSTER_FILESYS:	return fetch_filesys(item, inst, atom);
    case CLUSTER_DISK:		return fetch_disk(item, inst, atom);
    case CLUSTER_CPU:		return fetch_cpu(item, inst, atom);
    case CLUSTER_NETWORK:	return fetch_network(item, inst, atom);
    case CLUSTER_NFS:		return fetch_nfs(item, inst, atom);
    case CLUSTER_VFS:		return fetch_vfs(item, atom);
    case CLUSTER_UDP:		return fetch_udp(item, atom);
    case CLUSTER_ICMP:		return fetch_icmp(item, atom);
    case CLUSTER_TCP:		return fetch_tcp(item, atom);
    }
    return 0;
}

static int
darwin_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
    int			need_refresh[NUM_CLUSTERS] = { 0 };

    switch (pmInDom_serial(indom)) {
    case FILESYS_INDOM: need_refresh[CLUSTER_FILESYS]++; break;
    case DISK_INDOM:	need_refresh[CLUSTER_DISK]++; break;
    case CPU_INDOM:	need_refresh[CLUSTER_CPU]++; break;
    case NETWORK_INDOM:	need_refresh[CLUSTER_NETWORK]++; break;
    }
    darwin_refresh(need_refresh);
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
darwin_fetch(int numpmid, pmID pmidlist[], pmdaResult **resp, pmdaExt *pmda)
{
    int	i, need_refresh[NUM_CLUSTERS] = { 0 };

    for (i = 0; i < numpmid; i++) {
	unsigned int	cluster = pmID_cluster(pmidlist[i]);
	if (cluster >= 0 && cluster < NUM_CLUSTERS)
	    need_refresh[cluster]++;
    }
    darwin_refresh(need_refresh);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static void
check_tcp_stats_access(void)
{
	int flag_value = 1;
	size_t len = sizeof(flag_value);

	if (sysctlbyname("net.inet.tcp.disable_access_to_stats",
			 &flag_value, &len, NULL, 0) == 0) {
		if (flag_value != 0) {
			pmNotifyErr(LOG_WARNING,
				"TCP statistics access is DISABLED (net.inet.tcp.disable_access_to_stats=%d).\n"
				"All network.tcp.* metrics will report zero values.\n"
				"\n"
				"To enable TCP statistics, run as root:\n"
				"    sudo sysctl -w net.inet.tcp.disable_access_to_stats=0\n"
				"\n"
				"To make this permanent across reboots, add to /etc/sysctl.conf:\n"
				"    net.inet.tcp.disable_access_to_stats=0\n"
				"\n"
				"See pmdadarwin(1) for more information.",
				flag_value);
		}
	}
}

void
darwin_init(pmdaInterface *dp)
{
    int		sts;

    if (_isDSO) {
	int sep = pmPathSeparator();
	char helppath[MAXPATHLEN];
	pmsprintf(helppath, MAXPATHLEN, "%s%c" "darwin" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_3, "darwin DSO", helppath);
    } else {
	pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    dp->version.two.instance = darwin_instance;
    dp->version.two.fetch = darwin_fetch;
    pmdaSetFetchCallBack(dp, darwin_fetchCallBack);

    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]),
		metrictab, metrictab_sz);

    mach_host = mach_host_self();
    host_page_size(mach_host, &mach_page_size);
    mach_page_shift = ffs(mach_page_size) - 1;
    if (refresh_hertz(&mach_hertz) != 0)
	mach_hertz = 100;
    if ((sts = refresh_hinv()) != 0)
	fprintf(stderr, "darwin_init: refresh_hinv failed: %s\n", pmErrStr(sts));
    init_network();
    check_tcp_stats_access();
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmGetProgname());
    fputs("Options:\n"
"  -D debug     set debug options, see pmdbg(1)\n"
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
    int			c, sep = pmPathSeparator();
    int			errflag = 0;
    char		helppath[MAXPATHLEN];

    _isDSO = 0;
    pmSetProgname(argv[0]);
    pmGetUsername(&username);

    pmsprintf(helppath, MAXPATHLEN, "%s%c" "darwin" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_3, pmGetProgname(), DARWIN, "darwin.log",
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
