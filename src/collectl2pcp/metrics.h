/*
 * Copyright (c) 2013 Red Hat Inc.
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
 *
 * Mark Goodwin <mgoodwin@redhat.com> May 2013.
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <pcp/import.h>

/* domains from stdpmid */
#define LINUX_DOMAIN 60
#define PROC_DOMAIN 3

/* Linux PMDA instance domain identifiers */
#include "../../pmdas/linux/linux.h"

/* PCP metric, see metrics.c */
typedef struct {
	char *name;
	pmDesc desc;
} metric_t;

/* parsed input buffer, split into fields. See fields_new() et al */
typedef struct {
	int len;
	char *buf;
	int nfields;
	char **fields;
	int *fieldlen;
} fields_t;

/* handler to convert parsed fields into pcp metrics and emit them */
typedef struct handler {
	char *pattern;
	int (*handler)(struct handler *h, fields_t *f);
	char *metric_name;
} handler_t;

/* global options */
extern int vflag;
extern int kernel_all_hz;
extern int utc_offset;

/* metric table, see metrics.c (generated from pmdesc) */
extern metric_t metrics[];

/* instance domain count table - needed for dynamic instances */
extern int indom_cnt[NUM_INDOMS];

/* metric value handler table */
extern handler_t handlers[];

/* handlers */
extern int header_handler(FILE *fp, char *fname, char *buf, int buflen);
extern int timestamp_flush(void);
extern int timestamp_handler(handler_t *h, fields_t *f);
extern int cpu_handler(handler_t *h, fields_t *f);
extern int proc_handler(handler_t *h, fields_t *f);
extern int disk_handler(handler_t *h, fields_t *f);
extern int net_handler(handler_t *h, fields_t *f);
extern int net_tcp_handler(handler_t *h, fields_t *f);
extern int net_udp_handler(handler_t *h, fields_t *f);
extern int loadavg_handler(handler_t *h, fields_t *f);
extern int generic1_handler(handler_t *h, fields_t *f);
extern int generic2_handler(handler_t *h, fields_t *f);

/* various helpers, see util.c */
extern metric_t *find_metric(char *name);
extern handler_t *find_handler(char *buf);
extern int put_str_instance(pmInDom indom, char *instance);
extern int put_str_value(char *name, pmInDom indom, char *instance, char *val);
extern int put_int_value(char *name, pmInDom indom, char *instance, int val);
extern int put_ull_value(char *name, pmInDom indom, char *instance, unsigned long long val);

/* helpers to parse and manage input buffers */
extern int strfields(const char *s, int len, char **fields, int *fieldlen, int maxfields);
extern fields_t *fields_new(const char *s, int len);
extern fields_t *fields_dup(fields_t *f);
extern void fields_free(fields_t *f);
