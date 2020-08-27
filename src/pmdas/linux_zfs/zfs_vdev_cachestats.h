typedef struct zfs_vdev_cachestats {
    unsigned int delegations;
    unsigned int hits;
    unsigned int misses;
} zfs_vdev_cachestats_t;

void zfs_vdev_cachestats_fetch(zfs_vdev_cachestats_t *vdev_cachestats, regex_t *rgx_row);
