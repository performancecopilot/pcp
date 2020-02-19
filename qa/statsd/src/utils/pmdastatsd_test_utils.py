#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

import sys
import socket
import subprocess
import os
import shutil
import re		
import collections

# get pmdastatsd dir
get_pmdastatsd_dir_command = 'echo $PCP_PMDAS_DIR/statsd'
pmdastatsd_dir = subprocess.check_output(get_pmdastatsd_dir_command, shell=True).decode()
pmdastatsd_dir = pmdastatsd_dir.strip()

get_pmdastatsd_log_dir_command = 'echo $PCP_LOG_DIR/pmcd'
pmdastatsd_log_dir = subprocess.check_output(get_pmdastatsd_log_dir_command, shell=True).decode()
pmdastatsd_log_dir = pmdastatsd_log_dir.strip()
pmdastatsd_log_path = pmdastatsd_log_dir + "/statsd.log"

pmdastatsd_config_filename = "pmdastatsd.ini"
pmdastatsd_config_backup_filename = "backup_pmdastatsd.ini"
dbpmdarc_filename = ".dbpmdarc"

configs = {
	"default": """
[global]
max_udp_packet_size = 1472
port = 8125
max_unprocessed_packets = 1024
parser_type = 0
verbose = 0
debug = 0
debug_output_filename = debug
duration_aggregation_type = 1
""",
	"empty": "",
	"debug_output_filename": [
"""
[global]
debug_output_filename = debug
""",
"""
[global]
debug_output_filename = debug_test
"""],
	"duration_aggregation_type": [
"""
[global]
duration_aggregation_type = 0
""",
"""
[global]
duration_aggregation_type = 1
"""],
	"max_udp_packet_size": [
"""
[global]
max_udp_packet_size = 1472
""",
"""
[global]
max_udp_packet_size = 2944
""",
"""
[global]
max_udp_packet_size = 10
"""],
	"max_unprocessed_packets": [
"""
[global]
max_unprocessed_packets = 2048
""",
"""
[global]
max_unprocessed_packets = 1024
"""],
	"parser_type": [
"""
[global]
parser_type = 0
""",
"""
[global]
parser_type = 1
"""],
	"port": [
"""
[global]
port = 8125
""",
"""
[global]
port = 8126
"""],
	"verbose": [
"""
[global]
verbose = 0
""",
"""
[global]
verbose = 1
""",
"""
[global]
verbose = 2
"""]
}

def pmdastatsd_install(config_file = ""):
	"""
	install pmdastatsd agent
	you may specify config file path
	"""
	if len(config_file) > 0:
		set_config(config_file)
	command = 'cd $PCP_PMDAS_DIR/statsd && sudo ./Install'
	results = subprocess.check_output(command, shell=True).decode().strip()
	return results

def pmdastatsd_remove():
	"""removes pmdastatsd agent"""
	command = 'cd $PCP_PMDAS_DIR/statsd && sudo ./Remove'
	results = subprocess.check_output(command, shell=True).decode().strip()
	return results

def restore_config():
	"""restores old config to original name"""
	print("Restoring config file...");
	set_config(configs["default"], False)

def set_config(config, verbose = True):
	"""sets config file in pmdastatsd dir, old config is renamed"""
	if os.path.exists(os.path.join(pmdastatsd_dir, pmdastatsd_config_filename)):
		if verbose:
			print("Setting config:")
			print("~~~")
		print(config)
		if verbose:
			print("~~~")
		f = open(os.path.join(pmdastatsd_dir, pmdastatsd_config_filename), "w")
		f.write(config)
		f.close()
	else:
		print("Error setting config file.")

def get_pmdastatsd_pids():
	"""returns pmdastatsd pid"""
	command = 'pgrep pmdastatsd'
	results = subprocess.check_output(command, shell=True).decode()
	return results.strip().split('\n')

