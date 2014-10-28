/*
 * perfmanager interface
 *
 * Copyright (c) 2013 Joe White
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

#ifndef PERFMANAGER_H_
#define PERFMANAGER_H_

#include <stdint.h>
#include "perfinterface.h"

typedef intptr_t perfmanagerhandle_t;

perfmanagerhandle_t *manager_init(const char *configfilename);

void manager_destroy(perfmanagerhandle_t *mgr);

int perf_get_r(perfmanagerhandle_t *inst, perf_counter **data, int *size);

int perf_enabled(perfmanagerhandle_t *inst);

#endif // PERFMANAGER_H_
