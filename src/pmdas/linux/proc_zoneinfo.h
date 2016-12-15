/*
 * Linux /proc/zoneinfo metrics cluster
 *
 * Copyright (c) 2016 Fujitsu.
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
    unsigned long long          dma_free;
    unsigned long long          dma32_free;
    unsigned long long          normal_free;
    unsigned long long          highmem_free;
} zoneinfo_entry_t;

extern int refresh_proc_zoneinfo(pmInDom indom);
