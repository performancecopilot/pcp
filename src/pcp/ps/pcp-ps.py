#!/usr/bin/env pmpython
#
# Copyright (c) 2022 Oracle and/or its affiliates.
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
# pylint: disable=bad-whitespace,too-many-arguments,too-many-lines
# pylint: disable=redefined-outer-name,unnecessary-lambda
#


import sys
import time
import signal
from pcp import pmcc
from pcp import pmapi
from cpmapi import PM_CONTEXT_ARCHIVE

process_state_info = {}

PSSTAT_METRICS = ['kernel.uname.nodename', 'kernel.uname.release', 'kernel.uname.sysname',
                  'kernel.uname.machine', 'hinv.ncpu', 'proc.psinfo.pid', 'proc.psinfo.guest_time',
                  'proc.psinfo.utime', 'proc.psinfo.ppid', 'proc.psinfo.rt_priority', 'proc.psinfo.rss',
                  'proc.id.uid_nm', 'proc.psinfo.stime', 'kernel.all.boottime', 'proc.psinfo.sname',
                  'proc.psinfo.start_time', 'proc.psinfo.vsize', 'proc.psinfo.priority',
                  'proc.psinfo.nice', 'proc.psinfo.wchan_s', 'proc.psinfo.psargs', 'proc.psinfo.cmd',
                  'proc.psinfo.ttyname', 'mem.physmem', 'proc.psinfo.policy']

SCHED_POLICY = ['NORMAL', 'FIFO', 'RR', 'BATCH', '', 'IDLE', 'DEADLINE']


class StdoutPrinter:
    def Print(self, args):
        print(args)


class NoneHandlingPrinterDecorator:
    def __init__(self, printer):
        self.printer = printer

    def Print(self, args):
        new_args = args.replace('None', '?')
        self.printer.Print(new_args)


# After fetching non singular metric values, create a mapping of instance id
# to instance value rather than instance name to instance value.
# The reason is, in PCP, instance names require a separate pmGetIndom() request
# and some of the names may not be available.
class ReportingMetricRepository:
    def __init__(self, group):
        self.group = group
        self.current_cached_values = {}
        self.previous_cached_values = {}

    def __fetch_current_values(self, metric, instance):
        if instance:
            return dict(map(lambda x: (x[0].inst, x[2]), self.group[metric].netValues))
        else:
            return self.group[metric].netValues[0][2]

    def __fetch_previous_values(self, metric, instance):
        if instance:
            return dict(map(lambda x: (x[0].inst, x[2]), self.group[metric].netPrevValues))
        else:
            return self.group[metric].netPrevValues[0][2]

    def current_value(self, metric, instance):
        if not metric in self.group:
            return None
        if instance:
            if self.current_cached_values.get(metric, None) is None:
                lst = self.__fetch_current_values(metric, instance)
                self.current_cached_values[metric] = lst

            return self.current_cached_values[metric].get(instance, None)
        else:
            if self.current_cached_values.get(metric, None) is None:
                self.current_cached_values[metric] = self.__fetch_current_values(metric, instance)
            return self.current_cached_values.get(metric, None)

    def previous_value(self, metric, instance):
        if not metric in self.group:
            return None
        if instance:
            if self.previous_cached_values.get(metric, None) is None:
                lst = self.__fetch_previous_values(metric, instance)
                self.previous_cached_values[metric] = lst

            return self.previous_cached_values[metric].get(instance, None)
        else:
            if self.previous_cached_values.get(metric, None) is None:
                self.previous_cached_values[metric] = self.__fetch_previous_values(metric, instance)
            return self.previous_cached_values.get(metric, None)

    def current_values(self, metric_name):
        if self.group.get(metric_name, None) is None:
            return None
        if self.current_cached_values.get(metric_name, None) is None:
            self.current_cached_values[metric_name] = self.__fetch_current_values(metric_name, True)
        return self.current_cached_values.get(metric_name, None)

    def previous_values(self, metric_name):
        if self.group.get(metric_name, None) is None:
            return None
        if self.previous_cached_values.get(metric_name, None) is None:
            self.previous_cached_values[metric_name] = self.__fetch_previous_values(metric_name, True)
        return self.previous_cached_values.get(metric_name, None)


