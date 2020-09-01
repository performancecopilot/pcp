typedef struct zfs_dmu_tx {
    uint64_t assigned;
    uint64_t delay;
    uint64_t error;
    uint64_t suspended;
    uint64_t group;
    uint64_t memory_reserve;
    uint64_t memory_reclaim;
    uint64_t dirty_throttle;
    uint64_t dirty_delay;
    uint64_t dirty_over_max;
    uint64_t dirty_frees_delay;
    uint64_t quota;
} zfs_dmu_tx_t;

void zfs_dmu_tx_refresh(zfs_dmu_tx_t *dmu_tx, regex_t *rgx_row);
