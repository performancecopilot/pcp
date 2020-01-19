#!/usr/bin/env pmpython
# -*- coding: utf-8 -*-

import sys
import socket
import subprocess
import os
import shutil

# get pmdastatsd dir
get_pmdastatsd_dir_command = 'echo $PCP_PMDAS_DIR/statsd'
pmdastatsd_dir = subprocess.check_output(get_pmdastatsd_dir_command, shell=True)
pmdastatsd_dir = pmdastatsd_dir.strip()
pmdastatsd_config_filename = "pmdastatsd.ini"
pmdastatsd_config_backup_filename = "backup_pmdastatsd.ini"

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

def request_metric(metric_name):
	"""fetches metric with a given name."""
	command = 'pminfo {} -f'
	results = subprocess.check_output(command.format(metric_name, shell=True))
	return results.strip()

def print_metric(metric_name):
	"""fetches metric with a given name, printing out the response"""
	command = 'pminfo {} -f'
	results = subprocess.check_output(command.format(metric_name), shell=True)
	print(results.strip())

def print_config_metrics():
	"""fetches config-related metrics, printing out the response"""
	command = 'pminfo statsd.pmda.settings -f'
	results = subprocess.check_output(command, shell=True)
	print(results.strip())

def print_test_section_separator():
	print("----------------------")