def get_pmdastatsd_pids_ran_by_dbpmda():
	"""returns pmdastatsd pid"""
	command = 'pgrep -f pmdastatsd'
	results = subprocess.check_output(command, shell=True).decode()
	return results.strip().split('\n')

def get_dbpmda_pids():
	"""returns dbpmda pid"""
	command = 'pgrep -f dbpmda'
	results = subprocess.check_output(command, shell=True).decode()
	return results.strip().split('\n')

def setup_dbpmdarc():
	f = open(os.path.join(pmdastatsd_dir, dbpmdarc_filename), "w+")
	f.write("debug libpmda\n")
	f.write("open pipe pmdastatsd\n")
	f.write("namespace root_statsd\n")
	f.write("status\n")
	f.write("\n")
	f.close()

def remove_dbpmdarc():
	os.remove(os.path.join(pmdastatsd_dir, dbpmdarc_filename))

def send_INT_to_pid(pid):
	command = 'sudo kill -INT {}'
	results = subprocess.check_output(command.format(pid), shell=True).decode()
	print(results)

def send_KILL_to_pid(pid):
	command = 'sudo kill -KILL {}'
	results = subprocess.check_output(command.format(pid), shell=True).decode()
	print(results)

def send_debug_output_signal(pid):
	command = 'kill -USR1 {}'
	results = subprocess.check_output(command.format(pid), shell=True).decode()

# TODO: make output stable/deterministic
def get_debug_file(name):
	file_path = os.path.join(pmdastatsd_log_dir, "statsd_" + name) 
	f = open(file_path, "r")
	contents = []
	# replace "time spent parsing" and "time spent aggregating" lines as results may vary
	for line in f:
		if 'Label:' in line:
			pass
		elif 'pmid =' in line:
			contents.append("pmid = <filtered>\n")
		elif 'time spent parsing' in line:
			contents.append("time spent parsing: <filtered>\n")
		elif 'time spent aggregating' in line:
			contents.append("time spent aggregating: <filtered>\n")
		else:
			contents.append(line)
	f.close()
	return ''.join(contents)

def remove_debug_file(name):
	os.remove(os.path.join(pmdastatsd_log_dir, "statsd_" + name))

def request_metric(metric_name):
	"""fetches metric value with a given name."""
	command = 'pminfo {} -f'
	results = subprocess.check_output(command.format(metric_name), shell=True).decode()
	return results.strip()

def pminfo(str):
	"""pminfo wrapper."""
	command = 'pminfo {}'
	results = subprocess.check_output(command.format(str), shell=True).decode()
	return results.strip()

def get_instances(request_output):
	# good enough, not the best
	instance_lines = [line.strip() for line in request_output.split('\n') if "inst [" in line]
	if len(instance_lines) == 0:
		return collections.OrderedDict()
	instances = {}
	for line in instance_lines:
		# again, heuristics good enough
		key = re.findall(r"\"(.*)\"", line)[0]
		value = re.findall(r"(?<=\"\] value ).*", line)[0]
		instances[key] = value
	d = collections.OrderedDict(sorted(instances.items()))
	return d

def check_is_in_bounds(expected_value, measured_value, toleration_margin = 0.35):
    lower_bound = expected_value - (expected_value * toleration_margin)
    upper_bound = expected_value + (expected_value * toleration_margin)
    if measured_value >= lower_bound and measured_value <= upper_bound:
        return True
    return False

def check_is_in_range(expected_max, expected_min, measured_value):
	return measured_value >= expected_min and measured_value <= expected_max

def print_metric(metric_name):
	"""fetches metric with a given name, printing out the response"""
	print(request_metric(metric_name))

def print_config_metrics():
	"""fetches config-related metrics, printing out the response"""
	command = 'pminfo statsd.pmda.settings -f'
	results = subprocess.check_output(command, shell=True).decode()
	print(results.strip())

def print_test_section_separator():
	print("----------------------")

def print_test_file_separator():
	print("======================")