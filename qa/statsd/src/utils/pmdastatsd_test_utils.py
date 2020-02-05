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
pmdastatsd_dir = subprocess.check_output(get_pmdastatsd_dir_command, shell=True)
pmdastatsd_dir = pmdastatsd_dir.strip()

get_pmdastatsd_log_dir_command = 'echo $PCP_LOG_DIR/pmcd'
pmdastatsd_log_dir = subprocess.check_output(get_pmdastatsd_log_dir_command, shell=True)
pmdastatsd_log_dir = pmdastatsd_log_dir.strip()
pmdastatsd_log_path = pmdastatsd_log_dir + "/statsd.log"

pmdastatsd_config_filename = "pmdastatsd.ini"
pmdastatsd_config_backup_filename = "backup_pmdastatsd.ini"
dbpmdarc_filename = ".dbpmdarc"

# default as in, default that is distributed with PCP
default_config_file = os.path.join("configs", "complex", "0", "pmdastatsd.ini")

def pmdastatsd_install(config_file = ""):
	"""
	install pmdastatsd agent
	you may specify config file path
	"""
	if len(config_file) > 0:
		set_config(config_file)
	command = 'cd $PCP_PMDAS_DIR/statsd && sudo ./Install'
	results = subprocess.check_output(command, shell=True).strip()
	return results

def pmdastatsd_remove():
	"""removes pmdastatsd agent"""
	command = 'cd $PCP_PMDAS_DIR/statsd && sudo ./Remove'
	results = subprocess.check_output(command, shell=True).strip()
	return results

def backup_config():
	"""checkes if config file exists, if so, rename it as backup"""
	config_path = os.path.join(pmdastatsd_dir, pmdastatsd_config_filename)
	backup_config_path = os.path.join(pmdastatsd_dir, pmdastatsd_config_backup_filename)
	if os.path.exists(config_path):
		os.rename(config_path, backup_config_path)

def restore_config():
	"""restores old config to original name"""
	print("Restoring config file...");
	config_path = os.path.join(pmdastatsd_dir, pmdastatsd_config_filename)
	backup_config_path = os.path.join(pmdastatsd_dir, pmdastatsd_config_backup_filename)
	if os.path.exists(backup_config_path):
		os.rename(backup_config_path, config_path)

def set_config(path_to_config):
	"""moves test config file into pmdastatsd dir, old config is renamed"""
	if os.path.exists(path_to_config):
		backup_config()
		print("Setting config file... {}".format(path_to_config))
		shutil.copy(path_to_config, os.path.join(pmdastatsd_dir, pmdastatsd_config_filename))
	else:
		print("Error setting config file.")

def get_pmdastatsd_pids():
	"""returns pmdastatsd pid"""
	command = 'pgrep pmdastatsd'
	results = subprocess.check_output(command, shell=True)
	return results.strip().split('\n')

def get_pmdastatsd_pids_ran_by_dbpmda():
	"""returns pmdastatsd pid"""
	command = 'pgrep -f pmdastatsd'
	results = subprocess.check_output(command, shell=True)
	return results.strip().split('\n')

def get_dbpmda_pids():
	"""returns dbpmda pid"""
	command = 'pgrep -f dbpmda'
	results = subprocess.check_output(command, shell=True)
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
	results = subprocess.check_output(command.format(pid), shell=True)
	print(results)

def send_KILL_to_pid(pid):
	command = 'sudo kill -KILL {}'
	results = subprocess.check_output(command.format(pid), shell=True)
	print(results)

def send_debug_output_signal(pid):
	command = 'kill -USR1 {}'
	results = subprocess.check_output(command.format(pid), shell=True)

def get_debug_file(name):
	f = open(os.path.join(pmdastatsd_log_dir, "statsd_" + name), "r")
	contents = []
	# replace "time spent parsing" and "time spent aggregating" lines as results may vary
	for line in f:
		if 'time spent parsing' in line:
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
	results = subprocess.check_output(command.format(metric_name), shell=True)
	return results.strip()

def pminfo(str):
	"""pminfo wrapper."""
	command = 'pminfo {}'
	results = subprocess.check_output(command.format(str), shell=True)
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

def check_is_in_bounds(expected_value, measured_value, toleration_margin = 0.1):
    lower_bound = expected_value - (expected_value * toleration_margin)
    upper_bound = expected_value + (expected_value * toleration_margin)
    if measured_value >= lower_bound and measured_value <= upper_bound:
        return True
    return False

def print_metric(metric_name):
	"""fetches metric with a given name, printing out the response"""
	print(request_metric(metric_name))

def print_config_metrics():
	"""fetches config-related metrics, printing out the response"""
	command = 'pminfo statsd.pmda.settings -f'
	results = subprocess.check_output(command, shell=True)
	print(results.strip())

def print_test_section_separator():
	print("----------------------")

def print_test_file_separator():
	print("======================")