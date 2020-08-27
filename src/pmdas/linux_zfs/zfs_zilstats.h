typedef struct zfs_zilstats {
    unsigned int commit_count;
    unsigned int commit_writer_count;
    unsigned int itx_count;
    unsigned int itx_indirect_count;
    unsigned int itx_indirect_bytes;
    unsigned int itx_copied_count;
    unsigned int itx_copied_bytes;
    unsigned int itx_needcopy_count;
    unsigned int itx_needcopy_bytes;
    unsigned int itx_metaslab_normal_count;
    unsigned int itx_metaslab_normal_bytes;
    unsigned int itx_metaslab_slog_count;
    unsigned int itx_metaslab_slog_bytes;
} zfs_zilstats_t;

void zfs_zilstats_fetch(zfs_zilstats_t *zilstats, regex_t *rgx_row);
