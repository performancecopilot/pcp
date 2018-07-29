/*
 * Copyright (c) 2016-2018 Red Hat.
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
#ifndef LINUX_PMDA_H
#define LINUX_PMDA_H

#include "pmapi.h"
#include "pmda.h"

/*
 * fetch cluster numbers
 */
enum {
	CLUSTER_STAT = 0,	/*  0 /proc/stat (plus others, historical) */
	CLUSTER_MEMINFO,	/*  1 /proc/meminfo */
	CLUSTER_LOADAVG,	/*  2 /proc/loadavg */
	CLUSTER_NET_DEV,	/*  3 /proc/net/dev */
	CLUSTER_INTERRUPTS,	/*  4 /proc/interrupts */
	CLUSTER_FILESYS,	/*  5 /proc/mounts + statfs */
	CLUSTER_SWAPDEV,	/*  6 /proc/swaps */
	CLUSTER_NET_NFS,	/*  7 /proc/net/rpc/nfs + /proc/net/rpc/nfsd + /proc/fs/nfsd */
	PROC_PID_STAT,		/*  8 /proc/<pid>/stat -> proc PMDA */
	PROC_PID_STATM,		/*  9 /proc/<pid>/statm + /proc/<pid>/maps -> proc PMDA */
	CLUSTER_PARTITIONS,	/* 10 /proc/partitions */
	CLUSTER_NET_SOCKSTAT,	/* 11 /proc/net/sockstat */
	CLUSTER_KERNEL_UNAME,	/* 12 uname() system call */
	PROC_PROC_RUNQ,		/* 13 number of processes in various states -> proc PMDA */
	CLUSTER_NET_SNMP,	/* 14 /proc/net/snmp */
	CLUSTER_SCSI,		/* 15 /proc/scsi/scsi */
	CLUSTER_XFS,		/* 16 /proc/fs/xfs/stat -> xfs PMDA */
	CLUSTER_XFSBUF,		/* 17 /proc/fs/pagebuf/stat -> xfs PMDA */
	CLUSTER_CPUINFO,	/* 18 /proc/cpuinfo */
	CLUSTER_NET_TCP,	/* 19 /proc/net/tcp */
	CLUSTER_SLAB,		/* 20 /proc/slabinfo */
	CLUSTER_SEM_LIMITS,	/* 21 semctl(IPC_INFO) system call */
	CLUSTER_MSG_LIMITS,	/* 22 msgctl(IPC_INFO) system call */
	CLUSTER_SHM_LIMITS,	/* 23 shmctl(IPC_INFO) system call */
	PROC_PID_STATUS,	/* 24 /proc/<pid>/status -> proc PMDA */
	CLUSTER_UTMP,		/* 25 login records metrics */
	CLUSTER_UPTIME,		/* 26 /proc/uptime */
	CLUSTER_VFS,		/* 27 /proc/sys/fs */
	CLUSTER_VMSTAT,		/* 28 /proc/vmstat */
	CLUSTER_IB,		/* deprecated: do not re-use 29 infiniband */
	CLUSTER_QUOTA,		/* 30 quotactl() -> xfs PMDA */
	PROC_PID_SCHEDSTAT,	/* 31 /proc/<pid>/schedstat -> proc PMDA */
	PROC_PID_IO,		/* 32 /proc/<pid>/io -> proc PMDA */
	CLUSTER_NET_ADDR,	/* 33 /proc/net/dev and ioctl(SIOCGIFCONF) */
	CLUSTER_TMPFS,		/* 34 /proc/mounts + statfs (tmpfs only) */
	CLUSTER_SYSFS_KERNEL,	/* 35 /sys/kernel metrics */
	CLUSTER_NUMA_MEMINFO,	/* 36 /sys/devices/system/node* NUMA memory */
	PROC_CGROUP_SUBSYS,	/* 37 /proc/cgroups control group subsystems -> proc PMDA */
	PROC_CGROUP_MOUNTS,	/* 38 /proc/mounts active control groups -> proc PMDA */
	PROC_CPUSET_GROUPS,	/* 39 cpuset control groups -> proc PMDA */
	PROC_CPUSET_PROCS,	/* 40 cpuset control group processes -> proc PMDA */
	PROC_CPUACCT_GROUPS,	/* 41 cpu accounting control groups -> proc PMDA */
	PROC_CPUACCT_PROCS,	/* 42 cpu accounting group processes -> proc PMDA */
	PROC_CPUSCHED_GROUPS,	/* 43 scheduler control groups -> proc PMDA */
	PROC_CPUSCHED_PROCS,	/* 44 scheduler group processes -> proc PMDA */
	PROC_MEMORY_GROUPS,	/* 45 memory control groups -> proc PMDA */
	PROC_MEMORY_PROCS,	/* 46 memory group processes -> proc PMDA */
	PROC_NET_CLS_GROUPS,	/* 47 network class fication control groups -> proc PMDA */
	PROC_NET_CLS_PROCS,	/* 48 network classification group processes -> proc PMDA */
	CLUSTER_INTERRUPT_LINES,/* 49 /proc/interrupts percpu interrupts */
	CLUSTER_INTERRUPT_OTHER,/* 50 /proc/interrupts percpu interrupts */
	PROC_PID_FD,		/* 51 /proc/<pid>/fd -> proc PMDA */
	CLUSTER_LV,		/* deprecated: do not re-use 52 */
	CLUSTER_NET_NETSTAT,    /* 53 /proc/net/netstat */
	CLUSTER_DM,		/* 54 disk.dm.* */
	CLUSTER_SYSFS_DEVICES,	/* 55 /sys/devices metrics */
	CLUSTER_SHM_INFO,       /* 56 shmctl(SHM_INFO) system call */
	CLUSTER_NET_SOFTNET,	/* 57 /proc/net/softnet_stat */
	CLUSTER_NET_SNMP6,	/* 58 /proc/net/snmp6 */
	CLUSTER_MD,		/* 59 disk.md.* (not status) */
	CLUSTER_MDADM,		/* 60 disk.md.status */
	CLUSTER_SEM_INFO,	/* 61 shmctl(SEM_INFO) system call */
	CLUSTER_MSG_INFO,	/* 62 msgctl(MSG_INFO) system call */
	CLUSTER_SOFTIRQS,	/* 63 /proc/softirqs percpu counters */
	CLUSTER_SHM_STAT,	/* 64 shmctl(SHM_STAT) system call */
	CLUSTER_MSG_STAT,	/* 65 msgctl(MSG_STAT) system call */
	CLUSTER_SEM_STAT,	/* 66 msgctl(SEM_STAT) system call */
	CLUSTER_BUDDYINFO,	/* 67 /proc/buddyinfo */
	CLUSTER_ZONEINFO,	/* 68 /proc/zoneinfo */
	CLUSTER_KSM_INFO,	/* 69 /sys/kernel/mm/ksm */
	CLUSTER_ZONEINFO_PROTECTION,	/* 70 /proc/zoneinfo protection item */
	CLUSTER_TAPEDEV,	/* 71 /sys/class/scsi_tape */
	CLUSTER_SYS_KERNEL,	/* 72 /proc/sys/kernel metrics */
	CLUSTER_NET_SOCKSTAT6,	/* 73 /proc/net/sockstat6 */
	CLUSTER_TTY,            /* 74 proc/tty/device/serial metrics */
	CLUSTER_LOCKS,		/* 75 /proc/locks */
	CLUSTER_NET_TCP6,	/* 76 /proc/net/tcp6 */
	CLUSTER_NET_RAW,	/* 77 /proc/net/raw */
	CLUSTER_NET_RAW6,	/* 78 /proc/net/raw6 */
	CLUSTER_NET_UDP,	/* 79 /proc/net/udp */
	CLUSTER_NET_UDP6,	/* 80 /proc/net/udp6 */
	CLUSTER_NET_UNIX,	/* 81 /proc/net/unix */
	CLUSTER_SOFTIRQS_TOTAL,	/* 82 /proc/softirqs */

