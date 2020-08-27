typedef struct zfs_xuiostats {
    unsigned int onloan_read_buf;
    unsigned int onloan_write_buf;
    unsigned int read_buf_copied;
    unsigned int read_buf_nocopy;
    unsigned int write_buf_copied;
    unsigned int write_buf_nocopy;
} zfs_xuiostats_t;

void zfs_xuiostats_fetch(zfs_xuiostats_t *xuiostats, regex_t *rgx_row);
