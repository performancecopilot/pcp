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
extern const char *timeval_str(struct timeval *);

extern int context_labels(int, pmLabelSet **);
extern int metric_labelsets(struct metric *,
		char *, int,
		int (*filter)(const pmLabel *, const char *, void *),
		void *type);
extern int instance_labelsets(struct indom *, struct instance *,
		char *, int,
		int (*filter)(const pmLabel *, const char *, void *),
		void *type);

extern const char *indom_str(struct metric *);
extern const char *pmid_str(struct metric *);
extern const char *semantics_str(struct metric *);
extern const char *type_str(struct metric *);
extern const char *units_str(struct metric *);
extern const char *hash_str(const unsigned char *);

extern int source_hash(struct context *);
extern void metric_hash(struct metric *);
extern void instance_hash(struct indom *, struct instance *);

/*
 * More widely applicable web API helper routines
 */
extern int pmwebapi_source_meta(struct context *, char *, int);
extern int pmwebapi_source_hash(unsigned char *, const char *, int);
extern sds pmwebapi_hash_sds(const unsigned char *);
extern char *pmwebapi_hash_str(const unsigned char *);

/*
 * Generally useful sds buffer formatting and diagnostics callback macros
 */
#define seriesfmt(msg, fmt, ...)	\
	((msg) = sdscatprintf(sdsempty(), fmt, ##__VA_ARGS__))
#define seriesmsg(baton, level, msg)	\
	((baton)->info((level), (msg), (baton)->userdata), sdsfree(msg))
#define webapimsg(sp, level, msg)	\
	((sp)->settings->on_info((level), (msg), (sp)->userdata), sdsfree(msg))

#endif	/* SERIES_UTIL_H */
