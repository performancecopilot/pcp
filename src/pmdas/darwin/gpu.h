/*
 * GPU statistics for Darwin PMDA
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

#ifndef GPU_H
#define GPU_H

#include <stdint.h>

/* Per-GPU statistics */
struct gpustat {
    char name[32];              /* "gpu0", "gpu1", etc. */
    int utilization;            /* Device Utilization % (0-100) */
    uint64_t memory_used;       /* VRAM used in bytes */
    uint64_t memory_total;      /* Total VRAM in bytes */
};

/* Collection of all GPU statistics */
struct gpustats {
    int count;                  /* Number of GPUs (fixed at init) */
    struct gpustat *gpus;       /* Allocated once at init, freed at shutdown */
};

/* Initialize GPU monitoring (called once at startup) */
extern int init_gpu(void);

/* Enumerate GPUs and update statistics */
extern int gpu_iokit_enumerate(struct gpustats *stats);

/* Refresh GPU statistics (called before each fetch) */
struct pmdaIndom;
extern int refresh_gpus(struct gpustats *stats, struct pmdaIndom *indom);

/* Fetch GPU metrics */
extern int fetch_gpu(unsigned int item, unsigned int inst, pmAtomValue *atom);

#endif /* GPU_H */
