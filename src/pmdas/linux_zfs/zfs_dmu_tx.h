typedef struct zfs_dmu_tx {
    unsigned int assigned;
    unsigned int delay;
    unsigned int error;
    unsigned int suspended;
    unsigned int group;
    unsigned int memory_reserve;
    unsigned int memory_reclaim;
    unsigned int dirty_throttle;
    unsigned int dirty_delay;
    unsigned int dirty_over_max;
    unsigned int dirty_frees_delay;
    unsigned int quota;
} zfs_dmu_tx_t;

void zfs_dmu_tx_refresh(zfs_dmu_tx_t *dmu_tx, regex_t *rgx_row);
