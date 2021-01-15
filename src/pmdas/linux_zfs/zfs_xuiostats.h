typedef struct zfs_xuiostats {
    uint64_t onloan_read_buf;
    uint64_t onloan_write_buf;
    uint64_t read_buf_copied;
    uint64_t read_buf_nocopy;
    uint64_t write_buf_copied;
    uint64_t write_buf_nocopy;
} zfs_xuiostats_t;

void zfs_xuiostats_refresh(zfs_xuiostats_t *xuiostats);
