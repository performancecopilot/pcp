/*
 * Copyright (c) 2018 Red Hat.
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
#ifndef PMPROXY_PCP_H
#define PMPROXY_PCP_H

typedef enum {
    PCP_PROXY_UNKNOWN	= 0,
    PCP_PROXY_HEADER	= 1,
    PCP_PROXY_HOSTSPEC	= 2,
    PCP_PROXY_CONNECT	= 3,
    PCP_PROXY_SETUP	= 4,
} pcp_proxy_state;

#endif /* PMPROXY_PCP_H */
