/*
 * Copyright (c) 2020 Red Hat.
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
#ifndef PMLOGCONF_UTIL_H
#define PMLOGCONF_UTIL_H

#include "pmapi.h"
#include "libpcp.h"
#include <ctype.h>
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif
#include <sys/stat.h>

extern int istoken(const char *, const char *, size_t);
extern char *trim(const char *);
extern char *chop(char *);
extern char *copy_string(const char *);
extern char *append(char *, const char *, int);
extern char *copy_token(const char *);

typedef void (*fmt_t)(const char *, int, void *);
extern void fmt(const char *, char *, size_t, int, int, fmt_t, void *);

extern int values_hash(pmResult *);
extern pmValueSet *metric_values(pmID);

extern int descs_hash(int, pmDesc *);
extern pmDesc *metric_desc(pmID);

typedef int (*numeric_cmp_t)(double, double);
extern int number_equal(double, double);
extern int number_nequal(double, double);
extern int number_greater(double, double);
extern int number_gtequal(double, double);
extern int number_lessthan(double, double);
extern int number_ltequal(double, double);

typedef int (*string_cmp_t)(const char *, const char *);
extern int string_equal(const char *, const char *);
extern int string_nequal(const char *, const char *);
extern int string_greater(const char *, const char *);
extern int string_gtequal(const char *, const char *);
extern int string_lessthan(const char *, const char *);
extern int string_ltequal(const char *, const char *);

typedef int (*regex_cmp_t)(const regex_t *, const char *);
extern int string_regexp(const regex_t *, const char *);
extern int string_nregexp(const regex_t *, const char *);

enum { STATE_AVAILABLE = 1, STATE_INCLUDE, STATE_EXCLUDE };

#define AVAILABLE	"available"
#define AVAILABLE_LEN	(sizeof(AVAILABLE)-1)
#define INCLUDE		"include"
#define INCLUDE_LEN	(sizeof(INCLUDE)-1)
#define EXCLUDE		"exclude"
#define EXCLUDE_LEN	(sizeof(EXCLUDE)-1)
#define EXISTS		"exists"
#define EXISTS_LEN	(sizeof(EXISTS)-1)
#define VALUES		"values"
#define VALUES_LEN	(sizeof(VALUES)-1)

enum { PROBE_EXISTS = 1, PROBE_VALUES,
       PROBE_EQ, PROBE_NEQ, PROBE_GT, PROBE_GE,
       PROBE_LT, PROBE_LE, PROBE_RE, PROBE_NRE
};

#define EQUAL			"=="
#define EQUAL_LEN		(sizeof(EQUAL)-1)
#define NOTEQUAL		"!="
#define NOTEQUAL_LEN		(sizeof(NOTEQUAL)-1)
#define GREATER			">"
#define GREATER_LEN		(sizeof(GREATER)-1)
#define GTEQUAL			">="
#define GTEQUAL_LEN		(sizeof(GTEQUAL)-1)
#define LESSER			"<"
#define LESSER_LEN		(sizeof(LESSER)-1)
#define LTEQUAL			"<="
#define LTEQUAL_LEN		(sizeof(LTEQUAL)-1)
#define REGEXP			"~"
#define REGEXP_LEN		(sizeof(REGEXP)-1)
#define NOTREGEXP		"!~"
#define NOTREGEXP_LEN		(sizeof(NOTREGEXP)-1)

extern const char *operandstr(unsigned int);

enum { LOG_ADVISORY, LOG_MANDATORY };

extern const char *loggingstr(unsigned int);

#endif
