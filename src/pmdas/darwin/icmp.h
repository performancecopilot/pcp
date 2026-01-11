/*
 * ICMP protocol statistics types
 * Copyright (c) 2026 Red Hat.
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

/*
 * ICMP statistics structure
 * Tracks ICMP protocol statistics via net.inet.icmp.stats sysctl
 */
typedef struct icmpstats {
    __uint64_t	inmsgs;			/* total input ICMP messages */
    __uint64_t	outmsgs;		/* total output ICMP messages */
    __uint64_t	inerrors;		/* input errors (bad code, short, checksum, etc.) */
    __uint64_t	indestunreachs;		/* input destination unreachable */
    __uint64_t	inechos;		/* input echo requests */
    __uint64_t	inechoreps;		/* input echo replies */
    __uint64_t	outechos;		/* output echo requests */
    __uint64_t	outechoreps;		/* output echo replies */
} icmpstats_t;

extern int refresh_icmp(icmpstats_t *);
extern int fetch_icmp(unsigned int, pmAtomValue *);
