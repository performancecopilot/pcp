/*
 * Copyright (c) 2025 Red Hat.
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
 * Mock InfiniBand library for testing the InfiniBand PMDA without hardware.
 * This library provides fake, deterministic data to exercise PMDA code paths.
 *
 * Similar to the NVIDIA mock library approach (qa/src/nvidia-ml.c), this
 * implements the umad/mad library interfaces used by the IB PMDA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pcp/pmapi.h>
#include <infiniband/umad.h>
#include <infiniband/mad.h>

/* Mock HCA configuration table */
#define NUM_HCAS 2
#define PORTS_PER_HCA 2

typedef struct mock_hca_s {
    char ca_name[UMAD_CA_NAME_LEN];
    char ca_type[20];
    char hw_ver[20];
    char fw_ver[20];
    uint64_t node_guid;
    uint64_t system_guid;
    int node_type;
    int numports;
    struct {
        int portnum;
        uint64_t port_guid;
        int base_lid;
        int sm_lid;
        int state;
        int phys_state;
        char link_layer[20];
    } ports[UMAD_CA_MAX_PORTS];
} mock_hca_t;

static mock_hca_t mock_hcas[NUM_HCAS] = {
    {
        .ca_name = "mlx5_0",
        .ca_type = "MT4099",
        .hw_ver = "0",
        .fw_ver = "12.28.2006",
        .node_guid = 0x506b4b03005c7a20ULL,
        .system_guid = 0x506b4b03005c7a20ULL,
        .node_type = 1, /* CA */
        .numports = 1,
        .ports = {
            {
                .portnum = 1,
                .port_guid = 0x506b4b03005c7a21ULL,
                .base_lid = 1,
                .sm_lid = 1,
                .state = 4, /* Active */
                .phys_state = 5, /* LinkUp */
                .link_layer = "InfiniBand",
            },
        },
    },
    {
        .ca_name = "mlx5_1",
        .ca_type = "MT4119",
        .hw_ver = "0",
        .fw_ver = "16.35.3006",
        .node_guid = 0x248a0703009c7b30ULL,
        .system_guid = 0x248a0703009c7b30ULL,
        .node_type = 1, /* CA */
        .numports = 2,
        .ports = {
            {
                .portnum = 1,
                .port_guid = 0x248a0703009c7b31ULL,
                .base_lid = 2,
                .sm_lid = 1,
                .state = 4, /* Active */
                .phys_state = 5, /* LinkUp */
                .link_layer = "InfiniBand",
            },
            {
                .portnum = 2,
                .port_guid = 0x248a0703009c7b32ULL,
                .base_lid = 3,
                .sm_lid = 1,
                .state = 4, /* Active */
                .phys_state = 5, /* LinkUp */
                .link_layer = "Ethernet",
            },
        },
    },
};

