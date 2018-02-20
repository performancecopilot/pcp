/*
 * Copyright (c) 2015-2017 Red Hat.
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

typedef enum json_flags {
    pmjson_flag_bitfield = (1<<0),
    pmjson_flag_boolean  = (1<<1),
    pmjson_flag_s32      = (1<<2),
    pmjson_flag_u32      = (1<<3),
    pmjson_flag_s64      = (1<<4),
    pmjson_flag_u64      = (1<<5),
    pmjson_flag_float    = (1<<6),
    pmjson_flag_double   = (1<<7),
    pmjson_flag_minimal  = (1<<10),
    pmjson_flag_pretty   = (1<<11),
    pmjson_flag_quiet    = (1<<12),
    pmjson_flag_yaml     = (1<<13),
} json_flags;

typedef struct json_metric_desc {
    char          *json_pointer;	/* json pointer to metric */
    json_flags    flags;		/* flags to check if set */
    int           num_values;		/* number of values */
    pmAtomValue   values;		/* metric value */
    char          *dom;			/* instance name */
} json_metric_desc;

typedef int (*json_get)(char *, int, void *);
extern int pmjsonGet(json_metric_desc *, int, pmInDom, json_get, void *);

extern int pmjsonInit(int, json_metric_desc *, int);
extern int pmjsonInitIndom(int, json_metric_desc *, int, pmInDom);

extern int pmjsonPrint(FILE *, json_flags, const char *, json_get, void *);

#ifdef __cplusplus
}
#endif

#endif /* PCP_PMJSON_H */