	NUM_CLUSTERS		/* one more than highest numbered cluster */
};

/*
 * Extra refresh array indices for fine-grained (within-cluster) refresh
 */
enum {
	REFRESH_NET_MTU = NUM_CLUSTERS,
	REFRESH_NET_TYPE,
	REFRESH_NET_SPEED,
	REFRESH_NET_DUPLEX,
	REFRESH_NET_LINKUP,
	REFRESH_NET_RUNNING,
	REFRESH_NET_WIRELESS,

	REFRESH_NETADDR_INET,
	REFRESH_NETADDR_IPV6,
	REFRESH_NETADDR_HW,

	REFRESH_PROC_DISKSTATS,
	REFRESH_PROC_PARTITIONS,

	NUM_REFRESHES		/* one more than highest refresh index */
};

/*
 * instance domain numbers
 */
enum {
	CPU_INDOM = 0,		/* 0 - percpu */
	DISK_INDOM,		/* 1 - disks */
	LOADAVG_INDOM,		/* 2 - 1, 5, 15 minute load averages */
	NET_DEV_INDOM,		/* 3 - network interfaces */
	INTERRUPTS_INDOM,	/* 4 - interrupt lines */
	FILESYS_INDOM,		/* 5 - mounted bdev filesystems */
	SWAPDEV_INDOM,		/* 6 - swap devices */
	NFS_INDOM,		/* 7 - nfs operations */
	NFS3_INDOM,		/* 8 - nfs v3 operations */
	PROC_PROC_INDOM,	/* 9 - processes */
	PARTITIONS_INDOM, 	/* 10 - disk partitions */
	SCSI_INDOM,		/* 11 - scsi devices */
	SLAB_INDOM,		/* 12 - kernel slabs */
	STRINGS_INDOM,		/* 13 - string dictionary */
	NFS4_CLI_INDOM,		/* 14 - nfs v4 client operations */
	NFS4_SVR_INDOM,		/* 15 - nfs v4 server operations */
	QUOTA_PRJ_INDOM,	/* 16 - project quota -> xfs PMDA */
	NET_ADDR_INDOM,		/* 17 - inet/ipv6 addresses */
	TMPFS_INDOM,		/* 18 - tmpfs mounts */
	NODE_INDOM,		/* 19 - NUMA nodes */
	PROC_CGROUP_SUBSYS_INDOM,	/* 20 - control group subsystems -> proc PMDA */
	PROC_CGROUP_MOUNTS_INDOM,	/* 21 - control group mounts -> proc PMDA */
	LV_INDOM_DEPRECATED,	/* deprecated 22 - lvm devices. do not re-use. Use DM_INDOM instead */
	ICMPMSG_INDOM,          /* 23 - icmp message types */
	DM_INDOM,		/* 24 - device mapper devices */
	MD_INDOM,		/* 25 - multi-device devices */
	INTERRUPT_NAMES_INDOM,	/* 26 - persistent percpu interrupts IDs */
	SOFTIRQS_NAMES_INDOM,	/* 27 - persistent percpu softirqs IDs */
	IPC_STAT_INDOM,	        /* 28 - ipc shm_stat shmid */
	IPC_MSG_INDOM,	        /* 29 - ipc msg_stat msgid */
	IPC_SEM_INDOM,	        /* 30 - ipc sem_stat msgid */
	BUDDYINFO_INDOM,	/* 31 - kernel buddys */
	ZONEINFO_INDOM,	        /* 32 - proc zoneinfo */
	ZONEINFO_PROTECTION_INDOM,	/* 33 - proc zoneinfo protection item */
	TAPEDEV_INDOM,		/* 34 - tape devices */
	TTY_INDOM,              /* 35 - serial tty devices */
	SOFTIRQS_INDOM,		/* 36 - softirqs */

