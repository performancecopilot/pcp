/*
 * Copyright (c) 2017-2018 Red Hat.
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
#ifndef SERIES_UTIL_H
#define SERIES_UTIL_H

#include "sds.h"
#include "load.h"

extern int tsub(struct timeval *, struct timeval *);
extern int tadd(struct timeval *, struct timeval *);

static inline double tv2real(struct timeval *tv)
{
    return pmtimevalToReal(tv);
}
extern const char *timeval_str(struct timeval *);

extern void fputstamp(struct timeval *, int, FILE *);

extern int context_labels(int, pmLabelSet **);
extern int merge_labelsets(struct metric *, struct value *,
		char *, int,
		int (*filter)(const pmLabel *, const char *, void *),
		void *type);

extern const char *indom_str(struct metric *);
extern const char *pmid_str(struct metric *);
extern const char *semantics_str(struct metric *);
extern const char *type_str(struct metric *);
extern const char *units_str(struct metric *);

extern sds json_escaped_str(const char *);
extern const char *hash_str(const unsigned char *);

extern int source_hash(struct context *);
extern void metric_hash(struct metric *, pmDesc *);
extern void instance_hash(struct metric *, struct value *, sds, pmDesc *);

/*
 * More widely applicable web API helper routines
 */
extern int pmwebapi_source_meta(struct context *, char *, int);
extern int pmwebapi_source_hash(unsigned char *, const char *, int);
extern sds pmwebapi_hash_sds(const unsigned char *);
extern char *pmwebapi_hash_str(const unsigned char *);

#endif	/* SERIES_UTIL_H */
