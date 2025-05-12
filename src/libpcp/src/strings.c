/*
 * General Utility Routines
 *
 * Copyright (c) 2012-2018,2021 Red Hat.
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
 *
 * Returns strlen(buf).
 */
ssize_t
pmfstring(FILE *f, char **str)
{
    char	*buf = NULL;
    char	*buf_tmp;
    int		buflen = 0;
    int		i = 0;
    int		c;

    /* skip initial white space */
    while ((c = fgetc(f)) != EOF) {
	if (c == '\n')
	    /* \n before a non-whitespace char */
	    return 0;
	else if (!isspace(c))
	    break;
    }
    if (c == EOF)
	return -1; /* aka EOF */

    for ( ; ; ) {
	if (i > buflen-2) {
	    if (buf == NULL) {
		if ((buf = (char *)malloc(4)) == NULL) {
		    pmNoMem("pmfstring malloc", 4, PM_RECOV_ERR);
		    return -2;
		}
		buflen = 4;
	    }
	    else {
		if ((buf_tmp = (char *)realloc(buf, buflen*2)) == NULL) {
		    pmNoMem("pmfstring realloc", buflen*2, PM_RECOV_ERR);
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

    /* truncate allocation to length of the string + terminator */
    if ((buf_tmp = (char *)realloc(buf, i+1)) == NULL) {
	pmNoMem("pmfstring truncate", i+1, PM_RECOV_ERR);
	free(buf);
	return -2;
    }

    *str = buf_tmp;
    return (ssize_t)i;
}

/*
 * Safe version of strncpy() that guards against buffer overrun
 * and guarantees the dest[] is null-byte terminated, but does not
 * pad the remainder of the dest[] beyond the terminator null-byte.
 *
 * Notes:
 * 1) size and dest args swapped compared to strcpy()
 *
 * Returns 0/-1 for success/truncation (of src in dest)
 */
int
pmstrncpy(char *dest, size_t destlen, const char *src)
{
    char	*d = dest;
    const char	*s = src;

    for ( ; *s && d < &dest[destlen-1]; ) {
	*d++ = *s;
	if (*s != '\0') s++;
    }
    *d = '\0';

    return *s == '\0' ? 0 : -1;
}

/*
 * Safe version of strncat() that guards against buffer overrun
 * and guarantees the dest[] is null-byte terminated, but does not
 * pad the remainder of the dest[] beyond the terminator null-byte.
 *
 * Notes:
 * 1) size and dest args swapped compared to strcat()
 * 2) destlen is total size of dest[], not max # chars to copy
 *
 * Returns 0/-1 for success/truncation (of src in dest)
 */
int
pmstrncat(char *dest, size_t destlen, const char *src)
{
    char	*d = &dest[strlen(dest)];
    const char	*s = src;

    for ( ; *s && d < &dest[destlen-1]; ) {
	*d++ = *s;
	if (*s != '\0') s++;
    }
    *d = '\0';

    return *s == '\0' ? 0 : -1;
}

/*
 * Safe version of strlen() that guards against NULL pointers.
 */
size_t
pmstrlen(const char *s)
{
    if (s != NULL)
	return strlen(s);
    return 0;
}

static inline int
ishex(int x)
{
    return (x >= '0' && x <= '9') ||
	   (x >= 'a' && x <= 'f') ||
	   (x >= 'A' && x <= 'F');
}


/**
 * Takes an UTF-8 encoded string and performs URL encoding of a query parameter
 * component (application/x-www-form-urlencoded)
 *
 * The encoded string will be stored in *outp. It is the responsibility of the
 * caller to free this memory block (unless an error occured, then the function
 * frees this memory block).
 *
 * Returns 0 on success, negative numbers on error
 */
int
__pmUrlEncode(const char *inp, const size_t len, char **outp)
{
    const char		*inp_end = inp + len;
    char		*out, *out_start;
    unsigned char	c;

    /* worst case: every character needs to be encoded */
    out_start = out = malloc(len * 3 + 1);
    if (out == NULL)
	return -ENOMEM;

    while (inp < inp_end) {
	c = *inp++;
	if (isalnum(c) || c == '*' || c == '-' || c == '.' || c == '_') {
	    *out++ = c;
	}
	else if (c == ' ') {
	    *out++ = '+';
	}
	else {
	    if (snprintf(out, 4, "%%%02X", c) != 3)
		goto invalid_input;
	    out += 3;
	}
    }
    *out = '\0';
    *outp = out_start;
    return 0;

invalid_input:
    free(out_start);
    return -EINVAL;
}

/**
 * performs decoding of a string encoded with __pmUrlEncode()
 * (application/x-www-form-urlencoded)
 *
 * The decoded string will be stored in *outp. It is the responsibility of the
 * caller to free this memory block (unless an error occured, then the function
 * frees this memory block).
 *
 * Returns 0 on success, negative error codes on error
 */
int
__pmUrlDecode(const char *inp, const size_t len, char **outp)
{
    char		escape[4] = {0};
    const char		*inp_end = inp + len;
    char		*out, *out_start;
    unsigned int	c;

    out_start = out = malloc(len + 1);
    if (out == NULL)
	return -ENOMEM;

    for (; inp < inp_end; out++) {
	c = *inp;
	if (c == '+')
	    c = ' ';
	if (c != '%') {
	    inp++;
	}
	else {
	    if (inp + 3 > inp_end)
		goto invalid_input;
	    inp++;	/* move past percent character */
	    if (!ishex(*inp++) || !ishex(*inp++))
		goto invalid_input;
	    escape[0] = *(inp - 2);
	    escape[1] = *(inp - 1);
	    // escape is initialized with 4 null bytes, and only the first
	    // two bytes get overwritten, rendering this usage of sscanf safe
	    if (sscanf(escape, "%2x", &c) != 1)
		goto invalid_input;
	}

	*out = c;
    }
    *out = '\0';
    *outp = out_start;
    return 0;

invalid_input:
    free(out_start);
    return -EINVAL;
}
