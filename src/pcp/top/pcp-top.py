#!/usr/bin/env pmpython
#
# Copyright (c) 2023 Oracle and/or its affiliates.
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
# pylint: disable=bad-whitespace,too-many-lines,bad-continuation
# pylint: disable=too-many-arguments,too-many-positional-arguments
# pylint: disable=redefined-outer-name,unnecessary-lambda
#
import signal
import sys
import time
from datetime import datetime
from pcp import pmapi, pmcc
from cpmapi import PM_CONTEXT_ARCHIVE

GIGABYTE = 1024 ** 2

SYS_MECTRICS= ["kernel.uname.sysname","kernel.uname.release",
               "kernel.uname.nodename","kernel.uname.machine","hinv.ncpu",
               "kernel.all.uptime", "kernel.all.nusers", "kernel.all.load"]

TOP_METRICS = [ "mem.physmem", "hinv.pagesize", "kernel.all.boottime",
               "kernel.all.nusers", "kernel.all.uptime", "kernel.all.load",
               "proc.psinfo.pid", "proc.psinfo.utime", "proc.psinfo.stime", "proc.psinfo.guest_time",
                "proc.psinfo.vsize", "proc.psinfo.rss", "proc.psinfo.priority",
                "proc.psinfo.nice", "proc.psinfo.cmd", "proc.psinfo.psargs",
                "proc.psinfo.sname", "proc.psinfo.start_time", "proc.psinfo.policy",
                "proc.id.uid_nm" , "proc.nprocs", "proc.runq.runnable", "proc.runq.swapped",
                "proc.runq.sleeping", "proc.runq.stopped", "proc.runq.defunct", "proc.memory.share"]

ALL_METRICS = TOP_METRICS + SYS_MECTRICS

class MetricRepository(object):
    """
    Helper class to access current and previous metric values.
    This class offers utility functions to help calculate
    value diffs and diff rates for PCP counter-based metrics.
    """

    def __init__(self, group):
        self.group = group
        self.current_cached_values = {}
        self.previous_cached_values = {}

    def current_value(self, metric, instance):
        if metric not in self.group:
            print("Unknown metric: %s" % metric)
            return None

        if instance is not None:
            if self.current_cached_values.get(metric, None) is None:
                lst = self._fetch_current_values(metric, instance)
                self.current_cached_values[metric] = lst
            return self.current_cached_values[metric].get(instance, None)
        else:
            if self.current_cached_values.get(metric, None) is None:
                self.current_cached_values[
                    metric
                ] = self._fetch_current_values(metric, instance)
            return self.current_cached_values.get(metric, None)

    def previous_value(self, metric, instance):
        if metric not in self.group:
            return None

        if instance is not None:
            if self.previous_cached_values.get(metric, None) is None:
                lst = self._fetch_previous_values(metric, instance)
                self.previous_cached_values[metric] = lst
            return self.previous_cached_values[metric].get(instance, None)
        else:
            if self.previous_cached_values.get(metric, None) is None:
                self.previous_cached_values[
                    metric
                ] = self._fetch_previous_values(metric, instance)
            return self.previous_cached_values.get(metric, None)

    def current_values(self, metric_name):
        if self.group.get(metric_name, None) is None:
            return None
        if self.current_cached_values.get(metric_name, None) is None:
            self.current_cached_values[
                metric_name
            ] = self._fetch_current_values(metric_name, True)
        return self.current_cached_values.get(metric_name, None)

    def previous_values(self, metric_name):
        if self.group.get(metric_name, None) is None:
            return None
        if self.previous_cached_values.get(metric_name, None) is None:
            self.previous_cached_values[
                metric_name
            ] = self._fetch_previous_values(metric_name, True)
        return self.previous_cached_values.get(metric_name, None)

    def _fetch_current_values(self, metric, instance):
        if instance is not None:
            return dict(
                map(lambda x: (x[0].inst, x[2]), self.group[metric].netValues)
            )
        else:
            if self.group[metric].netValues == []:
                return None
            else:
                return self.group[metric].netValues[0][2]

    def _fetch_previous_values(self, metric, instance):
        if instance is not None:
            return dict(
                map(
                    lambda x: (x[0].inst, x[2]),
                    self.group[metric].netPrevValues,
                )
            )
        else:
            if self.group[metric].netPrevValues == []:
                return None
            else:
                return self.group[metric].netPrevValues[0][2]


