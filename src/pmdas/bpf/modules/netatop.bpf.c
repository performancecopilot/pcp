/*
 * SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
 * Copyright (c) 2021 Bytedance
 * https://github.com/bytedance/netatop-bpf
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>
#include <bpf/bpf_core_read.h>
#include <linux/version.h>
#include "netatop.h"

#define TASK_MAX_ENTRIES 40960

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_HASH); 
       __uint(max_entries, TASK_MAX_ENTRIES);
       __type(key, u64);
       __type(value, struct taskcount);
    //    __uint(key_size, u64);
    //    __uint(value_size, sizeof(struct taskcount));
} tgid_net_stat SEC(".maps");

// struct {
// 	__uint(type, BPF_MAP_TYPE_PERCPU_HASH);
// 	__uint(max_entries, TASK_MAX_ENTRIES);
// 	__type(key, u64);
// 	__type(value, struct taskcount);
// } tid_net_stat SEC(".maps");

static __always_inline u64 current_tgid()
{
	u64 tgid = bpf_get_current_pid_tgid() >> 32;
	// bpf_printk("bpf_get_current_tgid_tgid %d\n", tgid);
	return tgid;
}

// static __always_inline u64 current_tid()
// {
// 	u64 tid = bpf_get_current_pid_tgid() & 0x00000000ffffffff;
// 	// bpf_printk("bpf_get_current_tgid_tgid %d\n", tgid);
// 	return tid;
// }

struct sock_msg_length_args {
	unsigned long long common_tp_fields;
	struct sock *sk;
	__u16 family;
	__u16 protocol;
	int length;
	int error;
	int flags;
};

SEC("tracepoint/sock/sock_send_length")
int handle_tp_send(struct sock_msg_length_args *ctx)
{
	struct taskcount *stat_tgid;
	short family = ctx->family;
	short protocol = ctx->protocol;
	int length = ctx->length;
	int ret = 0;

	// AF_INET = 2
	// AF_INET6 = 10
	if (family == 2 || family ==  10) {
		u64 tgid = current_tgid();
		// u64 tid = current_tid();
		stat_tgid = bpf_map_lookup_elem(&tgid_net_stat, &tgid);
		// stat_tid = bpf_map_lookup_elem(&tid_net_stat, &tid);
		if (protocol == IPPROTO_TCP) {
			if (stat_tgid) {
				stat_tgid->tcpsndpacks++;
				stat_tgid->tcpsndbytes += length;
			} else {
				struct taskcount data ={
					.tcpsndpacks = 1,
					.tcpsndbytes = length
				};
				ret = bpf_map_update_elem(&tgid_net_stat, &tgid, &data, BPF_ANY);
			}
		} else if (protocol == IPPROTO_UDP) {
			if (stat_tgid) {
				stat_tgid->udpsndpacks++;
				stat_tgid->udpsndbytes += length;
			} else {
				struct taskcount data ={
					.udpsndpacks = 1,
					.udpsndbytes = length
				};
				ret = bpf_map_update_elem(&tgid_net_stat, &tgid, &data, BPF_ANY);
			}
		}
	}
	return ret;
}

SEC("tracepoint/sock/sock_recv_length")
int handle_tp_recv(struct sock_msg_length_args *ctx)
{
	struct taskcount *stat_tgid;
	short family = ctx->family;
	short protocol = ctx->protocol;
	int length = ctx->length;
	int ret = 0;

	if (family == 2 || family == 10) {
		u64 tgid = current_tgid();
		stat_tgid = bpf_map_lookup_elem(&tgid_net_stat, &tgid);
		if (protocol == IPPROTO_TCP) {

			if (stat_tgid) {
				stat_tgid->tcprcvpacks++;
				stat_tgid->tcprcvbytes += length;
			} else {
				struct taskcount data ={
					.tcprcvpacks = 1,
					.tcprcvbytes = length
				};
				ret = bpf_map_update_elem(&tgid_net_stat, &tgid, &data, BPF_ANY);

			}

		} else if (protocol == IPPROTO_UDP) {
			if (stat_tgid) {
				stat_tgid->udprcvpacks++;
				stat_tgid->udprcvbytes += length;
			} else {
				struct taskcount data ={
					.udprcvpacks = 1,
					.udprcvbytes = length
				};
				ret = bpf_map_update_elem(&tgid_net_stat, &tgid, &data, BPF_ANY);
			}
		}
	}
	return ret;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";
