/*
 * Copyright (c) 2021 Red Hat.
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
#include "pmapi.h"
#include "pmda.h"

#include "indom.h"
#include "domain.h"
#include "cluster.h"
#include "ss_stats.h"

#define OFFSET(S,X) (void *)&((S *)NULL)->X

pmdaMetric metrictable[] = {
    { /* network.persocket.filter */
	.m_user = &ss_filter,
	.m_desc = { PMDA_PMID(CLUSTER_GLOBAL, 0),
	    PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.netid */
	.m_user = OFFSET(ss_stats_t, netid),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 0),
	    PM_TYPE_STRING, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.state */
	.m_user = OFFSET(ss_stats_t, state),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 1),
	    PM_TYPE_STRING, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.recvq */
	.m_user = OFFSET(ss_stats_t, recvq),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 2),
	    PM_TYPE_32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }},

    { /* network.persocket.sendq */
	.m_user = OFFSET(ss_stats_t, sendq),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 3),
	    PM_TYPE_32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }},

    { /* network.persocket.src */
	.m_user = OFFSET(ss_stats_t, src),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 4),
	    PM_TYPE_STRING, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.dst */
	.m_user = OFFSET(ss_stats_t, dst),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 5),
	    PM_TYPE_STRING, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.inode */
	.m_user = OFFSET(ss_stats_t, inode),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 6),
	    PM_TYPE_64, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.uid */
	.m_user = OFFSET(ss_stats_t, uid),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 8),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.sk */
	.m_user = OFFSET(ss_stats_t, sk),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 9),
	    PM_TYPE_U64, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.cgroup */
	.m_user = OFFSET(ss_stats_t, cgroup),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 10),
	    PM_TYPE_STRING, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.v6only */
	.m_user = OFFSET(ss_stats_t, v6only),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 11),
	    PM_TYPE_32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.ts */
	.m_user = OFFSET(ss_stats_t, ts),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 13),
	    PM_TYPE_32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.sack */
	.m_user = OFFSET(ss_stats_t, sack),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 14),
	    PM_TYPE_32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.cubic */
	.m_user = OFFSET(ss_stats_t, cubic),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 15),
	    PM_TYPE_32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.ato */
	.m_user = OFFSET(ss_stats_t, ato),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 16),
	    PM_TYPE_DOUBLE, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }},

    { /* network.persocket.mss */
	.m_user = OFFSET(ss_stats_t, mss),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 17),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.pmtu */
	.m_user = OFFSET(ss_stats_t, pmtu),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 18),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.rcvmss */
	.m_user = OFFSET(ss_stats_t, rcvmss),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 19),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.advmss */
	.m_user = OFFSET(ss_stats_t, advmss),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 20),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.cwnd */
	.m_user = OFFSET(ss_stats_t, cwnd),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 21),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }},

    { /* network.persocket.ssthresh */
	.m_user = OFFSET(ss_stats_t, ssthresh),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 22),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }},

    { /* network.persocket.bytes_sent */
	.m_user = OFFSET(ss_stats_t, bytes_sent),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 23),
	    PM_TYPE_U64, SOCKETS_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.bytes_retrans */
	.m_user = OFFSET(ss_stats_t, bytes_retrans),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 24),
	    PM_TYPE_U64, SOCKETS_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.bytes_acked */
	.m_user = OFFSET(ss_stats_t, bytes_acked),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 25),
	    PM_TYPE_U64, SOCKETS_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.bytes_received */
	.m_user = OFFSET(ss_stats_t, bytes_received),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 36),
	    PM_TYPE_U64, SOCKETS_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.segs_out */
	.m_user = OFFSET(ss_stats_t, segs_out),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 37),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }},

    { /* network.persocket.segs_in */
	.m_user = OFFSET(ss_stats_t, segs_in),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 38),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }},

    { /* network.persocket.data_segs_out */
	.m_user = OFFSET(ss_stats_t, data_segs_out),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 39),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }},

    { /* network.persocket.data_segs_in */
	.m_user = OFFSET(ss_stats_t, data_segs_in),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 40),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }},

    { /* network.persocket.send */
	.m_user = OFFSET(ss_stats_t, send),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 41),
	    PM_TYPE_DOUBLE, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }},

    { /* network.persocket.lastsnd */
	.m_user = OFFSET(ss_stats_t, lastsnd),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 42),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }},

    { /* network.persocket.lastrcv */
	.m_user = OFFSET(ss_stats_t, lastrcv),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 43),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }},

    { /* network.persocket.lastack */
	.m_user = OFFSET(ss_stats_t, lastack),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 44),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }},

    { /* network.persocket.pacing_rate */
	.m_user = OFFSET(ss_stats_t, pacing_rate),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 45),
	    PM_TYPE_DOUBLE, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(1,-1,0,PM_SPACE_BYTE,PM_TIME_SEC,0) }},

    { /* network.persocket.delivery_rate */
	.m_user = OFFSET(ss_stats_t, delivery_rate),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 46),
	    PM_TYPE_DOUBLE, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(1,-1,0,PM_SPACE_BYTE,PM_TIME_SEC,0) }},

    { /* network.persocket.delivered */
	.m_user = OFFSET(ss_stats_t, delivered),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 47),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_COUNTER,
	    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }},

    { /* network.persocket.app_limited */
	.m_user = OFFSET(ss_stats_t, app_limited),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 48),
	    PM_TYPE_32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.reord_seen */
	.m_user = OFFSET(ss_stats_t, reord_seen),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 49),
	    PM_TYPE_32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }},

    { /* network.persocket.busy */
	.m_user = OFFSET(ss_stats_t, busy),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 50),
	    PM_TYPE_U64, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.dsack_dups */
	.m_user = OFFSET(ss_stats_t, dsack_dups),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 51),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }},

    { /* network.persocket.rcv_rtt */
	.m_user = OFFSET(ss_stats_t, rcv_rtt),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 52),
	    PM_TYPE_DOUBLE, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }},

    { /* network.persocket.rcv_space */
	.m_user = OFFSET(ss_stats_t, rcv_space),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 53),
	    PM_TYPE_32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.rcv_ssthresh */
	.m_user = OFFSET(ss_stats_t, rcv_ssthresh),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 54),
	    PM_TYPE_32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }},

    { /* network.persocket.minrtt */
	.m_user = OFFSET(ss_stats_t, minrtt),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 55),
	    PM_TYPE_DOUBLE, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }},

    { /* network.persocket.notsent */
	.m_user = OFFSET(ss_stats_t, notsent),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 56),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.timer.str */
	.m_user = OFFSET(ss_stats_t, timer_str),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 70),
	    PM_TYPE_STRING, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.timer.name */
	.m_user = OFFSET(ss_stats_t, timer_name),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 71),
	    PM_TYPE_STRING, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.timer.expire_str */
	.m_user = OFFSET(ss_stats_t, timer_expire_str),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 72),
	    PM_TYPE_STRING, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.timer.retrans */
	.m_user = OFFSET(ss_stats_t, timer_retrans),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 73),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.skmem.str */
	.m_user = OFFSET(ss_stats_t, skmem_str),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 80),
	    PM_TYPE_STRING, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.skmem.rmem_alloc */
	.m_user = OFFSET(ss_stats_t, skmem_rmem_alloc),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 81),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.skmem.wmem_alloc */
	.m_user = OFFSET(ss_stats_t, skmem_wmem_alloc),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 82),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.skmem.rcv_buf */
	.m_user = OFFSET(ss_stats_t, skmem_rcv_buf),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 83),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.skmem.snd_buf */
	.m_user = OFFSET(ss_stats_t, skmem_snd_buf),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 84),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.skmem.fwd_alloc */
	.m_user = OFFSET(ss_stats_t, skmem_fwd_alloc),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 95),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.skmem.wmem_queued */
	.m_user = OFFSET(ss_stats_t, skmem_wmem_queued),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 86),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.skmem.ropt_mem */
	.m_user = OFFSET(ss_stats_t, skmem_ropt_mem),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 87),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.skmem.back_log */
	.m_user = OFFSET(ss_stats_t, skmem_back_log),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 88),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }},

    { /* network.persocket.skmem.sock_drop */
	.m_user = OFFSET(ss_stats_t, skmem_sock_drop),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 89),
	    PM_TYPE_U32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.wscale.str */
	.m_user = OFFSET(ss_stats_t, wscale_str),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 60),
	    PM_TYPE_STRING, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.wscale.snd */
	.m_user = OFFSET(ss_stats_t, wscale_snd),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 61),
	    PM_TYPE_32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.wscale.rcv */
	.m_user = OFFSET(ss_stats_t, wscale_rcv),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 62),
	    PM_TYPE_32, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.round_trip.str */
	.m_user = OFFSET(ss_stats_t, round_trip_str),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 90),
	    PM_TYPE_STRING, SOCKETS_INDOM, PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0) }},

    { /* network.persocket.round_trip.rtt */
	.m_user = OFFSET(ss_stats_t, round_trip_rtt),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 91),
	    PM_TYPE_DOUBLE, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }},

    { /* network.persocket.round_trip.rttvar */
	.m_user = OFFSET(ss_stats_t, round_trip_rttvar),
	.m_desc = { PMDA_PMID(CLUSTER_SS, 92),
	    PM_TYPE_DOUBLE, SOCKETS_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }},
};

int nmetrics = sizeof(metrictable)/sizeof(metrictable[0]);
