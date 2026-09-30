/*
 * Memory pressure statistics types
 * Copyright (c) 2026 Red Hat, Paul Smith.
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

/*
 * Memory pressure structure
 * The kernel's own verdict on memory pressure, via kern.memorystatus_* sysctls
 */
typedef struct mempressurestats {
    __uint32_t	level;		/* 1 = normal, 2 = warning, 4 = critical */
    __uint32_t	available;	/* percentage of memory available */
} mempressurestats_t;

extern int refresh_mempressure(mempressurestats_t *);
extern int fetch_mempressure(unsigned int, pmAtomValue *);
