typedef struct zfs_zfetchstats {
    unsigned int hits;
    unsigned int misses;
    unsigned int max_streams;
} zfs_zfetchstats_t;

void zfs_zfetchstats_fetch(zfs_zfetchstats_t *zfetchstats, regex_t *rgx_row);
