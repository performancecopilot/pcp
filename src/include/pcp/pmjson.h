/*
 * Copyright (c) 2015 Red Hat.
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
 */
#ifndef PCP_PMJSON_H
#define PCP_PMJSON_H

#include "jsmn.h"

#ifdef __cplusplus
extern "C" {
#endif

PCP_CALL extern int jsmneq(const char *, jsmntok_t *, const char *);
PCP_CALL extern int jsmnflag(const char *, jsmntok_t *, int *, int);
PCP_CALL extern int jsmnint(const char *, jsmntok_t *, int *);
PCP_CALL extern int jsmnstrdup(const char *, jsmntok_t *, char**);

    
#ifdef __cplusplus
}
#endif

#endif /* PCP_PMJSON_H */
