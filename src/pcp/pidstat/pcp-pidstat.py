#!/usr/bin/env pmpython
#
# Copyright (C) 2020 Red Hat.
# Copyright (C) 2017 Alperen Karaoglu.
# Copyright (C) 2016 Sitaram Shelke.
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
# pylint: disable=bad-whitespace,too-many-arguments,too-many-lines
# pylint: disable=redefined-outer-name,unnecessary-lambda
#

import os
import sys
import re
import time
from pcp import pmcc
from pcp import pmapi

process_state_info = {}
# Metric list to be fetched
PIDSTAT_METRICS = [
    'hinv.ncpu',
    'kernel.all.cpu.guest',
    'kernel.all.cpu.idle',
    'kernel.all.cpu.nice',
    'kernel.all.cpu.sys',
    'kernel.all.cpu.user',
    'kernel.all.cpu.vuser',
    'kernel.uname.machine',
    'kernel.uname.nodename',
    'kernel.uname.release',
    'kernel.uname.sysname',
    'mem.physmem',
    'proc.id.uid',
    'proc.id.uid_nm',
    'proc.memory.vmstack',
    'proc.nprocs',
    'proc.psinfo.cmd',
    'proc.psinfo.guest_time',
    'proc.psinfo.maj_flt',
    'proc.psinfo.minflt',
    'proc.psinfo.pid',
    'proc.psinfo.policy',
    'proc.psinfo.processor',
    'proc.psinfo.rss',
    'proc.psinfo.rt_priority',
    'proc.psinfo.stime',
    'proc.psinfo.utime',
    'proc.psinfo.vsize',
]

PIDSTAT_METRICS_B = PIDSTAT_METRICS + [
    'proc.psinfo.sname',
    'proc.psinfo.start_time',
    'proc.psinfo.wchan_s',
]

PIDSTAT_METRICS_L = PIDSTAT_METRICS_B + [
    'proc.psinfo.psargs'
]

#We define a new metric array so that some missing metrics aren't flagged in existing archives using PIDSTAT_METRICS


SCHED_POLICY = ['NORMAL','FIFO','RR','BATCH','','IDLE','DEADLINE']

class StdoutPrinter:
    def Print(self, args):
        print(args)

# After fetching non singular metric values, create a mapping of instance id
# to instance value rather than instance name to instance value.
# The reason is, in PCP, instance names require a separate pmGetIndom() request
# and some of the names may not be available.
class ReportingMetricRepository:
    def __init__(self,group):
        self.group = group
        self.current_cached_values = {}
        self.previous_cached_values = {}
    def __fetch_current_values(self,metric,instance):
        if instance:
            return dict(map(lambda x: (x[0].inst, x[2]), self.group[metric].netValues))
        else:
            return self.group[metric].netValues[0][2]

    def __fetch_previous_values(self,metric,instance):
        if instance:
            return dict(map(lambda x: (x[0].inst, x[2]), self.group[metric].netPrevValues))
        else:
            return self.group[metric].netPrevValues[0][2]

    def current_value(self, metric, instance):
        if not metric in self.group:
            return None
        if instance:
            if self.current_cached_values.get(metric, None) is None:
                lst = self.__fetch_current_values(metric,instance)
                self.current_cached_values[metric] = lst

            return self.current_cached_values[metric].get(instance,None)
        else:
            if self.current_cached_values.get(metric, None) is None:
                self.current_cached_values[metric] = self.__fetch_current_values(metric,instance)
            return self.current_cached_values.get(metric, None)

    def previous_value(self, metric, instance):
        if not metric in self.group:
            return None
        if instance:
            if self.previous_cached_values.get(metric, None) is None:
                lst = self.__fetch_previous_values(metric,instance)
                self.previous_cached_values[metric] = lst

            return self.previous_cached_values[metric].get(instance,None)
        else:
            if self.previous_cached_values.get(metric, None) is None:
                self.previous_cached_values[metric] = self.__fetch_previous_values(metric,instance)
            return self.previous_cached_values.get(metric, None)

    def current_values(self, metric_name):
        if self.group.get(metric_name, None) is None:
            return None
        if self.current_cached_values.get(metric_name, None) is None:
            self.current_cached_values[metric_name] = self.__fetch_current_values(metric_name,True)
        return self.current_cached_values.get(metric_name, None)

    def previous_values(self, metric_name):
        if self.group.get(metric_name, None) is None:
            return None
        if self.previous_cached_values.get(metric_name, None) is None:
            self.previous_cached_values[metric_name] = self.__fetch_previous_values(metric_name,True)
        return self.previous_cached_values.get(metric_name, None)