class TopUtil(object):
    """
    Helper class to format and print the top-like output.
    """
    def __init__(self, mngr, interval_in_seconds):
        self.repo = MetricRepository(mngr)
        self.group = mngr
        self.interval = interval_in_seconds
        self.format_uptime = "%H:%M:%S"
        # Cache some values to avoid repeated lookups
        self.process_priority_cache = self.priority()
        self.process_list_cache = self.process_list()
        self.process_user_name_cache = self.user_name()
        self.process_command_cache = self.process_command()
        self.process_nice_cache = self.nice()
        self.process_state_cache = self.process_state()
        self.virtual_memory_cache = self.virtual_memory()
        self.resident_memory_cache = self.resident_memory()
        self.shared_memory_cache = self.shared_memory()
        self.process_utime_cache = self.utime()
        self.process_stime_cache = self.stime()

    def __kb_to_gb(self, kb):
        return str(round(kb / GIGABYTE, 2)) + "g" # 1 GB = 1024^2 KB

    def _fetch_value(self, metric, instance=None):
        if instance is None:
            return dict(self.repo.current_values(metric).items())
        value = self.repo.current_value(metric, instance)
        return "?" if value is None else value

    def priority(self, instance=None):
        return self._fetch_value('proc.psinfo.priority',instance)
    def process_list(self):
        return self._fetch_value('proc.psinfo.cmd', None)
    def user_name(self):
        return self._fetch_value('proc.id.uid_nm', None)

    def process_command(self, instance=None):
        return self._fetch_value('proc.psinfo.cmd', instance)
    def nice(self, instance=None):
        return self._fetch_value('proc.psinfo.nice', instance)
    def process_state(self, instance=None):
        return self._fetch_value('proc.psinfo.sname', instance)

    def shared_memory(self, instance=None):
        return self._fetch_value('proc.memory.share', instance)

    def virtual_memory(self, instance=None):
        return self._fetch_value('proc.psinfo.vsize', instance)
    def resident_memory(self, instance=None):
        return self._fetch_value('proc.psinfo.rss', instance)
    def utime(self, instance=None):
        return self._fetch_value('proc.psinfo.utime', instance)
    def stime(self, instance=None):
        return self._fetch_value('proc.psinfo.stime', instance)
    def guest_time(self, instance=None):
        return self._fetch_value('proc.psinfo.guest_time', instance)

    def uptime(self):
        seconds = self.repo.current_value("kernel.all.uptime", None) or 0
        return time.strftime(self.format_uptime, time.localtime(seconds))

    def no_of_users(self):
        users = self.repo.current_value("kernel.all.nusers",None) or 0
        return users

    def get_load_average(self):
        load_avg = self.repo.current_values("kernel.all.load")
        if load_avg is None:
            return 0.0, 0.0, 0.0
        if isinstance(load_avg, dict):
            vals = sorted(load_avg.values())
            if len(vals) == 0:
                return 0.0, 0.0, 0.0
            return tuple(vals + [vals[-1]]*(3-len(vals)))[:3]
        if isinstance(load_avg, (list, tuple)):
            if len(load_avg) == 0:
                return 0.0, 0.0, 0.0
            return tuple(load_avg + [load_avg[-1]]*(3-len(load_avg)))[:3]
        return 0.0, 0.0, 0.0

    def tasks(self):
        """
            Returns the total number of processes and the number of processes
            in different states: running, sleeping (includes swapped out), stopped,
            and zombie.

            :return: tuple of (total, running, sleeping, stopped, zombie)
        """
        total = self.repo.current_value("proc.nprocs", None) or 0
        running = self.repo.current_value("proc.runq.runnable", None) or 0
        sleeping = self.repo.current_value("proc.runq.sleeping", None) or 0
        swapped = self.repo.current_value("proc.runq.swapped", None) or 0
        stopped = self.repo.current_value("proc.runq.stopped", None) or 0
        zombie = self.repo.current_value("proc.runq.defunct", None) or 0
        return total, running, sleeping + swapped, stopped, zombie

    def top_header(self):
        """
        Generates the top header which includes uptime, number of users, 
        load averages and task information.

        :return: string
        """
        users, (avg1, avg5, avg15) = self.no_of_users(), self.get_load_average()
        uptime = self.uptime()
        return "Top - %s up %d users, Load Average: %.2f, %.2f, %.2f\n" % (uptime, users, avg1, avg5, avg15) + \
               "Tasks: %d total,    %d running,    %d sleeping,  %d stopped,    %d zombie" % self.tasks()

    def memory_usage(self, pid):
        """
        Calculate the percentage of memory used by a particular process
        """
        resident_memory = self.resident_memory_cache.get(pid)
        total_physical_memory = self.repo.current_value('mem.physmem', None)
        if resident_memory is not None and total_physical_memory is not None:
            return round(resident_memory / total_physical_memory * 100, 2)
        return "?"

    def calculate_delta(self, metric, instance):
        c_value = self.repo.current_value(metric, instance)
        p_value = self.repo.previous_value(metric, instance)
        if c_value is not None and p_value is not None and c_value >= p_value:
            return c_value / 1000 - p_value / 1000
        return c_value / 1000 if c_value is not None else 0

    def cpu_usage(self, instance):
        if instance is None:
            return '?'
        c_time = self.calculate_delta('proc.psinfo.utime', instance) or 0
        s_time = self.calculate_delta('proc.psinfo.stime', instance) or 0
        guest_time = self.calculate_delta('proc.psinfo.guest_time', instance) or 0
        total_time = c_time + s_time + guest_time
        if total_time == 0:
            return 0
        return float("%.2f" % ((total_time  / self.interval) * 100.0))
    def execution_time(self, pid):
        sys_time = self.process_stime_cache.get(pid, 0)
        user_time = self.process_utime_cache.get(pid, 0)
        runtime = sys_time + user_time
        return str(datetime.fromtimestamp(runtime/1000).strftime("%M:%S"))
    def process_data_list(self):
        """
        Bulk-fetch all required metrics for current sample, once

        :return: A dictionary of process data, where each key is a pid and
            the value is a list of process information in the following order:
            - user name
            - priority
            - nice
            - virtual memory size (in GB)
            - resident memory size (in GB)
            - shared memory size (in GB)
            - process state
            - CPU usage (as a percentage)
            - memory usage (as a percentage)
            - execution time (in minutes and seconds)
            - command name
        """
        user_name_map = self.process_user_name_cache
        priority_map = self.process_priority_cache
        nice_map = self.process_nice_cache
        vsize_map = self.virtual_memory_cache
        rss_map = self.resident_memory_cache
        shared_map = self.shared_memory_cache
        state_map = self.process_state_cache
        cmd_map = self.process_command_cache

        # Pre-compute utime and stime for all pids for execution time
        utime_map = self.process_utime_cache
        stime_map = self.process_stime_cache

        all_pids = list(self.process_list_cache.keys())

        sort_by = getattr(self, "sort_by", "%cpu")
        sort_index = 7 if sort_by == "%cpu" else 8

        # Original per-process metric access (pre-memoization)
        sort_values = {}
        for pid in all_pids:
            if sort_index == 7:
                sort_values[pid] = self.cpu_usage(pid)
            else:
                sort_values[pid] = self.memory_usage(pid)

        sorted_pids = sorted(
            all_pids,
            key=lambda pid: (sort_values[pid] if sort_values[pid] not in ("?", None) else -1.0),
            reverse=True
        )
        top_pids = sorted_pids[:int(getattr(self, "num_procs", 100))]

        res = {}
        for pid in top_pids:
            res[pid] = [
                user_name_map.get(pid, "?"),
                priority_map.get(pid, "?"),
                nice_map.get(pid, "?"),
                (lambda x: self.__kb_to_gb(x) if x is not None else "?")(vsize_map.get(pid, None)),
                (lambda x: self.__kb_to_gb(x) if x is not None else "?")(rss_map.get(pid, None)),
                shared_map.get(pid, "?"),
                state_map.get(pid, "UNKNOWN"),
                self.cpu_usage(pid),
                self.memory_usage(pid),
                str(datetime.fromtimestamp((stime_map.get(pid, 0) + utime_map.get(pid, 0))/1000).strftime("%M:%S")),
                cmd_map.get(pid, "?")
            ]
        return res

