#define MODEL_SIZE 32
extern char	hw_model[MODEL_SIZE];

struct xsw_usage;  /* Forward declaration for swap usage structure */

/*
 * Metric Instance Domain indices
 */
enum {
    LOADAVG_INDOM,		/* 0 - 1, 5, 15 minute run queue averages */
    FILESYS_INDOM,		/* 1 - set of all mounted filesystems */
    DISK_INDOM,			/* 2 - set of all disk devices */
    CPU_INDOM,			/* 3 - set of all processors */
    NETWORK_INDOM,		/* 4 - set of all network interfaces */
    NFS3_INDOM,			/* 5 - nfs v3 operations */
    GPU_INDOM,			/* 6 - set of all GPUs */
    APFS_CONTAINER_INDOM,	/* 7 - set of all APFS containers */
    APFS_VOLUME_INDOM,		/* 8 - set of all APFS volumes */
    NUM_INDOMS			/* total number of instance domains */
};

/*
 * Fetch clusters (metric groups)
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
    CLUSTER_LIMITS,		/* 18 = system resource limits */
    CLUSTER_GPU,		/* 19 = gpu statistics */
    CLUSTER_IPC,		/* 20 = ipc statistics */
    CLUSTER_POWER,		/* 21 = power/battery statistics */
    CLUSTER_IPV6,		/* 22 = ipv6 protocol statistics */
    CLUSTER_APFS,		/* 23 = apfs statistics */
    NUM_CLUSTERS		/* total number of clusters */
};

/*
 * NFS Instance Domain Constants
 */
#define NFS3_RPC_COUNT	25	/* number of NFS v3 RPC operations */

