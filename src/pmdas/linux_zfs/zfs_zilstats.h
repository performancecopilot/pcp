typedef struct zfs_zilstats {
    uint64_t commit_count;
    uint64_t commit_writer_count;
    uint64_t itx_count;
    uint64_t itx_indirect_count;
    uint64_t itx_indirect_bytes;
    uint64_t itx_copied_count;
    uint64_t itx_copied_bytes;
    uint64_t itx_needcopy_count;
    uint64_t itx_needcopy_bytes;
    uint64_t itx_metaslab_normal_count;
    uint64_t itx_metaslab_normal_bytes;
    uint64_t itx_metaslab_slog_count;
    uint64_t itx_metaslab_slog_bytes;
} zfs_zilstats_t;

void zfs_zilstats_refresh(zfs_zilstats_t *zilstats);