class TopReport(pmcc.MetricGroupPrinter):
    def __init__(self,opts,group):
        self.opts = opts
        self.group = group
        self.timefmt = opts.timefmt
        self.context = group.type
        self.samples = self.opts.pmGetOptionSamples()

    # Removed unused private member __timeStampDelta
    def __get_ncpu(self, group):
        return group['hinv.ncpu'].netValues[0][2]

    def __timeStampDelta(self, group):
        s = group.timestamp.tv_sec - group.prevTimestamp.tv_sec
        n = group.timestamp.tv_nsec - group.prevTimestamp.tv_nsec
        interval_in_seconds = s + n / 1000000000.0
        return interval_in_seconds

    def __print_machine_info(self, context):
        timestamp = self.group.pmLocaltime(context.timestamp.tv_sec)
        # Please check strftime(3) for different formatting options.
        # Also check TZ and LC_TIME environment variables for more
        # information on how to override the default formatting of
        # the date display in the header
        time_string = time.strftime("%x", timestamp.struct_time())
        header_string = ''
        header_string += context['kernel.uname.sysname'].netValues[0][2] + '  '
        header_string += context['kernel.uname.release'].netValues[0][2] + '  '
        header_string += '(' + context['kernel.uname.nodename'].netValues[0][2] + ')  '
        header_string += time_string + '  '
        header_string += context['kernel.uname.machine'].netValues[0][2] + '  '
        print("%s  (%s CPU)" % (header_string, self.__get_ncpu(context)))

    def print_report(self, group, timestamp, header_indentation, value_indentation, topinfo,interval_in_seconds):
        self.__print_machine_info(group)
        topinfo = TopUtil(topinfo,interval_in_seconds)
        print(topinfo.top_header())
        print("{:<10} {:>5} {:>12} {:>5} {:>5} {:>9} {:>10} {:>10} {:>4} {:>4} {:>5} {:>5} {:>20}".format(
            "Timestamp", "PID", "USER", "PR", "NI", "VIRT", "RES", "SHR", "STATE","%CPU", "%MEM", "START", "COMMAND"))
        if self.opts.sort_by == "%cpu":
            sorted_list = sorted(topinfo.process_data_list().items(), key=lambda x: x[1][7], reverse=True)
        elif self.opts.sort_by == "%mem":
            sorted_list = sorted(topinfo.process_data_list().items(), key=lambda x: x[1][8], reverse=True)
        else:
            sorted_list = list(topinfo.process_data_list().items())
        for pid, proc_values in sorted_list[:int(self.opts.num_procs)]:
            print(
                "{:<5} {:>10} {:>8} {:>5} {:>5} {:>10} {:>10} {:>10} {:>4} {:>5} {:>5} {:>5} {:>20}".format(
                    timestamp, pid, proc_values[0], proc_values[1], proc_values[2],
                    proc_values[3], proc_values[4], proc_values[5], proc_values[6], proc_values[7],
                    proc_values[8], proc_values[9], proc_values[10]
                )
            )

    def report(self, manager):
        group = manager["allinfo"]
        if group['proc.psinfo.utime'].netPrevValues is None:
            # skip the first iteration as we need previous values to calculate cpu usage
            return
        interval_in_seconds = self.__timeStampDelta(group)
        t_s = group.contextCache.pmLocaltime(int(group.timestamp))
        timestamp = time.strftime(self.timefmt, t_s.struct_time())
        header_indentation = "        " if len(timestamp) < 9 else (len(timestamp) - 7) * " "
        value_indentation = ((len(header_indentation) + 9) - len(timestamp)) * " "
        if self.context != PM_CONTEXT_ARCHIVE and self.samples is None:
            self.print_report(
                group,
                timestamp,
                header_indentation,
                value_indentation,
                manager['topinfo'],
                interval_in_seconds)
            sys.exit(0)
        elif self.context == PM_CONTEXT_ARCHIVE and self.samples is None:
            self.print_report(
                group,
                timestamp,
                header_indentation,
                value_indentation,
                manager['topinfo'],
                interval_in_seconds)
        elif self.samples >= 1:
            self.print_report(
                group,
                timestamp,
                header_indentation,
                value_indentation,
                manager['topinfo'],
                interval_in_seconds)
            self.samples -= 1
        else:
            pass

