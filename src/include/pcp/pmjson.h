/*
 * Copyright (c) 2015-2016 Red Hat.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * Javascript Object Notation metric value extraction (libpcp_web)
 */
#ifndef PCP_PMJSON_H
#define PCP_PMJSON_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_metric_desc {
    char          *json_pointer;
    int           flags;
    int           num_values;
    pmAtomValue   values;
    char          *dom;
} json_metric_desc;

PCP_CALL extern int pmjsonInit(int fd, json_metric_desc *, int);
PCP_CALL extern int pmjsonInitIndom(int fd, json_metric_desc *, int, pmInDom);

#ifdef __cplusplus
}
#endif

#endif /* PCP_PMJSON_H */