class ProcessCpuUsage:
    def __init__(self, instance, delta_time, metrics_repository):
        self.instance = instance
        self.__delta_time = delta_time
        self.__metric_repository = metrics_repository

    def user_percent(self):
        c_usertime = self.__metric_repository.current_value('proc.psinfo.utime', self.instance)
        p_usertime = self.__metric_repository.previous_value('proc.psinfo.utime', self.instance)
        if c_usertime is not None and p_usertime is not None:
            percent_of_time =  100 * float(c_usertime - p_usertime) / float(1000 * self.__delta_time)
            return float("%.2f"%percent_of_time)
        else:
            return None

    def guest_percent(self):
        c_guesttime = self.__metric_repository.current_value('proc.psinfo.guest_time', self.instance)
        p_guesttime = self.__metric_repository.previous_value('proc.psinfo.guest_time', self.instance)
        if  c_guesttime is not None and p_guesttime is not None:
            percent_of_time =  100 * float(c_guesttime - p_guesttime) / float(1000 * self.__delta_time)
            return float("%.2f"%percent_of_time)
        else:
            return None

    def system_percent(self):
        c_systemtime = self.__metric_repository.current_value('proc.psinfo.stime', self.instance)
        p_systemtime = self.__metric_repository.previous_value('proc.psinfo.stime', self.instance)
        if  c_systemtime is not None and p_systemtime is not None:
            percent_of_time =  100 * float(c_systemtime - p_systemtime) / float(1000 * self.__delta_time)
            return float("%.2f"%percent_of_time)
        else:
            return None

    def total_percent(self):
        if self.user_percent() is not None and self.guest_percent() is not None and self.system_percent() is not None:
            return float("%.2f"%(self.user_percent()+self.guest_percent()+self.system_percent()))
        else:
            return None

    def pid(self):
        return self.__metric_repository.current_value('proc.psinfo.pid', self.instance)

    def process_name(self):
        return self.__metric_repository.current_value('proc.psinfo.cmd', self.instance)

    def process_name_with_args(self):
        return self.__metric_repository.current_value('proc.psinfo.psargs', self.instance)

    def cpu_number(self):
        return self.__metric_repository.current_value('proc.psinfo.processor', self.instance)

    def user_id(self):
        return self.__metric_repository.current_value('proc.id.uid', self.instance)

    def user_name(self):
        return self.__metric_repository.current_value('proc.id.uid_nm', self.instance)

class CpuUsage:
    def __init__(self, metric_repository):
        self.__metric_repository = metric_repository

    def get_processes(self, delta_time):
        return map(lambda pid: (ProcessCpuUsage(pid,delta_time,self.__metric_repository)), self.__pids())

    def __pids(self):
        pid_dict = self.__metric_repository.current_values('proc.psinfo.pid')
        return sorted(pid_dict.values())


class ProcessPriority:
    def __init__(self, instance, metrics_repository):
        self.instance = instance
        self.__metric_repository = metrics_repository

    def pid(self):
        return self.__metric_repository.current_value('proc.psinfo.pid', self.instance)

    def user_id(self):
        return self.__metric_repository.current_value('proc.id.uid', self.instance)

    def process_name(self):
        return self.__metric_repository.current_value('proc.psinfo.cmd', self.instance)

    def process_name_with_args(self):
        return self.__metric_repository.current_value('proc.psinfo.psargs', self.instance)

    def priority(self):
        return self.__metric_repository.current_value('proc.psinfo.rt_priority', self.instance)

    def policy_int(self):
        return self.__metric_repository.current_value('proc.psinfo.policy', self.instance)

    def policy(self):
        policy_int = self.__metric_repository.current_value('proc.psinfo.policy', self.instance)
        if policy_int is not None:
            return SCHED_POLICY[policy_int]
        return None

    def user_name(self):
        return self.__metric_repository.current_value('proc.id.uid_nm', self.instance)

class CpuProcessPriorities:
    def __init__(self, metric_repository):
        self.__metric_repository = metric_repository
    def get_processes(self):
        return map((lambda pid: (ProcessPriority(pid,self.__metric_repository))), self.__pids())

    def __pids(self):
        pid_dict = self.__metric_repository.current_values('proc.psinfo.pid')
        return sorted(pid_dict.values())

class ProcessMemoryUtil:
    def __init__(self, instance, delta_time,  metric_repository):
        self.instance = instance
        self.__metric_repository = metric_repository
        self.delta_time = delta_time

    def pid(self):
        return self.__metric_repository.current_value('proc.psinfo.pid', self.instance)

    def user_id(self):
        return self.__metric_repository.current_value('proc.id.uid', self.instance)

    def process_name(self):
        return self.__metric_repository.current_value('proc.psinfo.cmd', self.instance)

    def process_name_with_args(self):
        return self.__metric_repository.current_value('proc.psinfo.psargs', self.instance)

    def minflt(self):
        c_min_flt = self.__metric_repository.current_value('proc.psinfo.minflt', self.instance)
        p_min_flt = self.__metric_repository.previous_value('proc.psinfo.minflt', self.instance)
        if c_min_flt is not None and p_min_flt is not None:
            return float("%.2f" % ((c_min_flt - p_min_flt)/self.delta_time))
        else:
            return None

    def majflt(self):
        c_maj_flt = self.__metric_repository.current_value('proc.psinfo.maj_flt', self.instance)
        p_maj_flt = self.__metric_repository.previous_value('proc.psinfo.maj_flt', self.instance)
        if c_maj_flt is not None and p_maj_flt is not None:
            return float("%.2f" % ((c_maj_flt - p_maj_flt)/self.delta_time))
        else:
            return None

    def vsize(self):
        return self.__metric_repository.current_value('proc.psinfo.vsize', self.instance)

    def rss(self):
        return self.__metric_repository.current_value('proc.psinfo.rss', self.instance)

    def mem(self):
        total_mem = self.__metric_repository.current_value('mem.physmem', None)
        rss = self.__metric_repository.current_value('proc.psinfo.rss', self.instance)
        if total_mem is not None and rss is not None:
            return float("%.2f" % (100*float(rss)/total_mem))
        else:
            return None

    def user_name(self):
        return self.__metric_repository.current_value('proc.id.uid_nm', self.instance)

