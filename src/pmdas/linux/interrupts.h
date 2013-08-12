/*
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

extern unsigned int irq_err_count;

extern void interrupts_init(pmdaMetric *, int);
extern int refresh_interrupt_values(void);
extern int interrupts_fetch(int, int, unsigned int, pmAtomValue *);
