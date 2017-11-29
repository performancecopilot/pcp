/*
 * Copyright (c) 2017 Red Hat.
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
#ifndef UTIL_H
#define UTIL_H

#include "load.h"

extern int tsub(struct timeval *, struct timeval *);
extern int tadd(struct timeval *, struct timeval *);

static inline double tv2real(struct timeval *tv)
{
    return pmtimevalToReal(tv);
}

extern void fputstamp(struct timeval *, int, FILE *);

extern int merge_labelsets(struct metric *, struct value *,
		char *, int,
		int (*filter)(const pmLabel *, const char *, void *),
		void *type);

extern unsigned int value_instid(struct value *);
extern const char *value_instname(struct value *);

extern const char *value_atomstr(struct metric *, struct value *);
extern char *value_labels(struct metric *, struct value *);

#endif	/* UTIL_H */
