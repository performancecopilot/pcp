typedef struct zfs_vdev_mirrorstats {
    unsigned int rotating_linear;
    unsigned int rotating_offset;
    unsigned int rotating_seek;
    unsigned int non_rotating_linear;
    unsigned int non_rotating_seek;
    unsigned int preferred_found;
    unsigned int preferred_not_found;
} zfs_vdev_mirrorstats_t;

void zfs_vdev_mirrorstats_fetch(zfs_vdev_mirrorstats_t *vdev_mirrorstats, regex_t *rgx_row);
