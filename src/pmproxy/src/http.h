/*
 * Copyright (c) 2019-2020 Red Hat.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#ifndef PMPROXY_HTTP_H
#define PMPROXY_HTTP_H

#include "http_parser.h"

struct dict;
struct proxy;
struct client;
struct servlet;

typedef enum json_flags {
    JSON_FLAG_ARRAY	= (1<<0),
    JSON_FLAG_OBJECT	= (1<<1),
} json_flags_t;

extern sds json_push_suffix(sds, json_flags_t);
extern sds json_pop_suffix(sds);
extern sds json_string(const sds);

typedef enum http_flags {
    HTTP_FLAG_JSON	= (1<<0),
    HTTP_FLAG_TEXT	= (1<<1),
    HTTP_FLAG_HTML	= (1<<2),
    HTTP_FLAG_UTF8	= (1<<10),
    HTTP_FLAG_UTF16	= (1<<11),
    HTTP_FLAG_NO_BODY	= (1<<12),
    HTTP_FLAG_COMPRESS_GZIP	= (1<<14),
    HTTP_FLAG_STREAMING	= (1<<15),
    /* maximum 16 for server.h */
} http_flags_t;

typedef enum http_options {
    HTTP_OPT_GET	= (1 << HTTP_GET),
    HTTP_OPT_PUT	= (1 << HTTP_PUT),
    HTTP_OPT_HEAD	= (1 << HTTP_HEAD),
    HTTP_OPT_POST	= (1 << HTTP_POST),
    HTTP_OPT_TRACE	= (1 << HTTP_TRACE),
    HTTP_OPT_OPTIONS	= (1 << HTTP_OPTIONS),
    /* maximum 16 in command opts fields */
} http_options_t;

#define HTTP_COMMON_OPTIONS (HTTP_OPT_HEAD | HTTP_OPT_TRACE | HTTP_OPT_OPTIONS)
#define HTTP_OPTIONS_GET    (HTTP_COMMON_OPTIONS | HTTP_OPT_GET)
#define HTTP_OPTIONS_PUT    (HTTP_COMMON_OPTIONS | HTTP_OPT_PUT)
#define HTTP_OPTIONS_POST   (HTTP_COMMON_OPTIONS | HTTP_OPT_POST)
#define HTTP_SERVER_OPTIONS (HTTP_OPTIONS_GET | HTTP_OPT_PUT | HTTP_OPT_POST)

typedef unsigned int http_code_t;

extern void http_transfer(struct client *);
extern void http_reply(struct client *, sds, http_code_t, http_flags_t, http_options_t);
extern void http_error(struct client *, http_code_t, const char *);

extern int http_parameters(const char *, size_t, dict **);
extern int http_decode(const char *, size_t, sds);
extern const char *http_status_mapping(http_code_t);
extern const char *http_content_type(http_flags_t);

extern sds http_get_buffer(struct client *);
extern void http_set_buffer(struct client *, sds, http_flags_t);

typedef void (*httpSetupCallBack)(struct proxy *);
typedef void (*httpCloseCallBack)(struct proxy *);
typedef int (*httpHeadersCallBack)(struct client *, struct dict *);
typedef int (*httpUrlCallBack)(struct client *, sds, struct dict *);
typedef int (*httpBodyCallBack)(struct client *, const char *, size_t);
typedef int (*httpDoneCallBack)(struct client *);
typedef void (*httpReleaseCallBack)(struct client *);

typedef struct servlet {
    const char * const	name;
    struct servlet	*next;
    httpSetupCallBack	setup;
    httpCloseCallBack	close;
    httpUrlCallBack	on_url;
    httpHeadersCallBack	on_headers;
    httpBodyCallBack	on_body;
    httpDoneCallBack	on_done;
    httpReleaseCallBack	on_release;
} servlet_t;

extern struct servlet pmsearch_servlet;
extern struct servlet pmseries_servlet;
extern struct servlet pmwebapi_servlet;

#endif /* PMPROXY_HTTP_H */
