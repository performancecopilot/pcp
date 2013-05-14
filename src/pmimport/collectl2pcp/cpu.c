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

int
cpu_handler(char *buf)
{
    char *s;
    metric_t *m;
    char *inst = NULL;

    if (isdigit(buf[3])) {
	/* kernel.percpu.cpu.* */
	inst = strtok(buf, " ");
	m = find_metric("kernel.percpu.cpu.user");
	for (; m && m->name; m++) {
	    if ((s = strtok(NULL, " ")) != NULL)
		put_str_value(m->name, CPU_INDOM, inst, s);
	}
    }
    else {
	s = strtok(buf, " ");
	m = find_metric("kernel.all.cpu.user");
	for (; m && m->name; m++) {
	    if ((s = strtok(NULL, " ")) != NULL)
		put_str_value(m->name, PM_INDOM_NULL, NULL, s);
	}
    }

    return 0;
}
