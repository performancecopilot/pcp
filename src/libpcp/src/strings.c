/*
 * General Utility Routines
 *
 * Copyright (c) 2012-2018 Red Hat.
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
 * Copyright (c) 1995-2002,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2021 Ken McDonell.  All Rights Reserved.
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
 *
 * Thread-safe notes
 *
 * Nothing to see here.
 */

#include <stdarg.h>
#include <ctype.h>

#include "pmapi.h"
#include "libpcp.h"
#include "fault.h"
#include "deprecated.h"

/*
 * safe version of sprintf()
 * - ensure no buffer over-runs
 */
int
pmsprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list	arg;
    int		bytes;

    /* bad input given - treated as garbage in, garbage out */
    if (size == 0)
	return 0;

    va_start(arg, fmt);
    bytes = vsnprintf(str, size, fmt, arg);
    va_end(arg);
    if (bytes < size) {
	if (bytes > 0)	/* usual case, null terminated here */
	    return bytes;
	/* safest option - treat all errors as empty string */
	*str = '\0';
	return 1;
    }
    /* ensure (truncated) string is always null terminated. */
    bytes = size - 1;
    str[bytes] = '\0';
    return bytes;
}

/*
 * Safe replacement for scanf( ...%s...) to avoid buffer over-run.
 *
 * Read a string (using scanf's %s semantics) from f, and return a
 * malloc'd array holding the string with null-byte termination.
 *
 * Allocator uses a "buffer doubling" algorithm, but the result is
 * truncated to the null-byte terminator before returning.
 */
int
pmfstring(FILE *f, char **str)
{
    char	*buf = NULL;
    char	*buf_tmp;
    int		buflen = 0;
    int		i = 0;
    char	c;

    /* skip initial white space */
    while ((c = fgetc(f)) != EOF) {
	if (c == EOF)
	    return -1; /* aka EOF */
	else if (c == '\n')
	    /* \n before a non-whitespace char */
	    return 0;
	else if (!isspace(c))
	    break;
    }

    for ( ; ; ) {
	if (i > buflen-1) {
	    if (buf == NULL) {
		if ((buf = (char *)malloc(4)) == NULL) {
		    pmNoMem("pmfbufing malloc", 4, PM_RECOV_ERR);
		    return -2;
		}
		buflen = 4;
	    }
	    else {
		if ((buf_tmp = (char *)realloc(buf, buflen*2)) == NULL) {
		    pmNoMem("pmfbufing realloc", buflen*2, PM_RECOV_ERR);
		    free(buf);
		    return -2;
		}
		buf = buf_tmp;
		buflen *= 2;
	    }
	}
	buf[i++] = c;
	c = fgetc(f);
	if (c == EOF || isspace(c))
	    break;
    }
    buf[i] = '\0';		/* null-byte termination */

    /* truncate allocation to length of the bufing + terminator */
    if ((buf_tmp = (char *)realloc(buf, i+1)) == NULL) {
	pmNoMem("pmfbufing truncate", i+1, PM_RECOV_ERR);
	free(buf);
	return -2;
    }

    *str = buf_tmp;
    return i;
}
