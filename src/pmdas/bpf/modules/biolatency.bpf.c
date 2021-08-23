// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2020 Wenbo Zhang

// Extracted from iovisor https://github.com/iovisor/bcc/blob/master/libbpf-tools/
// most of the branching configurable elements removed for the initial implementation

#include "common.h"

#define MAX_ENTRIES	10240
#define MAX_SLOTS 27

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MAX_ENTRIES);
	__type(key, struct request *);
	__type(value, u64);
	__uint(map_flags, BPF_F_NO_PREALLOC);
} start SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MAX_ENTRIES);
	__type(key, u64);
	__type(value, u64);
	__uint(map_flags, BPF_F_NO_PREALLOC);
} hist SEC(".maps");

extern int LINUX_KERNEL_VERSION __kconfig;

static __always_inline
int trace_rq_start(struct request *rq, int issue)
{
	u64 ts = bpf_ktime_get_ns();

	bpf_map_update_elem(&start, &rq, &ts, 0);
	return 0;
}

SEC("tp_btf/block_rq_insert")
int block_rq_insert(u64 *ctx)
{
	/**
	 * commit a54895fa (v5.11-rc1) changed tracepoint argument list
	 * from TP_PROTO(struct request_queue *q, struct request *rq)
	 * to TP_PROTO(struct request *rq)
	 */
	if (LINUX_KERNEL_VERSION <= KERNEL_VERSION(5, 10, 0))
		return trace_rq_start((void *)ctx[1], false);
	else
		return trace_rq_start((void *)ctx[0], false);
}

SEC("tp_btf/block_rq_issue")
int block_rq_issue(u64 *ctx)
{
	/**
	 * commit a54895fa (v5.11-rc1) changed tracepoint argument list
	 * from TP_PROTO(struct request_queue *q, struct request *rq)
	 * to TP_PROTO(struct request *rq)
	 */
	if (LINUX_KERNEL_VERSION <= KERNEL_VERSION(5, 10, 0))
		return trace_rq_start((void *)ctx[1], true);
	else
		return trace_rq_start((void *)ctx[0], true);
}

SEC("tp_btf/block_rq_complete")
int BPF_PROG(block_rq_complete, struct request *rq, int error,
	unsigned int nr_bytes)
{
	u64 slot, *tsp, ts = bpf_ktime_get_ns();
	s64 delta;

	tsp = bpf_map_lookup_elem(&start, &rq);
	if (!tsp)
		return 0;
	delta = (s64)(ts - *tsp);
	if (delta < 0)
		goto cleanup;


    delta /= 1000U;
	slot = log2l(delta);
	if (slot >= MAX_SLOTS)
		slot = MAX_SLOTS - 1;
    add_or_create_entry(&hist, &slot, 1);

cleanup:
	bpf_map_delete_elem(&start, &rq);
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
