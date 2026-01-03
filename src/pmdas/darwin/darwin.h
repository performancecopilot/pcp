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
    NUM_INDOMS			/* total number of instance domains */
};

