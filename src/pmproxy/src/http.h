/*
 * Copyright (c) 2019 Red Hat.
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
} json_flags;

extern sds json_push_suffix(sds, json_flags);
extern sds json_pop_suffix(sds);

typedef enum http_flags {
    HTTP_FLAG_JSON	= (1<<0),
    HTTP_FLAG_TEXT	= (1<<1),
    HTTP_FLAG_HTML	= (1<<2),
    HTTP_FLAG_JS	= (1<<3),
    HTTP_FLAG_CSS	= (1<<4),
    HTTP_FLAG_ICO	= (1<<5),
    HTTP_FLAG_JPG	= (1<<6),
    HTTP_FLAG_PNG	= (1<<7),
    HTTP_FLAG_GIF	= (1<<8),
    HTTP_FLAG_UTF8	= (1<<10),
    HTTP_FLAG_UTF16	= (1<<11),
    HTTP_FLAG_COMPRESS	= (1<<14),
    HTTP_FLAG_STREAMING	= (1<<15),
    /* maximum 16 for server.h */
} http_flags;

typedef unsigned int http_code;

extern void http_transfer(struct client *);
extern void http_reply(struct client *, sds, http_code, http_flags);
extern void http_error(struct client *, http_code, const char *);
extern void http_close(struct client *);

extern int http_decode(const char *, size_t, sds);
extern const char *http_status_mapping(http_code);
extern const char *http_content_type(http_flags);
extern http_flags http_suffix_type(const char *);

extern sds http_get_buffer(struct client *);
extern void http_set_buffer(struct client *, sds, http_flags);

typedef void (*httpSetupCallBack)(struct proxy *);
typedef void (*httpCloseCallBack)(void);
typedef int (*httpHeadersCallBack)(struct client *, struct dict *);
typedef int (*httpUrlCallBack)(struct client *, sds, struct dict *);
typedef int (*httpBodyCallBack)(struct client *, const char *, size_t);
typedef int (*httpDoneCallBack)(struct client *);

typedef struct servlet {
    const char * const	name;
    struct servlet	*next;
    httpSetupCallBack	setup;
    httpCloseCallBack	close;
    httpUrlCallBack	on_url;
    httpHeadersCallBack	on_headers;
    httpBodyCallBack	on_body;
    httpDoneCallBack	on_done;
} servlet;

#endif /* PMPROXY_HTTP_H */
