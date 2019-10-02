/*
 * Copyright (c) 2019 Red Hat.
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
 */
#include <ctype.h>
#include <stdlib.h>
#include "encoding.h"

static const char base64_decoding_table[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 62, 0, 0, 0, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
    0, 0, 0, -1, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 0, 0, 26,
    27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
    45, 46, 47, 48, 49, 50, 51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const char base64_encoding_table[] = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
};

sds
base64_decode(const char *src, size_t len)
{
    sds			result, dest;
    char		a, b, c, d;

    if (len % 4)	/* invalid base64 string length */
	return NULL;
    if ((result = dest = sdsnewlen(SDS_NOINIT, len / 4 * 3)) == NULL)
	return NULL;
    while (*src) {
	a = base64_decoding_table[(unsigned char)*(src++)];
	b = base64_decoding_table[(unsigned char)*(src++)];
	c = base64_decoding_table[(unsigned char)*(src++)];
	d = base64_decoding_table[(unsigned char)*(src++)];
	*(dest++) = (a << 2) | ((b & 0x30) >> 4);
	if (c == (char)-1)
	    break;
	*(dest++) = ((b & 0x0f) << 4) | ((c & 0x3c) >> 2);
	if (d == (char)-1)
	    break;
	*(dest++) = ((c & 0x03) << 6) | d;
    }
    *dest = '\0';
    return result;
}

sds
base64_encode(const char *src, size_t len)
{
    sds			result, dest;
    unsigned int	i, triple;
    unsigned char	a, b, c;

    if ((result = dest = sdsnewlen(SDS_NOINIT, len * 4 / 3)) == NULL)
	return NULL;
    for (i = 0; i < len; dest++) {
	a = i < len ? src[i++] : 0;
	b = i < len ? src[i++] : 0;
	c = i < len ? src[i++] : 0;
	triple = (a << 16) | (b << 8) | c;
	*(dest)++ = base64_encoding_table[(triple >> 3 * 6) & 63];
	*(dest)++ = base64_encoding_table[(triple >> 2 * 6) & 63];
	*(dest)++ = base64_encoding_table[(triple >> 1 * 6) & 63];
	*(dest)++ = base64_encoding_table[(triple >> 0 * 6) & 63];
    }
    *dest = '\0';
    return result;
}

sds
unicode_encode(const char *p, size_t length)
{
    static const char	hex[] = "0123456789ABCDEF";
    unsigned int	i;
    sds			s = sdsnewlen("\"", 1);

    for (i = 0; i < length; i++) {
	char	c = p[i];

	if (!isascii(c))
	    s = sdscatlen(s, "\\uFFFD", 6);
	else if (isalnum(c) || c == ' ' ||
		(ispunct(c) && !iscntrl(c) && c != '\\' && c != '\"'))
	    s = sdscatprintf(s, "%c", c);
	else
	    s = sdscatprintf(s, "\\u00%c%c", hex[(c>>4) & 0xf], hex[c & 0xf]);
    }
    return sdscatlen(s, "\"", 1);
}