class ProcessFilter:
    def __init__(self, options):
        self.options = options

    def filter_processes(self, processes):
        return filter(lambda p: self.__predicate(p), processes)

    def __predicate(self, process):
        if self.options.filter_flag:
            return bool(self.__matches_process_username(process)
                        and self.__matches_process_pid(process)
                        and self.__matches_process_name(process)
                        and self.__matches_process_ppid(process))
        else:
            return True

    def __matches_process_username(self, process):
        if self.options.username_filter_flag is True and self.options.filtered_process_user is not None:
            return process.user_name().strip() == self.options.filtered_process_user.strip()
        else:
            return True

    def __matches_process_pid(self, process):
        if self.options.pid_filter_flag:
            if self.options.pid_list is not None:
                pid = int(process.pid())
                return bool(pid in self.options.pid_list)
        return True

    def __matches_process_ppid(self, process):
        if self.options.ppid_filter_flag:
            if self.options.ppid_list is not None:
                ppid = int(process.ppid())
                return bool(ppid in self.options.ppid_list)
        return True

    def __matches_process_name(self, process):
        name = process.process_name()
        if self.options.command_filter_flag is True and self.options.command_list is not None and name is not None:
            return name.strip() in self.options.command_list
        else:
            return True


class ProcessStatusUtil:
    def __init__(self, instance, delta_time, metrics_repository):
        self.instance = instance
        self.__delta_time = delta_time
        self.__metric_repository = metrics_repository

    def pid(self):
        data = str(self.__metric_repository.current_value('proc.psinfo.pid', self.instance))
        if len(str(data)) < 8:
            whitespace = 8 - len(str(data))
            res = data.ljust(whitespace + len(str(data)), ' ')
            return res
        else:
            return data

    def ppid(self):
        data = str(self.__metric_repository.current_value('proc.psinfo.ppid', self.instance))
        if len(str(data)) < 8:
            whitespace = 8 - len(str(data))
            res = data.ljust(whitespace + len(str(data)), ' ')
            return res
        else:
            return data

    def user_name(self):
        data = self.__metric_repository.current_value('proc.id.uid_nm', self.instance)[:10]
        if len(data) < 10:
            whitespace = 10 - len(data)
            res = data.ljust(whitespace + len(data), ' ')
            return res
        else:
            return data

    def process_name(self):
        try:
            data = self.__metric_repository.current_value('proc.psinfo.cmd', self.instance)[:20]
            if len(data) < 20:
                whitespace = 20 - len(data)
                res = data.ljust(whitespace + len(data), ' ')
                return res
            else:
                return data
        except TypeError:
            data = '-'
            return data

    def process_name_with_args(self):
        data = self.__metric_repository.current_value('proc.psinfo.psargs', self.instance)[:30]
        if len(data) < 30:
            whitespace = 30 - len(data)
            res = data.ljust(whitespace + len(data), ' ')
            return res
        else:
            return data

    def vsize(self):
        return self.__metric_repository.current_value('proc.psinfo.vsize', self.instance)

    def rss(self):
        return self.__metric_repository.current_value('proc.psinfo.rss', self.instance)

    def mem(self):
        total_mem = self.__metric_repository.current_value('mem.physmem', None)
        rss = self.__metric_repository.current_value('proc.psinfo.rss', self.instance)
        if total_mem is not None and rss is not None:
            return float("%.2f" % (100 * float(rss) / total_mem))
        else:
            return None

    def s_name(self):
        return self.__metric_repository.current_value('proc.psinfo.sname', self.instance)

    def cpu_number(self):
        return self.__metric_repository.current_value('proc.psinfo.processor', self.instance)

    def system_percent(self):
        c_systemtime = self.__metric_repository.current_value('proc.psinfo.stime', self.instance)
        p_systemtime = self.__metric_repository.previous_value('proc.psinfo.stime', self.instance)
        if c_systemtime is not None and p_systemtime is not None:
            percent_of_time = 100 * float(c_systemtime - p_systemtime) / float(1000 * self.__delta_time)
            return float("%.2f" % percent_of_time)
        else:
            return None

    def wchan_s(self):
        process = self.__metric_repository.current_value('proc.psinfo.wchan_s', self.instance)
        if process is None:
            process = '-'
            return process
        elif len(process) < 30:
            whitespace = 30 - len(process)
            res = process.ljust(whitespace + len(process), ' ')
            return res
        return process[:30]

    def priority(self):
        return self.__metric_repository.current_value('proc.psinfo.priority', self.instance)

    def user_percent(self):
        c_usertime = self.__metric_repository.current_value('proc.psinfo.utime', self.instance)
        p_usertime = self.__metric_repository.previous_value('proc.psinfo.utime', self.instance)
        if c_usertime is not None and p_usertime is not None:
            percent_of_time = 100 * float(c_usertime - p_usertime) / float(1000 * self.__delta_time)
            return float("%.2f" % percent_of_time)
        else:
            return None

    def guest_percent(self):
        c_guesttime = self.__metric_repository.current_value('proc.psinfo.guest_time', self.instance)
        p_guesttime = self.__metric_repository.previous_value('proc.psinfo.guest_time', self.instance)
        if c_guesttime is not None and p_guesttime is not None:
            percent_of_time = 100 * float(c_guesttime - p_guesttime) / float(1000 * self.__delta_time)
            return float("%.2f" % percent_of_time)
        else:
            return None

    def total_percent(self):
        if self.user_percent() is not None and self.guest_percent() is not None and self.system_percent() is not None:
            return float("%.2f" % (self.user_percent() + self.guest_percent() + self.system_percent()))
        else:
            return None

    def stime(self):
        c_systime = self.__metric_repository.current_value('proc.psinfo.stime', self.instance)
        p_systime = self.__metric_repository.previous_value('proc.psinfo.stime', self.instance)
        # sometimes the previous_value seems to be Nonetype, not sure why
        if p_systime is None:  # print a '?' here
            return '?'
        else:
            return c_systime - p_systime

    def start(self):
        s_time = self.__metric_repository.current_value('proc.psinfo.start_time', self.instance)
        group = manager['psstat']
        kernel_boottime = group['kernel.all.boottime'].netValues[0][2]
        ts = group.contextCache.pmLocaltime(int(kernel_boottime + (s_time / 1000)))
        if group.timestamp.tv_sec - (kernel_boottime + s_time / 1000) >= 24*60*60:
            # started one day or more ago, use MmmDD HH:MM
            return time.strftime("%b%d %H:%M", ts.struct_time())
        else:
            # started less than one day ago, use HH:MM:SS
            return time.strftime("%H:%M:%S", ts.struct_time())


    def total_time(self):
        c_usertime = self.__metric_repository.current_value('proc.psinfo.stime', self.instance)
        p_guesttime = self.__metric_repository.previous_value('proc.psinfo.utime', self.instance)
        timefmt = "%H:%M:%S"
        if c_usertime and p_guesttime is not None:
            total_time = (c_usertime / 1000) + (p_guesttime / 1000)
        else:
            total_time = 0
        return time.strftime(timefmt, time.gmtime(total_time))

    def tty_name(self):
        return self.__metric_repository.current_value('proc.psinfo.ttyname', self.instance)

    def user_id(self):
        return self.__metric_repository.current_value('proc.id.uid', self.instance)

    def start_time(self):
        return self.__metric_repository.current_value('proc.psinfo.start_time', self.instance)

    def func_state(self):
        s_name = self.__metric_repository.current_value('proc.psinfo.sname', self.instance)
        if s_name == 'R':
            return 'N/A'
        elif s_name is None:
            return '?'
        else:
            return self.wchan_s()

    def policy(self):
        policy_int = self.__metric_repository.current_value('proc.psinfo.policy', self.instance)
        if policy_int is not None and policy_int <= len(SCHED_POLICY):
            # return policy_int
            return SCHED_POLICY[policy_int]
        return None


