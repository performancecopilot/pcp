/*
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

#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

#include "ibpmda.h"

/*
 * Metric Table
 */
pmdaMetric metrictab[] = {
    /* ib.hca.type */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_type),
	 PM_TYPE_STRING, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.hca.ca_type */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_ca_type),
	 PM_TYPE_STRING, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.hca.numports */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_numports),
	 PM_TYPE_32, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.hca.fw_ver */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_fw_ver),
	 PM_TYPE_STRING, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.hca.hw_ver */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_hw_ver),
	 PM_TYPE_STRING, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.hca.node_guid */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_node_guid),
	 PM_TYPE_U64, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.hca.system_guid */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_hca_system_guid),
	 PM_TYPE_U64, IB_HCA_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.port.guid */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_guid),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
         PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.port.gid_prefix */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_gid_prefix),
	 PM_TYPE_U64, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.port.lid */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_lid),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.port.state */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_state),
	 PM_TYPE_STRING, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.port.phystate */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_phystate),
	 PM_TYPE_STRING, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.port.rate */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_rate),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.port.capabilities */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_capabilities),
	 PM_TYPE_STRING, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.port.linkspeed */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_linkspeed),
	 PM_TYPE_STRING, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.port.linkwidth */
    { NULL,
	{PMDA_PMID(0,METRIC_ib_port_linkwidth),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* ib.port.in.bytes */
    { (void *)IBPMDA_RCV_BYTES,
	{PMDA_PMID(1,METRIC_ib_port_in_bytes),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

    /* ib.port.in.packets */
    { (void *)IBPMDA_RCV_PKTS,
	{PMDA_PMID(1,METRIC_ib_port_in_packets),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) }, },

    /* ib.port.in.errors.drop */
    { (void *)IBPMDA_ERR_SWITCH_REL,
	{PMDA_PMID(1,METRIC_ib_port_in_errors_drop),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* ib.port.in.errors.filter */
    { (void *)IBPMDA_ERR_RCVCONSTR,
	{PMDA_PMID(1,METRIC_ib_port_in_errors_filter),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* ib.port.in.errors.local */
    { (void *)IBPMDA_ERR_RCV,
	{PMDA_PMID(1,METRIC_ib_port_in_errors_local),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* ib.port.in.errors.filter */
    { (void *)IBPMDA_ERR_PHYSRCV,
	{PMDA_PMID(1,METRIC_ib_port_in_errors_remote),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* ib.port.out.bytes */
    { (void *)IBPMDA_XMT_BYTES,
	{PMDA_PMID(1,METRIC_ib_port_out_bytes),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

    /* ib.port.out.packets */
    {(void *)IBPMDA_XMT_PKTS,
	{PMDA_PMID(1,METRIC_ib_port_out_packets),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) }, },

    /* ib.port.out.errors.drop */
    { (void *)IBPMDA_XMT_DISCARDS,
	{PMDA_PMID(1,METRIC_ib_port_out_errors_drop),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* ib.port.out.errors.filter */
    { (void *)IBPMDA_ERR_XMTCONSTR,
	{PMDA_PMID(1,METRIC_ib_port_out_errors_filter),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* ib.port.total.bytes */
    { (void *)-1,
	{PMDA_PMID(1,METRIC_ib_port_total_bytes),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

    /* ib.port.total.packets */
    { (void *)-1,
	{PMDA_PMID(1,METRIC_ib_port_total_packets),
	 PM_TYPE_64, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* ib.port.total.errors.drop */
    { (void *)-1,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_drop),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* ib.port.total.errors.filter */
    { (void *)-1,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_filter),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* ib.port.total.errors.link */
    { (void *)IBPMDA_LINK_DOWNED,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_link),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* ib.port.total.errors.recover */
    { (void *)IBPMDA_LINK_RECOVERS,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_recover),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* ib.port.total.errors.integrity */
    { (void *)IBPMDA_ERR_LOCALINTEG,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_integrity),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* ib.port.total.errors.vl15 */
    { (void *)IBPMDA_VL15_DROPPED,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_vl15),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* ib.port.total.errors.overrun */
    { (void *)IBPMDA_ERR_EXCESS_OVR,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_overrun),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* ib.port.total.errors.symbol */
    { (void *)IBPMDA_ERR_SYM,
	{PMDA_PMID(1,METRIC_ib_port_total_errors_symbol),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_COUNTER, 
	 PMDA_PMUNITS(0,0,1,0,0,0) } },

    /* ib.control.query_timeout */
    { NULL,
	{PMDA_PMID(2,METRIC_ib_control_query_timeout),
	 PM_TYPE_32, IB_PORT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },

    /* ib.control.hiwat */
    { NULL,
	{PMDA_PMID(2,METRIC_ib_control_hiwat),
	 PM_TYPE_U32, IB_CNT_INDOM, PM_SEM_DISCRETE, 
	 PMDA_PMUNITS(0,0,0,0,0,0) } },
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
ib_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
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
    int sep = __pmPathSeparator();
    int i;

    if (dp->status != 0)
         return;

    if (confpath == NULL) {
	snprintf(defconf, sizeof(defconf), "%s%c" "ib" "%c" "config", 
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	confpath = defconf;
    }

    for (i=0; i < ARRAYSZ(indomtab); i++) {
	__pmindom_int(&indomtab[i].it_indom)->domain = dp->domain;
	if (IB_CNT_INDOM != __pmindom_int(&indomtab[i].it_indom)->serial) {
	    pmdaCacheOp (indomtab[i].it_indom, PMDA_CACHE_LOAD);
	}
    }


    if ((dp->status = ib_load_config(confpath, writeconf, indomtab, ARRAYSZ(indomtab))))
	return;

    for (i=0; i < ARRAYSZ(indomtab); i++) {
	if (IB_CNT_INDOM != __pmindom_int(&indomtab[i].it_indom)->serial) {
	    pmdaCacheOp (indomtab[i].it_indom, PMDA_CACHE_SAVE);
	}
    }
 
    dp->version.two.fetch = ib_fetch;
    dp->version.two.store = ib_store;
    pmdaSetFetchCallBack(dp, ib_fetch_val);

    pmdaInit(dp, indomtab, ARRAYSZ(indomtab), metrictab, ARRAYSZ(metrictab));
}
