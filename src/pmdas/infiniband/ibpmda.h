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
#include "libpcp.h"
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
int ib_store(pmdaResult *, pmdaExt *);

#define IB_HCA_INDOM    0
#define IB_PORT_INDOM   1
#define IB_CNT_INDOM    2

#define IB_PORTINFO_UPDATE	0x1
#define IB_HCA_PERF_UPDATE	0x2
#define IB_SWITCH_PERF_UPDATE	0x8
#define IB_CM_STATS_UPDATE	0x16

#define ARRAYSZ(a) (sizeof(a)/sizeof(a[0]))

#define METRIC_ib_hca_type                     0
#define METRIC_ib_hca_ca_type                  1 
#define METRIC_ib_hca_numports                 2
#define METRIC_ib_hca_fw_ver                   3
#define METRIC_ib_hca_hw_ver                   4
#define METRIC_ib_hca_node_guid                5
#define METRIC_ib_hca_system_guid              6
#define METRIC_ib_port_guid                    7 
#define METRIC_ib_port_gid_prefix              8
#define METRIC_ib_port_lid                     9
#define METRIC_ib_port_state                   10 
#define METRIC_ib_port_phystate                11
#define METRIC_ib_port_rate                    12
#define METRIC_ib_port_capabilities            13
#define METRIC_ib_port_linkspeed               14
#define METRIC_ib_port_linkwidth               15

#define METRIC_ib_hca_transport                16
#define METRIC_ib_hca_board_id                 17
#define METRIC_ib_hca_vendor_id                18

#define METRIC_ib_hca_res_pd                   19
#define METRIC_ib_hca_res_cq                   20
#define METRIC_ib_hca_res_qp                   21
#define METRIC_ib_hca_res_cm_id                22
#define METRIC_ib_hca_res_mr                   23
#define METRIC_ib_hca_res_ctx                  24
#define METRIC_ib_hca_res_srq                  25

#define METRIC_ib_port_sm_lid                  26
#define METRIC_ib_port_lmc                     27
#define METRIC_ib_port_max_mtu                 28
#define METRIC_ib_port_active_mtu              29
#define METRIC_ib_port_link_layer              30
#define METRIC_ib_port_netdev_name             31
#define METRIC_ib_port_capmask					32
#define METRIC_ib_port_node_desc             	33

// Bandwidth
// #define METRIC_ib_port_rx_bw             32
// #define METRIC_ib_port_tx_bw             33

/* Per-port performance counters, cluster #1 */
#define METRIC_ib_port_in_bytes                 0
#define METRIC_ib_port_in_packets               1
#define METRIC_ib_port_out_bytes                2
#define METRIC_ib_port_out_packets              3
#define METRIC_ib_port_in_errors_drop           4
#define METRIC_ib_port_out_errors_drop          5
#define METRIC_ib_port_total_bytes              6
#define METRIC_ib_port_total_packets            7
#define METRIC_ib_port_total_errors_drop        8
#define METRIC_ib_port_in_errors_filter         9
#define METRIC_ib_port_in_errors_local         10
#define METRIC_ib_port_in_errors_remote        11
#define METRIC_ib_port_out_errors_filter       12
#define METRIC_ib_port_total_errors_filter     13
#define METRIC_ib_port_total_errors_link       14
#define METRIC_ib_port_total_errors_recover    15
#define METRIC_ib_port_total_errors_integrity  16
#define METRIC_ib_port_total_errors_vl15       17
#define METRIC_ib_port_total_errors_overrun    18
#define METRIC_ib_port_total_errors_symbol     19

#define METRIC_ib_port_select					20
#define METRIC_ib_port_counter_select     		21

#define METRIC_ib_port_in_upkts					22
#define METRIC_ib_port_in_mpkts					23

#define METRIC_ib_port_out_upkts				24
#define METRIC_ib_port_out_mpkts				25

#define METRIC_ib_port_out_data                26
#define METRIC_ib_port_in_data 	               27

/* Control metrics */
#define METRIC_ib_control_query_timeout         0
#define METRIC_ib_control_hiwat                 1

/* Per-port switch performance counters, cluster #3 */
#define METRIC_ib_port_switch_in_bytes          0
#define METRIC_ib_port_switch_in_packets        1
#define METRIC_ib_port_switch_out_bytes         2
#define METRIC_ib_port_switch_out_packets       3
#define METRIC_ib_port_switch_total_bytes       4
#define METRIC_ib_port_switch_total_packets     5

