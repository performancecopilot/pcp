typedef struct zfs_vdev_cachestats {
    uint64_t delegations;
    uint64_t hits;
    uint64_t misses;
} zfs_vdev_cachestats_t;

void zfs_vdev_cachestats_refresh(zfs_vdev_cachestats_t *vdev_cachestats);
