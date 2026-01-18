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
#include "nfs.h"
#include "vfs.h"
#include "udp.h"
#include "icmp.h"
#include "sockstat.h"
#include "tcpconn.h"
#include "tcp.h"
#include "vmstat.h"
#include "filesys.h"
#include "cpu.h"
#include "loadavg.h"
#include "cpuload.h"
#include "uname.h"
#include "metrics.h"

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

int			mach_loadavg_error = 0;
float			mach_loadavg[3] = { 0,0,0 };

int			mach_cpuload_error = 0;
struct host_cpu_load_info	mach_cpuload = { { 0 } };

int			mach_vmstat_error = 0;
struct vm_statistics64	mach_vmstat = { 0 };
extern int refresh_vmstat(struct vm_statistics64 *);

int			mach_swap_error = 0;
struct xsw_usage	mach_swap = { 0 };
extern int refresh_swap(struct xsw_usage *);

int			mach_fs_error = 0;
struct statfs		*mach_fs = NULL;

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
