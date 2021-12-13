/*
 * Copyright (c) 2020-2021 Red Hat.  All Rights Reserved.
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
#include "util.h"

static __pmHashCtl	valuesctl;	/* pointers to values in pmResult */
static __pmHashCtl	descsctl;	/* metric descs from pmLookupDesc */

int
values_hash(pmResult *result)
{
    unsigned int	i;
    pmValueSet		*vp;
    int			sts;

    if ((sts = __pmHashPreAlloc(result->numpmid, &valuesctl)) < 0)
	return sts;

    for (i = 0; i < result->numpmid; i++) {
	vp = result->vset[i];
	if ((sts = __pmHashAdd(vp->pmid, vp, &valuesctl)) < 0)
	    return sts;
    }
    return result->numpmid;
}

pmValueSet *
metric_values(pmID pmid)
{
    __pmHashNode	*node;

    if (pmid == PM_IN_NULL)
	return NULL;
    if ((node = __pmHashSearch(pmid, &valuesctl)) == NULL)
	return NULL;
    return (pmValueSet *)node->data;
}

int
descs_hash(int numpmid, pmDesc *descs)
{
    unsigned int	i;
    pmDesc		*dp;
    int			sts;

    if ((sts = __pmHashPreAlloc(numpmid, &descsctl)) < 0)
	return sts;

    for (i = 0; i < numpmid; i++) {
	dp = &descs[i];
	if ((sts = __pmHashAdd(dp->pmid, dp, &descsctl)) < 0)
	    return sts;
    }
    return numpmid;
}

pmDesc *
metric_desc(pmID pmid)
{
    __pmHashNode	*node;

    if (pmid == PM_IN_NULL)
	return NULL;
    if ((node = __pmHashSearch(pmid, &descsctl)) == NULL)
	return NULL;
    return (pmDesc *)node->data;
}

int
number_equal(double value, double given)
{
    return value == given;
}

int
number_nequal(double value, double given)
{
    if (value != given)
	return -1;
    return 0;
}

int
number_greater(double value, double given)
{
    return value > given;
}

int
number_gtequal(double value, double given)
{
    return value >= given;
}

int
number_lessthan(double value, double given)
{
    return value < given;
}

int
number_ltequal(double value, double given)
{
    return value <= given;
}

int
string_equal(const char *value, const char *given)
{
    size_t	value_length = strlen(value);
    size_t	given_length = strlen(given);

    if (given_length > 1 && given[0] == '"' && given[given_length-1] == '"' &&
	given_length == value_length + 2)	/* is given quoted? strip */
	return strncmp(value, given + 1, value_length) == 0;
    return strcmp(value, given) == 0;
}

int
string_nequal(const char *value, const char *given)
{
    size_t	value_length = strlen(value);
    size_t	given_length = strlen(given);

    if (given_length > 1 && given[0] == '"' && given[given_length-1] == '"' &&
	given_length == value_length + 2) {	/* is given quoted? strip */
	if (strncmp(value, given + 1, value_length) != 0)
	    return -1;
	return 0;
    }
    if (strcmp(value, given) != 0)
	return -1;
    return 0;
}

int
string_greater(const char *value, const char *given)
{
    size_t	value_length = strlen(value);
    size_t	given_length = strlen(given);

    if (given_length > 1 && given[0] == '"' && given[given_length-1] == '"' &&
	given_length == value_length + 2)	/* is given quoted? strip */
	return strncmp(value, given + 1, value_length) > 0;
    return strcmp(value, given) > 0;
}

int
string_gtequal(const char *value, const char *given)
{
    size_t	value_length = strlen(value);
    size_t	given_length = strlen(given);

    if (given_length > 1 && given[0] == '"' && given[given_length-1] == '"' &&
	given_length == value_length + 2)	/* is given quoted? strip */
	return strncmp(value, given + 1, value_length) >= 0;
    return strcmp(value, given) >= 0;
}

int
string_lessthan(const char *value, const char *given)
{
    size_t	value_length = strlen(value);
    size_t	given_length = strlen(given);

    if (given_length > 1 && given[0] == '"' && given[given_length-1] == '"' &&
	given_length == value_length + 2)	/* is given quoted? strip */
	return strncmp(value, given + 1, value_length) < 0;
    return strcmp(value, given) < 0;
}

int
string_ltequal(const char *value, const char *given)
{
    size_t	value_length = strlen(value);
    size_t	given_length = strlen(given);

    if (given_length > 1 && given[0] == '"' && given[given_length-1] == '"' &&
	given_length == value_length + 2)	/* is given quoted? strip */
	return strncmp(value, given + 1, value_length) <= 0;
    return strcmp(value, given) <= 0;
}

