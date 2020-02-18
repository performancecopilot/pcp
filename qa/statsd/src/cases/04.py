#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises updates of non-config hardcoded metrics:
# - statsd.pmda.received (before: 0, after: 75)
# - statsd.pmda.parsed (before: 0, after: 45)
# - statsd.pmda.aggregated (before: 0, after: 35)
# - statsd.pmda.dropped (before: 0, after: 40)
# - statsd.pmda.time_spent_parsing (before: 0, after: non-zero)
# - statsd.pmda.time_spent_aggregating (before: 0, after: non-zero)
# - statsd.pmda.metrics_tracked, with its counter, gauge, duration and total instances (before: 0, 0, 0, 0; after: 4, 4, 2, 10)

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

payloads = [
    # These are parsed and aggregated
    "stat_login:1|c",
    "stat_login:5|c",
    "stat_login:20|c",
    "stat_login:19|c",
    "stat_login:100|c",
    "stat_logout:2|c",
    "stat_logout:10|c",
    "stat_tagged_counter_a,tagX=X:1|c",
    "stat_tagged_counter_a,tagY=Y:2|c",
    "stat_tagged_counter_a,tagZ=Z:3|c",
    "stat_tagged_counter_b:4|c",
    "stat_tagged_counter_b,tagX=X,tagW=W:5|c",
    "stat_success:0|g",
    "stat_success:+5|g",
    "stat_success:-12|g",
    "stat_error:0|g",
    "stat_error:+9|g",
    "stat_error:-0|g",
    "stat_tagged_gauge_a,tagX=X:1|g",
    "stat_tagged_gauge_a,tagY=Y:+2|g",
    "stat_tagged_gauge_a,tagY=Y:-1|g",
    "stat_tagged_gauge_a,tagZ=Z:-3|g",
    "stat_tagged_gauge_b:4|g",
    "stat_tagged_gauge_b,tagX=X,tagW=W:-5|g",
    "stat_cpu_wait:200|ms",
    "stat_cpu_wait:100|ms",
    "stat_cpu_busy:100|ms",
    "stat_cpu_busy:10|ms",
    "stat_cpu_busy:20|ms",
    "stat_cpu_wait,target=cpu0:10|ms",
    "stat_cpu_wait,target=cpu0:100|ms",
    "stat_cpu_wait,target=cpu0:1000|ms",
    "stat_cpu_wait,target=cpu1:20|ms",
    "stat_cpu_wait,target=cpu1:200|ms",
    "stat_cpu_wait,target=cpu1:2000|ms",
    # These are parsed, not aggregated and then dropped
    "stat_login:1|g",
    "stat_login:3|g",
    "stat_login:5|g",
    "stat_logout:4|g",
    "stat_logout:2|g",
    "stat_logout:2|g",
    "stat_login:+0.5|g",
    "stat_logout:0.128|g",
    "cache_cleared:-4|c",
    "cache_cleared:-1|c",
    # These are not parsed and then dropped
    "session_started:1wq|c",
    u"cache_cleared:4ěš|c",
    "session_started:1_4w|c",
    "session_started:1|cx",
    "cache_cleared:4|cw",
    "cache_cleared:1|rc",
    "session_started:|c",
    ":20|c",
    "session_started:1wq|g",
    u"cache_cleared:4ěš|g",
    "session_started:1_4w|g",
    "session_started:-we|g",
    u"cache_cleared:-0ě2|g",
    "cache_cleared:-02x|g",
    "session_started:1|gx",
    "cache_cleared:4|gw",
    "cache_cleared:1|rg",
    "session_started:|g",
    "cache_cleared:|g",
    "session_duration:|ms",
    "cache_loopup:|ms",
    "session_duration:1wq|ms",
    u"cache_cleared:4ěš|ms",
    "session_started:1_4w|ms",
    "session_started:2-1|ms",
    "session_started:1|mss",
    "cache_cleared:4|msd",
    "cache_cleared:1|msa",
    "session_started:|ms",
    ":20|ms"
]

basis_parser_config = utils.configs["parser_type"][0]
ragel_parser_config = utils.configs["parser_type"][1]

duration_aggregation_basic_config = utils.configs["duration_aggregation_type"][0]
duration_aggregation_hdr_histogram_config = utils.configs["duration_aggregation_type"][1]

testconfigs = [basis_parser_config, ragel_parser_config, duration_aggregation_basic_config, duration_aggregation_hdr_histogram_config]

def extract_value(str):
    return str.split("\n")[1].strip()

def run_test():
    for testconfig in testconfigs:
        utils.print_test_section_separator()
        utils.pmdastatsd_install(testconfig)
        for payload in payloads:
            sock.sendto(payload.encode("utf-8"), (ip, port))
        utils.print_metric('statsd.pmda.received')
        utils.print_metric('statsd.pmda.parsed')
        utils.print_metric('statsd.pmda.aggregated')
        utils.print_metric('statsd.pmda.dropped')
        time_spent_parsing = utils.request_metric('statsd.pmda.time_spent_parsing')
        if time_spent_parsing:
            val = extract_value(time_spent_parsing)
            if not val == "value 0":
                print("time_spent_parsing is not 0")
            else:
                print("time_spent_parsing is 0")
        time_spent_aggregating = utils.request_metric('statsd.pmda.time_spent_aggregating')
        if time_spent_aggregating:
            val = extract_value(time_spent_aggregating)
            if not val == "value 0":
                print("time_spent_aggregating is not 0")
            else:
                print("time_spent_aggregating is 0")
        utils.print_metric('statsd.pmda.metrics_tracked')
        utils.pmdastatsd_remove()
        utils.restore_config()

run_test()


