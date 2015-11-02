/*
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "linux_table.h"
#include "pcp/config.h"

extern int linux_table_lookup(const char *field, struct linux_table *table, uint64_t *val);
extern struct linux_table *linux_table_clone(struct linux_table *table);
extern int linux_table_scan(FILE *fp, struct linux_table *table);

inline int
linux_table_lookup(const char *field, struct linux_table *table, uint64_t *val)
{
    struct linux_table *t;

    for (t=table; t && t->field; t++) {
	if (strncmp(field, t->field, t->field_len) == 0) {
	    if (t->valid) {
		*val = t->val;
		return 1;
	    }
	    /* Invalid */
	    return 0;
	}
    }

    fprintf(stderr, "Warning: linux_table_lookup failed for \"%s\"\n", field);
    return 0;
}

inline struct linux_table *
linux_table_clone(struct linux_table *table)
{
    struct linux_table *ret;
    struct linux_table *t;
    int len;

    if (!table)
	return NULL;
    for (len=1, t=table; t->field; t++)
    	len++;
    ret = (struct linux_table *)malloc(len * sizeof(struct linux_table));
    if (!ret)
	return NULL;
    memcpy(ret, table, len * sizeof(struct linux_table));

    /* Initialize the table */
    for (t=ret; t && t->field; t++) {
	if (!t->field_len)
	    t->field_len = strlen(t->field);
	t->valid = LINUX_TABLE_INVALID;
    }

    return ret;
}

inline int
linux_table_scan(FILE *fp, struct linux_table *table)
{
    char *p;
    struct linux_table *t;
    char buf[1024];
    int ret = 0;

    while(fgets(buf, sizeof(buf), fp) != NULL) {
	for (t=table; t && t->field; t++) {
	    if ((p = strstr(buf, t->field)) != NULL) {
		/* first digit after the matched field */
		for (p += t->field_len; *p; p++) {
		    if (isdigit((int)*p))
			break;
		}
		if (isdigit((int)*p)) {
		    t->this = strtouint64(p, NULL, 10);
		    t->valid = LINUX_TABLE_VALID;
		    ret++;
		    break;
		}
	    }
	}
    }

    /* calculate current value, accounting for counter wrap */
    for (t=table; t && t->field; t++) {
    	if (t->maxval == 0)
	    /* instantaneous value */
	    t->val = t->this;
	else {
	    /* counter value */
	    if (t->this >= t->prev)
		t->val += t->this - t->prev;
	    else
	        t->val += t->this + (t->maxval - t->prev);
	    t->prev = t->this;
	}
    }

    return ret;
}