class CpuProcessMemoryUtil:
    def __init__(self, metric_repository):
        self.__metric_repository = metric_repository

    def get_processes(self, delta_time):
        return map((lambda pid: (ProcessMemoryUtil(pid, delta_time, self.__metric_repository))), self.__pids())

    def __pids(self):
        pid_dict = self.__metric_repository.current_values('proc.psinfo.pid')
        return sorted(pid_dict.values())

class ProcessStackUtil:
    def __init__(self, instance, metric_repository):
        self.instance = instance
        self.__metric_repository = metric_repository

    def pid(self):
        return self.__metric_repository.current_value('proc.psinfo.pid', self.instance)

    def user_id(self):
        return self.__metric_repository.current_value('proc.id.uid', self.instance)

    def process_name(self):
        return self.__metric_repository.current_value('proc.psinfo.cmd', self.instance)

    def process_name_with_args(self):
        return self.__metric_repository.current_value('proc.psinfo.psargs', self.instance)

    def stack_size(self):
        return self.__metric_repository.current_value('proc.memory.vmstack', self.instance)

    def user_name(self):
        return self.__metric_repository.current_value('proc.id.uid_nm', self.instance)


class CpuProcessStackUtil:
    def __init__(self, metric_repository):
        self.__metric_repository = metric_repository

    def get_processes(self):
        return map((lambda pid: (ProcessStackUtil(pid, self.__metric_repository))), self.__pids())

    def __pids(self):
        pid_dict = self.__metric_repository.current_values('proc.psinfo.pid')
        return sorted(pid_dict.values())

# ==============================================================================
# process state reporting

class ProcessState:
#    def __init__(self, instance, metric_repository):
    def __init__(self, instance, delta_time,  metric_repository):
        self.instance = instance
        self.__metric_repository = metric_repository

    def pid(self):
        return self.__metric_repository.current_value('proc.psinfo.pid', self.instance)

    def user_id(self):
        return self.__metric_repository.current_value('proc.id.uid', self.instance)

    def process_name(self):
        return self.__metric_repository.current_value('proc.psinfo.cmd', self.instance)

    def process_name_with_args(self):
        return self.__metric_repository.current_value('proc.psinfo.psargs', self.instance)

    def s_name(self):
        return self.__metric_repository.current_value('proc.psinfo.sname', self.instance)

    def start_time(self):
        return self.__metric_repository.current_value('proc.psinfo.start_time', self.instance)

    def wchan_s(self):
        return self.__metric_repository.current_value('proc.psinfo.wchan_s', self.instance)

    def user_name(self):
        return self.__metric_repository.current_value('proc.id.uid_nm', self.instance)

    #def process_blocked(self):
    #   return self.__metric_repository.current_value('proc.psinfo.blocked', self.instance)

    def utime(self):
        #return self.__metric_repository.current_value('proc.psinfo.utime', self.instance)
        c_usertime = self.__metric_repository.current_value('proc.psinfo.utime', self.instance)
        p_usertime = self.__metric_repository.previous_value('proc.psinfo.utime', self.instance)
        # sometimes the previous_value seems to be Nonetype, not sure why
        if p_usertime is None: # print a '?' here
            #return c_usertime
            return '?'
        else:
            return c_usertime - p_usertime

    def stime(self):
        c_systime = self.__metric_repository.current_value('proc.psinfo.stime', self.instance)
        p_systime = self.__metric_repository.previous_value('proc.psinfo.stime', self.instance)
        # sometimes the previous_value seems to be Nonetype, not sure why
        if p_systime is None: # print a '?' here
            return '?'
        else:
            return c_systime - p_systime


class CpuProcessState:
    def __init__(self, metric_repository):
        self.__metric_repository = metric_repository

#   def get_processes(self):
#        return map((lambda pid: (ProcessState(pid, self.__metric_repository))), self.__pids())
    def get_processes(self, delta_time):
#       return map(lambda pid: (ProcessState(pid,delta_time,self.__metric_repository)), self.__pids())
        return map((lambda pid: (ProcessState(pid, delta_time, self.__metric_repository))), self.__pids())

    def __pids(self):
        pid_dict = self.__metric_repository.current_values('proc.psinfo.pid')
        return sorted(pid_dict.values())

class CpuProcessStateReporter:
    def __init__(self, process_state, process_filter, delta_time, printer, pidstat_options):
        self.process_state = process_state
        self.process_filter = process_filter
        self.printer = printer
        self.pidstat_options = pidstat_options
        self.delta_time = delta_time

