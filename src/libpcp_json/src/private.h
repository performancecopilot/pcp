/*
 * Copyright (c) 2016 Red Hat.
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
#ifndef _PRIVATE_H
#define _PRIVATE_H

#if defined(__GNUC__) && (__GNUC__ >= 4) && !defined(IS_MINGW)
# define _PMJSON_HIDDEN __attribute__ ((visibility ("hidden")))
#else
# define _PMJSON_HIDDEN
#endif
#include "jsmn.h"

PCP_CALL extern int jsmneq(const char *, jsmntok_t *, const char *) _PMJSON_HIDDEN;
PCP_CALL extern int jsmnflag(const char *, jsmntok_t *, int *, int) _PMJSON_HIDDEN;
PCP_CALL extern int jsmnint(const char *, jsmntok_t *, int *) _PMJSON_HIDDEN;
PCP_CALL extern int jsmnstrdup(const char *, jsmntok_t *, char**) _PMJSON_HIDDEN;

#endif /* _PRIVATE_H */
