/*
 * Copyright (c) 2026 Red Hat.
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

typedef struct delayacct {
    unsigned long long swapin_delay_total;
    unsigned long long blkio_delay_total;
    unsigned long long cpu_delay_total;
} delayacct_t;

extern void delayacct_init(void);
extern int delayacct_info(int, delayacct_t *);

