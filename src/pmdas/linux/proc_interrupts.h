/*
 * Copyright (c) 2016,2021 Red Hat.
 * Copyright (c) 2011 Aconex.  All Rights Reserved.
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
    char		*label;		/* short interrupt label text */
    unsigned long long	total;		/* aggregation of interrupt counts */
} interrupt_t;

typedef struct {
    unsigned int	cpuid;		/* CPU identifier */
    unsigned int	value;		/* individual CPU interrupt value */
    interrupt_t		*row;		/* row data in /proc/interrupts */
} interrupt_cpu_t;

typedef struct {
    unsigned int	cpuid;		/* CPU identifier */
    unsigned long long	intr_count;	/* per-CPU sum of interrupt counts */
    unsigned long long	sirq_count;	/* per-CPU sum of softirq counters */
} online_cpu_t;

extern unsigned int irq_err_count;
extern unsigned int irq_mis_count;

extern int refresh_proc_interrupts(void);
extern int refresh_proc_softirqs(void);
extern int proc_interrupts_fetch(int, int, unsigned int, pmAtomValue *);
extern int proc_softirqs_fetch(int, int, unsigned int, pmAtomValue *);