PIDINFO_PAIR = {"%cpu": ('%CPU', ProcessStatusUtil.system_percent),
                "%mem": ('%MEM', ProcessStatusUtil.mem),
                "start": ("START\t", ProcessStatusUtil.start),
                "time": ("TIME\t", ProcessStatusUtil.total_time),
                "cls": ("CLS", ProcessStatusUtil.policy),
                "cmd": ("Command\t\t\t", ProcessStatusUtil.process_name_with_args),
                "pid": ("PID\t", ProcessStatusUtil.pid),
                "ppid": ("PPID\t", ProcessStatusUtil.ppid),
                "pri": ("PRI", ProcessStatusUtil.priority),
                "state": ("S", ProcessStatusUtil.s_name),
                "rss": ("RSS", ProcessStatusUtil.rss),
                "rtprio": ("RTPRIO", ProcessStatusUtil.priority),
                "tty": ("TT", ProcessStatusUtil.tty_name),
                "pname": ("Pname\t\t", ProcessStatusUtil.process_name),
                "vsize": ("VSZ", ProcessStatusUtil.vsize),
                "uname": ("USER\t", ProcessStatusUtil.user_name),
                "uid": ("USER_ID", ProcessStatusUtil.user_id),
                "wchan": ("WCHAN\t\t\t", ProcessStatusUtil.wchan_s)}


