#include <pcp/pmapi.h>
#include <pcp/pmjson.h>
#include <stdio.h>

json_metric_desc json_metrics[] = {
    { "WBThrottle/bytes_wb", 0, 1, {0}, ""},
    { "WBThrottle/ios_dirtied", 0, 1, {0}, ""},
    { "WBThrottle/ios_wb", 0, 1, {0}, ""},
    { "WBThrottle/inodes_dirtied", 0, 1, {0}, ""},
    { "WBThrottle/inodes_wb", 0, 1, {0}, ""},
    { "filestore/journal_queue_max_ops", 0, 1, {0}, ""},
    { "filestore/journal_queue_ops", 0, 1, {0}, ""},
    { "filestore/journal_queue_max_bytes", 0, 1, {0}, ""},
    { "filestore/journal_bytes", 0, 1, {0}, ""},
    { "filestore/journal_latency/avgcount", 0, 1, {0}, ""},
    { "filestore/journal_latency/sum", 0, 1, {0}, ""},
    { "filestore/journal_wr", 0, 1, {0}, ""},
    { "filestore/journal_wr_bytes/avgcount", 0, 1, {0}, ""},
    { "filestore/journal_wr_bytes/sum", 0, 1, {0}, ""},
    { "filestore/journal_full", 0, 1, {0}, ""},
    { "filestore/committing", 0, 1, {0}, ""},
    { "filestore/commitcycle", 0, 1, {0}, ""},
    { "filestore/commitcycle_interval/avgcount", 0, 1, {0}, ""},
    { "filestore/commitcycle_interval/sum", 0, 1, {0}, ""},
    { "filestore/commitcycle_latency/avgcount", 0, 1, {0}, ""},
    { "filestore/commitcycle_latency/sum", 0, 1, {0}, ""},
    { "filestore/op_queue_max_ops", 0, 1, {0}, ""},
    { "filestore/op_queue_ops", 0, 1, {0}, ""},
    { "filestore/ops", 0, 1, {0}, ""},
    { "filestore/op_queue_max_bytes", 0, 1, {0}, ""},
    { "filestore/op_queue_bytes", 0, 1, {0}, ""},
    { "filestore/bytes", 0, 1, {0}, ""},
    { "filestore/apply_latency/avgcount", 0, 1, {0}, ""},
    { "filestore/apply_latency/sum", 0, 1, {0}, ""},
    { "filestore/transaction_latency/avgcount", 0, 1, {0}, ""},
    { "filestore/transaction_latency/sum", 0, 1, {0}, ""},
    { "leveldb/leveldb_get", 0, 1, {0}, ""},
    { "leveldb/leveldb_transaction", 0, 1, {0}, ""},
    { "leveldb/leveldb_compact", 0, 1, {0}, ""},
    { "leveldb/leveldb_compact_range", 0, 1, {0}, ""},
    { "leveldb/leveldb_compact_queue_merge", 0, 1, {0}, ""},
    { "leveldb/leveldb_compact_queue_len", 0, 1, {0}, ""},
    { "mutex-FileJournal::completions_lock/wait/avgcount", 0, 1, {0}, ""},
    { "mutex-FileJournal::completions_lock/wait/sum", 0, 1, {0}, ""},
    { "mutex-FileJournal::finisher_lock/wait/avgcount", 0, 1, {0}, ""},
    { "mutex-FileJournal::finisher_lock/wait/sum", 0, 1, {0}, ""},
    { "mutex-FileJournal::write_lock/wait/avgcount", 0, 1, {0}, ""},
    { "mutex-FileJournal::write_lock/wait/sum", 0, 1, {0}, ""},
    { "mutex-FileJournal::writeq_lock/wait/avgcount", 0, 1, {0}, ""},
    { "mutex-FileJournal::writeq_lock/wait/sum", 0, 1, {0}, ""},
    { "mutex-JOS::ApplyManager::apply_lock/wait/avgcount", 0, 1, {0}, ""},
    { "mutex-JOS::ApplyManager::apply_lock/wait/sum", 0, 1, {0}, ""},
    { "mutex-JOS::ApplyManager::com_lock/wait/avgcount", 0, 1, {0}, ""},
    { "mutex-JOS::ApplyManager::com_lock/wait/sum", 0, 1, {0}, ""},
    { "mutex-JOS::ApplyManager::com_lock/wait/avgcount", 0, 1, {0}, ""},
    { "mutex-JOS::ApplyManager::com_lock/wait/sum", 0, 1, {0}, ""},
    { "mutex-JOS::SubmitManager::lock/wait/avgcount", 0, 1, {0}, ""},
    { "mutex-JOS::SubmitManager::lock/wait/sum", 0, 1, {0}, ""},
    { "mutex-WBThrottle::lock/wait/avgcount", 0, 1, {0}, ""},
    { "mutex-WBThrottle::lock/wait/sum", 0, 1, {0}, ""},
    { "objecter/op_active", 0, 1, {0}, ""},
    { "objecter/op_laggy", 0, 1, {0}, ""},
    { "objecter/op_send", 0, 1, {0}, ""},
    { "objecter/op_send_bytes", 0, 1, {0}, ""},
    { "objecter/op_resend", 0, 1, {0}, ""},
    { "objecter/op_ack", 0, 1, {0}, ""},
    { "objecter/op_commit", 0, 1, {0}, ""},
    { "objecter/op", 0, 1, {0}, ""},
    { "objecter/op_r", 0, 1, {0}, ""},
    { "objecter/op_w", 0, 1, {0}, ""},
    { "objecter/op_rmw", 0, 1, {0}, ""},
    { "objecter/op_pg", 0, 1, {0}, ""},
    { "objecter/osdop_stat", 0, 1, {0}, ""},
    { "objecter/osdop_create", 0, 1, {0}, ""},
    { "objecter/osdop_read", 0, 1, {0}, ""},
    { "objecter/osdop_write", 0, 1, {0}, ""},
    { "objecter/osdop_writefull", 0, 1, {0}, ""},
    { "objecter/osdop_append", 0, 1, {0}, ""},
    { "objecter/osdop_zero", 0, 1, {0}, ""},
    { "objecter/osdop_truncate", 0, 1, {0}, ""},
    { "objecter/osdop_delete", 0, 1, {0}, ""},
    { "objecter/osdop_mapext", 0, 1, {0}, ""},
    { "objecter/osdop_sparse_read", 0, 1, {0}, ""},
    { "objecter/osdop_clonerange", 0, 1, {0}, ""},
    { "objecter/osdop_getxattr", 0, 1, {0}, ""},
    { "objecter/osdop_setxattr", 0, 1, {0}, ""},
    { "objecter/osdop_cmpxattr", 0, 1, {0}, ""},
    { "objecter/osdop_rmxattr", 0, 1, {0}, ""},
    { "objecter/osdop_resetxattrs", 0, 1, {0}, ""},
    { "objecter/osdop_tmap_up", 0, 1, {0}, ""},
    { "objecter/osdop_tmap_put", 0, 1, {0}, ""},
    { "objecter/osdop_tmap_get", 0, 1, {0}, ""},
    { "objecter/osdop_call", 0, 1, {0}, ""},
    { "objecter/osdop_watch", 0, 1, {0}, ""},
    { "objecter/osdop_notify", 0, 1, {0}, ""},
    { "objecter/osdop_src_cmpxattr", 0, 1, {0}, ""},
    { "objecter/osdop_pgls", 0, 1, {0}, ""},
    { "objecter/osdop_pgls_filter", 0, 1, {0}, ""},
    { "objecter/osdop_other", 0, 1, {0}, ""},
    { "objecter/linger_active", 0, 1, {0}, ""},
    { "objecter/linger_send", 0, 1, {0}, ""},
    { "objecter/linger_resend", 0, 1, {0}, ""},
    { "objecter/poolop_active", 0, 1, {0}, ""},
    { "objecter/poolop_send", 0, 1, {0}, ""},
    { "objecter/poolop_resend", 0, 1, {0}, ""},
    { "objecter/poolstat_active", 0, 1, {0}, ""},
    { "objecter/poolstat_send", 0, 1, {0}, ""},
    { "objecter/poolstat_resend", 0, 1, {0}, ""},
    { "objecter/statfs_active", 0, 1, {0}, ""},
    { "objecter/statfs_send", 0, 1, {0}, ""},
    { "objecter/statfs_resend", 0, 1, {0}, ""},
    { "objecter/command_active", 0, 1, {0}, ""},
    { "objecter/command_send", 0, 1, {0}, ""},
    { "objecter/command_resend", 0, 1, {0}, ""},
    { "objecter/map_epoch", 0, 1, {0}, ""},
    { "objecter/map_full", 0, 1, {0}, ""},
    { "objecter/map_inc", 0, 1, {0}, ""},
    { "objecter/osd_sessions", 0, 1, {0}, ""},
    { "objecter/osd_session_open", 0, 1, {0}, ""},
    { "objecter/osd_session_close", 0, 1, {0}, ""},
    { "objecter/osd_laggy", 0, 1, {0}, ""},
    { "osd/opq", 0, 1, {0}, ""},
    { "osd/op_wip", 0, 1, {0}, ""},
    { "osd/op", 0, 1, {0}, ""},
    { "osd/op_in_bytes", 0, 1, {0}, ""},
    { "osd/op_out_bytes", 0, 1, {0}, ""},
    { "osd/op_latency/avgcount", 0, 1, {0}, ""},
    { "osd/op_latency/sum", 0, 1, {0}, ""},
    { "osd/op_process_latency/avgcount", 0, 1, {0}, ""},
    { "osd/op_process_latency/sum", 0, 1, {0}, ""},
    { "osd/op_r", 0, 1, {0}, ""},
    { "osd/op_r_out_bytes", 0, 1, {0}, ""},
    { "osd/op_r_latency/avgcount", 0, 1, {0}, ""},
    { "osd/op_r_latency/sum", 0, 1, {0}, ""},
    { "osd/op_r_process_latency/avgcount", 0, 1, {0}, ""},
    { "osd/op_r_process_latencysum", 0, 1, {0}, ""},
    { "osd/op_w", 0, 1, {0}, ""},
    { "osd/op_w_in_bytes", 0, 1, {0}, ""},
    { "osd/op_w_rlat/avgcount", 0, 1, {0}, ""},
    { "osd/op_w_rlat/sum", 0, 1, {0}, ""},
    { "osd/op_w_latency/avgcount", 0, 1, {0}, ""},
    { "osd/op_w_latency/sum", 0, 1, {0}, ""},
    { "osd/op_w_process_latency/avgcount", 0, 1, {0}, ""},
    { "osd/op_w_process_latency/sum", 0, 1, {0}, ""},
    { "osd/op_rw", 0, 1, {0}, ""},
    { "osd/op_rw_in_bytes", 0, 1, {0}, ""},
    { "osd/op_rw_out_bytes", 0, 1, {0}, ""},
    { "osd/op_rw_rlat/avgcount", 0, 1, {0}, ""},
    { "osd/op_rw_rlat/sum", 0, 1, {0}, ""},
    { "osd/op_rw_latency/avgcount", 0, 1, {0}, ""},
    { "osd/op_rw_latency/sum", 0, 1, {0}, ""},
    { "osd/op_rw_process_latency/avgcount", 0, 1, {0}, ""},
    { "osd/op_rw_process_latency/sum", 0, 1, {0}, ""},
    { "osd/subop", 0, 1, {0}, ""},
    { "osd/subop_in_bytes", 0, 1, {0}, ""},
    { "osd/subop_latency/avgcount", 0, 1, {0}, ""},
    { "osd/subop_latency/sum", 0, 1, {0}, ""},
    { "osd/subop_w", 0, 1, {0}, ""},
    { "osd/subop_w_in_bytes", 0, 1, {0}, ""},
    { "osd/subop_w_latency/avgcount", 0, 1, {0}, ""},
    { "osd/subop_w_latency/sum", 0, 1, {0}, ""},
    { "osd/subop_pull", 0, 1, {0}, ""},
    { "osd/subop_pull_latency/avgcount", 0, 1, {0}, ""},
    { "osd/subop_pull_latency/sum", 0, 1, {0}, ""},
    { "osd/subop_push", 0, 1, {0}, ""},
    { "osd/subop_push_in_bytes", 0, 1, {0}, ""},
    { "osd/subop_push_latency/avgcount", 0, 1, {0}, ""},
    { "osd/subop_push_latency/sum", 0, 1, {0}, ""},
    { "osd/pull", 0, 1, {0}, ""},
    { "osd/push", 0, 1, {0}, ""},
    { "osd/push_out_bytes", 0, 1, {0}, ""},
    { "osd/push_in", 0, 1, {0}, ""},
    { "osd/push_in_bytes", 0, 1, {0}, ""},
    { "osd/recovery_ops", 0, 1, {0}, ""},
    { "osd/loadavg", 0, 1, {0}, ""},
    { "osd/buffer_bytes", 0, 1, {0}, ""},
    { "osd/numpg", 0, 1, {0}, ""},
    { "osd/numpg_primary", 0, 1, {0}, ""},
    { "osd/numpg_replica", 0, 1, {0}, ""},
    { "osd/numpg_stray", 0, 1, {0}, ""},
    { "osd/heartbeat_to_peers", 0, 1, {0}, ""},
    { "osd/heartbeat_from_peers", 0, 1, {0}, ""},
    { "osd/map_messages", 0, 1, {0}, ""},
    { "osd/map_message_epochs", 0, 1, {0}, ""},
    { "osd/map_message_epoch_dups", 0, 1, {0}, ""},
    { "osd/messages_delayed_for_map", 0, 1, {0}, ""},
    { "osd/stat_bytes", pmjson_flag_u64, 1, {0}, ""},
    { "osd/stat_bytes_used", pmjson_flag_u64, 1, {0}, ""},
    { "osd/stat_bytes_avail", pmjson_flag_u64, 1, {0}, ""},
    { "osd/copyfrom", 0, 1, {0}, ""},
    { "osd/tier_promote", 0, 1, {0}, ""},
    { "osd/tier_flush", 0, 1, {0}, ""},
    { "osd/tier_flush_fail", 0, 1, {0}, ""},
    { "osd/tier_try_flush", 0, 1, {0}, ""},
    { "osd/tier_try_flush_fail", 0, 1, {0}, ""},
    { "osd/tier_evict", 0, 1, {0}, ""},
    { "osd/tier_whiteout", 0, 1, {0}, ""},
    { "osd/tier_dirty", 0, 1, {0}, ""},
    { "osd/tier_clean", 0, 1, {0}, ""},
    { "osd/tier_delay", 0, 1, {0}, ""},
    { "osd/agent_wake", 0, 1, {0}, ""},
    { "osd/agent_skip", 0, 1, {0}, ""},
    { "osd/agent_flush", 0, 1, {0}, ""},
    { "osd/agent_evict", 0, 1, {0}, ""},
};

