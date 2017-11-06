/*
 * CAVEAT
 * 	The interfaces defined in this header file are provided by
 * 	libpcp, but unlike those in pmapi.h, these ones are not
 * 	guaranteed to remain fixed across PCP releases.
 *
 * Copyright (c) 2012-2017 Red Hat.
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef PCP_LIBPCP_H
#define PCP_LIBPCP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internally, this is how to decode a PMID!
 * - flag is to denote state internally in some operations
 * - domain is usually the unique domain number of a PMDA, but see
 *   below for some special cases
 * - cluster and item together uniquely identify a metric within a domain
 */
typedef struct {
#ifdef HAVE_BITFIELDS_LTOR
	unsigned int	flag : 1;
	unsigned int	domain : 9;
	unsigned int	cluster : 12;
	unsigned int	item : 10;
#else
	unsigned int	item : 10;
	unsigned int	cluster : 12;
	unsigned int	domain : 9;
	unsigned int	flag : 1;
#endif
} __pmID_int;

#endif /* PCP_LIBPCP_H */