	NUM_INDOMS		/* one more than highest numbered cluster */
};

extern pmInDom linux_indom(int);
#define INDOM(i) linux_indom(i)

extern pmdaIndom *linux_pmda_indom(int);
#define PMDAINDOM(i) linux_pmda_indom(i)

/*
 * Bitflag states representing various modes for testing.
 */
#define	LINUX_TEST_MODE		(1<<0)
#define	LINUX_TEST_STATSPATH	(1<<1)
#define	LINUX_TEST_MEMINFO	(1<<2)
#define	LINUX_TEST_NCPUS	(1<<3)
extern int linux_test_mode;

/*  
 * Optional path prefix for all stats files, used for testing.
 */
extern char *linux_statspath;
extern FILE *linux_statsfile(const char *, char *, int);
extern char *linux_mdadm;

/*
 * Atatic string dictionary - one copy of oft-repeated strings;
 * implemented using STRINGS_INDOM and pmdaCache(3) routines.
 */
char *linux_strings_lookup(int);
int linux_strings_insert(const char *);

/*
 * Some metrics are exported by the kernel as "unsigned long".
 * On most 64bit platforms this is not the same size as an
 * "unsigned int". 
 */
#if defined(HAVE_64BIT_LONG)
#define KERNEL_ULONG PM_TYPE_U64
#define _pm_assign_ulong(atomp, val) do { (atomp)->ull = (val); } while (0)
#define __pm_kernel_ulong_t __uint64_t
#else
#define KERNEL_ULONG PM_TYPE_U32
#define _pm_assign_ulong(atomp, val) do { (atomp)->ul = (val); } while (0)
#define __pm_kernel_ulong_t __uint32_t
#endif

