/*
 * Copyright (c) 2016 Red Hat.
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
#ifndef http_client_h
#define http_client_h

#include "http_parser.h"

/* Flag values for http_client.flags field */
enum {
    F_LOCATION		= 1 << 0,
    F_REDIRECTED	= 1 << 1,
    F_DISCONNECT	= 1 << 2,
    F_CONTENT_TYPE	= 1 << 3,
    F_MESSAGE_END	= 1 << 4,
};

typedef struct http_client {
    int			fd;		/* connection to server, or -1 */
    int			error_code;
    struct timeval	timeout;
    const char		*user_agent;
    const char		*agent_vers;
    unsigned int	flags;
    unsigned int	max_redirect;
    http_protocol	http_version;
    http_parser_url	parser_url;
    char		*url;		/* copy of user URL / redirected */
    http_parser		parser;
    char		*body_buffer;	/* user-supplied result buffer */
    size_t		body_length;	/* full length of that buffer */
    size_t		offset;		/* buffer written up to here */
    char		*type_buffer;	/* optional content type buffer */
    size_t		type_length;	/* full length of that buffer */
} http_client;

#endif
