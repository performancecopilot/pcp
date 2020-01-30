#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

# Exercises agent's memory handling with Valgrind
# TODO: complete the test

import sys
import socket
import glob
import os
import time
import subprocess

from subprocess import PIPE, Popen
from threading  import Thread

try:
    from queue import Queue, Empty
except ImportError:
    from Queue import Queue, Empty  # python 2.x

ON_POSIX = 'posix' in sys.builtin_module_names

utils_path = os.path.abspath(os.path.join("utils"))
sys.path.append(utils_path)

import pmdastatsd_test_utils as utils

utils.print_test_file_separator()
print(os.path.basename(__file__))

ip = "0.0.0.0"
port = 8125
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
valgrind_out_path = os.path.join(sys.argv[1], "valgrind.out")

payloads = [
    "test_labels:0|c", # no label
    "test_labels,tagX=X:1|c", # single label
    "test_labels,tagX=X,tagY=Y:2|c", # two labels
    "test_labels,tagC=C,tagB=B,tagA=A:3|c", # labels that will be ordered
    "test_labels:4|c|#A:A", # labels in dogstatsd-ruby format
    "test_labels,A=A:5|c|#B:B,C:C", # labels in dogstatsd-ruby format combined with standard format
    "test_labels,A=A,A=10:6|c" # labels with non-unique keys, right-most takes precedence,
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

basis_parser_config = os.path.join("configs", "single", "parser_type", "0", "pmdastatsd.ini")
ragel_parser_config = os.path.join("configs", "single", "parser_type", "1", "pmdastatsd.ini")

duration_aggregation_basic_config = os.path.join("configs", "single", "duration_aggregation_type", "0", "pmdastatsd.ini")
duration_aggregation_hdr_histogram_config = os.path.join("configs", "single", "duration_aggregation_type", "1", "pmdastatsd.ini")

testconfigs = [basis_parser_config, ragel_parser_config, duration_aggregation_basic_config, duration_aggregation_hdr_histogram_config]

def enqueue_output(out, queue):
    for line in iter(out.readline, b''):
        queue.put(line)
    out.close()

def run_test():
    utils.print_test_section_separator()
    utils.pmdastatsd_remove()

    utils.setup_dbpmdarc()
    utils.set_config(basis_parser_config)

    p = Popen('valgrind --trace-children=yes --leak-check=full --log-file=' + valgrind_out_path + ' dbpmda', cwd=utils.pmdastatsd_dir, stdout=PIPE, stdin=PIPE, bufsize=1, close_fds=ON_POSIX, shell=True)
    q = Queue()
    t = Thread(target=enqueue_output, args=(p.stdout, q))
    t.daemon = True # thread dies with the program
    t.start()
    # read line without blocking
    try:
        while p.poll() is None:
            line = q.get(timeout=10)
            if line == ".dbpmdarc> \n":
                break
    except Empty:
        print("Unable to get expected output from dbmda.")
        p.kill()
        return

    # get pmdastatsdpid
    pmdastatsd_pid = utils.get_pmdastatsd_pids_ran_by_dbpmda()[0]
    print("pmdastatsd: " + pmdastatsd_pid)
    # send payloads
    for payload in payloads:
        sock.sendto(payload.encode("utf-8"), (ip, port))
    # wait to make sure the agent handler the payloads
    time.sleep(5)
    # exercise populating pmns when metric is requested
    # p.stdin.write("fetch statsd.pmda.dropped")
    # p.stdin.flush()
    time.sleep(1)
    # p.stdin.write("fetch statsd.stat_cpu_wait")
    # p.stdin.flush()
    # time.sleep(1)
    # p.stdin.write("fetch statsd.stat_login")
    # p.stdin.flush()
    # time.sleep(1)
    # p.stdin.write("fetch statsd.stat_logout")
    # p.stdin.flush()
    time.sleep(1)
    # trigger cleanup in agent by sending SIGINT
    utils.send_INT_to_pid(pmdastatsd_pid)
    # again, wait for cleanup
    time.sleep(5)
    # attempt to get response after PMDA clean up
    try:
        while p.poll() is None:
            line = q.get(timeout=10)
            print(line)
    except Empty:
        print("Unable to get expected output from dbpma after fetching values.")
        p.kill()
        return
    utils.restore_config()

run_test()


