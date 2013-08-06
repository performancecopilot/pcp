/* 
 * Copyright (C) 2013 Red Hat.
 * Copyright (C) 2007,2008 Silicon Graphics, Inc. All Rights Reserved.
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

#ifndef _IBPMDA_H
#define _IBPMDA_H

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include "domain.h"

#ifdef HAVE_PORT_PERFORMANCE_QUERY_VIA
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


void ibpmda_init (const char *configpath, int, pmdaInterface *);

int ib_fetch_val(pmdaMetric *, unsigned int, pmAtomValue *);
int ib_load_config(const char *, int, pmdaIndom *, unsigned int);
void ib_rearm_for_update(void *);
void ib_reset_perfcounters (void *);
int ib_store(pmResult *, pmdaExt *);

#define IB_HCA_INDOM    0
#define IB_PORT_INDOM   1
#define IB_CNT_INDOM    2

#define ARRAYSZ(a) (sizeof(a)/sizeof(a[0]))

#define METRIC_ib_hca_type		0
#define METRIC_ib_hca_ca_type		1 
#define METRIC_ib_hca_numports		2
#define METRIC_ib_hca_fw_ver		3
#define METRIC_ib_hca_hw_ver		4
#define METRIC_ib_hca_node_guid		5
#define METRIC_ib_hca_system_guid	6
#define METRIC_ib_port_guid		7 
#define METRIC_ib_port_gid_prefix	8
#define METRIC_ib_port_lid		9
#define METRIC_ib_port_state		10 
#define METRIC_ib_port_phystate		11
#define METRIC_ib_port_rate		12
#define METRIC_ib_port_capabilities	13
#define METRIC_ib_port_linkspeed	14
#define METRIC_ib_port_linkwidth	15

/* Per-port performance counters, cluster #1 */
#define METRIC_ib_port_in_bytes                  0
#define METRIC_ib_port_in_packets                1
#define METRIC_ib_port_out_bytes                 2
#define METRIC_ib_port_out_packets               3
#define METRIC_ib_port_in_errors_drop            4
#define METRIC_ib_port_out_errors_drop           5
#define METRIC_ib_port_total_bytes               6
#define METRIC_ib_port_total_packets             7
#define METRIC_ib_port_total_errors_drop         8
#define METRIC_ib_port_in_errors_filter          9
#define METRIC_ib_port_in_errors_local           10
#define METRIC_ib_port_in_errors_remote          11
#define METRIC_ib_port_out_errors_filter         12
#define METRIC_ib_port_total_errors_filter       13
#define METRIC_ib_port_total_errors_link         14
#define METRIC_ib_port_total_errors_recover      15
#define METRIC_ib_port_total_errors_integrity    16
#define METRIC_ib_port_total_errors_vl15         17
#define METRIC_ib_port_total_errors_overrun      18
#define METRIC_ib_port_total_errors_symbol       19

/* Control metrics */
#define METRIC_ib_control_query_timeout	0
#define METRIC_ib_control_hiwat		1

enum ibpmd_cndid {
	IBPMDA_ERR_SYM = 0,
	IBPMDA_LINK_RECOVERS,
	IBPMDA_LINK_DOWNED,
	IBPMDA_ERR_RCV,
	IBPMDA_ERR_PHYSRCV,
	IBPMDA_ERR_SWITCH_REL,
	IBPMDA_XMT_DISCARDS,
	IBPMDA_ERR_XMTCONSTR,
	IBPMDA_ERR_RCVCONSTR,
	IBPMDA_ERR_LOCALINTEG,
	IBPMDA_ERR_EXCESS_OVR,
	IBPMDA_VL15_DROPPED,
	IBPMDA_XMT_BYTES,
	IBPMDA_RCV_BYTES,
	IBPMDA_XMT_PKTS,
	IBPMDA_RCV_PKTS
};

#endif /* _IBPMDA_H */
