#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises ability of the agent to log its workings in various verbosity levels
# - doesnt check the actual log output, just that length of verbose lvl X is higher than the length of of verbose lvl X-1

import sys
import socket
import glob
import os
import time

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

for i in range(0, 200):
    payloads.append(":20|c")

verbosity_lvl_0 = utils.configs["verbose"][0]
verbosity_lvl_1 = utils.configs["verbose"][1]
verbosity_lvl_2 = utils.configs["verbose"][2]

def get_number_of_lines_in_file(path):
    f = open(path, "r")
    contents = f.read()
    f.close()
    return len(contents.split('\n'))

def run_test():
    utils.print_test_section_separator()

    utils.pmdastatsd_install(verbosity_lvl_0)
    for payload in payloads:
        sock.sendto(payload.encode("utf-8"), (ip, port))
    # time sleep to wait for log files to be filled with logs
    time.sleep(1)
    # explicitly refresh pmns
    utils.pminfo("statsd")
    time.sleep(4)
    log_lvl_0_count = get_number_of_lines_in_file(utils.pmdastatsd_log_path)
    utils.pmdastatsd_remove()
    utils.restore_config()

    utils.pmdastatsd_install(verbosity_lvl_1)
    for payload in payloads:
        sock.sendto(payload.encode("utf-8"), (ip, port))
    time.sleep(1)
    utils.pminfo("statsd")
    time.sleep(4)
    log_lvl_1_count = get_number_of_lines_in_file(utils.pmdastatsd_log_path)
    utils.pmdastatsd_remove()
    utils.restore_config()

    utils.pmdastatsd_install(verbosity_lvl_2)
    for payload in payloads:
        sock.sendto(payload.encode("utf-8"), (ip, port))
    time.sleep(1)
    utils.pminfo("statsd")
    time.sleep(4)
    log_lvl_2_count = get_number_of_lines_in_file(utils.pmdastatsd_log_path)
    utils.pmdastatsd_remove()
    utils.restore_config()
    
    if log_lvl_0_count < log_lvl_1_count and log_lvl_1_count < log_lvl_2_count:
        print("Verbosity specificity is OK. (lvl 0 < lvl 1 < lvl 2)")

run_test()