class ProcessStatus:
    def __init__(self, metric_repository):
        self.__metric_repository = metric_repository

    def get_processes(self, delta_time):
        return map((lambda pid: (ProcessStatusUtil(pid, delta_time, self.__metric_repository))), self.__pids())

    def __pids(self):
        pid_dict = self.__metric_repository.current_values('proc.psinfo.pid')
        return sorted(pid_dict.values())


class DynamicProcessReporter:
    def __init__(self, process_report, process_filter, delta_time, printer, processStatOptions):
        self.delta_time = delta_time
        self.process_report = process_report
        self.process_filter = process_filter
        self.printer = printer
        self.processStatOptions = processStatOptions

    def print_report(self, timestamp, header_indentation, value_indentation):

        # when the print count is exhausted exit the program gracefully
        # we can't use break here because it's being called by the run manager
        if self.processStatOptions.context is not PM_CONTEXT_ARCHIVE:
            if self.processStatOptions.print_count == 0:
                sys.exit(0)
            else:
                self.processStatOptions.print_count -= 1

        if self.processStatOptions.filterstate is not None:
            self.printer("Timestamp" + header_indentation +
                         "USER\t\tPID\t\tPPID\t\tPRI\t%CPU\t%MEM\tVSZ\tRSS\tS\tSTARTED\t\tTIME\t\tWCHAN\t\t\t\tCommand")
            processes = self.process_filter.filter_processes(self.process_report.get_processes(self.delta_time))
            for process in processes:
                total_percent = process.total_percent()
                current_process_pid = process.pid()
                current_process_sname = process.s_name()
                if process.wchan_s() is not None:
                    wchan = process.wchan_s()
                else:
                    wchan = '-'
                key = (current_process_sname, current_process_pid)
                if key in process_state_info:
                    process_state_info[key] = process_state_info[key] + self.delta_time
                else:
                    process_state_info[key] = self.delta_time
                process_name = process.process_name_with_args()
                if 7 < len(wchan) < 15:
                    self.printer("%s%s%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t\t%s" %
                                 (timestamp, value_indentation, process.user_name(), process.pid(), process.ppid(),
                                  process.priority(), total_percent, process.system_percent(), process.vsize(),
                                  process.rss(), current_process_sname, process.start(), process.total_time(), wchan,
                                  process_name))
                elif len(wchan) >= 15:
                    self.printer("%s%s%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s" %
                                 (timestamp, value_indentation, process.user_name(), process.pid(), process.ppid(),
                                  process.priority(), total_percent, process.system_percent(), process.vsize(),
                                  process.rss(), current_process_sname, process.start(), process.total_time(), wchan,
                                  process_name))
                else:
                    self.printer("%s%s%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t\t\t%s" %
                                 (timestamp, value_indentation, process.user_name(), process.pid(), process.ppid(),
                                  process.priority(), total_percent, process.system_percent(), process.vsize(),
                                  process.rss(), current_process_sname, process.start(), process.total_time(), wchan,
                                  process_name))
        elif self.processStatOptions.colum_list is not None:
            header = "Timestamp" + '\t'
            for key in self.processStatOptions.colum_list:
                if key in PIDINFO_PAIR:
                    header += PIDINFO_PAIR[key][0] + '\t\t'
            print(header)
            processes = self.process_filter.filter_processes(self.process_report.get_processes(self.delta_time))
            for process in processes:
                data_to_print = timestamp + '\t'
                for key in self.processStatOptions.colum_list:
                    if key in PIDINFO_PAIR:
                        data_to_print += str(PIDINFO_PAIR[key][1](process)) + '\t\t'
                print(data_to_print)


