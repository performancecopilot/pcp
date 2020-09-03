int POOLS_INDOM = 0;
char *ZFS_PROC_DIR = "/proc/spl/kstat/zfs/";

struct zfs_poolstats {
        char state[32];
	uint64_t nread;
 	uint64_t nwritten;
 	uint64_t reads;
 	uint64_t writes;
 	uint64_t wtime;
 	uint64_t wlentime;
 	uint64_t wupdate;
 	uint64_t rtime;
 	uint64_t rlentime;
 	uint64_t rupdate;
 	uint64_t wcnt;
 	uint64_t rcnt;
} zfs_poolstats_t;

void zfs_pools_init();
void zfs_poolstats_refresh();
