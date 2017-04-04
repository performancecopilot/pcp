/*
 * Copyright (c) 2016-2017 Fujitsu.
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
 */

typedef struct {
        /* Number of times that KSM has scanned for duplicated content. */
	unsigned long full_scans;
        /* Allows merging across NUMA nodes. */
	unsigned long merge_across_nodes;
        /* The number of nodes in the stable tree */
        unsigned long pages_shared;
        /* The number of virtual pages that are sharing a single page. */ 
        unsigned long pages_sharing;
        /* Number of pages to scan at a time. */ 
        unsigned int pages_to_scan;
        /* The number of nodes in the unstable tree */ 
        unsigned long pages_unshared;
        /* Number of pages that are candidate to be shared */
        unsigned long pages_volatile;
        /* Wheter the KSM is run */
        int run;
        /* Milliseconds ksmd should sleep between batches */
        unsigned int sleep_millisecs;
} ksm_info_t;

extern int refresh_ksm_info(ksm_info_t *ksm_info);