class TopOptions(pmapi.pmOptions):

    def __init__(self):
        pmapi.pmOptions.__init__(self, "a:s:Z:zV:o:c:?")
        self.options()
        self.pmSetOptionCallback(self.extraOptions)
        self.samples = None
        self.context = None
        self.timefmt = "%H:%M:%S"
        self.sort_by = "%cpu"
        self.num_procs = 100

    def options(self):
        self.pmSetLongOptionHeader("General options")
        self.pmSetLongOptionHostZone()
        self.pmSetLongOptionArchive()
        self.pmSetLongOption("", 1, "o", "[%mem, %cpu]","Sort by %mem or %cpu (default %cpu)")
        self.pmSetLongOption("", 1, "c", "N (default 100)","Show only top N processes (default 100)")
        self.pmSetLongOptionTimeZone()
        self.pmSetLongOptionSamples()
        self.pmSetLongOptionVersion()
        self.pmSetLongOptionHelp()

    def extraOptions(self,opts, optarg, index):
        if opts == 'o':
            self.sort_by = optarg
        elif opts == 'c':
            self.num_procs = optarg
        else:
            return False
        return True

if __name__ == '__main__':
    try:
        opts = TopOptions()
        mngr = pmcc.MetricGroupManager.builder(opts,sys.argv)
        opts.context = mngr.type
        missing = mngr.checkMissingMetrics(ALL_METRICS)
        if missing is not None:
            sys.stderr.write('Error: not all required metrics are available\nMissing %s\n' % missing)
            sys.exit(1)
        mngr["topinfo"] = TOP_METRICS
        mngr["sysinfo"] = SYS_MECTRICS
        mngr["allinfo"] = ALL_METRICS
        mngr.printer = TopReport(opts,mngr)
        sts = mngr.run()
        sys.exit(sts)
    except pmapi.pmErr as error:
        sys.stderr.write('%s\n' % (error.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except IOError:
        signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    except KeyboardInterrupt:
        pass
