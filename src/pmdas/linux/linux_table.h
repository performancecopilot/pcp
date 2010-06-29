/*
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

#ifndef _LINUX_TABLE_H
#define _LINUX_TABLE_H
/*
 * scans linux style /proc tables, e.g. :
 *
 * numa_hit 266809
 * numa_miss 0
 * numa_foreign 0
 * interleave_hit 0
 * local_node 265680
 * other_node 1129
 *
 * Value is a counter that wraps at maxval,
 * unless maxval is 0, in which case the 
 * value is treated as instantaneous and no
 * wrap detection is attempted.
 *
 * Tables are typically declared as a static array, and
 * then allocated dynamically with linux_table_clone().
 * e.g. :
 *
 *	static struct linux_table numa_meminfo_table[] = {
 *	    { "numa_hit",		0xffffffffffffffff },
 *	    { "numa_miss",		0xffffffffffffffff },
 *	    { "numa_foreign",		0xffffffffffffffff },
 *	    { "interleave_hit",		0xffffffffffffffff },
 *	    { "local_node",		0xffffffffffffffff },
 *	    { "other_node",		0xffffffffffffffff },
 *          { NULL };
 *      };
 */

enum {
	LINUX_TABLE_INVALID,
	LINUX_TABLE_VALID
};

struct linux_table {
	char		*field;
	uint64_t	maxval;
	uint64_t	val;
	uint64_t	this;
	uint64_t	prev;
	int		field_len;
	int		valid;
};

extern int linux_table_lookup(const char *field, struct linux_table *table, uint64_t *val);
extern struct linux_table *linux_table_clone(struct linux_table *table);
extern int linux_table_scan(FILE *fp, struct linux_table *table);

#endif /* _LINUX_TABLE_H */