class ProcessStatusReporter:
    def __init__(self, process_report, process_filter, delta_time, printer, processStatOptions):
        self.delta_time = delta_time
        self.process_report = process_report
        self.process_filter = process_filter
        self.printer = printer
        self.processStatOptions = processStatOptions

    def print_report(self, timestamp, header_indentation, value_indentation):

        # when the print count is exhausted exit the program gracefully
        # we can't use break here because it's being called by the run manager
        if self.processStatOptions.context is not PM_CONTEXT_ARCHIVE:
            if self.processStatOptions.print_count == 0:
                sys.exit(0)
            else:
                self.processStatOptions.print_count -= 1

        if self.processStatOptions.show_all_process:
            self.printer("Timestamp" + header_indentation + "PID\t\t\tTTY\tTIME\t\tCMD")
            processes = self.process_filter.filter_processes(self.process_report.get_processes(self.delta_time))
            for process in processes:
                command = process.process_name()
                ttyname = process.tty_name()
                self.printer("%s%s%s\t\t%s\t%s\t%s" % (timestamp, value_indentation, process.pid(), ttyname,
                                                       process.total_time(), command))
        elif self.processStatOptions.empty_arg_flag:
            self.printer("Timestamp" + header_indentation + "PID\t\tTIME\t\tCMD")
            processes = self.process_filter.filter_processes(self.process_report.get_processes(self.delta_time))
            for process in processes:
                pid = process.pid()
                command = process.process_name()
                self.printer("%s%s%s\t%s\t%s" % (timestamp, value_indentation, pid,
                                                 process.total_time(), command))
        elif self.processStatOptions.pid_filter_flag:
            self.printer("Timestamp" + header_indentation + "PID\t\tPPID\t\tTTY\tTIME\t\tCMD")
            processes = self.process_filter.filter_processes(self.process_report.get_processes(self.delta_time))
            for process in processes:
                pid = process.pid()
                command = process.process_name()
                ttyname = process.tty_name()
                self.printer("%s%s%s\t%s\t%s\t%s\t%s" % (timestamp, value_indentation, pid, process.ppid(),
                                                         ttyname, process.total_time(), command))
        elif self.processStatOptions.ppid_filter_flag:
            self.printer("Timestamp" + header_indentation + "PID\t\tPPID\t\tTTY\tTIME\t\tCMD")
            processes = self.process_filter.filter_processes(self.process_report.get_processes(self.delta_time))
            for process in processes:
                ppid = process.ppid()
                command = process.process_name()
                ttyname = process.tty_name()
                self.printer("%s%s%s\t%s\t%s\t%s\t%s" % (timestamp, value_indentation, process.pid(), ppid, ttyname,
                                                         process.total_time(), command))
        elif self.processStatOptions.username_filter_flag:
            self.printer("Timestamp" + header_indentation + "USERNAME\t\tPID\t\t%CPU\t%MEM\tVSZ\tRSS\t" +
                         "TTY\tSTAT\t\tTIME\t\tSTART\t\tCOMMAND")
            processes = self.process_filter.filter_processes(self.process_report.get_processes(self.delta_time))
            for process in processes:
                self.printer("%s%s%s\t\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s" % (
                    timestamp, value_indentation, process.user_name(), process.pid(),
                    process.system_percent(), process.total_percent(), process.vsize(), process.rss(),
                    process.tty_name(), process.ppid(), process.total_time(), process.start(),
                    process.process_name()))

        elif self.processStatOptions.user_oriented_format:
            self.printer("Timestamp" + header_indentation + "USERNAME\tPID\t\t%CPU\t%MEM\tVSZ\tRSS\t" +
                         "TTY\tSTAT\tTIME\t\tSTART\t\tCOMMAND")
            processes = self.process_filter.filter_processes(self.process_report.get_processes(self.delta_time))
            for process in processes:
                self.printer("%s%s%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s" % (
                    timestamp, value_indentation, process.user_name(), process.pid(),
                    process.system_percent(), process.total_percent(), process.vsize(), process.rss(),
                    process.tty_name(), process.s_name(), process.total_time(), process.start(),
                    process.process_name()))

        elif self.processStatOptions.command_filter_flag:
            self.printer("Timestamp" + header_indentation + "PID\t\tPPID\t\tTTY\tTIME\t\tCMD")
            processes = self.process_filter.filter_processes(self.process_report.get_processes(self.delta_time))
            for process in processes:
                ppid = process.ppid()
                command = process.process_name()
                ttyname = process.tty_name()
                self.printer("%s%s%s\t%s\t%s\t%s\t%s" % (timestamp, value_indentation, process.pid(), ppid, ttyname,
                                                         process.total_time(), command))


