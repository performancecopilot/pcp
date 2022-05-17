/**
 * Common imports and helpers for BPF module that will be loaded by the kernel.
 */

/* next imports require this defined so they are configured to run in kernel */
#define __KERNEL__

/* represents all of the kernel structs; generated via bpftool */
#include <vmlinux.h>

/* bpf kernel-side functions */
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

/* iovisor bpf kernel side helpers */
#include "bits.bpf.h"
#include "maps.bpf.h"

/* not provided by some libbpf versions, normally in bpf_helpers.h */
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + ((c) > 255 ? 255 : (c)))
#endif

/* create new element or update existing */
#define BPF_ANY       0

/**
 * Create an entry in a BPF map. If the entry already exists, add the value.
 */
static inline void add_or_create_entry(void *map, const void *key, const unsigned long val) {
    unsigned long *value = bpf_map_lookup_elem(map, key);
    if (value != 0)
    {
	    // equivalent to a LOCK XADD to the existing entry
        ((void)__sync_fetch_and_add(value, val));
    }
    else
    {
        // does not exist yet, create it
        bpf_map_update_elem(map, key, &val, BPF_ANY);
    }
}

/* kernel version compatibility; this is required for earlier kernels but ignored in later libbpf */
__u32 _version SEC("version") = 1;
