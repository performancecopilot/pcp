typedef struct zfs_vdev_mirrorstats {
    uint64_t rotating_linear;
    uint64_t rotating_offset;
    uint64_t rotating_seek;
    uint64_t non_rotating_linear;
    uint64_t non_rotating_seek;
    uint64_t preferred_found;
    uint64_t preferred_not_found;
} zfs_vdev_mirrorstats_t;

void zfs_vdev_mirrorstats_refresh(zfs_vdev_mirrorstats_t *vdev_mirrorstats, regex_t *rgx_row);
