typedef struct zfs_fmstats {
    uint64_t erpt_dropped;
    uint64_t erpt_set_failed;
    uint64_t fmri_set_failed;
    uint64_t payload_set_failed;
} zfs_fmstats_t;

void zfs_fmstats_refresh(zfs_fmstats_t *fmstats);