int
string_regexp(const regex_t *regex, const char *given)
{
    size_t	given_length = strlen(given);
    char	buf[1024];

    if (given_length > 1 && given[0] == '"' && given[given_length-1] == '"') {
	pmsprintf(buf, sizeof(buf), "%.*s", (int)given_length - 2, given + 1);
	return regexec(regex, buf, 0, NULL, 0) == 0;
    }
    return regexec(regex, given, 0, NULL, 0) == 0;
}

int
string_nregexp(const regex_t *regex, const char *given)
{
    size_t	given_length = strlen(given);
    char	buf[1024];

    if (given_length > 1 && given[0] == '"' && given[given_length-1] == '"') {
	pmsprintf(buf, sizeof(buf), "%.*s", (int)given_length - 2, given + 1);
	if (regexec(regex, buf, 0, NULL, 0) != 0)
	    return -1;
	return 0;
    }
    if (regexec(regex, given, 0, NULL, 0) != 0)
	return -1;
    return 0;
}

void
fmt(const char *input, char *output, size_t length, int goal, int maximum,
		fmt_t callback, void *arg)
{
    const char		*line, *cut;
    unsigned int	bytes = 0;

    if ((line = cut = input) == NULL)
	return;

    /*
     * 'input' is now one long line after concatenating lines earlier
     * on during pre-processing.  Split it based on a goal length and
     * a maximum length, in a very simplified fmt(1)-alike fashion.
     */
    while (*cut != '\0') {
	if (isspace((int)*line)) {	/* skip whitespace at start of line */
	    cut = ++line;
	    continue;
	}
	bytes = ++cut - line;
	if (bytes <= goal)	/* too early, keep scanning */
	    continue;
	if (bytes < maximum && !isspace((int)*cut))
	    continue;
	callback(line, bytes, arg);
	line = cut;		/* start on the next line */
    }
    if (line != cut)		/* partial line remaining */
	callback(line, strlen(line), arg);
}

int
istoken(const char *input, const char *token, size_t length)
{
    if (strncmp(input, token, length) != 0)
	return 0;
    return input[length] == '\0' || isspace((int)input[length]);
}

char *
chop(char *input)
{
    char	*p = strrchr(input, '\n');

    while (p && *p == '\n')
	*p-- = '\0';
    return input;
}

char *
trim(const char *input)
{
    const char	*p = input;

    while (isspace((int)*p) && *p != '\n')
	p++;
    return (char *)p;
}

char *
copy_string(const char *input)
{
    const char	*end = input;

    while (*end != '\n' && *end != '\0')
	end++;
    return strndup(input, end - input);
}

char *
copy_string_raw(const char *input)
{
    const char	*end = input;

    while (*end != '\0')
	end++;
    return strndup(input, end - input);
}

char *
append(char *string, const char *input, int trailer)
{
    const char	*end = input;
    size_t	length;
    char	tail[2] = { (char)trailer, '\0'};
    char	*s;

    if (string == NULL) {
	if (trailer != '\n')
	    return copy_string(input);
	return copy_string_raw(input);
    }

    while (*end != '\n' && *end != '\0')
	end++;
    length = end - input + 2;
    if (trailer)
	length++;
    if (!(s = realloc(string, strlen(string) + length)))
	return string;
    if (trailer && *end != (char)trailer)
	s = strcat(s, tail);
    return strcat(s, input);
}

char *
copy_token(const char *input)
{
    const char	*end = input;

    while (!isspace((int)*end) && *end != '\0')
	end++;
    return strndup(input, end - input);
}

const char *
operandstr(unsigned int operand)
{
    switch (operand) {
    case PROBE_EQ:	return EQUAL;
    case PROBE_NEQ:	return NOTEQUAL;
    case PROBE_GT:	return GREATER;
    case PROBE_GE:	return GTEQUAL;
    case PROBE_LT:	return LESSER;
    case PROBE_LE:	return LTEQUAL;
    case PROBE_RE:	return REGEXP;
    case PROBE_NRE:	return NOTREGEXP;
    default:		break;
    }
    return NULL;
}

const char *
loggingstr(unsigned int logging)
{
    switch (logging) {
    case LOG_ADVISORY:	return "advisory";
    case LOG_MANDATORY:	return "mandatory";
    default:		break;
    }
    return NULL;
}
