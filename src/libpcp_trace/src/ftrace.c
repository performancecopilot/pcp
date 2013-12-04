/*
 * ftrace.c - Fortran front-end to the libpcp_trace entry points
 *
 * Copyright (c) 1997-2000 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "trace.h"
#include "trace_dev.h"

int
pmtracebegin_(const char *tag, int tag_len)
{
    char	*tmp = NULL;
    int		sts;

    if ((tmp = malloc(tag_len + 1)) == NULL)
	return -oserror();
    strncpy(tmp, tag, tag_len);
    tmp[tag_len] = '\0';
    sts = pmtracebegin(tmp);
    free(tmp);
    return sts;
}

int
pmtraceend_(const char *tag, int tag_len)
{
    char	*tmp = NULL;
    int		sts;

    if ((tmp = malloc(tag_len + 1)) == NULL)
	return -oserror();
    strncpy(tmp, tag, tag_len);
    tmp[tag_len] = '\0';
    sts = pmtraceend(tmp);
    free(tmp);
    return sts;
}

int
pmtraceabort_(const char *tag, int tag_len)
{
    char	*tmp = NULL;
    int		sts;

    if ((tmp = malloc(tag_len + 1)) == NULL)
	return -oserror();
    strncpy(tmp, tag, tag_len);
    tmp[tag_len] = '\0';
    sts = pmtraceabort(tmp);
    free(tmp);
    return sts;
}

int
pmtracepoint_(const char *tag, int tag_len)
{
    char	*tmp = NULL;
    int		sts;

    if ((tmp = malloc(tag_len + 1)) == NULL)
	return -oserror();
    strncpy(tmp, tag, tag_len);
    tmp[tag_len] = '\0';
    sts = pmtracepoint(tmp);
    free(tmp);
    return sts;
}

int
pmtracecounter_(const char *tag, double *value, int tag_len)
{
    char	*tmp = NULL;
    int		sts;

    if ((tmp = malloc(tag_len + 1)) == NULL)
	return -oserror();
    strncpy(tmp, tag, tag_len);
    tmp[tag_len] = '\0';
    sts = pmtracecounter(tmp, *value);
    free(tmp);
    return sts;
}

int
#ifdef __GNUC__
pmtraceobs_(const char *tag, int tag_len, double *value)
#else
pmtraceobs_(const char *tag, double *value, int tag_len)
#endif
{
    char	*tmp = NULL;
    int		sts;

    if ((tmp = malloc(tag_len + 1)) == NULL)
	return -oserror();
    strncpy(tmp, tag, tag_len);
    tmp[tag_len] = '\0';
    sts = pmtraceobs(tmp, *value);
    free(tmp);
    return sts;
}

void
pmtraceerrstr_(int *code, char *msg, int msg_len)
{
    char	*tmp;
    int		len;

    tmp = pmtraceerrstr(*code);
    len = (int)strlen(tmp);
    len = (len < msg_len ? len : msg_len);

    strncpy(msg, tmp, len);
    for (; len < msg_len; len++)	/* blank fill */
	msg[len-1] = ' ';
}

int
pmtracestate_(int *code)
{
    return pmtracestate(*code);
}
