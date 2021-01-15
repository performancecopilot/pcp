typedef struct zfs_zfetchstats {
    uint64_t hits;
    uint64_t misses;
    uint64_t max_streams;
} zfs_zfetchstats_t;

void zfs_zfetchstats_refresh(zfs_zfetchstats_t *zfetchstats);