/* Mock performance counter values - deterministic for testing */
static struct {
    uint32_t symbol_error_counter;
    uint32_t link_error_recovery_counter;
    uint32_t link_downed_counter;
    uint32_t port_rcv_errors;
    uint32_t port_rcv_remote_physical_errors;
    uint32_t port_rcv_switch_relay_errors;
    uint32_t port_xmit_discards;
    uint32_t port_xmit_constraint_errors;
    uint32_t port_rcv_constraint_errors;
    uint32_t local_link_integrity_errors;
    uint32_t excessive_buffer_overrun_errors;
    uint32_t vl15_dropped;
    uint32_t port_xmit_data;
    uint32_t port_rcv_data;
    uint32_t port_xmit_pkts;
    uint32_t port_rcv_pkts;
    uint64_t port_xmit_data_ext;
    uint64_t port_rcv_data_ext;
    uint64_t port_xmit_pkts_ext;
    uint64_t port_rcv_pkts_ext;
    uint64_t port_unicast_xmit_pkts;
    uint64_t port_unicast_rcv_pkts;
    uint64_t port_multicast_xmit_pkts;
    uint64_t port_multicast_rcv_pkts;
} mock_perf_counters[NUM_HCAS][UMAD_CA_MAX_PORTS] = {
    {
        /* mlx5_0 port 1 */
        {
            .symbol_error_counter = 0,
            .link_error_recovery_counter = 0,
            .link_downed_counter = 0,
            .port_rcv_errors = 5,
            .port_rcv_remote_physical_errors = 0,
            .port_rcv_switch_relay_errors = 0,
            .port_xmit_discards = 0,
            .port_xmit_constraint_errors = 0,
            .port_rcv_constraint_errors = 0,
            .local_link_integrity_errors = 0,
            .excessive_buffer_overrun_errors = 0,
            .vl15_dropped = 0,
            .port_xmit_data = 123456789,
            .port_rcv_data = 987654321,
            .port_xmit_pkts = 1234567,
            .port_rcv_pkts = 9876543,
            .port_xmit_data_ext = 1234567890123ULL,
            .port_rcv_data_ext = 9876543210987ULL,
            .port_xmit_pkts_ext = 12345678901ULL,
            .port_rcv_pkts_ext = 98765432109ULL,
            .port_unicast_xmit_pkts = 12000000000ULL,
            .port_unicast_rcv_pkts = 98000000000ULL,
            .port_multicast_xmit_pkts = 345678901ULL,
            .port_multicast_rcv_pkts = 765432109ULL,
        },
    },
    {
        /* mlx5_1 port 1 */
        {
            .symbol_error_counter = 2,
            .link_error_recovery_counter = 1,
            .link_downed_counter = 0,
            .port_rcv_errors = 10,
            .port_rcv_remote_physical_errors = 0,
            .port_rcv_switch_relay_errors = 0,
            .port_xmit_discards = 3,
            .port_xmit_constraint_errors = 0,
            .port_rcv_constraint_errors = 0,
            .local_link_integrity_errors = 0,
            .excessive_buffer_overrun_errors = 0,
            .vl15_dropped = 0,
            .port_xmit_data = 223456789,
            .port_rcv_data = 887654321,
            .port_xmit_pkts = 2234567,
            .port_rcv_pkts = 8876543,
            .port_xmit_data_ext = 2234567890123ULL,
            .port_rcv_data_ext = 8876543210987ULL,
            .port_xmit_pkts_ext = 22345678901ULL,
            .port_rcv_pkts_ext = 88765432109ULL,
            .port_unicast_xmit_pkts = 22000000000ULL,
            .port_unicast_rcv_pkts = 88000000000ULL,
            .port_multicast_xmit_pkts = 245678901ULL,
            .port_multicast_rcv_pkts = 665432109ULL,
        },
        /* mlx5_1 port 2 */
        {
            .symbol_error_counter = 0,
            .link_error_recovery_counter = 0,
            .link_downed_counter = 0,
            .port_rcv_errors = 0,
            .port_rcv_remote_physical_errors = 0,
            .port_rcv_switch_relay_errors = 0,
            .port_xmit_discards = 0,
            .port_xmit_constraint_errors = 0,
            .port_rcv_constraint_errors = 0,
            .local_link_integrity_errors = 0,
            .excessive_buffer_overrun_errors = 0,
            .vl15_dropped = 0,
            .port_xmit_data = 323456789,
            .port_rcv_data = 787654321,
            .port_xmit_pkts = 3234567,
            .port_rcv_pkts = 7876543,
            .port_xmit_data_ext = 3234567890123ULL,
            .port_rcv_data_ext = 7876543210987ULL,
            .port_xmit_pkts_ext = 32345678901ULL,
            .port_rcv_pkts_ext = 78765432109ULL,
            .port_unicast_xmit_pkts = 32000000000ULL,
            .port_unicast_rcv_pkts = 78000000000ULL,
            .port_multicast_xmit_pkts = 145678901ULL,
            .port_multicast_rcv_pkts = 565432109ULL,
        },
    },
};

/* umad library function implementations */

int
umad_init(void)
{
    return 0; /* Success */
}

int
umad_done(void)
{
    return 0;
}

int
umad_get_cas_names(char cas[][UMAD_CA_NAME_LEN], int max)
{
    int i;
    int count = (max < NUM_HCAS) ? max : NUM_HCAS;

    for (i = 0; i < count; i++) {
        pmstrncpy(cas[i], UMAD_CA_NAME_LEN, mock_hcas[i].ca_name);
    }

    return count;
}

