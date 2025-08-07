#!/usr/bin/pmpython
#
# Copyright (c) 2025 Oracle and/or its affiliates.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# pylint: disable=bad-whitespace,too-many-arguments,too-many-lines, bad-continuation, line-too-long
# pylint: disable=redefined-outer-name,unnecessary-lambda, wildcard-import, unused-wildcard-import
#
from datetime import datetime
from pcp import pmapi
from pcp import pmcc
import sys
import time

ROCESTAT_HW_METRICS = [
    'rocestat.hw.link.link_error_recovery',
    'rocestat.hw.link.link_downed',
    'rocestat.hw.link.local_link_integrity_errors',
    'rocestat.hw.rnr.rnr_nak_retry_err',
    'rocestat.hw.resp.resp_local_length_error',
    'rocestat.hw.resp.resp_cqe_error',
    'rocestat.hw.resp.resp_cqe_flush_error',
    'rocestat.hw.resp.resp_remote_access_errors',
    'rocestat.hw.req.req_remote_invalid_request',
    'rocestat.hw.req.req_cqe_error',
    'rocestat.hw.req.req_cqe_flush_error',
    'rocestat.hw.req.duplicate_request',
    'rocestat.hw.req.rx_read_requests',
    'rocestat.hw.req.rx_atomic_requests',
    'rocestat.hw.req.req_remote_access_errors',
    'rocestat.hw.req.rx_write_requests',
    'rocestat.hw.mcast.multicast_rcv_packets',
    'rocestat.hw.mcast.multicast_xmit_packets',
    'rocestat.hw.ucast.unicast_rcv_packets',
    'rocestat.hw.ucast.unicast_xmit_packets',
    'rocestat.hw.rcv.port_rcv_errors',
    'rocestat.hw.rcv.port_rcv_remote_physical_errors',
    'rocestat.hw.rcv.port_rcv_packets',
    'rocestat.hw.rcv.port_rcv_data',
    'rocestat.hw.rcv.port_rcv_constraint_errors',
    'rocestat.hw.rcv.port_rcv_switch_relay_errors',
    'rocestat.hw.xmit.port_xmit_data',
    'rocestat.hw.xmit.port_xmit_constraint_errors',
    'rocestat.hw.xmit.port_xmit_wait',
    'rocestat.hw.xmit.port_xmit_packets',
    'rocestat.hw.xmit.port_xmit_discards',
    'rocestat.hw.roce_slow_restart_trans',
    'rocestat.hw.roce_slow_restart_cnps',
    'rocestat.hw.roce_slow_restart',
    'rocestat.hw.roce_adp_retrans_to',
    'rocestat.hw.clear_counters',
    'rocestat.hw.local_ack_timeout_err',
    'rocestat.hw.lifespan',
    'rocestat.hw.implied_nak_seq_err',
    'rocestat.hw.packet_seq_err',
    'rocestat.hw.roce_adp_retrans',
    'rocestat.hw.out_of_buffer',
    'rocestat.hw.out_of_sequence',
    'rocestat.hw.VL15_dropped',
    'rocestat.hw.excessive_buffer_overrun_errors',
    'rocestat.hw.symbol_error',
]

ROCESTAT_HW_METRICS_DESC = [
    'link_error_recovery',
    'link_downed',
    'local_link_integrity_errors',
    'rnr_nak_retry_err',
    'resp_local_length_error',
    'resp_cqe_error',
    'resp_cqe_flush_error',
    'resp_remote_access_errors',
    'req_remote_invalid_request',
    'req_cqe_error',
    'req_cqe_flush_error',
    'duplicate_request',
    'rx_read_requests',
    'rx_atomic_requests',
    'req_remote_access_errors',
    'rx_write_requests',
    'multicast_rcv_packets',
    'multicast_xmit_packets',
    'unicast_rcv_packets',
    'unicast_xmit_packets',
    'port_rcv_errors',
    'port_rcv_remote_physical_errors',
    'port_rcv_packets',
    'port_rcv_data',
    'port_rcv_constraint_errors',
    'port_rcv_switch_relay_errors',
    'port_xmit_data',
    'port_xmit_constraint_errors',
    'port_xmit_wait',
    'port_xmit_packets',
    'port_xmit_discards',
    'roce_slow_restart_trans',
    'roce_slow_restart_cnps',
    'roce_slow_restart',
    'roce_adp_retrans_to',
    'clear_counters',
    'local_ack_timeout_err',
    'lifespan',
    'implied_nak_seq_err',
    'packet_seq_err',
    'roce_adp_retrans',
    'out_of_buffer',
    'out_of_sequence',
    'VL15_dropped',
    'excessive_buffer_overrun_errors',
    'symbol_error',
]