#TODO: SORTING
# for sorting this report, we need to put every process info in a table
# without printing individual processes as we run through the for loop below,
# then sort as required based on which field we need to sort on, and then
# display that entire table for each time print_report is called.

    def print_report(self, timestamp, header_indentation, value_indentation):
        #if not "detail" == self.pidstat_options.filterstate:
        #    self.printer ("\nTimestamp" + "\tPID\tState\tUtime\tStime\tTotal Time\tFunction\t\t\tCommand")
        #else:
        #    self.printer ("\nTimestamp" + "\tPID\t\tR\t\tS\t\tZ\t\tT\t\tD\t\tCommand")

        # Print out the header only if there are entries for that particular iteration
        # So this print statement is moved inside the loop protected by a flag print_once
        print_once = 0

#       processes = self.process_filter.filter_processes(self.process_state.get_processes())
        processes = self.process_filter.filter_processes(self.process_state.get_processes(self.delta_time))
        #print self.pidstat_options.filterstate
        for process in processes:
            current_process_sname = process.s_name()
            if self.pidstat_options.filterstate != "detail":
                if not "all" in self.pidstat_options.filterstate:
                    if not current_process_sname in self.pidstat_options.filterstate:
                        # if not detailed report (which shows all states),
                        # and we dont find "all" after -B, filter out the processes
                        # in states we dont want
                        continue
            current_process_pid = process.pid()
            # tuple key to map to 1-d dictionary of {(pid,state):total_time}
            key = (current_process_sname,current_process_pid)
            if key in process_state_info:
                process_state_info[key]=process_state_info[key] + self.delta_time
            else:
                process_state_info[key]=self.delta_time
# ------------------------------------------------------------------------------
# TODO : need to add the ability to filter by username
            #if self.pidstat_options.show_process_user:
            #    self.printer("%s%s%s\t%s\t%s\t%s" %
            #                 (timestamp, value_indentation, process.user_name(),
            #                  process.pid(), process.process_name(),
            #                  process.s_name(), process.start_time(),
            #                  process.wchan_s(), process.process_blocked()))
            #else:
# ------------------------------------------------------------------------------
            if self.pidstat_options.filterstate != "detail":
                if print_once == 0:
                    print_once = 1 # dont print ever again for this loop iteration
                    self.printer ("\nTimestamp" + "\tPID\tState\tUtime\tStime\tTotal Time\tFunction\t\t\tCommand")
                if process.s_name() == 'R':
                    func = "N/A"  # if a process is running, there will be no function it's waiting on
                elif process.s_name is None:
                    func = "?"
                else:
                    func = process.wchan_s()
                if process.s_name() is not None and (len(process.wchan_s()) < 8 or func == "N/A"):
                    if self.pidstat_options.process_name_with_args:
                        self.printer("%s\t%s\t%s\t%s\t%s\t%.2f\t\t%s\t\t\t\t%s" %
                                     (timestamp, process.pid(), process.s_name(),
                                      process.utime(), process.stime(),
                                      process_state_info[key], func,
                                      process.process_name_with_args()))
                    else:
                        self.printer("%s\t%s\t%s\t%s\t%s\t%.2f\t\t%s\t\t\t\t%s" %
                                     (timestamp, process.pid(), process.s_name(),
                                      process.utime(), process.stime(),
                                      process_state_info[key], func,
                                      process.process_name()))
                elif process.s_name() is not None and (len(process.wchan_s()) < 16 or func == "N/A"):
                    if self.pidstat_options.process_name_with_args:
                        self.printer("%s\t%s\t%s\t%s\t%s\t%.2f\t\t%s\t\t\t%s" %
                                     (timestamp, process.pid(), process.s_name(),
                                      process.utime(), process.stime(),
                                      process_state_info[key], func,
                                      process.process_name_with_args()))
                    else:
                        self.printer("%s\t%s\t%s\t%s\t%s\t%.2f\t\t%s\t\t\t%s" %
                                     (timestamp, process.pid(), process.s_name(),
                                      process.utime(), process.stime(),
                                      process_state_info[key], func,
                                      process.process_name()))
                elif process.s_name() is not None and len(process.wchan_s()) < 24:
                    if self.pidstat_options.process_name_with_args:
                        self.printer("%s\t%s\t%s\t%s\t%s\t%.2f\t\t%s\t\t%s" %
                                     (timestamp, process.pid(), process.s_name(),
                                      process.utime(), process.stime(),
                                      process_state_info[key], func,
                                      process.process_name_with_args()))
                    else:
                        self.printer("%s\t%s\t%s\t%s\t%s\t%.2f\t\t%s\t\t%s" %
                                     (timestamp, process.pid(), process.s_name(),
                                      process.utime(), process.stime(),
                                      process_state_info[key], func,
                                      process.process_name()))
                else:
                    if process.s_name() is not None:
                        self.printer("%s\t%s\t%s\t%s\t%s\t%.2f\t\t%s\t%s" %
                                     (timestamp, process.pid(), process.s_name(),
                                      process.utime(), process.stime(),
                                      process_state_info[key], func,
                                      process.process_name()))
                #continue
            else:
                if print_once == 0:
                    print_once = 1 # dont print again for loop iteration
                    self.printer("\nTimestamp" + "\tPID\t\tR\t\tS\t\tZ\t\tT\t\tD\t\tCommand")
                # show detailed report of processes with accumulated timings in each state
                key1 = ("R", current_process_pid)
                key2 = ("S", current_process_pid)
                key3 = ("Z", current_process_pid)
                key4 = ("T", current_process_pid)
                key5 = ("D", current_process_pid)
                R = process_state_info.get(key1, 0)
                S = process_state_info.get(key2, 0)
                Z = process_state_info.get(key3, 0)
                T = process_state_info.get(key4, 0)
                D = process_state_info.get(key5, 0)
                if self.pidstat_options.process_name_with_args:
                    self.printer("%s\t%s\t\t%.2f\t\t%.2f\t\t%.2f\t\t%.2f\t\t%.2f\t\t%s" %
                                 (timestamp, process.pid(), R, S, Z, T, D,
                                  process.process_name_with_args()))
                else:
                    self.printer("%s\t%s\t\t%.2f\t\t%.2f\t\t%.2f\t\t%.2f\t\t%.2f\t\t%s" %
                                 (timestamp, process.pid(), R, S, Z, T, D,
                                  process.process_name()))