int
umad_get_ca(const char *ca_name, umad_ca_t *ca)
{
    int i, j;

    /* Find the matching HCA */
    for (i = 0; i < NUM_HCAS; i++) {
        if (strcmp(ca_name, mock_hcas[i].ca_name) == 0) {
            /* Allocate and fill the CA structure */
            memset(ca, 0, sizeof(*ca));

            pmstrncpy(ca->ca_name, sizeof(ca->ca_name), mock_hcas[i].ca_name);
            ca->node_type = mock_hcas[i].node_type;
            pmstrncpy(ca->ca_type, sizeof(ca->ca_type), mock_hcas[i].ca_type);
            pmstrncpy(ca->hw_ver, sizeof(ca->hw_ver), mock_hcas[i].hw_ver);
            pmstrncpy(ca->fw_ver, sizeof(ca->fw_ver), mock_hcas[i].fw_ver);
            ca->node_guid = mock_hcas[i].node_guid;
            ca->system_guid = mock_hcas[i].system_guid;
            ca->numports = mock_hcas[i].numports;

            /* Allocate and fill port structures */
            for (j = 0; j < UMAD_CA_MAX_PORTS; j++) {
                if (j < mock_hcas[i].numports &&
                    mock_hcas[i].ports[j].portnum > 0) {
                    umad_port_t *port = malloc(sizeof(umad_port_t));
                    if (port) {
                        memset(port, 0, sizeof(*port));
                        pmstrncpy(port->ca_name, sizeof(port->ca_name),
                                mock_hcas[i].ca_name);
                        port->portnum = mock_hcas[i].ports[j].portnum;
                        port->port_guid = mock_hcas[i].ports[j].port_guid;
                        port->base_lid = mock_hcas[i].ports[j].base_lid;
                        port->sm_lid = mock_hcas[i].ports[j].sm_lid;
                        port->state = mock_hcas[i].ports[j].state;
                        port->phys_state = mock_hcas[i].ports[j].phys_state;
                        pmstrncpy(port->link_layer, sizeof(port->link_layer),
                                mock_hcas[i].ports[j].link_layer);
                        ca->ports[port->portnum] = port;
                    }
                } else {
                    ca->ports[j] = NULL;
                }
            }

            return 0; /* Success */
        }
    }

    return -1; /* CA not found */
}

int
umad_release_ca(umad_ca_t *ca)
{
    int i;

    if (!ca)
        return -1;

    /* Free all allocated ports */
    for (i = 0; i < UMAD_CA_MAX_PORTS; i++) {
        if (ca->ports[i]) {
            free(ca->ports[i]);
            ca->ports[i] = NULL;
        }
    }

    return 0;
}

int
umad_get_port(const char *ca_name, int portnum, umad_port_t *port)
{
    int i;

    if (!port)
        return -1;

    /* Find the matching HCA and port */
    for (i = 0; i < NUM_HCAS; i++) {
        if (strcmp(ca_name, mock_hcas[i].ca_name) == 0) {
            int j;
            for (j = 0; j < UMAD_CA_MAX_PORTS; j++) {
                if (mock_hcas[i].ports[j].portnum == portnum) {
                    memset(port, 0, sizeof(*port));
                    pmstrncpy(port->ca_name, sizeof(port->ca_name),
                            mock_hcas[i].ca_name);
                    port->portnum = mock_hcas[i].ports[j].portnum;
                    port->port_guid = mock_hcas[i].ports[j].port_guid;
                    port->base_lid = mock_hcas[i].ports[j].base_lid;
                    port->sm_lid = mock_hcas[i].ports[j].sm_lid;
                    port->state = mock_hcas[i].ports[j].state;
                    port->phys_state = mock_hcas[i].ports[j].phys_state;
                    pmstrncpy(port->link_layer, sizeof(port->link_layer),
                            mock_hcas[i].ports[j].link_layer);
                    return 0;
                }
            }
        }
    }

    return -1; /* Port not found */
}

int
umad_release_port(umad_port_t *port)
{
    /* Nothing to do for mock library */
    return 0;
}

/* MAD library function implementations */

struct ibmad_port *
mad_rpc_open_port(char *dev_name, int dev_port, int *mgmt_classes, int num_classes)
{
    /* Return a dummy handle - just needs to be non-NULL */
    static int dummy_handle = 1;
    return (struct ibmad_port *)&dummy_handle;
}

void
mad_rpc_close_port(struct ibmad_port *srcport)
{
    /* Nothing to do */
}

