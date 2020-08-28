typedef struct zfs_fmstats {
    unsigned int erpt_dropped;
    unsigned int erpt_set_failed;
    unsigned int fmri_set_failed;
    unsigned int payload_set_failed;
} zfs_fmstats_t;

void zfs_fmstats_refresh(zfs_fmstats_t *fmstats, regex_t *rgx_row);
