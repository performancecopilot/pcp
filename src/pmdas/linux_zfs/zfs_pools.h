#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#define ZFS_PROC_DIR "/proc/spl/kstat/zfs/"
#define ZFS_POOLS_INDOM 0

enum { /* metric item identifiers */
	ZFS_POOLS_STATE = 0,
	ZFS_POOLS_NREAD,
	ZFS_POOLS_NWRITTEN,
	ZFS_POOLS_READS,
	ZFS_POOLS_WRITES,
	ZFS_POOLS_WTIME,
	ZFS_POOLS_WLENTIME,
	ZFS_POOLS_WUPDATE,
	ZFS_POOLS_RTIME,
	ZFS_POOLS_RLENTIME,
	ZFS_POOLS_RUPDATE,
	ZFS_POOLS_WCNT,
	ZFS_POOLS_RCNT,
};

typedef struct zfs_poolstats {
        char *state;
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

void zfs_pools_init(zfs_poolstats_t *, pmdaInstid *, pmdaIndom *);
void zfs_pools_clear(zfs_poolstats_t *, pmdaInstid *, pmdaIndom *);
void zfs_poolstats_refresh(zfs_poolstats_t *, pmdaInstid *, pmdaIndom *);