#===============================================================================

class ProcessFilter:
    def __init__(self,options):
        self.options = options

    def filter_processes(self, processes):
        return filter(lambda p: self.__predicate(p), processes)

    def __predicate(self, process):
        return bool(self.__matches_process_username(process)
                    and self.__matches_process_pid(process)
                    and self.__matches_process_name(process)
                    and self.__matches_process_priority(process)
                    and self.__matches_process_memory_util(process)
                    and self.__matches_process_stack_size(process))

    def __matches_process_username(self, process):
        if self.options.filtered_process_user is not None:
            return self.options.filtered_process_user == process.user_name()
        return True

    def __matches_process_pid(self, process):
        if self.options.pid_filter is not None:
            pid = process.pid()
            return bool(pid in self.options.pid_list)
        return True

    def __matches_process_name(self, process):
        name = process.process_name()
        if name is None:
            return False # no match
        if self.options.process_name is not None:
            return re.search(self.options.process_name, name)
        return True # all match

    def __matches_process_priority(self, process):
        if self.options.show_process_priority and process.priority() is not None:
            return process.priority() > 0
        return True

    def __matches_process_memory_util(self, process):
        if self.options.show_process_memory_util and process.vsize() is not None:
            return process.vsize() > 0
        return True

    def __matches_process_stack_size(self, process):
        if self.options.show_process_stack_util and process.stack_size() is not None:
            return process.stack_size() > 0
        return True

class CpuUsageReporter:
    def __init__(self, cpu_usage, process_filter, delta_time, printer, pidstat_options):
        self.cpu_usage = cpu_usage
        self.process_filter = process_filter
        self.printer = printer
        self.delta_time = delta_time
        self.pidstat_options = pidstat_options

    def print_report(self, timestamp, ncpu, header_indentation, value_indentation):
        if self.pidstat_options.show_process_user:
            self.printer("Timestamp" + header_indentation +
                         "UName\tPID\tusr\tsystem\tguest\t%CPU\tCPU\tCommand")
        else:
            self.printer("Timestamp" + header_indentation +
                         "UID\tPID\tusr\tsystem\tguest\t%CPU\tCPU\tCommand")
        processes = self.process_filter.filter_processes(self.cpu_usage.get_processes(self.delta_time))
        for process in processes:
            user_percent = process.user_percent()
            guest_percent = process.guest_percent()
            system_percent = process.system_percent()
            total_percent = process.total_percent()

            if self.pidstat_options.per_processor_usage and total_percent is not None:
                total_percent = float("%.2f"%(total_percent/ncpu))

            if self.pidstat_options.show_process_user:
                if self.pidstat_options.process_name_with_args:
                    self.printer("%s%s%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_name(),
                                  process.pid(), user_percent, system_percent,
                                  guest_percent, total_percent, process.cpu_number(),
                                  process.process_name_with_args()))
                else:
                    self.printer("%s%s%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_name(),
                                  process.pid(), user_percent, system_percent,
                                  guest_percent, total_percent, process.cpu_number(),
                                  process.process_name()))
            else:
                if self.pidstat_options.process_name_with_args:
                    self.printer("%s%s%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_id(),
                                  process.pid(), user_percent, system_percent,
                                  guest_percent, total_percent, process.cpu_number(),
                                  process.process_name_with_args()))
                else:
                    self.printer("%s%s%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_id(),
                                  process.pid(), user_percent, system_percent,
                                  guest_percent, total_percent, process.cpu_number(),
                                  process.process_name()))

class CpuProcessPrioritiesReporter:
    def __init__(self, process_priority, process_filter, printer, pidstat_options):
        self.process_priority = process_priority
        self.process_filter = process_filter
        self.printer = printer
        self.pidstat_options = pidstat_options

    def print_report(self, timestamp, header_indentation, value_indentation):
        self.printer("Timestamp" + header_indentation +
                     "UID\tPID\tprio\tpolicy\tCommand")
        processes = self.process_filter.filter_processes(self.process_priority.get_processes())
        for process in processes:
            if self.pidstat_options.show_process_user:
                if self.pidstat_options.process_name_with_args:
                    self.printer("%s%s%s\t%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_name(),
                                  process.pid(), process.priority(), process.policy(),
                                  process.process_name_with_args()))
                else:
                    self.printer("%s%s%s\t%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_name(),
                                  process.pid(), process.priority(), process.policy(),
                                  process.process_name()))
            else:
                if self.pidstat_options.process_name_with_args:
                    self.printer("%s%s%s\t%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_id(),
                                  process.pid(), process.priority(), process.policy(),
                                  process.process_name_with_args()))
                else:
                    self.printer("%s%s%s\t%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_id(),
                                  process.pid(), process.priority(), process.policy(),
                                  process.process_name()))

