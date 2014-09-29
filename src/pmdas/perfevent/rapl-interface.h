/* RAPL interface 
 *
 * Copyright (C) 2014  Joe White
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef RAPL_INTERFACE_H_
#define RAPL_INTERFACE_H_

#include <stdint.h>

typedef struct rapl_data_t_ {
    int eventcode;
    int cpuidx;
} rapl_data_t;

void rapl_init();
void rapl_destroy();

int rapl_get_os_event_encoding(const char *eventname, int cpu, rapl_data_t *arg);
int rapl_open(rapl_data_t *arg);
int rapl_read(rapl_data_t *arg, uint64_t *result);

#endif
