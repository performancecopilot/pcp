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
import collections


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
        # self.time = self.group.contextCache.pmLocaltime(int(self.group.timestamp))
        self.format_uptime = "%H:%M:%S"
    def __kb_to_gb(self, kb):
        return str(round(kb / (1024 ** 2), 2)) + "g" # 1 GB = 1024^2 KB
    
    def process_list(self):
        return dict(self.repo.current_values("proc.psinfo.pid").items())
    def uptime(self):
        seconds = self.repo.current_value("kernel.all.uptime", None) or 0
        return time.strftime(self.format_uptime, time.localtime(seconds))

    def no_of_users(self):
        users = self.repo.current_value("kernel.all.nusers",None) or 0
        return users

    def get_load_average(self):
        load_avg = self.repo.current_values("kernel.all.load")
        return load_avg[1], load_avg[5], load_avg[15]

    def tasks(self):
        total = self.repo.current_value("proc.nprocs", None) or 0
        running = self.repo.current_value("proc.runq.runnable", None) or 0
        sleeping = self.repo.current_value("proc.runq.sleeping", None) or 0
        swapped = self.repo.current_value("proc.runq.swapped", None) or 0
        stopped = self.repo.current_value("proc.runq.stopped", None) or 0
        zombie = self.repo.current_value("proc.runq.defunct", None) or 0
        return total, running, sleeping + swapped, stopped, zombie

    def top_header(self):
        users, (avg1, avg5, avg15) = self.no_of_users(), self.get_load_average()
        uptime = self.uptime()
        return "Top - %s up %d users, Load Average: %.2f, %.2f, %.2f\n" % (uptime, users, avg1, avg5, avg15) + \
               "Tasks: %d total,    %d running,    %d sleeping,  %d stopped,    %d zombie" % self.tasks()

    def mem_usage(self, instance=None):
        rss = self.repo.current_value('proc.psinfo.rss', instance)
        total_mem = self.repo.current_value('mem.physmem', None)
        if rss is not None and total_mem is not None:
            return float("%.2f" % (100 * float(rss) / total_mem))
        return "?"

    def process_command(self, instance=None):
        cmd = self.repo.current_value('proc.psinfo.cmd', instance)
        return cmd[:40] if cmd is not None else "?"
    def user_name(self, instance=None):
        user = self.repo.current_value('proc.id.uid_nm', instance)
        return "?" if user is None else user[:6]
    def priority(self, instance=None):
        priority = self.repo.current_value('proc.psinfo.priority', instance)
        return "?" if priority is None else priority
    def nice(self, instance=None):
        nice = self.repo.current_value('proc.psinfo.nice', instance)
        return "?" if nice is None else nice
    def virtual_memory(self, instance=None):
        vms = self.repo.current_value('proc.psinfo.vsize', instance)
        return self.__kb_to_gb(vms) if vms is not None and vms > 1024 ** 2 else vms if vms is not None else "?"
    def resident_memory(self, instance=None):
        rss = self.repo.current_value('proc.psinfo.rss', instance)
        return self.__kb_to_gb(rss) if rss is not None and rss > 1024 ** 2 else rss if rss is not None else "?"
    def process_state(self, instance=None):
        state = self.repo.current_value('proc.psinfo.sname', instance)
        return state if state is not None else "UNKNOWN"
    def calculate_delta(self, metric, instance):
        c_value = self.repo.current_value(metric, instance)
        p_value = self.repo.previous_value(metric, instance)
        if c_value is not None and p_value is not None and c_value >= p_value:
           return c_value / 1000 - p_value / 1000
        return c_value / 1000 if c_value is not None else 0
    def shared_memory(self, instance):
        smem = self.repo.current_value('proc.memory.share', instance)
        return smem if smem is not None else "?"
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
    def execution_time(self, instance=None):
        system_time = self.repo.current_value('proc.psinfo.stime', instance) or 0
        user_time = self.repo.current_value('proc.psinfo.utime', instance) or 0
        runtime =  system_time + user_time
        return str(datetime.fromtimestamp(runtime/1000).strftime("%M:%S"))
    def process_data_list(self):
        res = {}
        for pid, proc in self.process_list().items():
            res[pid] = [
                self.user_name(pid),
                self.priority(pid),
                self.nice(pid),
                self.virtual_memory(pid),
                self.resident_memory(pid),
                self.shared_memory(pid),
                self.process_state(pid),
                self.cpu_usage(pid),
                self.mem_usage(pid),
                self.execution_time(pid),
                self.process_command(pid),
            ]
        return res

class TopReport(pmcc.MetricGroupPrinter):
    def __init__(self,opts,group):
        self.opts = opts
        self.group = group
        self.samples = opts.samples
        self.context = opts.context

    def __timeStampDelta(self,group):
        s = group.timestamp.tv_sec - group.prevTimestamp.tv_sec
        n = group.timestamp.tv_nsec - group.prevTimestamp.tv_nsec
        return s + n / 1000000000.0
    def __get_ncpu(self, group):
        return group['hinv.ncpu'].netValues[0][2]

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
        for pid, proc in sorted_list[:int(self.opts.num_procs)]:
            print("{:<5} {:>10} {:>8} {:>5} {:>5} {:>10} {:>10} {:>10} {:>4} {:>5} {:>5} {:>5} {:>20}".format(
                    timestamp, pid, proc[0], proc[1], proc[2], proc[3], proc[4], proc[5], proc[6], proc[7], proc[8], proc[9], proc[10]))


    def report(self, manager):
        group = manager["allinfo"]
        if group['proc.psinfo.utime'].netPrevValues is None:
            # skip the first iteration as we need previous values to calculate cpu usage
            return
        interval_in_seconds = self.__timeStampDelta(group)
        self.samples = self.opts.pmGetOptionSamples()
        t_s = group.contextCache.pmLocaltime(int(group.timestamp))
        timestamp = time.strftime(TopOptions.timefmt, t_s.struct_time())
        header_indentation = "        " if len(timestamp) < 9 else (len(timestamp) - 7) * " "
        value_indentation = ((len(header_indentation) + 9) - len(timestamp)) * " "
        self.print_report(group,timestamp,header_indentation,value_indentation,manager['topinfo'],interval_in_seconds)

class TopOptions(pmapi.pmOptions):
    timefmt = "%H:%M:%S"
    sort_by = "%cpu"
    num_procs = 2000
    def __init__(self):
        pmapi.pmOptions.__init__(self, "a:s:Z:zV:o:c:?")
        self.options()
        self.pmSetOptionCallback(self.extraOptions)
        self.samples = None
        self.context = None

    def options(self):
        self.pmSetLongOptionHeader("General options")
        self.pmSetLongOptionHostZone()
        self.pmSetLongOptionArchive()
        self.pmSetLongOption("", 1, "o", "[%mem, %cpu]","Sort by %mem or %cpu (default %cpu)")
        self.pmSetLongOption("", 1, "c", "N (default 2000)","Show only top N processes (default 2000)")
        self.pmSetLongOptionTimeZone()
        self.pmSetLongOptionSamples()
        self.pmSetLongOptionVersion()
        self.pmSetLongOptionHelp()

    def extraOptions(self,opts, optarg, index):
        if opts == 'o':
            TopOptions.sort_by = optarg
        elif opts == 'c':
            TopOptions.num_procs = optarg
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