class ProcessstatReport(pmcc.MetricGroupPrinter):
    Machine_info_count = 0

    def timeStampDelta(self, group):
        s = group.timestamp.tv_sec - group.prevTimestamp.tv_sec
        u = group.timestamp.tv_usec - group.prevTimestamp.tv_usec
        return s + u / 1000000.0

    def print_machine_info(self, group, context):
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

    def get_ncpu(self, group):
        return group['hinv.ncpu'].netValues[0][2]

    def report(self, manager):
        try:
            group = manager['psstat']
            if group['proc.psinfo.utime'].netPrevValues is None:
                # need two fetches to report rate converted counter metrics
                return

            if not group['hinv.ncpu'].netValues or not group['kernel.uname.sysname'].netValues:
                return

            try:
                if not self.Machine_info_count:
                    self.print_machine_info(group, manager)
                    self.Machine_info_count = 1
            except IndexError:
                # missing some metrics
                return

            ts = group.contextCache.pmLocaltime(int(group.timestamp))
            timestamp = time.strftime(ProcessStatOptions.timefmt, ts.struct_time())
            interval_in_seconds = self.timeStampDelta(group)
            header_indentation = "        " if len(timestamp) < 9 else (len(timestamp) - 7) * " "
            value_indentation = ((len(header_indentation) + 9) - len(timestamp)) * " "

            metric_repository = ReportingMetricRepository(group)

            # Doing this for one single print instance in case there is no count specified
            if ProcessStatOptions.print_count is None:
                ProcessStatOptions.print_count = 1

            # ================================================================
            if ProcessStatOptions.show_all_process:
                process_report = ProcessStatus(metric_repository)
                process_filter = ProcessFilter(ProcessStatOptions)
                stdout = StdoutPrinter()
                printdecorator = NoneHandlingPrinterDecorator(stdout)
                report = ProcessStatusReporter(process_report, process_filter, interval_in_seconds,
                                               printdecorator.Print, ProcessStatOptions)
                report.print_report(timestamp, header_indentation, value_indentation)

            if ProcessStatOptions.empty_arg_flag:
                process_report = ProcessStatus(metric_repository)
                process_filter = ProcessFilter(ProcessStatOptions)
                stdout = StdoutPrinter()
                printdecorator = NoneHandlingPrinterDecorator(stdout)
                report = ProcessStatusReporter(process_report, process_filter, interval_in_seconds,
                                               printdecorator.Print, ProcessStatOptions)
                report.print_report(timestamp, header_indentation, value_indentation)

            if ProcessStatOptions.pid_filter_flag:
                process_report = ProcessStatus(metric_repository)
                process_filter = ProcessFilter(ProcessStatOptions)
                stdout = StdoutPrinter()
                printdecorator = NoneHandlingPrinterDecorator(stdout)
                report = ProcessStatusReporter(process_report, process_filter, interval_in_seconds,
                                               printdecorator.Print, ProcessStatOptions)
                report.print_report(timestamp, header_indentation, value_indentation)
            if ProcessStatOptions.ppid_filter_flag:
                process_report = ProcessStatus(metric_repository)
                process_filter = ProcessFilter(ProcessStatOptions)
                stdout = StdoutPrinter()
                printdecorator = NoneHandlingPrinterDecorator(stdout)
                report = ProcessStatusReporter(process_report, process_filter, interval_in_seconds,
                                               printdecorator.Print, ProcessStatOptions)
                report.print_report(timestamp, header_indentation, value_indentation)

            if ProcessStatOptions.command_filter_flag:
                process_report = ProcessStatus(metric_repository)
                process_filter = ProcessFilter(ProcessStatOptions)
                stdout = StdoutPrinter()
                printdecorator = NoneHandlingPrinterDecorator(stdout)
                report = ProcessStatusReporter(process_report, process_filter, interval_in_seconds,
                                               printdecorator.Print, ProcessStatOptions)
                report.print_report(timestamp, header_indentation, value_indentation)

            if ProcessStatOptions.user_oriented_format:
                process_report = ProcessStatus(metric_repository)
                process_filter = ProcessFilter(ProcessStatOptions)
                stdout = StdoutPrinter()
                printdecorator = NoneHandlingPrinterDecorator(stdout)
                report = ProcessStatusReporter(process_report, process_filter, interval_in_seconds,
                                               printdecorator.Print, ProcessStatOptions)
                report.print_report(timestamp, header_indentation, value_indentation)

            if ProcessStatOptions.username_filter_flag:
                process_report = ProcessStatus(metric_repository)
                process_filter = ProcessFilter(ProcessStatOptions)
                stdout = StdoutPrinter()
                printdecorator = NoneHandlingPrinterDecorator(stdout)
                report = ProcessStatusReporter(process_report, process_filter, interval_in_seconds,
                                               printdecorator.Print, ProcessStatOptions)
                report.print_report(timestamp, header_indentation, value_indentation)

            # ================================================================
            if ProcessStatOptions.selective_colum_flag:
                process_report = ProcessStatus(metric_repository)
                process_filter = ProcessFilter(ProcessStatOptions)
                stdout = StdoutPrinter()
                printdecorator = NoneHandlingPrinterDecorator(stdout)
                report = DynamicProcessReporter(process_report, process_filter, interval_in_seconds,
                                                printdecorator.Print, ProcessStatOptions)
                report.print_report(timestamp, header_indentation, value_indentation)
        finally:
            sys.stdout.flush()