/* Helper to set MAD field values */
static void
set_mock_portinfo(uint8_t *buf)
{
    /* Set some basic port info fields */
    /* IB_PORT_LID_F */
    mad_set_field(buf, 0, IB_PORT_LID_F, 1);
    /* IB_PORT_SMLID_F */
    mad_set_field(buf, 0, IB_PORT_SMLID_F, 1);
    /* IB_PORT_STATE_F - Active */
    mad_set_field(buf, 0, IB_PORT_STATE_F, 4);
    /* IB_PORT_PHYS_STATE_F - LinkUp */
    mad_set_field(buf, 0, IB_PORT_PHYS_STATE_F, 5);
    /* IB_PORT_LINK_WIDTH_ACTIVE_F - 4X */
    mad_set_field(buf, 0, IB_PORT_LINK_WIDTH_ACTIVE_F, 2);
    /* IB_PORT_LINK_SPEED_ACTIVE_F - 10.0 Gbps */
    mad_set_field(buf, 0, IB_PORT_LINK_SPEED_ACTIVE_F, 4);
    /* IB_PORT_LMC_F */
    mad_set_field(buf, 0, IB_PORT_LMC_F, 0);
    /* IB_PORT_MTU_CAP_F - 4096 */
    mad_set_field(buf, 0, IB_PORT_MTU_CAP_F, 5);
    /* IB_PORT_NEIGHBOR_MTU_F - 4096 */
    mad_set_field(buf, 0, IB_PORT_NEIGHBOR_MTU_F, 5);
    /* IB_PORT_CAPMASK_F */
    mad_set_field(buf, 0, IB_PORT_CAPMASK_F, 0x02510a6a);
    /* IB_PORT_GID_PREFIX_F */
    mad_set_field64(buf, 0, IB_PORT_GID_PREFIX_F, 0xfe80000000000000ULL);
}

static void
set_mock_perfdata(uint8_t *buf, int hca_idx, int port_idx)
{
    if (hca_idx >= NUM_HCAS || port_idx >= UMAD_CA_MAX_PORTS)
        return;

    /* Set performance counter values */
    mad_set_field(buf, 0, IB_PC_ERR_SYM_F,
                  mock_perf_counters[hca_idx][port_idx].symbol_error_counter);
    mad_set_field(buf, 0, IB_PC_LINK_RECOVERS_F,
                  mock_perf_counters[hca_idx][port_idx].link_error_recovery_counter);
    mad_set_field(buf, 0, IB_PC_LINK_DOWNED_F,
                  mock_perf_counters[hca_idx][port_idx].link_downed_counter);
    mad_set_field(buf, 0, IB_PC_ERR_RCV_F,
                  mock_perf_counters[hca_idx][port_idx].port_rcv_errors);
    mad_set_field(buf, 0, IB_PC_ERR_PHYSRCV_F,
                  mock_perf_counters[hca_idx][port_idx].port_rcv_remote_physical_errors);
    mad_set_field(buf, 0, IB_PC_ERR_SWITCH_REL_F,
                  mock_perf_counters[hca_idx][port_idx].port_rcv_switch_relay_errors);
    mad_set_field(buf, 0, IB_PC_XMT_DISCARDS_F,
                  mock_perf_counters[hca_idx][port_idx].port_xmit_discards);
    mad_set_field(buf, 0, IB_PC_ERR_XMTCONSTR_F,
                  mock_perf_counters[hca_idx][port_idx].port_xmit_constraint_errors);
    mad_set_field(buf, 0, IB_PC_ERR_RCVCONSTR_F,
                  mock_perf_counters[hca_idx][port_idx].port_rcv_constraint_errors);
    mad_set_field(buf, 0, IB_PC_ERR_LOCALINTEG_F,
                  mock_perf_counters[hca_idx][port_idx].local_link_integrity_errors);
    mad_set_field(buf, 0, IB_PC_ERR_EXCESS_OVR_F,
                  mock_perf_counters[hca_idx][port_idx].excessive_buffer_overrun_errors);
    mad_set_field(buf, 0, IB_PC_VL15_DROPPED_F,
                  mock_perf_counters[hca_idx][port_idx].vl15_dropped);
    mad_set_field(buf, 0, IB_PC_XMT_BYTES_F,
                  mock_perf_counters[hca_idx][port_idx].port_xmit_data);
    mad_set_field(buf, 0, IB_PC_RCV_BYTES_F,
                  mock_perf_counters[hca_idx][port_idx].port_rcv_data);
    mad_set_field(buf, 0, IB_PC_XMT_PKTS_F,
                  mock_perf_counters[hca_idx][port_idx].port_xmit_pkts);
    mad_set_field(buf, 0, IB_PC_RCV_PKTS_F,
                  mock_perf_counters[hca_idx][port_idx].port_rcv_pkts);

    /* Extended counters */
    mad_set_field64(buf, 0, IB_PC_EXT_XMT_BYTES_F,
                    mock_perf_counters[hca_idx][port_idx].port_xmit_data_ext);
    mad_set_field64(buf, 0, IB_PC_EXT_RCV_BYTES_F,
                    mock_perf_counters[hca_idx][port_idx].port_rcv_data_ext);
    mad_set_field64(buf, 0, IB_PC_EXT_XMT_PKTS_F,
                    mock_perf_counters[hca_idx][port_idx].port_xmit_pkts_ext);
    mad_set_field64(buf, 0, IB_PC_EXT_RCV_PKTS_F,
                    mock_perf_counters[hca_idx][port_idx].port_rcv_pkts_ext);
    mad_set_field64(buf, 0, IB_PC_EXT_XMT_UPKTS_F,
                    mock_perf_counters[hca_idx][port_idx].port_unicast_xmit_pkts);
    mad_set_field64(buf, 0, IB_PC_EXT_RCV_UPKTS_F,
                    mock_perf_counters[hca_idx][port_idx].port_unicast_rcv_pkts);
    mad_set_field64(buf, 0, IB_PC_EXT_XMT_MPKTS_F,
                    mock_perf_counters[hca_idx][port_idx].port_multicast_xmit_pkts);
    mad_set_field64(buf, 0, IB_PC_EXT_RCV_MPKTS_F,
                    mock_perf_counters[hca_idx][port_idx].port_multicast_rcv_pkts);
}