ROCESTAT_PER_LANE_METRICS = ['rocestat.lane.tx_bytes', 'rocestat.lane.rx_bytes', 'rocestat.lane.rx_pause']


class RoceStatOptions(pmapi.pmOptions):
    context = None
    timefmt = "%H:%M:%S"
    samples = 0
    hw_stats_flag = False
    lane_stats_flag = False

    def extra_options(self, opt, optarg, index):
        if opt == "hw_stats":
            RoceStatOptions.hw_stats_flag = True

        elif opt == "lane_stats":
            RoceStatOptions.lane_stats_flag = True

    def __init__(self):
        pmapi.pmOptions.__init__(self, "a:s:S:T:t:")
        self.pmSetLongOption("hw_stats", 0, "", "", "Display hw counters")
        self.pmSetLongOption("lane_stats", 0, "", "", "Display lane stats")
        self.pmSetLongOptionStart()
        self.pmSetLongOptionFinish()
        self.pmSetLongOption("samples", 1, "s", "COUNT",
                             "Number of samples to collect")
        self.pmSetLongOptionArchive()
        self.pmSetLongOptionHelp()

        self.pmSetOptionCallback(self.extra_options)


class ROCeStatReport:
    prev_counter_data = None
    def __init__(self, samples):
        self.samples = samples

    def report(self, manager):
        group = manager["rocestat"]

        opts.pmGetOptionSamples()

        timestamp = manager.pmLocaltime(group.timestamp.tv_sec)
        time_string = time.strftime("%x", timestamp.struct_time()) + " "
        t_s = group.contextCache.pmLocaltime(int(group.timestamp))
        time_string += time.strftime(RoceStatOptions.timefmt, t_s.struct_time())
        print("zzz <%s>" % (time_string))
        hw_stats_flag = RoceStatOptions.hw_stats_flag
        lane_stats_flag = RoceStatOptions.lane_stats_flag

        # HW Counters
        if hw_stats_flag or (hw_stats_flag == lane_stats_flag):
            header = ["Counter"]
            for val in group[ROCESTAT_HW_METRICS[0]].netValues:
                if not val[1].endswith("_delta"):
                    header.append(val[1])
            card_idxs = {}
            for i in range(1, len(header)):
                card_idxs[header[i]] = i
            prev_counter_data = self.prev_counter_data
            curr_counter_data = {}
            curr_counter_data["ts"] = datetime.now()
            for card in card_idxs:
                curr_counter_data[card] = {}

            for metric_name in ROCESTAT_HW_METRICS_DESC:
                metric_idx = ROCESTAT_HW_METRICS_DESC.index(metric_name)
                metric = ROCESTAT_HW_METRICS[metric_idx]
                for val in group[metric].netValues:
                    card_name, value = val[1], val[2]
                    curr_counter_data[card_name][metric_name] = value

            counters = []
            for counter in ROCESTAT_HW_METRICS_DESC:
                row = [counter] + ["" for _ in range(len(card_idxs.keys()))]
                for card, card_data in curr_counter_data.items():
                    if card == "ts":
                        continue
                    card_idx = card_idxs[card]
                    count = card_data[counter]
                    if prev_counter_data is not None and card in prev_counter_data:
                        count_delta = count - prev_counter_data[card][counter]
                        time_diff = (curr_counter_data["ts"] - prev_counter_data["ts"]).total_seconds()
                        count_delta /= time_diff
                        if "data" in counter:
                            count_delta = self.convert_to_gbps(count_delta)
                        count_delta = round(count_delta, 2)
                        if int(count_delta) == count_delta:
                            count_delta = int(count_delta)
                    else:
                        count_delta = 0
                    row[card_idx] = f"{count}({count_delta})"
                counters.append(row)
            self.prev_counter_data = curr_counter_data

            space = 5 + max(len(counter)
                            for counter in ROCESTAT_HW_METRICS_DESC)
            print_format = f"%{space}s " + \
                " ".join(["%25s" for _ in range(len(header)-1)])
            if len(header) > 1:
                print(print_format % tuple(header))
                for counter in counters:
                    print(print_format % tuple(counter))
            print()

        # Per lane stats
        if lane_stats_flag or (lane_stats_flag == hw_stats_flag):
            lane_stats = {}
            for metric in ROCESTAT_PER_LANE_METRICS:
                idx = 0
                try:
                    val = group[metric].netValues
                except IndexError:
                    idx += 1
                    continue

                for elem in val:
                    lane = elem[1]
                    value = elem[2]

                    if lane not in lane_stats:
                        lane_stats[lane] = {
                            m: 0 for m in ROCESTAT_PER_LANE_METRICS}
                    lane_stats[lane][metric] = value

            if lane_stats:
                print("%10s %20s %12s %12s %12s %12s %12s %12s" % (
                    "Netdev", "Queue", "tx_bytes", "rx_bytes", "tx/s(Gbps)", "rx/s(Gbps)", "pause", "pause_delta"))
                for lane, metrics in lane_stats.items():
                    netdev, queue = self.split_lane(lane)
                    tx_bw_gbps = self.convert_to_gbps(
                        metrics.get('rocestat.lane.tx_bytes_bw', 0))
                    rx_bw_gbps = self.convert_to_gbps(
                        metrics.get('rocestat.lane.rx_bytes_bw', 0))
                    print("%10s %20s %12s %12s %12s %12s %12s %12s" % (
                        netdev, queue,
                        metrics.get('tx_bytes', 0),
                        metrics.get('rx_bytes', 0),
                        tx_bw_gbps,
                        rx_bw_gbps,
                        metrics.get('rx_pause', 0),
                        metrics.get('rx_pause_delta', 0)
                    ))
        print()

    def split_lane(self, lane):
        parts = lane.split('_')
        if len(parts) == 2:
            netdev = parts[0]
            queue_id = parts[1].replace("lane", "")

            queue_name = {
                "0": "Default(TCP) - 0",
                "1": "Normal Large - 1",
                "2": "VIP Small - 2",
                "3": "VIP Large - 3",
                "4": "Normal Small - 4",
                "5": "VIP Small - 5",
                "6": "Unused - 6",
                "7": "CNP - 7"
            }.get(queue_id, f"Queue - {queue_id}")

            return netdev, queue_name
        return lane, "Unknown Queue"

    def convert_to_gbps(self, value):
        if value == 0:
            return 0
        return value / 1000000000


if __name__ == '__main__':
    try:
        opts = RoceStatOptions()
        mngr = pmcc.MetricGroupManager.builder(opts, sys.argv)
        RoceStatOptions.context = mngr.type
        missing = mngr.checkMissingMetrics(
            ROCESTAT_HW_METRICS + ROCESTAT_PER_LANE_METRICS)
        if missing is not None:
            sys.stderr.write(
                'Error: not all required metrics are available\nMissing %s\n' % missing)
            sys.exit(1)
        mngr["rocestat"] = ROCESTAT_HW_METRICS + ROCESTAT_PER_LANE_METRICS
        mngr.printer = ROCeStatReport(opts.samples)
        sts = mngr.run()
        sys.exit(sts)

    except pmapi.pmErr as error:
        sys.stderr.write("%s %s\n" % (error.progname(), error.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except KeyboardInterrupt:
        pass
