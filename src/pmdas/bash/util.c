/*
 * Utility routines for the bash tracing PMDA
 *
 * Copyright (c) 2012 Nathan Scott.
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

#include "event.h"
#include "pmda.h"
#include <ctype.h>

int
extract_int(char *s, const char *field, size_t length, int *value)
{
    char *endnum;
    int num;

    if (strncmp(s, field, length) != 0)
	return 0;
    num = strtol(s + length, &endnum, 10);
    if (*endnum != ',' && *endnum != '\0' && !isspace((int)*endnum))
	return 0;
    *value = num;
    return endnum - s + 1;
}

int
extract_str(char *s, size_t end, const char *field, size_t length, char *value, size_t vsz)
{
    char *p;

    if (strncmp(s, field, length) != 0)
	return 0;
    p = s + length;
    while (*p != ',' && *p != '\0' && !isspace((int)*p))
	p++;
    *p = '\0';
    strncpy(value, s + length, vsz);
    return p - s + 1;
}

int
extract_cmd(char *s, size_t end, const char *field, size_t length, char *value, size_t vsz)
{
    char *start = NULL, *stop = NULL, *p;
    int len;

    /* find the start of the command */
    for (p = s; p < s + end; p++) {
	if (strncmp(p, field, length) != 0)
	    continue;
	p++;
	if (*p == ' ')
	    p++;
	break;
    }
    if (p == s + end)
	return 0;
    start = p;

    /* find the command terminator */
    while (*p != '\n' && *p != '\0' && p < s + end)
	p++;
    stop = p;

    /* truncate it if necessary */
    len = stop - start;
    if (len > vsz - 1)
	len = vsz - 1;

    /* copy it over to "value" */
    start[len] = '\0';
    strncpy(value, start, len + 1);
    return len;
}
