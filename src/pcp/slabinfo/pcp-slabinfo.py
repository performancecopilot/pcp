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
from cpmapi import PM_CONTEXT_ARCHIVE

SYS_MECTRICS= ["kernel.uname.sysname","kernel.uname.release",
               "kernel.uname.nodename","kernel.uname.machine","hinv.ncpu"]
SLABSTAT_METRICS = ["mem.slabinfo.slabs.active",
                    "mem.slabinfo.slabs.pages_per_slab","mem.slabinfo.slabs.total_size",
                    "mem.slabinfo.slabs.objects_per_slab","mem.slabinfo.objects.active",
                    "mem.slabinfo.objects.total","mem.slabinfo.objects.size",
                    "mem.slabinfo.slabs.total" ]
ALL_METRICS = SLABSTAT_METRICS + SYS_MECTRICS
def adjust_length(name):
    return name.ljust(25)
class ReportingMetricRepository:

    def __init__(self,group):
        self.group=group
        self.current_cached_values = {}

    def __sorted(self,data):
        return dict(sorted(data.items(), key=lambda item: item[0].lower()))

    def __fetch_current_value(self,metric):
        val=dict(map(lambda x: (x[1], x[2]), self.group[metric].netValues))
        val=self.__sorted(val)
        return dict(val)

    def current_value(self,metric):
        if not metric in self.group:
            return None
        if self.current_cached_values.get(metric) is None:
            first_value=self.__fetch_current_value(metric)
            self.current_cached_values[metric]=first_value
        return self.current_cached_values[metric]

class SlabStatUtil:
    def __init__(self,metrics_repository):
        self.__metric_repository=metrics_repository
        self.report=ReportingMetricRepository(self.__metric_repository)

    def active_objs(self):
        return self.report.current_value('mem.slabinfo.objects.active')

    def objs_size(self):
        return self.report.current_value('mem.slabinfo.objects.size')

    def obj_total(self):
        return self.report.current_value('mem.slabinfo.objects.total')

    def slab_total_size(self):
        return self.report.current_value('mem.slabinfo.slabs.total_size')

    def objects_per_slab(self):
        return self.report.current_value('mem.slabinfo.slabs.objects_per_slab')

    def page_per_slab(self):
        return self.report.current_value('mem.slabinfo.slabs.pages_per_slab')

    def slab_active(self):
        return self.report.current_value('mem.slabinfo.slabs.active')

    def slabs_total(self):
        return self.report.current_value('mem.slabinfo.slabs.total')

    def names(self):
        data = self.report.current_value('mem.slabinfo.objects.active')
        return data.keys()

class SlabinfoReport(pmcc.MetricGroupPrinter):
    def __init__(self,samples,group,context):
        self.samples = samples
        self.group=group
        self.context=context

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

    def __print_header(self,header_indentation,value_indentation):
        print("TimeStamp"+ " "+header_indentation + "Name" + " "*22 +value_indentation+ "active_objs" +
              value_indentation +"num_objs" + value_indentation + "objsize byte" + value_indentation +
              "objperslab" + value_indentation +"pagesperslab" + value_indentation + "active_slabs" +
              value_indentation + "num_slabs")

    def __print_values(self,timestamp,header_indentation,\
                     value_indentation,slabstatus):
        names=slabstatus.names()
        active_objects=slabstatus.active_objs()
        num_objs=slabstatus.objs_size()
        objsize=slabstatus.obj_total()
        objperslab=slabstatus.objects_per_slab()
        pageperslab=slabstatus.page_per_slab()
        active_slabs=slabstatus.slab_active()
        num_slabs=slabstatus.slabs_total()
        for name in names:
            data = adjust_length(name) if len(name) < 25 else name
            print("%s %s %s %s %8d %s %8d %s %8d %s %7d %s %9d %s %8d %s %8d"
                                    % (timestamp, header_indentation,data, value_indentation, active_objects[name],
                                       value_indentation,objsize[name],value_indentation, num_objs[name],
                                       value_indentation, objperslab[name],value_indentation,pageperslab[name],
                                       value_indentation, active_slabs[name], value_indentation, num_slabs[name]))

    def print_report(self,group,timestamp,header_indentation,value_indentation,manager_slabinfo):
        def __print_slab_status():
            slabstatus = SlabStatUtil(manager_slabinfo)
            if slabstatus.names():
                try:
                    self.__print_machine_info(group)
                    self.__print_header(header_indentation, value_indentation)
                    self.__print_values(timestamp, header_indentation, value_indentation, slabstatus)
                except IndexError:
                    print("Incorrect machine info due to some missing metrics")
                return
            else:
                pass

        if self.context != PM_CONTEXT_ARCHIVE and self.samples is None:
            __print_slab_status()
            sys.exit(0)
        elif self.context == PM_CONTEXT_ARCHIVE and self.samples is None:
            __print_slab_status()
        elif self.samples >=1:
            __print_slab_status()
            self.samples-=1
        else:
            pass

    def report(self, manager):
        group = manager["sysinfo"]
        self.samples=opts.pmGetOptionSamples()
        t_s = group.contextCache.pmLocaltime(int(group.timestamp))
        timestamp = time.strftime(SlabinfoOptions.timefmt, t_s.struct_time())
        header_indentation = "        " if len(timestamp) < 9 else (len(timestamp) - 7) * " "
        value_indentation = ((len(header_indentation) + 9) - len(timestamp)) * " "
        self.print_report(group,timestamp,header_indentation,value_indentation,manager['slabinfo'])

class SlabinfoOptions(pmapi.pmOptions):
    timefmt = "%H:%M:%S"
    def __init__(self):
        pmapi.pmOptions.__init__(self, "a:s:Z:zV?")
        self.pmSetLongOptionHeader("General options")
        self.pmSetLongOptionHostZone()
        self.pmSetLongOptionTimeZone()
        self.pmSetLongOptionHelp()
        self.pmSetLongOptionSamples()
        self.pmSetLongOptionVersion()
        self.samples=None
        self.context=None

if __name__ == '__main__':
    try:
        opts = SlabinfoOptions()
        mngr = pmcc.MetricGroupManager.builder(opts,sys.argv)
        opts.context=mngr.type
        missing = mngr.checkMissingMetrics(ALL_METRICS)
        if missing is not None:
            sys.stderr.write('Error: not all required metrics are available\nMissing %s\n' % missing)
            sys.exit(1)
        mngr["slabinfo"] = SLABSTAT_METRICS
        mngr["sysinfo"] = SYS_MECTRICS
        mngr["allinfo"]=ALL_METRICS
        mngr.printer = SlabinfoReport(opts.samples,mngr,opts.context)
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