class CpuProcessMemoryUtilReporter:
    def __init__(self, process_memory_util, process_filter, delta_time, printer, pidstat_options):
        self.process_memory_util = process_memory_util
        self.process_filter = process_filter
        self.printer = printer
        self.delta_time = delta_time
        self.pidstat_options = pidstat_options

    def print_report(self, timestamp, header_indentation, value_indentation):
        self.printer("Timestamp" + header_indentation +
                     "UID\tPID\tMinFlt/s\tMajFlt/s\tVSize\tRSS\t%Mem\tCommand")
        processes = self.process_filter.filter_processes(self.process_memory_util.get_processes(self.delta_time))
        for process in processes:
            maj_flt = process.majflt()
            min_flt = process.minflt()
            if self.pidstat_options.show_process_user:
                if self.pidstat_options.process_name_with_args:
                    self.printer("%s%s%s\t%s\t%s\t\t%s\t\t%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_name(),
                                  process.pid(), min_flt, maj_flt, process.vsize(),
                                  process.rss(), process.mem(),
                                  process.process_name_with_args()))
                else:
                    self.printer("%s%s%s\t%s\t%s\t\t%s\t\t%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_name(),
                                  process.pid(), min_flt, maj_flt, process.vsize(),
                                  process.rss(), process.mem(), process.process_name()))
            else:
                if self.pidstat_options.process_name_with_args:
                    self.printer("%s%s%s\t%s\t%s\t\t%s\t\t%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_id(),
                                  process.pid(), min_flt, maj_flt, process.vsize(),
                                  process.rss(), process.mem(),
                                  process.process_name_with_args()))
                else:
                    self.printer("%s%s%s\t%s\t%s\t\t%s\t\t%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation,process.user_id(),
                                  process.pid(), min_flt, maj_flt, process.vsize(),
                                  process.rss(), process.mem(), process.process_name()))

class CpuProcessStackUtilReporter:
    def __init__(self, process_stack_util, process_filter, printer, pidstat_options):
        self.process_stack_util = process_stack_util
        self.process_filter = process_filter
        self.printer = printer
        self.pidstat_options = pidstat_options

    def print_report(self, timestamp, header_indentation, value_indentation):
        self.printer ("Timestamp" + header_indentation + "UID\tPID\tStkSize\tCommand")
        processes = self.process_filter.filter_processes(self.process_stack_util.get_processes())
        for process in processes:
            if self.pidstat_options.show_process_user:
                if self.pidstat_options.process_name_with_args:
                    self.printer("%s%s%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_name(),
                                  process.pid(), process.stack_size(),
                                  process.process_name_with_args()))
                else :
                    self.printer("%s%s%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_name(),
                                  process.pid(), process.stack_size(),
                                  process.process_name()))
            else:
                if self.pidstat_options.process_name_with_args:
                    self.printer("%s%s%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_id(),
                                  process.pid(), process.stack_size(),
                                  process.process_name_with_args()))
                else :
                    self.printer("%s%s%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_id(),
                                  process.pid(), process.stack_size(),
                                  process.process_name()))


class NoneHandlingPrinterDecorator:
    def __init__(self, printer):
        self.printer = printer

    def Print(self, args):
        new_args = args.replace('None','?')
        self.printer.Print(new_args)


