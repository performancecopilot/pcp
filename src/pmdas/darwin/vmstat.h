/*
 * VM statistics types
 * Copyright (c) 2026 Red Hat.
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <mach/mach.h>

struct xsw_usage;

/*
 * Memory compressor deep dive statistics
 * Timing buckets and health metrics from sysctl
 */
struct compressor_stats {
    uint64_t swapouts_under_30s;
    uint64_t swapouts_under_60s;
    uint64_t swapouts_under_300s;
    uint64_t thrashing_detected;
    uint64_t major_compactions;
    uint64_t lz4_compressions;
};

extern int refresh_vmstat(struct vm_statistics64 *);
extern int refresh_swap(struct xsw_usage *);
extern int refresh_compressor_stats(struct compressor_stats *);
extern int fetch_vmstat(unsigned int, unsigned int, pmAtomValue *);
