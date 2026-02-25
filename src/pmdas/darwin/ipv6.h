/*
 * IPv6 protocol statistics types
 * Copyright (c) 2026 Red Hat, Paul Smith.
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
#include <sys/types.h>
#include "pmapi.h"

/*
 * IPv6 statistics structure
 * Tracks IPv6 protocol statistics via net.inet6.ip6.stats sysctl
 */
typedef struct ipv6stats {
    uint64_t	inreceives;		/* total input IPv6 packets */
    uint64_t	outforwarded;		/* packets forwarded */
    uint64_t	indiscards;		/* input packets discarded */
    uint64_t	outdiscards;		/* output packets discarded */
    uint64_t	fragcreates;		/* output fragments created */
    uint64_t	reasmoks;		/* packets reassembled ok */
} ipv6stats_t;

extern int refresh_ipv6(ipv6stats_t *);
extern int fetch_ipv6(unsigned int, pmAtomValue *);
