/*
 * Copyright (c) 2012-2013,2018-2019 Red Hat.
 * Copyright (c) 2002 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef PMPROXY_H
#define PMPROXY_H

typedef void *(*proxyOpenRequestPorts)(const char *, int);
typedef void (*proxyDumpRequestPorts)(FILE *, void *);
typedef void (*proxyShutdownPorts)(void *);
typedef void (*proxyMainLoop)(void *);

typedef struct pmproxy {
    proxyOpenRequestPorts	openports;
    proxyDumpRequestPorts	dumpports;
    proxyShutdownPorts		shutdown;
    proxyMainLoop		loop;
} pmproxy;

extern struct pmproxy libpcp_pmproxy;
extern struct pmproxy libuv_pmproxy;

extern void *GetServerInfo(void);
extern void Shutdown(void);

extern struct dict *config;

#endif /* PMPROXY_H */