class PidstatOptions(pmapi.pmOptions):
    process_name = None
    process_name_with_args = False
    ps_args_flag=False
    show_process_memory_util = False
    show_process_priority = False
    show_process_stack_util = False
    per_processor_usage = False
    show_process_user = False
    show_process_state = False
    flag_error = False
    filtered_process_user = None
    state = ""
    filterstate = []
    pid_filter = None
    pid_list = []
    timefmt = "%H:%M:%S"

    def checkOptions(self):
        if self.show_process_priority and self.show_process_memory_util:
            print("Error: -R is incompatible with -r")
            return False
        elif self.show_process_priority and self.show_process_stack_util:
            print("Error: -R is incompatible with -k")
            return False
        elif self.show_process_memory_util and self.show_process_stack_util:
            print("Error: -r is incompatible with -k")
            return False
        elif (self.show_process_memory_util or self.show_process_stack_util or
              self.show_process_priority) and self.show_process_state:
            print("Error: Incompatible flags provided")
            return False
        elif self.flag_error:
            print("Error: Incorrect usage of the B flag")
            return False
        elif self.ps_args_flag:
            print("Error: Incorrect usage of the -l flag")
            return False
        else:
            return True

    def extraOptions(self, opt,optarg, index):
        if opt == 'k':
            PidstatOptions.show_process_stack_util = True
        elif opt == 'r':
            PidstatOptions.show_process_memory_util = True
        elif opt == 'R':
            PidstatOptions.show_process_priority = True
        #process state
        elif opt == 'B':
            if PidstatOptions.show_process_state:
                #print("Error: Cannot use -B multiple times")
                PidstatOptions.flag_error = True
            PidstatOptions.show_process_state = True
            if optarg in ["All", "all"]:
                PidstatOptions.filterstate = "all"
            elif optarg in ["detail", "Detail"]:
                PidstatOptions.filterstate = "detail"
            else:
                # tried to handle the error usage like pcp-pidstat.py -B all R,S
                # or pcp-pidstat.py -B detail all
                # or pcp-pidstat.py -B R all, etc but seems like the first optarg is all we have and
                # we ignore the following ones. So pcp-pidstat.py -B detail all will treat it as
                # pcp.pidstat.py -B detail

                #if not PidstatOptions.flag_error:
                #   if (PidstatOptions.filterstate == "all" or PidstatOptions.filterstate == "detail"):
                #       print("Error: Use either all/detail or specific filters for states")
                #       PidstatOptions.flag_error = True
                #   else:

                # need to put checks for correct states in this string like UN,TT
                # shouldnt be accepted because TT isnt a valid state
                # TODO: make sure only R,S,T,D,Z are part of this optarg so if
                # anything other than these exists in
                # PidstatOptions.filterstate, we might want to flag error of usage ?

                PidstatOptions.filterstate += optarg.replace(',', ' ').split(' ')
        elif opt == 'G':
            PidstatOptions.process_name = optarg
        elif opt == 'I':
            PidstatOptions.per_processor_usage = True
        elif opt == 'U':
            PidstatOptions.show_process_user = True
            PidstatOptions.filtered_process_user = optarg
        elif opt == 'p':
            if optarg == "ALL":
                PidstatOptions.pid_filter = None
            elif optarg == "SELF":
                PidstatOptions.pid_filter = "SELF"
                PidstatOptions.pid_list = os.getpid()
            else:
                PidstatOptions.pid_filter = "LIST"
                try:
                    PidstatOptions.pid_list = list(map(lambda x:int(x),optarg.split(',')))
                except ValueError:
                    print("Invalid Process Id List: use comma separated pids without whitespaces")
                    sys.exit(1)
        elif opt == 'f':
            PidstatOptions.timefmt = optarg
        elif opt == 'l':
            if PidstatOptions.process_name_with_args:
                PidstatOptions.ps_args_flag=True
            PidstatOptions.process_name_with_args = True

    def override(self, opt):
        """ Override standard PCP options to match pidstat(1) """
        return bool(opt == 'p')

    #After reading in the provided command line options
    #initalize them by passing them in
    def __init__(self):
        pmapi.pmOptions.__init__(self,"a:s:t:G:IU::p:RrkVZ:z?:f:B:l")
        self.pmSetOptionCallback(self.extraOptions)
        self.pmSetOverrideCallback(self.override)
        self.pmSetLongOptionHeader("General options")
        self.pmSetLongOptionArchive()
        self.pmSetLongOptionSamples()
        self.pmSetLongOptionInterval()
        self.pmSetLongOption("process-name", 1, "G", "NAME",
                             "Select process names using regular expression.")
        self.pmSetLongOption("", 0, "I", "", "Show CPU usage per processor.")
        self.pmSetLongOption("user-name", 2, "U","[USERNAME]",
                             "Show real user name of the tasks and optionally filter by user name.")
        self.pmSetLongOption("pid-list", 1, "p", "PID1,PID2..  ",
                             "Show stats for specified pids; " +
                             "use SELF for current process and ALL for all processes.")
        self.pmSetLongOption("", 0, "R", "",
                             "Report realtime priority and scheduling policy information.")
        self.pmSetLongOption("", 0,"r","","Report page faults and memory utilization.")
        self.pmSetLongOption("", 0,"k","","Report stack utilization.")
        self.pmSetLongOption("", 0,"f","","Format the timestamp output")
        self.pmSetLongOption("", 0, "B", "state1,state2,..",
                             "Report process state information. " +
                             "Use -B [all] or -B [comma separated states]. " +
                             "Use -B detail for showing time spent in every state per process")
        self.pmSetLongOptionVersion()
        self.pmSetLongOptionTimeZone()
        self.pmSetLongOptionHostZone()
        self.pmSetLongOption("", 0, "l", "", "Display the process command name and all its arguments.")
        self.pmSetLongOptionHelp()


