#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises hardcoded metrics, including conflict handling when such metrics are received

import sys
import socket
import glob
import os

utils_path = os.path.abspath(os.path.join("utils"))
sys.path.append(utils_path)

import pmdastatsd_test_utils as utils

utils.print_test_file_separator()
print(os.path.basename(__file__))

ip = "0.0.0.0"
port = 8125
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)


def run_test():
    utils.print_test_section_separator()
    utils.pmdastatsd_install()
    utils.print_metric('statsd.pmda.received')
    utils.print_metric('statsd.pmda.parsed')
    utils.print_metric('statsd.pmda.aggregated')
    utils.print_metric('statsd.pmda.dropped')
    utils.print_metric('statsd.pmda.time_spent_parsing')
    utils.print_metric('statsd.pmda.time_spent_aggregating')
    utils.print_metric('statsd.pmda.metrics_tracked')
    utils.print_metric('statsd.pmda.settings.duration_aggregation_type')
    utils.print_metric('statsd.pmda.settings.parser_type')
    utils.print_metric('statsd.pmda.settings.port')
    utils.print_metric('statsd.pmda.settings.debug_output_filename')
    utils.print_metric('statsd.pmda.settings.verbose')
    utils.print_metric('statsd.pmda.settings.max_unprocessed_packets')
    utils.print_metric('statsd.pmda.settings.max_udp_packet_size')

    sock.sendto("pmda.received:1|c".encode("utf-8"), (ip, port))
    sock.sendto("pmda.parsed:1|c".encode("utf-8"), (ip, port))
    sock.sendto("pmda.aggregated:1|c".encode("utf-8"), (ip, port))
    sock.sendto("pmda.dropped:1|c".encode("utf-8"), (ip, port))
    sock.sendto("pmda.time_spent_parsing:1|c".encode("utf-8"), (ip, port))
    sock.sendto("pmda.time_spent_aggregating:1|c".encode("utf-8"), (ip, port))
    sock.sendto("pmda.metrics_tracked:1|c".encode("utf-8"), (ip, port))
    sock.sendto('pmda.settings.duration_aggregation_type:1|c'.encode("utf-8"), (ip, port))
    sock.sendto('pmda.settings.parser_type:1|c'.encode("utf-8"), (ip, port))
    sock.sendto('pmda.settings.port:1|c'.encode("utf-8"), (ip, port))
    sock.sendto('pmda.settings.debug_output_filename:1|c'.encode("utf-8"), (ip, port))
    sock.sendto('pmda.settings.verbose:1|c'.encode("utf-8"), (ip, port))
    sock.sendto('pmda.settings.max_unprocessed_packets:1|c'.encode("utf-8"), (ip, port))
    sock.sendto('pmda.settings.max_udp_packet_size:1|c'.encode("utf-8"), (ip, port))

    utils.print_metric('statsd.pmda.received')
    utils.print_metric('statsd.pmda.parsed')
    utils.print_metric('statsd.pmda.aggregated')
    utils.print_metric('statsd.pmda.dropped')
    utils.print_metric('statsd.pmda.metrics_tracked')
    
    utils.pmdastatsd_remove()
    utils.restore_config()

run_test()
