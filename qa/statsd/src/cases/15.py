#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises agent's memory handling with Valgrind

import sys
import socket
import glob
import os
import time
import subprocess

from subprocess import PIPE, Popen
from threading  import Thread

ON_POSIX = 'posix' in sys.builtin_module_names

utils_path = os.path.abspath(os.path.join("utils"))
sys.path.append(utils_path)

import pmdastatsd_test_utils as utils

utils.print_test_file_separator()
print(os.path.basename(__file__))

ip = "0.0.0.0"
port = 8125
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
valgrind_out_dir = os.path.join(sys.argv[1])
valgrind_out_path = os.path.join(sys.argv[1], "valgrind-%p.out")
dbpmda_out_path = os.path.join(sys.argv[1], "dbpmda.out")

payloads = [
    "test_labels:0|c", # no label
    "test_labels,tagX=X:1|c", # single label
    "test_labels,tagX=X,tagY=Y:2|c", # two labels
    "test_labels,tagC=C,tagB=B,tagA=A:3|c", # labels that will be ordered
    "test_labels:4|c|#A:A", # labels in dogstatsd-ruby format
    "test_labels,A=A:5|c|#B:B,C:C", # labels in dogstatsd-ruby format combined with standard format
    "test_labels,A=A,A=10:6|c", # labels with non-unique keys, right-most takes precedence,
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

command_to_execute = [
    'echo "fetch statsd.pmda.settings.duration_aggregation_type"',
    'echo "fetch statsd.pmda.settings.parser_type"',
    'echo "fetch statsd.pmda.settings.port"',
    'echo "fetch statsd.pmda.settings.debug_output_filename"',
    'echo "fetch statsd.pmda.settings.verbose"',
    'echo "fetch statsd.pmda.settings.max_unprocessed_packets"',
    'echo "fetch statsd.pmda.settings.max_udp_packet_size"',
    'echo "fetch statsd.pmda.time_spent_aggregating"',
    'echo "fetch statsd.pmda.time_spent_parsing"',
    'echo "fetch statsd.pmda.metrics_tracked"',
    'echo "fetch statsd.pmda.aggregated"',
    'echo "fetch statsd.pmda.dropped"',
    'echo "fetch statsd.pmda.parsed"',
    'echo "fetch statsd.pmda.received"',
    'echo "fetch statsd.pmda.received"',
    'echo "fetch statsd.stat_login"',
    'echo "fetch statsd.stat_logout"',
    'echo "fetch statsd.stat_success"',
    'echo "fetch statsd.stat_error"',
    'echo "fetch statsd.stat_cpu_wait"',
    'echo "fetch statsd.stat_cpu_busy"',
    'echo "fetch statsd.stat_tagged_counter_b"'
    'echo "close"'
    'echo "quit"'
]
# delay between commands <1 was returning "bad metric name"
composed_command = '; sleep 0.1; '.join(command_to_execute)
basic_parser_config = utils.configs["parser_type"][0]
ragel_parser_config = utils.configs["parser_type"][1]

duration_aggregation_basic_config = utils.configs["duration_aggregation_type"][0]
duration_aggregation_hdr_histogram_config = utils.configs["duration_aggregation_type"][1]

testconfigs = [basic_parser_config, ragel_parser_config, duration_aggregation_basic_config, duration_aggregation_hdr_histogram_config]

def run_test():
    utils.pmdastatsd_remove()
    utils.setup_dbpmdarc()
    command = '(sleep 8;' + composed_command + ') | sudo valgrind --trace-children=yes --leak-check=full --log-file=' + valgrind_out_path + ' dbpmda -e -q 60 -i 2>&1 >>' + dbpmda_out_path;
    for config in testconfigs:
        utils.print_test_section_separator()
        utils.set_config(config)
        p = Popen(command, cwd=utils.pmdastatsd_dir, stdout=PIPE, stdin=PIPE, bufsize=1, text=True, close_fds=ON_POSIX, shell=True)
        print('+++ pipe pid ' + str(p.pid)) #debug#
        time.sleep(4)
        # get pmdastatsdpid
        pmdastatsd_pid = utils.get_pmdastatsd_pids_ran_by_dbpmda()[0]
        print('+++ pmdastatsd pid ' + str(pmdastatsd_pid)) #debug#
        # send payloads
        for payload in payloads:
           sock.sendto(payload.encode("utf-8"), (ip, port))
        # wait to make sure the agent handles the payloads AND dbpmda gets delayed echo statements
        time.sleep(8)
        # trigger cleanup in agent by sending SIGINT
        utils.send_INT_to_pid(pmdastatsd_pid)
        valgrind_pmdastatsd_output = valgrind_out_path.replace("%p", pmdastatsd_pid)
        print('+++ ' + valgrind_pmdastatsd_output) #debug#
        print('+++ initial log size ' + str(os.path.getsize(valgrind_pmdastatsd_output))) #debug#
        # again, wait for cleanup
        time.sleep(5)
        # safe bet that pstree and echo -n are OK on any system where
        # the statsd PMDA is working
        os.system('echo -n "+++ "; pstree -l -p ' + str(p.pid)) #debug#
        # sometimes agent hangs due to dbpmda exit probably? Doesn't happen when its './Remove'd
        p.kill()
        print('+++ final log size ' + str(os.path.getsize(valgrind_pmdastatsd_output))) #debug#
        f = open(valgrind_pmdastatsd_output, "r")
        show_next_line = 0
        for line in f:
            if 'LEAK SUMMARY' in line:
                sys.stdout.write(line.replace("=={}==".format(pmdastatsd_pid), ""))
                show_next_line = 1
            elif show_next_line:
                sys.stdout.write(line.replace("=={}==".format(pmdastatsd_pid), ""))
                show_next_line = 0        
         
        # don't clean up valgrind output files ... leave that to the
        # QA test so we have a a chance to triage in the event of failure
         
        utils.restore_config()

run_test()


