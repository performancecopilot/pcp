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

#include "ibpmda.h"

/*
 * Metric Table
 */
pmdaMetric metrictab[] = {
    /* infiniband.hca.type */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_type),
	 PM_TYPE_STRING, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.hca.ca_type */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_ca_type),
	 PM_TYPE_STRING, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.hca.numports */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_numports),
	 PM_TYPE_32, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.hca.fw_ver */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_fw_ver),
	 PM_TYPE_STRING, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.hca.hw_ver */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_hw_ver),
	 PM_TYPE_STRING, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.hca.node_guid */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_node_guid),
	 PM_TYPE_STRING, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.hca.system_guid */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_system_guid),
	 PM_TYPE_STRING, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.guid */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_guid),
	 PM_TYPE_STRING, IB_PORT_INDOM, PM_SEM_DISCRETE, 
         PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.gid_prefix */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_gid_prefix),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.lid */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_lid),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.state */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_state),
	 PM_TYPE_STRING, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.phystate */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_phystate),
	 PM_TYPE_STRING, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.rate */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_rate),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.capabilities */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_capabilities),
	 PM_TYPE_STRING, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.linkspeed */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_linkspeed),
	 PM_TYPE_STRING, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.linkwidth */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_linkwidth),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.in.bytes */
    { (void *)IBPMDA_RCV_BYTES,
	{PMDA_PMID(1,METRIC_ib_port_in_bytes),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.in.data */
    { (void *)IBPMDA_RCV_DATA,
	{PMDA_PMID(1,METRIC_ib_port_in_data),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.in.packets */
    { (void *)IBPMDA_RCV_PKTS,
	{PMDA_PMID(1,METRIC_ib_port_in_packets),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,1,0,0,0) }, },

    /* infiniband.port.select */
    { (void *)IBPMDA_PORT_SELECT,
	{PMDA_PMID(1,METRIC_ib_port_select),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

	/* infiniband.port.counter_select */
    { (void *)IBPMDA_COUNTER_SELECT,
	{PMDA_PMID(1,METRIC_ib_port_counter_select),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.in.ucast_packets */
    { (void *)IBPMDA_RCV_UPKTS,
	{PMDA_PMID(1,METRIC_ib_port_in_upkts),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.in.mcast_packets */
    { (void *)IBPMDA_RCV_MPKTS,
	{PMDA_PMID(1,METRIC_ib_port_in_mpkts),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.out.ucast_packets */
    { (void *)IBPMDA_XMT_UPKTS,
	{PMDA_PMID(1,METRIC_ib_port_out_upkts),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.out.mcast_packets */
    { (void *)IBPMDA_XMT_MPKTS,
	{PMDA_PMID(1,METRIC_ib_port_out_mpkts),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.in.errors.drop */
    { (void *)IBPMDA_ERR_SWITCH_REL,
	{PMDA_PMID(1,METRIC_ib_port_in_errors_drop),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.in.errors.filter */
    { (void *)IBPMDA_ERR_RCVCONSTR,
	{PMDA_PMID(1,METRIC_ib_port_in_errors_filter),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.in.errors.local */
    { (void *)IBPMDA_ERR_RCV,
	{PMDA_PMID(1,METRIC_ib_port_in_errors_local),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.in.errors.filter */
    { (void *)IBPMDA_ERR_PHYSRCV,
	{PMDA_PMID(1,METRIC_ib_port_in_errors_remote),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.out.bytes */
    { (void *)IBPMDA_XMT_BYTES,
	{PMDA_PMID(1,METRIC_ib_port_out_bytes),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

    /* infiniband.port.out.data */
    { (void *)IBPMDA_XMT_DATA,
	{PMDA_PMID(1,METRIC_ib_port_out_data),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.port.out.packets */
    {(void *)IBPMDA_XMT_PKTS,
	{PMDA_PMID(1,METRIC_ib_port_out_packets),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) }, },

    /* infiniband.port.out.errors.drop */
    { (void *)IBPMDA_XMT_DISCARDS,
	{PMDA_PMID(1,METRIC_ib_port_out_errors_drop),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.out.errors.filter */
    { (void *)IBPMDA_ERR_XMTCONSTR,
	{PMDA_PMID(1,METRIC_ib_port_out_errors_filter),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.total.bytes */
    { (void *)-1,
	{PMDA_PMID(1,METRIC_ib_port_total_bytes),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

    /* infiniband.port.total.packets */
    { (void *)-1,
	{PMDA_PMID(1,METRIC_ib_port_total_packets),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.total.errors.drop */
    { (void *)-1,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_drop),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.total.errors.filter */
    { (void *)-1,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_filter),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.total.errors.link */
    { (void *)IBPMDA_LINK_DOWNED,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_link),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.total.errors.recover */
    { (void *)IBPMDA_LINK_RECOVERS,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_recover),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.total.errors.integrity */
    { (void *)IBPMDA_ERR_LOCALINTEG,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_integrity),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.total.errors.vl15 */
    { (void *)IBPMDA_VL15_DROPPED,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_vl15),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.total.errors.overrun */
    { (void *)IBPMDA_ERR_EXCESS_OVR,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_overrun),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.total.errors.symbol */
    { (void *)IBPMDA_ERR_SYM,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_symbol),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.control.query_timeout */
    { NULL,
	{PMDA_PMID(2,METRIC_ib_control_query_timeout),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

    /* infiniband.control.hiwat */
    { NULL,
	{PMDA_PMID(2,METRIC_ib_control_hiwat),
	 PM_TYPE_U32, IB_CNT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

    /* infiniband.port.switch.in.bytes */
    { NULL,
	{PMDA_PMID(3,METRIC_ib_port_switch_in_bytes),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

    /* infiniband.port.switch.in.packets */
    { NULL,
	{PMDA_PMID(3,METRIC_ib_port_switch_in_packets),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.switch.out.bytes */
    { NULL,
	{PMDA_PMID(3,METRIC_ib_port_switch_out_bytes),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

    /* infiniband.port.switch.out.packets */
    { NULL,
	{PMDA_PMID(3,METRIC_ib_port_switch_out_packets),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* infiniband.port.switch.total.bytes */
    { NULL,
	{PMDA_PMID(3,METRIC_ib_port_switch_total_bytes),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

    /* infiniband.port.switch.total.packets */
    { NULL,
	{PMDA_PMID(3,METRIC_ib_port_switch_total_packets),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },
	
    /* CM rx_duplicates stats*/
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_duplicates_apr),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_duplicates_drep),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_duplicates_dreq),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_duplicates_lap),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_duplicates_mra),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_duplicates_rej),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_duplicates_rep),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_duplicates_req),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_duplicates_rtu),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_duplicates_sidr_rep),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_duplicates_sidr_req),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

    /* CM rx_msgs stats*/
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_msgs_apr),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_msgs_drep),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_msgs_dreq),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_msgs_lap),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_msgs_mra),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_msgs_rej),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_msgs_rep),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_msgs_req),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_msgs_rtu),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_msgs_sidr_rep),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_rx_msgs_sidr_req),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

    /* CM rx_msgs stats*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_msgs_apr),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_msgs_drep),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_msgs_dreq),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_msgs_lap),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_msgs_mra),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_msgs_rej),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_msgs_rep),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_msgs_req),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_msgs_rtu),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_msgs_sidr_rep),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_msgs_sidr_req),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

    /* CM tx_retries stats*/
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_retries_apr),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_retries_drep),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_retries_dreq),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_retries_lap),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_retries_mra),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_retries_rej),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_retries_rep),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_retries_req),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_retries_rtu),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_retries_sidr_rep),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,
	{PMDA_PMID(4, METRIC_ib_cm_tx_retries_sidr_req),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.rq.dup*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_rq_num_dup),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.rq.lle*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_rq_num_lle),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.rq.lpe*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_rq_num_lpe),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.rq.lqpoe*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_rq_num_lqpoe),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.rq.oos*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_rq_num_oos),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.rq.rae*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_rq_num_rae),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.rq.rire*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_rq_num_rire),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.rq.rnr*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_rq_num_rnr),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.rq.wrfe*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_rq_num_wrfe),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },


	/* infiniband.port.diag.sq.bre*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_sq_num_bre),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.sq.lle*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_sq_num_lle),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.sq.lpe*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_sq_num_lpe),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.sq.lqpoe*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_sq_num_lqpoe),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.sq.mwbe*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_sq_num_mwbe),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.sq.oos*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_sq_num_oos),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.sq.rae*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_sq_num_rae),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.sq.rire*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_sq_num_rire),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.sq.rnr*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_sq_num_rnr),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.sq.roe*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_sq_num_roe),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.sq.rree*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_sq_num_rree),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.sq.to*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_sq_num_to),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.sq.tree*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_sq_num_tree),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

	/* infiniband.port.diag.sq.wrfe*/
	{ NULL,
	{PMDA_PMID(4, METRIC_ib_port_diag_sq_num_wrfe),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

    /* infiniband.hca.transport */		
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_transport),
	 PM_TYPE_STRING, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },	

    /* infiniband.hca.board_id */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_board_id),
	 PM_TYPE_STRING, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.hca.vendor_id */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_vendor_id),
	 PM_TYPE_STRING, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* infiniband.hca.resource */
	{ NULL,
	{PMDA_PMID(0,METRIC_ib_hca_res_pd),
	 PM_TYPE_U64, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ NULL,
	{PMDA_PMID(0,METRIC_ib_hca_res_cq),
	 PM_TYPE_U64, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ NULL,
	{PMDA_PMID(0,METRIC_ib_hca_res_qp),
	 PM_TYPE_U64, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ NULL,
	{PMDA_PMID(0,METRIC_ib_hca_res_cm_id),
	 PM_TYPE_U64, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ NULL,
	{PMDA_PMID(0,METRIC_ib_hca_res_mr),
	 PM_TYPE_U64, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ NULL,
	{PMDA_PMID(0,METRIC_ib_hca_res_ctx),
	 PM_TYPE_U64, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ NULL,
	{PMDA_PMID(0,METRIC_ib_hca_res_srq),
	 PM_TYPE_U64, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

	/* infiniband.port.sm_lid*/ 
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_sm_lid),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

	/* infiniband.port.lmc*/ 
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_lmc),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

	/* infiniband.port.max_mtu*/ 
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_max_mtu),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

	/* infiniband.port.max_mtu*/ 
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_active_mtu),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

	/* infiniband.port.link_layer*/ 
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_link_layer),
	 PM_TYPE_STRING, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

	/* */ 
	{ NULL,
		{PMDA_PMID(0,METRIC_ib_port_netdev_name),
		PM_TYPE_STRING, IB_PORT_INDOM, PM_SEM_DISCRETE, 
		PMDA_PMUNITS(0,0,0,0,0,0) }, },

	/* infiniband.port.capmask */ 
	{ NULL,
		{PMDA_PMID(0,METRIC_ib_port_capmask),
		PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
	
	/* infiniband.port.node_desc */ 
	{ NULL,
		{PMDA_PMID(0,METRIC_ib_port_node_desc),
		PM_TYPE_STRING, IB_PORT_INDOM, PM_SEM_DISCRETE, 
		PMDA_PMUNITS(0,0,0,0,0,0) }, }
};

pmdaIndom indomtab[] = {
    { IB_HCA_INDOM, 0, NULL },
    { IB_PORT_INDOM, 0, NULL },
    { IB_CNT_INDOM, 0, NULL },
};

static void
foreach_inst(pmInDom indom, void (*cb)(void *state))
{
    int i;
    pmdaCacheOp (indom, PMDA_CACHE_WALK_REWIND);

    while ((i = pmdaCacheOp (indom, PMDA_CACHE_WALK_NEXT)) >= 0) {
        void *state = NULL;

        if (pmdaCacheLookup (indom, i, NULL, &state) != PMDA_CACHE_ACTIVE) {
                abort();
        }
	cb (state);
    }
}

int
ib_fetch(int numpmid, pmID pmidlist[], pmdaResult **resp, pmdaExt *pmda)
{
    int rv;

    foreach_inst (indomtab[IB_PORT_INDOM].it_indom, ib_rearm_for_update);
    rv =  pmdaFetch (numpmid, pmidlist, resp, pmda);
    foreach_inst (indomtab[IB_PORT_INDOM].it_indom, ib_reset_perfcounters);

    return (rv);
}

void
ibpmda_init(const char *confpath, int writeconf, pmdaInterface *dp)
{
    char defconf[MAXPATHLEN];
    int sep = pmPathSeparator();
    int i;

    if (dp->status != 0)
         return;
	// pmsprintf()
    if (confpath == NULL) {
	pmsprintf(defconf, sizeof(defconf), "%s%c" "infiniband" "%c" "config", 
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	// pmNotifyErr(LOG_INFO, "Path : %s\n",defconf);
	confpath = defconf;
    }

    for (i=0; i < ARRAYSZ(indomtab); i++) {
	indomtab[i].it_indom = pmInDom_build(dp->domain, pmInDom_serial(indomtab[i].it_indom));
	if (IB_CNT_INDOM != pmInDom_serial(indomtab[i].it_indom)) {
	    pmdaCacheOp (indomtab[i].it_indom, PMDA_CACHE_LOAD);
	}
    }


    if ((dp->status = ib_load_config(confpath, writeconf, indomtab, ARRAYSZ(indomtab))))
	return;

    for (i=0; i < ARRAYSZ(indomtab); i++) {
	if (IB_CNT_INDOM != pmInDom_serial(indomtab[i].it_indom)) {
	    pmdaCacheOp (indomtab[i].it_indom, PMDA_CACHE_SAVE);
	}
    }
 
    dp->version.two.fetch = ib_fetch;
    dp->version.two.store = ib_store;
    pmdaSetFetchCallBack(dp, ib_fetch_val);
	// pmNotifyErr(LOG_INFO, "sizeof metric tab %d", sizeof(metrictab));
    pmdaInit(dp, indomtab, ARRAYSZ(indomtab), metrictab, ARRAYSZ(metrictab));
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmGetProgname());
    fputs("Options:\n"
	  "  -D debug   set debug options, see pmdbg(1)\n"
          "  -d domain  use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile write log into logfile rather than using default log name\n"
	  "  -c path to configuration file\n"
	  "  -w write the basic configuration file\n",
          stderr);
    exit(1);
}

int
main(int argc, char **argv)
{
    int err = 0;
    int sep = pmPathSeparator();
    pmdaInterface dispatch;
    char helppath[MAXPATHLEN];
    char *confpath = NULL;
    int opt;
    int writeconf = 0;

    pmSetProgname(argv[0]);
    pmsprintf(helppath, sizeof(helppath), "%s%c" "infiniband" "%c" "help", 
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_3, pmGetProgname(), IB, "infiniband.log", helppath);

    while ((opt = pmdaGetOpt(argc, argv, "D:c:d:l:w?", &dispatch, &err)) != EOF) {
	switch (opt) {
	case 'c':
	    confpath = optarg;
	    break;
	case 'w':
	    writeconf = 1;
	    break;
	default:
	    err++;
	}
    }

    if (err) {
        usage();
    }

    if (!writeconf) {
	/* If writeconf is specified, then errors should go to stdout
	 * since the PMDA daemon will exit immediately after writing
	 * out the default config file
	 */
	pmdaOpenLog(&dispatch);
    }
    ibpmda_init(confpath, writeconf, &dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
