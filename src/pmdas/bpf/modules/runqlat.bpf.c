// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2020 Wenbo Zhang

// Extracted from iovisor https://github.com/iovisor/bcc/blob/master/libbpf-tools/
// most of the branching configurable elements removed for the initial implementation

#include "common.h"
#include "core_fixes.bpf.h"

#define MAX_ENTRIES	10240
#define TASK_RUNNING 	0
#define MAX_SLOTS 64

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MAX_ENTRIES);
	__type(key, u32);
	__type(value, u64);
} start SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MAX_ENTRIES);
	__type(key, u64);
	__type(value, u64);
} hist SEC(".maps");

static __always_inline
int trace_enqueue(u32 tgid, u32 pid)
{
	u64 ts;

	if (!pid)
		return 0;

	ts = bpf_ktime_get_ns();
	bpf_map_update_elem(&start, &pid, &ts, 0);
	return 0;
}

SEC("tp_btf/sched_wakeup")
int BPF_PROG(sched_wakeup, struct task_struct *p)
{
	return trace_enqueue(p->tgid, p->pid);
}

SEC("tp_btf/sched_wakeup_new")
int BPF_PROG(sched_wakeup_new, struct task_struct *p)
{
	return trace_enqueue(p->tgid, p->pid);
}

SEC("tp_btf/sched_switch")
int BPF_PROG(sched_swith, bool preempt, struct task_struct *prev,
	struct task_struct *next)
{
	u64 *tsp, slot;
	u32 pid;
	s64 delta;

	if (get_task_state(prev) == TASK_RUNNING)
		trace_enqueue(prev->tgid, prev->pid);

	pid = next->pid;

	tsp = bpf_map_lookup_elem(&start, &pid);
	if (!tsp)
		return 0;
	delta = bpf_ktime_get_ns() - *tsp;
	if (delta < 0)
		goto cleanup;

	slot = log2l(delta);
	if (slot >= MAX_SLOTS)
		slot = MAX_SLOTS - 1;
    add_or_create_entry(&hist, &slot, 1);

cleanup:
	bpf_map_delete_elem(&start, &pid);
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
