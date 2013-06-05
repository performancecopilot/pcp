/*
 * Copyright (c) 2013 Red Hat Inc.
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
 * Handler for kernel.{percpu,all}.cpu.*
 *
 * cpu  66057896 4079 13437134 575696782 4513632 1463907 1701801 0 28602 0
 * cpu0 24489004 2578 3862841 132669978 2807446 852698 928386 0 28602 0
 *
 */

#include "metrics.h"

#define ticks_to_msec(ticks) (1000ULL * strtoull(ticks, NULL, 0) / kernel_all_hz)

int
cpu_handler(handler_t *h, fields_t *f)
{
    char *inst = NULL;

    if (f->fieldlen[0] < 3 || f->nfields < 9)
    	return -1;

    if (f->fieldlen[0] > 3 && isdigit(f->fields[0][3])) {
	/* kernel.percpu.cpu.* */
	pmInDom indom = pmInDom_build(LINUX_DOMAIN, CPU_INDOM);

	inst = f->fields[0]; /* cpuN */

	put_ull_value("kernel.percpu.cpu.user", indom, inst, ticks_to_msec(f->fields[1]));
	put_ull_value("kernel.percpu.cpu.nice", indom, inst, ticks_to_msec(f->fields[2]));
	put_ull_value("kernel.percpu.cpu.sys", indom, inst, ticks_to_msec(f->fields[3]));
	put_ull_value("kernel.percpu.cpu.idle", indom, inst, ticks_to_msec(f->fields[4]));
	put_ull_value("kernel.percpu.cpu.wait.total", indom, inst, ticks_to_msec(f->fields[5]));
	put_ull_value("kernel.percpu.cpu.irq.hard", indom, inst, ticks_to_msec(f->fields[6]));
	put_ull_value("kernel.percpu.cpu.irq.soft", indom, inst, ticks_to_msec(f->fields[7]));
	put_ull_value("kernel.percpu.cpu.steal", indom, inst, ticks_to_msec(f->fields[8]));
	if (f->nfields > 9) /* guest cpu usage is only in more recent kernels */
	    put_ull_value("kernel.percpu.cpu.guest", indom, inst, ticks_to_msec(f->fields[9]));

	put_ull_value("kernel.percpu.cpu.intr", indom, inst,
		1000 * ((double)strtoull(f->fields[6], NULL, 0) +
			(double)strtoull(f->fields[7], NULL, 0)) / kernel_all_hz);
    }
    else {
	put_ull_value("kernel.all.cpu.user", PM_INDOM_NULL, NULL, ticks_to_msec(f->fields[1]));
	put_ull_value("kernel.all.cpu.nice", PM_INDOM_NULL, NULL, ticks_to_msec(f->fields[2]));
	put_ull_value("kernel.all.cpu.sys", PM_INDOM_NULL, NULL, ticks_to_msec(f->fields[3]));
	put_ull_value("kernel.all.cpu.idle", PM_INDOM_NULL, NULL, ticks_to_msec(f->fields[4]));
	put_ull_value("kernel.all.cpu.wait.total", PM_INDOM_NULL, NULL, ticks_to_msec(f->fields[5]));
	put_ull_value("kernel.all.cpu.irq.hard", PM_INDOM_NULL, NULL, ticks_to_msec(f->fields[6]));
	put_ull_value("kernel.all.cpu.irq.soft", PM_INDOM_NULL, NULL, ticks_to_msec(f->fields[7]));
	put_ull_value("kernel.all.cpu.steal", PM_INDOM_NULL, NULL, ticks_to_msec(f->fields[8]));
	if (f->nfields > 9)
	    put_ull_value("kernel.all.cpu.guest", PM_INDOM_NULL, NULL, ticks_to_msec(f->fields[9]));

	put_ull_value("kernel.all.cpu.intr", PM_INDOM_NULL, NULL,
		1000 * ((double)strtoull(f->fields[6], NULL, 0) +
			(double)strtoull(f->fields[7], NULL, 0)) / kernel_all_hz);
    }

    return 0;
}