uint8_t *
smp_query_via(void *rcvbuf, ib_portid_t *portid, unsigned attrid,
              unsigned mod, unsigned timeout, const struct ibmad_port *srcport)
{
    if (!rcvbuf)
        return NULL;

    memset(rcvbuf, 0, IB_MAD_SIZE);

    if (attrid == IB_ATTR_PORT_INFO) {
        set_mock_portinfo(rcvbuf);
        return rcvbuf;
    }

    return rcvbuf;
}

uint8_t *
port_performance_query_via(void *rcvbuf, ib_portid_t *portid, int port,
                unsigned timeout, const struct ibmad_port *srcport)
{
    if (!rcvbuf)
        return NULL;

    memset(rcvbuf, 0, IB_MAD_SIZE);

    /* Use first HCA, first port for now - tests can be enhanced later */
    set_mock_perfdata(rcvbuf, 0, 0);

    return rcvbuf;
}

uint8_t *
performance_reset_via(void *rcvbuf, ib_portid_t *portid, int port, unsigned mask,
                unsigned timeout, unsigned id, const struct ibmad_port *srcport)
{
    /* Just return success - counters would be reset in real hardware */
    if (rcvbuf)
        memset(rcvbuf, 0, IB_MAD_SIZE);
    return rcvbuf;
}

int
ib_resolve_guid_via(ib_portid_t *portid, uint64_t *guid, ib_portid_t *sm_id,
                     int timeout, const struct ibmad_port *srcport)
{
    int i, j;

    if (!portid || !guid)
        return -1;

    /* Look up the GUID in our mock HCA table */
    for (i = 0; i < NUM_HCAS; i++) {
        for (j = 0; j < UMAD_CA_MAX_PORTS; j++) {
            if (mock_hcas[i].ports[j].port_guid == *guid) {
                memset(portid, 0, sizeof(*portid));
                portid->lid = mock_hcas[i].ports[j].base_lid;
                return 0;
            }
        }
    }

    /* If not found, return a default LID */
    memset(portid, 0, sizeof(*portid));
    portid->lid = 1;
    return 0;
}

#ifdef HAVE_PMA_QUERY_VIA
uint8_t *
pma_query_via(void *rcvbuf, ib_portid_t *portid, int port,
              unsigned timeout, unsigned id, const struct ibmad_port *srcport)
{
    if (!rcvbuf)
        return NULL;

    memset(rcvbuf, 0, 1024);

    if (id == IB_GSI_PORT_COUNTERS_EXT) {
        /* Set extended counter values for switch */
        mad_set_field64(rcvbuf, 0, IB_PC_EXT_XMT_BYTES_F, 5555555555ULL);
        mad_set_field64(rcvbuf, 0, IB_PC_EXT_RCV_BYTES_F, 6666666666ULL);
        mad_set_field64(rcvbuf, 0, IB_PC_EXT_XMT_PKTS_F, 55555555ULL);
        mad_set_field64(rcvbuf, 0, IB_PC_EXT_RCV_PKTS_F, 66666666ULL);
        return rcvbuf;
    }

    return rcvbuf;
}
#endif
