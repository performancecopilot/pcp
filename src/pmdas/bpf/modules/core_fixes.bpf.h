/* SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause) */
/* Copyright (c) 2021 Hengqi Chen */
/* Copyright (c) 2021 Red Hat */
/* source: https://github.com/iovisor/bcc/blob/adf3a7970ce8ff66bf97d4841d2c178bfce541be/libbpf-tools/core_fixes.bpf.h */

#ifndef CORE_FIXES_BPF_H
#define CORE_FIXES_BPF_H

#include <bpf/bpf_core_read.h>

/**
 * commit 2f064a59a1 ("sched: Change task_struct::state") changes
 * the name of task_struct::state to task_struct::__state
 * see:
 *     https://github.com/torvalds/linux/commit/2f064a59a1
 */
struct task_struct__5_14 {
	unsigned int __state;
};
struct task_struct__prev {
	unsigned int state;
};

static __s64 get_task_state(void *task)
{
	if (bpf_core_field_exists(((struct task_struct__5_14 *)task)->__state))
		return ((struct task_struct__5_14 *)task)->__state;
	return ((struct task_struct__prev *)task)->state;
}

#endif /* CORE_FIXES_BPF_H */