class ProcessStatOptions(pmapi.pmOptions):
    show_all_process = False
    command_filter_flag = False
    ppid_filter_flag = False
    pid_filter_flag = False
    username_filter_flag = False
    selective_colum_flag = False
    filter_flag = False
    user_oriented_format = False
    empty_arg_flag = False
    filterstate = None
    timefmt = "%H:%M:%S"
    print_count = None
    colum_list = []
    command_list = []
    pid_list = []
    ppid_list = []
    filtered_process_user = None
    context = None

    def __init__(self):
        pmapi.pmOptions.__init__(self, "t:c:e::p:ukVZ:z?:o:P:l:U:k")
        self.pmSetOptionCallback(self.extraOptions)
        self.pmSetOverrideCallback(self.override)
        self.options()

    def options(self):
        self.pmSetLongOptionHeader("General options")
        self.pmSetLongOption("", 0, "e", "",
                             "Show all process")
        self.pmSetLongOption("", 1, "c", "[Command name]",
                             "Show the stats for specified command name process")
        self.pmSetLongOption("", 1, "p", "[pid1,pid2,...]",
                             "Select the process by process ID")
        self.pmSetLongOption("", 1, "P", "[ppid1,ppid2,...]", "Select the process by process parent ID")
        self.pmSetLongOption("", 1, "U", "[User Name]", "Select the process by user name")
        self.pmSetLongOption("", 1, "o", "[col1,col2,... Or ALL]", "User -defined format " +
                             "USE -o [all] or -B [col1, col2 , ...]" +
                             "\n\t\t\tsupported user defined colums are command, wchan, started, Time, pid, ppid, "
                             "%mem, pri, user, %cpu and S "
                             "\n\t\t\tALL option shows USER,PID,PPID,PRI,%CPU,%MEM,VSZ,RSS,S,STARTED,TIME,WCHAN and "
                             "Command")
        self.pmSetLongOptionText("\tCOL\tHEADER \tDESCRIPTION")
        self.pmSetLongOptionText("\t%cpu\t%CPU \tcpu utilization of the process")
        self.pmSetLongOptionText("\t%mem\t%MEM  \tphysical memory on the machine expressed as a percentage")
        self.pmSetLongOptionText("\tstart\tSTART \ttime the command started")
        self.pmSetLongOptionText("\ttime\tTIME \taccumulated cpu time, user + system")
        self.pmSetLongOptionText("\tcls\tCLS \tscheduling class of the process")
        self.pmSetLongOptionText("\tcmd\tCMD\tsee args.  (alias args, command).")
        self.pmSetLongOptionText("\tpid\tPID \tthe process ID")
        self.pmSetLongOptionText("\tppid\tPPID\tparent process ID")
        self.pmSetLongOptionText("\tpri\tPRI \tpriority of the process")
        self.pmSetLongOptionText("\tstate\tS \tsee s")
        self.pmSetLongOptionText("\trss\tRSS \tthe non-swapped physical memory that a task has used")
        self.pmSetLongOptionText("\trtprio\tRTPRIO \trealtime priority")
        self.pmSetLongOptionText("\tpname\tPname\tProcess name")
        # self.pmSetLongOptionText("\ttime\tTIME \tcumulative CPU time")
        self.pmSetLongOptionText("\ttty\tTT \tcontrolling tty (terminal)")
        self.pmSetLongOptionText("\tuid\tUID \tsee euid")
        self.pmSetLongOptionText("\tvsize\tVSZ \tsee vsz")
        self.pmSetLongOptionText("\tuname\tUSER \tsee euser")
        self.pmSetLongOptionText("\twchan\tWCHAN \tname of the kernel function in which the process is sleeping")
        self.pmSetLongOption("", 0, 'u', "", "Display user-oriented format")
        self.pmSetLongOptionVersion()
        self.pmSetLongOptionTimeZone()
        self.pmSetLongOptionHostZone()
        self.pmSetLongOptionHelp()

    def override(self, opts):
        ProcessStatOptions.print_count = self.pmGetOptionSamples()
        # """Override standard Pcp-ps option to show all process """
        return bool(opts in ['p', 'c', 'o', 'P', 'U'])

    def extraOptions(self, opts, optarg, index):
        if opts == 'e':
            ProcessStatOptions.show_all_process = True
        elif opts == 'c':
            ProcessStatOptions.command_filter_flag = True
            ProcessStatOptions.filter_flag = True
            try:
                if optarg is not None:
                    ProcessStatOptions.command_list += optarg.replace(',', ' ').split(' ')
            except ValueError:
                print("Invalid command Id List: use comma separated pids without whitespaces")
                sys.exit(1)
        elif opts == 'p':
            ProcessStatOptions.filter_flag = True
            ProcessStatOptions.pid_filter_flag = True
            try:
                if optarg is not None:
                    dummy_list = optarg.replace(',', ' ').split(' ')
                    ProcessStatOptions.pid_list += [int(x) for x in dummy_list]
            except ValueError:
                print("Invalid pid Id List: use comma separated pids without whitespaces")
                sys.exit(1)
        elif opts == 'P':
            ProcessStatOptions.filter_flag = True
            ProcessStatOptions.ppid_filter_flag = True
            try:
                if optarg is not None:
                    dummy_list = optarg.replace(',', ' ').split(' ')
                    ProcessStatOptions.ppid_list += [int(x) for x in dummy_list]
            except ValueError:
                print("Invalid ppid Id List: use comma separated pids without whitespaces")
                sys.exit(1)
        elif opts == 'u':
            ProcessStatOptions.user_oriented_format = True
        elif opts == 'o':
            ProcessStatOptions.selective_colum_flag = True
            try:
                if optarg.upper() == "ALL":
                    ProcessStatOptions.filterstate = optarg.upper()
                else:
                    dummy_list = optarg.replace(',', ' ').split(' ')
                    for key in dummy_list:
                        if key.lower() in PIDINFO_PAIR:
                            ProcessStatOptions.colum_list.append(key.lower())
                        else:
                            raise ValueError
            except ValueError:
                print("Invalid ppid Id List: Either column name is not correct "
                      "or use comma separated column names without whitespaces")
                sys.exit(1)
        elif opts == 'U':
            ProcessStatOptions.username_filter_flag = True
            ProcessStatOptions.filter_flag = True
            ProcessStatOptions.filtered_process_user = optarg
        elif opts is None:
            ProcessStatOptions.show_all_process = True

    @staticmethod
    def checkOptions():
        if ProcessStatOptions.selective_colum_flag or \
                ProcessStatOptions.filter_flag or \
                ProcessStatOptions.user_oriented_format:
            return True
        else:
            ProcessStatOptions.empty_arg_flag = True
            return True


if __name__ == "__main__":
    try:
        opts = ProcessStatOptions()
        manager = pmcc.MetricGroupManager.builder(opts, sys.argv)
        ProcessStatOptions.context = manager.type
        if not opts.checkOptions():
            raise pmapi.pmUsageErr
        missing = manager.checkMissingMetrics(PSSTAT_METRICS)
        if missing is not None:
            sys.stderr.write('Error: not all required metrics are available\nMissing %s\n' % missing)
            sys.exit(1)
        manager['psstat'] = PSSTAT_METRICS
        manager.printer = ProcessstatReport()
        sts = manager.run()
        sys.exit(sts)
    except pmapi.pmErr as pmerror:
        sys.stderr.write('%s\n' % (pmerror.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except IOError:
        signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    except KeyboardInterrupt:
        print("Interrupted")
        sys.exit(0)
