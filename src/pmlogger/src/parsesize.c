/*
 * Copyright (c) 2012-2018,2021-2022 Red Hat.
 * Copyright (c) 1995-2001,2003 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "logger.h"
#include <ctype.h>

/*
 * tolower_str - convert a string to all lowercase
 */
static void 
tolower_str(char *str)
{
    char *s = str;

    while (*s) {
      *s = tolower((int)*s);
      s++;
    }
}

/*
 * ParseSize - parse a size argument given in a command option
 *
 * The size can be in one of the following forms:
 *   "40"    = sample counter of 40
 *   "40b"   = byte size of 40
 *   "40Kb"  = byte size of 40*1024 bytes = 40 kilobytes (kibibytes)
 *   "40Mb"  = byte size of 40*1024*1024 bytes = 40 megabytes (mebibytes)
 *   time-format = time delta in seconds
 *
 */
int
ParseSize(char *size_arg, int *sample_counter, __int64_t *byte_size, 
          struct timeval *time_delta)
{
    long x = 0; /* the size number */
    char *ptr = NULL;
    char *interval_err;

    *sample_counter = -1;
    *byte_size = -1;
    time_delta->tv_sec = -1;
    time_delta->tv_usec = -1;
  
    x = strtol(size_arg, &ptr, 10);

    /* must be positive */
    if (x <= 0)
	return -1;

    if (*ptr == '\0') {
	/* we have consumed entire string as a long */
	/* => we have a sample counter */
	*sample_counter = x;
	return 1;
    }

    /* we have a number followed by something else */
    if (ptr != size_arg) {
	int len;

	tolower_str(ptr);

	/* chomp off plurals */
	len = strlen(ptr);
	if (ptr[len-1] == 's')
	    ptr[len-1] = '\0';

	/* if bytes */
	if (strcmp(ptr, "b") == 0 ||
	    strcmp(ptr, "byte") == 0) {
	    *byte_size = x;
	    return 1;
	}  

	/* if kilobytes */
	if (strcmp(ptr, "k") == 0 ||
	    strcmp(ptr, "kb") == 0 ||
	    strcmp(ptr, "kib") == 0 ||
	    strcmp(ptr, "kbyte") == 0 ||
	    strcmp(ptr, "kibibyte") == 0 ||
	    strcmp(ptr, "kilobyte") == 0) {
	    *byte_size = x*1024;
	    return 1;
	}

	/* if megabytes */
	if (strcmp(ptr, "m") == 0 ||
	    strcmp(ptr, "mb") == 0 ||
	    strcmp(ptr, "mib") == 0 ||
	    strcmp(ptr, "mbyte") == 0 ||
	    strcmp(ptr, "mebibyte") == 0 ||
	    strcmp(ptr, "megabyte") == 0) {
	    *byte_size = x*1024*1024;
	    return 1;
	}

	/* if gigabytes */
	if (strcmp(ptr, "g") == 0 ||
	    strcmp(ptr, "gb") == 0 ||
	    strcmp(ptr, "gib") == 0 ||
	    strcmp(ptr, "gbyte") == 0 ||
	    strcmp(ptr, "gibibyte") == 0 ||
	    strcmp(ptr, "gigabyte") == 0) {
	    *byte_size = ((__int64_t)x)*1024*1024*1024;
	    return 1;
	}
    }

    /* Doesn't fit pattern above, try a time interval */
    if (pmParseInterval(size_arg, time_delta, &interval_err) >= 0)
        return 1;
    /* error message not used here */
    free(interval_err);
  
    /* Doesn't match anything, return an error */
    return -1;
}