class PidstatReport(pmcc.MetricGroupPrinter):
    Machine_info_count = 0

    def timeStampDelta(self, group):
        s = group.timestamp.tv_sec - group.prevTimestamp.tv_sec
        u = group.timestamp.tv_usec - group.prevTimestamp.tv_usec
        return s + u / 1000000.0

    def print_machine_info(self,group, context):
        timestamp = context.pmLocaltime(group.timestamp.tv_sec)
        # Please check strftime(3) for different formatting options.
        # Also check TZ and LC_TIME environment variables for more
        # information on how to override the default formatting of
        # the date display in the header
        time_string = time.strftime("%x", timestamp.struct_time())
        header_string = ''
        header_string += group['kernel.uname.sysname'].netValues[0][2] + '  '
        header_string += group['kernel.uname.release'].netValues[0][2] + '  '
        header_string += '(' + group['kernel.uname.nodename'].netValues[0][2] + ')  '
        header_string += time_string + '  '
        header_string += group['kernel.uname.machine'].netValues[0][2] + '  '
        print("%s  (%s CPU)" % (header_string, self.get_ncpu(group)))

    def get_ncpu(self,group):
        return group['hinv.ncpu'].netValues[0][2]

    def report(self,manager):
        group = manager['pidstat']
        if group['proc.psinfo.utime'].netPrevValues is None:
            # need two fetches to report rate converted counter metrics
            return

        if not group['hinv.ncpu'].netValues or not group['kernel.uname.sysname'].netValues:
            return

        try:
            ncpu = self.get_ncpu(group)
            if not self.Machine_info_count:
                self.print_machine_info(group, manager)
                self.Machine_info_count = 1
        except IndexError:
            # missing some metrics
            return

        ts = group.contextCache.pmLocaltime(int(group.timestamp))
        timestamp = time.strftime(PidstatOptions.timefmt, ts.struct_time())
        interval_in_seconds = self.timeStampDelta(group)
        header_indentation = "        " if len(timestamp)<9 else (len(timestamp)-7)*" "
        value_indentation = ((len(header_indentation)+9)-len(timestamp))*" "

        metric_repository = ReportingMetricRepository(group)

        if PidstatOptions.show_process_stack_util:
            process_stack_util = CpuProcessStackUtil(metric_repository)
            process_filter = ProcessFilter(PidstatOptions)
            stdout = StdoutPrinter()
            printdecorator = NoneHandlingPrinterDecorator(stdout)
            report = CpuProcessStackUtilReporter(process_stack_util, process_filter,
                                                 printdecorator.Print, PidstatOptions)

            report.print_report(timestamp, header_indentation, value_indentation)
        elif PidstatOptions.show_process_memory_util:
            process_memory_util = CpuProcessMemoryUtil(metric_repository)
            process_filter = ProcessFilter(PidstatOptions)
            stdout = StdoutPrinter()
            printdecorator = NoneHandlingPrinterDecorator(stdout)
            report = CpuProcessMemoryUtilReporter(process_memory_util, process_filter,
                                                  interval_in_seconds,
                                                  printdecorator.Print, PidstatOptions)

            report.print_report(timestamp, header_indentation, value_indentation)
        elif PidstatOptions.show_process_priority:
            process_priority = CpuProcessPriorities(metric_repository)
            process_filter = ProcessFilter(PidstatOptions)
            stdout = StdoutPrinter()
            printdecorator = NoneHandlingPrinterDecorator(stdout)
            report = CpuProcessPrioritiesReporter(process_priority, process_filter,
                                                  printdecorator.Print, PidstatOptions)

            report.print_report(timestamp, header_indentation, value_indentation)

        #===========================================================================================================
        elif PidstatOptions.show_process_state:
            process_state = CpuProcessState(metric_repository)
            process_filter = ProcessFilter(PidstatOptions)
            stdout = StdoutPrinter()
            printdecorator = NoneHandlingPrinterDecorator(stdout)
            report = CpuProcessStateReporter(process_state, process_filter,
                                             interval_in_seconds,
                                             printdecorator.Print, PidstatOptions)

            report.print_report(timestamp, header_indentation, value_indentation)
        #===========================================================================================================
        else:
            cpu_usage = CpuUsage(metric_repository)
            process_filter = ProcessFilter(PidstatOptions)
            stdout = StdoutPrinter()
            printdecorator = NoneHandlingPrinterDecorator(stdout)
            report = CpuUsageReporter(cpu_usage, process_filter, interval_in_seconds,
                                      printdecorator.Print, PidstatOptions)
            report.print_report(timestamp, ncpu, header_indentation, value_indentation)


if __name__ == "__main__":
    try:
        opts = PidstatOptions()
        manager = pmcc.MetricGroupManager.builder(opts,sys.argv)
        if not opts.checkOptions():
            raise pmapi.pmUsageErr
        if opts.show_process_state:
            missing = manager.checkMissingMetrics(PIDSTAT_METRICS_B)
        elif opts.process_name_with_args:
            missing = manager.checkMissingMetrics(PIDSTAT_METRICS_L)
        else:
            missing = manager.checkMissingMetrics(PIDSTAT_METRICS)
        if missing is not None:
            sys.stderr.write('Error: not all required metrics are available\nMissing %s\n' % (missing))
            sys.exit(1)
        if opts.process_name_with_args:
            manager['pidstat'] = PIDSTAT_METRICS_L
        elif opts.show_process_state:
            manager['pidstat'] = PIDSTAT_METRICS_B
        else:
            manager['pidstat'] = PIDSTAT_METRICS
        manager.printer = PidstatReport()
        sts = manager.run()
        sys.exit(sts)
    except pmapi.pmErr as pmerror:
        sys.stderr.write('%s: %s\n' % (pmerror.progname,pmerror.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except KeyboardInterrupt:
        pass
