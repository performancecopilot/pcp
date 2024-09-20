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
# pylint: disable=bad-whitespace,too-many-arguments,too-many-lines, bad-continuation
# pylint: disable=redefined-outer-name,unnecessary-lambda
#
import signal
import sys
import time
from pcp import pmapi, pmcc
from cpmapi import PM_CONTEXT_ARCHIVE, PM_MODE_FORW

SYS_MECTRICS= ["kernel.uname.sysname","kernel.uname.release",
               "kernel.uname.nodename","kernel.uname.machine","hinv.ncpu"]
BUDDYSTAT_METRICS = ["mem.buddyinfo.pages","mem.buddyinfo.total"]

ALL_METRICS = BUDDYSTAT_METRICS + SYS_MECTRICS

def adjust_length(name,size):
    return name.ljust(size)

class ReportingMetricRepository:

    def __init__(self,group):
        self.group=group
        self.current_cached_values = {}

    def __fetch_current_value(self,metric):
        val=dict(map(lambda x: (x[1], x[2]), self.group[metric].netValues))
        return dict(val)

    def current_value(self,metric):
        if metric not in self.group:
            return None
        if self.current_cached_values.get(metric) is None:
            first_value=self.__fetch_current_value(metric)
            self.current_cached_values[metric]=first_value
        return self.current_cached_values[metric]

class BuddyStatUtil:
    def __init__(self,metrics_repository):
        self.__metric_repository=metrics_repository
        self.report=ReportingMetricRepository(self.__metric_repository)

    def buddy_pages(self):
        return self.report.current_value('mem.buddyinfo.pages')

    def buddy_total_pages(self):
        return self.report.current_value('mem.buddyinfo.total')

    def names(self):
        data = self.report.current_value('mem.buddyinfo.pages')
        return data.keys()

class BuddyinfoReport(pmcc.MetricGroupPrinter):
    def __init__(self,opts,group):
        self.opts=opts
        self.group=group
        self.context=opts.context
        self.samples=opts.samples
        self.header = "unknown"

    def __get_ncpu(self, group):
        return group['hinv.ncpu'].netValues[0][2]

    def __print_machine_info(self, context):
        if self.header == "unknown":
            try:
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
                self.header = header_string
            except  IndexError:
                pass
        print("%s  (%s CPU)" % (self.header, self.__get_ncpu(context)))

    def __print_header(self,header_indentation,value_indentation):
        value_indentation+=" "*2
        print("TimeStamp"+ " "+header_indentation + "Normal" + header_indentation+value_indentation+" " +
              "Nodes" + header_indentation + "Order0" + value_indentation +
              "Order1" + value_indentation +"Order2" + value_indentation + "Order3" + value_indentation +
              "Order4" + value_indentation +"Order5" + value_indentation + "Order6" + value_indentation +
              "Order7" + value_indentation +"Order8" + value_indentation + "Order9" +
              value_indentation + "Order10")

    def __print_values(self,timestamp,header_indentation,\
                     value_indentation,buddystatus):
        names=buddystatus.names()
        pages=buddystatus.buddy_pages()
        order_set = set()
        nodes_set = set()
        no_of_nodes_set = set()
        for name in names:
            part=name.split('::')
            if len(part)==3:
                nodes_set.add(part[0])
                order_set.add(part[1])
                no_of_nodes_set.add(part[2])
        def __extract_numeric_part(element):
            return int(element[5:])
        for Normal in sorted(nodes_set):
            for node in no_of_nodes_set:
                value=""
                for order in sorted(order_set,key=__extract_numeric_part):
                    nodename = adjust_length(Normal,9) if len(Normal) < 9 else Normal
                    key = "%s::%s::%s" % (Normal, order, node)
                    data = str(pages.get(key,0))
                    value +=  adjust_length(data,8) if len(data) < 8 else data
                    value += value_indentation

                print("%s %s %s %s %s %s %s "%(timestamp,header_indentation,nodename,header_indentation,node,
                                                  header_indentation,value))

    def print_report(self,group,timestamp,header_indentation,value_indentation,manager_buddyinfo):

        def __print_buddy_status():
            buddystatus = BuddyStatUtil(manager_buddyinfo)
            if buddystatus.names():
                try:
                    self.__print_machine_info(group)
                    self.__print_header(header_indentation, value_indentation)
                    self.__print_values(timestamp, header_indentation, value_indentation, buddystatus)
                except IndexError:
                    print("Incorrect machine info due to some missing metrics")
                return
            else:
                return

        if self.context != PM_CONTEXT_ARCHIVE and self.samples is None:
            __print_buddy_status()
            sys.exit(0)
        elif self.context == PM_CONTEXT_ARCHIVE and self.samples is None:
            __print_buddy_status()
        elif self.samples >=1:
            __print_buddy_status()
            self.samples-=1

    def report(self, manager):
        group = manager["sysinfo"]
        self.samples=self.opts.pmGetOptionSamples()
        t_s = group.contextCache.pmLocaltime(int(group.timestamp))
        timestamp = time.strftime(BuddyinfoOptions.timefmt, t_s.struct_time())
        header_indentation = "        " if len(timestamp) < 9 else (len(timestamp) - 7) * " "
        value_indentation = ((len(header_indentation) + 2) - len(timestamp)) * " "
        self.print_report(group,timestamp,header_indentation,value_indentation,manager['buddyinfo'])

class BuddyinfoOptions(pmapi.pmOptions):
    timefmt = "%H:%M:%S"
    uflag = False

    def extraOptions(self,opt,optarg,index):
        if opt == 'u':
            BuddyinfoOptions.uflag = True

    def __init__(self):
        pmapi.pmOptions.__init__(self, "a:s:Z:uzV?")
        self.pmSetLongOptionHeader("General options")
        self.pmSetOptionCallback(self.extraOptions)
        self.pmSetLongOptionArchive()
        self.pmSetLongOption("no-interpolation", 0, "u", "", "disable interpolation mode with archives")
        self.pmSetLongOptionHostZone()
        self.pmSetLongOptionTimeZone()
        self.pmSetLongOptionHelp()
        self.pmSetLongOptionSamples()
        self.pmSetLongOptionVersion()
        self.samples=None
        self.context=None
    def checkOptions(self, manager):
        if BuddyinfoOptions.uflag:
            if manager._options.pmGetOptionInterval():
                print("Error: -t incompatible with -u")
                return False
            if manager.type != PM_CONTEXT_ARCHIVE:
                print("Error: -u can only be specified with -a archive")
                return False
        return True

if __name__ == '__main__':
    try:
        opts = BuddyinfoOptions()
        mngr = pmcc.MetricGroupManager.builder(opts,sys.argv)
        opts.context=mngr.type

        if not opts.checkOptions(mngr):
            raise pmapi.pmUsageErr
        if BuddyinfoOptions.uflag:
            # -u turns off interpolation
            mngr.pmSetMode(PM_MODE_FORW, mngr._options.pmGetOptionOrigin(), 0)

        missing = mngr.checkMissingMetrics(ALL_METRICS)
        if missing is not None:
            sys.stderr.write('Error: not all required metrics are available\nMissing: %s\n' % (missing))
            sys.exit(1)
        mngr["buddyinfo"] = BUDDYSTAT_METRICS
        mngr["sysinfo"] = SYS_MECTRICS
        mngr["allinfo"]=ALL_METRICS
        mngr.printer = BuddyinfoReport(opts,mngr)
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
