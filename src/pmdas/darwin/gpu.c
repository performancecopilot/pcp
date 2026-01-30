/*
 * GPU statistics for Darwin PMDA - PCP integration layer
 *
 * Copyright (c) 2026 Paul Smith.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pmapi.h"
#include "pmda.h"

#include "darwin.h"
#include "gpu.h"

extern pmdaIndom indomtab[];

/*
 * Update GPU instance domain
 */
static int
update_gpu_indom(struct gpustats *stats, int count, pmdaIndom *indom)
{
    int i;
    pmdaInstid *instids;

    if (count == 0) {
        indom->it_numinst = 0;
        indom->it_set = NULL;
        return 0;
    }

    instids = malloc(count * sizeof(pmdaInstid));
    if (!instids)
        return -ENOMEM;

    for (i = 0; i < count; i++) {
        instids[i].i_inst = i;
        instids[i].i_name = stats->gpus[i].name;
    }

    indom->it_numinst = count;
    indom->it_set = instids;
    return 0;
}

/*
 * Initialize GPU monitoring
 * Called once at PMDA startup
 */
int
init_gpu(void)
{
    /* Initialization happens in first refresh */
    return 0;
}

/*
 * Refresh GPU statistics
 * Called before each metric fetch
 */
int
refresh_gpus(struct gpustats *stats, pmdaIndom *indom)
{
    static int inited = 0;
    int count = 0;
    int i, status;

    if (!inited) {
        memset(stats, 0, sizeof(struct gpustats));

        /* Discover GPU count */
        status = gpu_iokit_enumerate(stats);
        if (status != 0) {
            /* No GPUs or IOKit access failed */
            indom->it_numinst = 0;
            return PM_ERR_AGAIN;
        }

        /* Allocate GPU array if we found any */
        if (stats->count > 0) {
            stats->gpus = calloc(stats->count, sizeof(struct gpustat));
            if (!stats->gpus) {
                stats->count = 0;
                return -ENOMEM;
            }

            /* Setup instance names */
            for (i = 0; i < stats->count; i++) {
                snprintf(stats->gpus[i].name, sizeof(stats->gpus[i].name),
                         "gpu%d", i);
            }
        }

        inited = 1;
    }

    /* Refresh GPU statistics */
    if (stats->gpus && stats->count > 0) {
        status = gpu_iokit_enumerate(stats);
        if (status == 0)
            count = stats->count;
    }

    /* Update instance domain */
    indom->it_numinst = 0;
    if (count)
        status = update_gpu_indom(stats, count, indom);

    return status;
}

/*
 * Fetch GPU metrics
 */
int
fetch_gpu(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    extern struct gpustats mach_gpu;
    extern int mach_gpu_error;
    extern pmdaIndom indomtab[];

    if (mach_gpu_error)
        return mach_gpu_error;

    /* hinv.ngpu */
    if (item == 99) {
        atom->ul = indomtab[GPU_INDOM].it_numinst;
        return 1;
    }

    /* No GPU instances available */
    if (indomtab[GPU_INDOM].it_numinst == 0)
        return 0;

    /* Validate instance number */
    if (inst >= indomtab[GPU_INDOM].it_numinst)
        return PM_ERR_INST;

    switch (item) {
    case 0:  /* darwin.gpu.util */
        atom->ul = mach_gpu.gpus[inst].utilization;
        break;
    case 1:  /* darwin.gpu.memory.used */
        atom->ull = mach_gpu.gpus[inst].memory_used;
        break;
    case 2:  /* darwin.gpu.memory.free */
        atom->ull = mach_gpu.gpus[inst].memory_total - mach_gpu.gpus[inst].memory_used;
        break;
    default:
        return PM_ERR_PMID;
    }

    return 1;
}