/*
 * Some metrics need to have their type set at runtime, based on the
 * running kernel version (not simply a 64 vs 32 bit machine issue).
 */
#define KERNEL_UTYPE PM_TYPE_NOSUPPORT	/* set to real type at runtime */
#define _pm_metric_type(type, size) \
    do { \
	(type) = ((size)==8 ? PM_TYPE_U64 : PM_TYPE_U32); \
    } while (0)
#define _pm_assign_utype(size, atomp, val) \
    do { \
	if ((size)==8) { (atomp)->ull = (val); } else { (atomp)->ul = (val); } \
    } while (0)

extern int _pm_ncpus;			/* max CPU count, not online CPUs */
extern int _pm_pageshift;

typedef struct {
    unsigned long long	user;
    unsigned long long	sys;
    unsigned long long	nice;
    unsigned long long	idle;
    unsigned long long	wait;
    unsigned long long	irq;
    unsigned long long	sirq;
    unsigned long long	steal;
    unsigned long long	guest;
    unsigned long long	guest_nice;
} cpuacct_t;

typedef struct {
    float		clock;
    float		bogomips;
    int			sapic;		/* strings dictionary hash key */
    int			vendor;		/* strings dictionary hash key */
    int			model;		/* strings dictionary hash key */
    int			model_name;	/* strings dictionary hash key */
    int			stepping;	/* strings dictionary hash key */
    int			flags;		/* strings dictionary hash key */
    unsigned int	cache;
    unsigned int	cache_align;
} cpuinfo_t;

typedef struct {
    unsigned int	flags;
    uint64_t		processed;
    uint64_t		dropped;
    uint64_t		time_squeeze;
    uint64_t		cpu_collision;
    uint64_t		received_rps;
    uint64_t		flow_limit_count;
} softnet_t;

typedef struct {
    unsigned int	cpuid;
    unsigned int	nodeid;
    char		*name;
    cpuacct_t		stat;
    cpuinfo_t		info;
    softnet_t		*softnet;
} percpu_t;

typedef struct {
    unsigned int	nodeid;
    cpuacct_t		stat;
    struct linux_table	*meminfo;
    struct linux_table	*memstat;
    double		bandwidth;
} pernode_t;

#endif /* LINUX_PMDA_H */