// Received duplicates metrics
#define METRIC_ib_cm_rx_duplicates_apr          1
#define METRIC_ib_cm_rx_duplicates_drep         2
#define METRIC_ib_cm_rx_duplicates_dreq         3
#define METRIC_ib_cm_rx_duplicates_lap          4
#define METRIC_ib_cm_rx_duplicates_mra          5
#define METRIC_ib_cm_rx_duplicates_rej          6
#define METRIC_ib_cm_rx_duplicates_rep          7
#define METRIC_ib_cm_rx_duplicates_req          8
#define METRIC_ib_cm_rx_duplicates_rtu          9
#define METRIC_ib_cm_rx_duplicates_sidr_rep    10
#define METRIC_ib_cm_rx_duplicates_sidr_req    11

// Received messages metrics
#define METRIC_ib_cm_rx_msgs_apr                12
#define METRIC_ib_cm_rx_msgs_drep               13
#define METRIC_ib_cm_rx_msgs_dreq               14
#define METRIC_ib_cm_rx_msgs_lap                15
#define METRIC_ib_cm_rx_msgs_mra                16
#define METRIC_ib_cm_rx_msgs_rej                17
#define METRIC_ib_cm_rx_msgs_rep                18
#define METRIC_ib_cm_rx_msgs_req                19
#define METRIC_ib_cm_rx_msgs_rtu                20
#define METRIC_ib_cm_rx_msgs_sidr_rep           21
#define METRIC_ib_cm_rx_msgs_sidr_req           22

// Transmitted messages metrics
#define METRIC_ib_cm_tx_msgs_apr                23
#define METRIC_ib_cm_tx_msgs_drep               24
#define METRIC_ib_cm_tx_msgs_dreq               25
#define METRIC_ib_cm_tx_msgs_lap                26
#define METRIC_ib_cm_tx_msgs_mra                27
#define METRIC_ib_cm_tx_msgs_rej                28
#define METRIC_ib_cm_tx_msgs_rep                29
#define METRIC_ib_cm_tx_msgs_req                30
#define METRIC_ib_cm_tx_msgs_rtu                31
#define METRIC_ib_cm_tx_msgs_sidr_rep           32
#define METRIC_ib_cm_tx_msgs_sidr_req           33

// Transmitted retries metrics
#define METRIC_ib_cm_tx_retries_apr             34
#define METRIC_ib_cm_tx_retries_drep            35
#define METRIC_ib_cm_tx_retries_dreq            36
#define METRIC_ib_cm_tx_retries_lap             37
#define METRIC_ib_cm_tx_retries_mra             38
#define METRIC_ib_cm_tx_retries_rej             39
#define METRIC_ib_cm_tx_retries_rep             40
#define METRIC_ib_cm_tx_retries_req             41
#define METRIC_ib_cm_tx_retries_rtu             42
#define METRIC_ib_cm_tx_retries_sidr_rep        43
#define METRIC_ib_cm_tx_retries_sidr_req        44

// diag counters
#define METRIC_ib_port_diag_rq_num_dup          45
#define METRIC_ib_port_diag_rq_num_lle          46
#define METRIC_ib_port_diag_rq_num_lpe          47
#define METRIC_ib_port_diag_rq_num_lqpoe        48
#define METRIC_ib_port_diag_rq_num_oos          49
#define METRIC_ib_port_diag_rq_num_rae          50
#define METRIC_ib_port_diag_rq_num_rire         51
#define METRIC_ib_port_diag_rq_num_rnr          52
#define METRIC_ib_port_diag_rq_num_wrfe         53

#define METRIC_ib_port_diag_sq_num_bre          54
#define METRIC_ib_port_diag_sq_num_lle          55
#define METRIC_ib_port_diag_sq_num_lpe          56
#define METRIC_ib_port_diag_sq_num_lqpoe        57
#define METRIC_ib_port_diag_sq_num_mwbe         58
#define METRIC_ib_port_diag_sq_num_oos          59
#define METRIC_ib_port_diag_sq_num_rae          60
#define METRIC_ib_port_diag_sq_num_rire         61
#define METRIC_ib_port_diag_sq_num_rnr          62
#define METRIC_ib_port_diag_sq_num_roe          63
#define METRIC_ib_port_diag_sq_num_rree         64
#define METRIC_ib_port_diag_sq_num_to           65
#define METRIC_ib_port_diag_sq_num_tree         66
#define METRIC_ib_port_diag_sq_num_wrfe         67

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
	IBPMDA_XMT_DATA,
	IBPMDA_RCV_DATA,
	IBPMDA_XMT_PKTS,
	IBPMDA_RCV_PKTS,
	IBPMDA_PORT_SELECT,
	IBPMDA_COUNTER_SELECT,
	IBPMDA_RCV_UPKTS,
	IBPMDA_RCV_MPKTS,
	IBPMDA_XMT_UPKTS,
	IBPMDA_XMT_MPKTS
};

#endif /* _IBPMDA_H */
