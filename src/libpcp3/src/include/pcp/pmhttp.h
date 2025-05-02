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
 *
 * Simple, light-weight HTTP client interface (libpcp_web)
 */
#ifndef PCP_PMHTTP_H
#define PCP_PMHTTP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum http_protocol {
    PV_HTTP_1_0,
    PV_HTTP_1_1,
    PV_MAX
} http_protocol;

struct timeval;
struct http_client;

extern struct http_client *pmhttpNewClient(void);
extern void pmhttpFreeClient(struct http_client *);
extern int pmhttpClientFetch(struct http_client *, const char *,
                             char *, size_t, char *, size_t);
extern int pmhttpClientSetTimeout(struct http_client *, struct timeval *);
extern int pmhttpClientSetProtocol(struct http_client *, enum http_protocol);
extern int pmhttpClientSetUserAgent(struct http_client *, const char *, const char *);

#ifdef __cplusplus
}
#endif

#endif /* PCP_PMHTTP_H */