#define JSON_SZ (sizeof(json_metrics)/sizeof(json_metrics[0]))

int main(int argc, char** argv){
    FILE *fp;
    int fd;
    unsigned int i;
    int c;
    int sts;
    int errflag = 0;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (argc-1 > optind)
	errflag++;

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options] inputfile\n\
\n\
Options:\n\
  -D debugspec    set debug options\n",
                pmGetProgname());
        exit(1);
    }

    if ((fp = fopen(argv[optind], "r")) == NULL) {
	return 1;
    }

    fd = fileno(fp);
    pmjsonInit(fd, json_metrics, JSON_SZ);
    for(i = 0; i < JSON_SZ; i++) {
	switch (json_metrics[i].flags) {
	case pmjson_flag_boolean:
	    break;
	case pmjson_flag_s32:
	    printf("%s: %d\n", json_metrics[i].json_pointer, json_metrics[i].values.l);
	    break;
	case pmjson_flag_u32:
	    printf("%s: %u\n", json_metrics[i].json_pointer, json_metrics[i].values.ul);
	    break;
	case pmjson_flag_s64:
	    printf("%s: %" FMT_INT64 "\n", json_metrics[i].json_pointer, json_metrics[i].values.ll);
	    break;
	case pmjson_flag_u64:
	    printf("%s: %" FMT_UINT64 "\n", json_metrics[i].json_pointer, json_metrics[i].values.ull);
	    break;
	case pmjson_flag_float:
	    printf("%s: %f\n", json_metrics[i].json_pointer, json_metrics[i].values.f);
	    break;
	case pmjson_flag_double:
	    printf("%s: %f\n", json_metrics[i].json_pointer, json_metrics[i].values.d);
	    break;
	default:
	    printf("%s: %d\n", json_metrics[i].json_pointer, json_metrics[i].values.ul);
	    break;
	}
    }
    fclose(fp);
    return 0;
}
