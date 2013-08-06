/*
 * Copyright (C) 2010 Silicon Graphics, Inc. All Rights Reserved.
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

#ifndef _IB_H
#define _IB_H

#ifdef HAVE_PORT_PERfORMANCE_QUERY_VIA
#define port_perf_query(data, dst, port, timeout, srcport) \
	port_performance_query_via(data, dst, port, timeout, srcport)
#define port_perf_reset(data, dst, port, mask, timeout, srcport) \
	port_performance_reset_via(data, dst, port, mask, timeout, srcport)
#else
#define port_perf_query(data, dst, port, timeout, srcport) \
	pma_query_via(data, dst, port, timeout, IB_GSI_PORT_COUNTERS, srcport)
#define port_perf_reset(data, dst, port, mask, timeout, srcport) \
	performance_reset_via(data, dst, port, mask, timeout, IB_GSI_PORT_COUNTERS, srcport)
#endif

#endif /* _IB_H */
