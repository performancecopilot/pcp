/*
 * Copyright (c) 2025 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#ifndef OPEN_TELEMETRY_H
#define OPEN_TELEMETRY_H

#include "pmwebapi.h"
#include "dict.h"

/* convert PCP metric semantics to Open Telemetry form */
const char *open_telemetry_semantics(sds);

/* convert PCP metric type to Open Telemetry form */
const char *open_telemetry_type(sds);

/* convert PCP metric units to Open Telemetry form */
extern sds open_telemetry_units(sds);

/* convert an array of PCP labelsets into Open Telemetry form */
extern void open_telemetry_labels(pmWebLabelSet *, dict **, sds *);

#endif	/* OPEN_TELEMETRY_H */
