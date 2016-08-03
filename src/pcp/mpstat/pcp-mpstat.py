#!/usr/bin/env pmpython
#
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

from pcp import pmapi
from pcp import pmcc
import sys
MPSTAT_METRICS = ['pmda.uname', 'hinv.map.cpu_num', 'hinv.ncpu', 'hinv.cpu.online', 'kernel.all.cpu.user',
                'kernel.all.cpu.nice', 'kernel.all.cpu.sys', 'kernel.all.cpu.wait.total',
                'kernel.all.cpu.irq.hard', 'kernel.all.cpu.irq.soft', 'kernel.all.cpu.steal',
                'kernel.all.cpu.guest', 'kernel.all.cpu.guest_nice', 'kernel.all.cpu.idle',
                'kernel.percpu.cpu.user', 'kernel.percpu.cpu.nice', 'kernel.percpu.cpu.sys',
                'kernel.percpu.cpu.wait.total', 'kernel.percpu.cpu.irq.hard', 'kernel.percpu.cpu.irq.soft',
                'kernel.percpu.cpu.steal', 'kernel.percpu.cpu.guest','kernel.percpu.cpu.guest_nice',
                'kernel.percpu.cpu.idle', 'kernel.all.intr', 'kernel.percpu.cpu.intr']
interrupts_list = []
soft_interrupts_list = []

class StdoutPrinter:
    def Print(self, args):
        print(args)

class NamedInterrupts:
    def __init__(self, context, metric):
        self.context = context
        self.interrupt_list = []
        self.metric = metric

    def append_callback(self, args):
        self.interrupt_list.append(args)

    def get_all_named_interrupt_metrics(self):
        if not self.interrupt_list:
            self.context.pmTraversePMNS(self.metric,self.append_callback)
            self.interrupt_list.reverse()
        return self.interrupt_list

class MetricRepository:
    def __init__(self,group):
        self.group = group
        self.current_cached_values = {}
        self.previous_cached_values = {}

    def current_value(self, metric, instance):
        if not metric in self.group:
            return None
        if instance is not None:
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
        if instance is not None:
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

    def __fetch_current_values(self,metric,instance):
        if instance is not None:
            return dict(map(lambda x: (x[0].inst, x[2]), self.group[metric].netValues))
        else:
            return self.group[metric].netValues[0][2]

    def __fetch_previous_values(self,metric,instance):
        if instance is not None:
            return dict(map(lambda x: (x[0].inst, x[2]), self.group[metric].netPrevValues))
        else:
            if self.group[metric].netPrevValues == []:
                return None
            else:
                return self.group[metric].netPrevValues[0][2]


class CoreCpuUtil:
    def __init__(self, instance, delta_time, metric_repository):
        self.delta_time = delta_time
        self.instance = instance
        self.metric_repository = metric_repository

    def total_cpus(self):
        return self.metric_repository.current_value('hinv.ncpu', None)
    def cpu_number(self):
        return self.instance

    def cpu_online(self):
        return self.metric_repository.current_value('hinv.cpu.online', self.instance)

    def user_time(self):
        metric = 'kernel.' + self.__all_or_percpu() + '.cpu.user'
        p_time = self.metric_repository.previous_value(metric, self.instance)
        c_time = self.metric_repository.current_value(metric, self.instance)
        if p_time is not None and c_time is not None:
            value = (100*(c_time - p_time))/(1000*self.delta_time)
            if self.instance is None:
                return float("%.2f"%(value/self.total_cpus()))
            else:
                return float("%.2f"%(value))

        else:
            return None

    def nice_time(self):
        metric = 'kernel.' + self.__all_or_percpu() + '.cpu.nice'
        p_time = self.metric_repository.previous_value(metric, self.instance)
        c_time = self.metric_repository.current_value(metric, self.instance)
        if p_time is not None and c_time is not None:
            value = (100*(c_time - p_time))/(1000*self.delta_time)
            if self.instance is None:
                return float("%.2f"%(value/self.total_cpus()))
            else:
                return float("%.2f"%(value))
        else:
            return None

    def sys_time(self):
        metric = 'kernel.' + self.__all_or_percpu() + '.cpu.sys'
        p_time = self.metric_repository.previous_value(metric, self.instance)
        c_time = self.metric_repository.current_value(metric, self.instance)
        if p_time is not None and c_time is not None:
            value = (100*(c_time - p_time))/(1000*self.delta_time)
            if self.instance is None:
                return float("%.2f"%(value/self.total_cpus()))
            else:
                return float("%.2f"%(value))
        else:
            return None

    def iowait_time(self):
        metric = 'kernel.' + self.__all_or_percpu() + '.cpu.wait.total'
        p_time = self.metric_repository.previous_value(metric, self.instance)
        c_time = self.metric_repository.current_value(metric, self.instance)
        if p_time is not None and c_time is not None:
            value = (100*(c_time - p_time))/(1000*self.delta_time)
            if self.instance is None:
                return float("%.2f"%(value/self.total_cpus()))
            else:
                return float("%.2f"%(value))
        else:
            return None

    def irq_hard(self):
        metric = 'kernel.' + self.__all_or_percpu() + '.cpu.irq.hard'
        p_time = self.metric_repository.previous_value(metric, self.instance)
        c_time = self.metric_repository.current_value(metric, self.instance)
        if p_time is not None and c_time is not None:
            value = (100*(c_time - p_time))/(1000*self.delta_time)
            if self.instance is None:
                return float("%.2f"%(value/self.total_cpus()))
            else:
                return float("%.2f"%(value))
        else:
            return None

    def irq_soft(self):
        metric = 'kernel.' + self.__all_or_percpu() + '.cpu.irq.soft'
        p_time = self.metric_repository.previous_value(metric, self.instance)
        c_time = self.metric_repository.current_value(metric, self.instance)
        if p_time is not None and c_time is not None:
            value = (100*(c_time - p_time))/(1000*self.delta_time)
            if self.instance is None:
                return float("%.2f"%(value/self.total_cpus()))
            else:
                return float("%.2f"%(value))
        else:
            return None

    def steal(self):
        metric = 'kernel.' + self.__all_or_percpu() + '.cpu.steal'
        p_time = self.metric_repository.previous_value(metric, self.instance)
        c_time = self.metric_repository.current_value(metric, self.instance)
        if p_time is not None and c_time is not None:
            value = (100*(c_time - p_time))/(1000*self.delta_time)
            if self.instance is None:
                return float("%.2f"%(value/self.total_cpus()))
            else:
                return float("%.2f"%(value))
        else:
            return None

    def guest_time(self):
        metric = 'kernel.' + self.__all_or_percpu() + '.cpu.guest'
        p_time = self.metric_repository.previous_value(metric, self.instance)
        c_time = self.metric_repository.current_value(metric, self.instance)
        if p_time is not None and c_time is not None:
            value = (100*(c_time - p_time))/(1000*self.delta_time)
            if self.instance is None:
                return float("%.2f"%(value/self.total_cpus()))
            else:
                return float("%.2f"%(value))
        else:
            return None

    def guest_nice(self):
        metric = 'kernel.' + self.__all_or_percpu() + '.cpu.guest_nice'
        p_time = self.metric_repository.previous_value(metric, self.instance)
        c_time = self.metric_repository.current_value(metric, self.instance)
        if p_time is not None and c_time is not None:
            value = (100*(c_time - p_time))/(1000*self.delta_time)
            if self.instance is None:
                return float("%.2f"%(value/self.total_cpus()))
            else:
                return float("%.2f"%(value))
        else:
            return None

    def idle_time(self):
        metric = 'kernel.' + self.__all_or_percpu() + '.cpu.idle'
        p_time = self.metric_repository.previous_value(metric, self.instance)
        c_time = self.metric_repository.current_value(metric, self.instance)
        if p_time is not None and c_time is not None:
            value = (100*(c_time - p_time))/(1000*self.delta_time)
            if self.instance is None:
                return float("%.2f"%(value/self.total_cpus()))
            else:
                return float("%.2f"%(value))
        else:
            return None

    def __all_or_percpu(self):
        return 'all' if self.instance is None else 'percpu'

class CpuUtil:
    def __init__(self, delta_time, metric_repository):
        self.__metric_repository = metric_repository
        self.delta_time = delta_time

    def get_totalcpu_util(self):
        return CoreCpuUtil(None, self.delta_time, self.__metric_repository)

    def get_percpu_util(self):
        return list(map((lambda cpuid: (CoreCpuUtil(cpuid, self.delta_time, self.__metric_repository))), self.__cpus()))

    def __cpus(self):
        cpu_dict = self.__metric_repository.current_values('hinv.map.cpu_num')
        return sorted(cpu_dict.values())
class TotalCpuInterrupt:
    def __init__(self, cpu_num, intr_value, metric_repository):
        self.cpu_num = cpu_num
        self.intr_value = intr_value
        self.metric_repository = metric_repository
    def cpu_number(self):
        return self.cpu_num
    def cpu_online(self):
        return self.metric_repository.current_value('hinv.cpu.online', self.cpu_num)
    def value(self):
        return self.intr_value

class TotalInterruptUsage:
    def __init__(self, delta_time, metric_repository):
        self.delta_time = delta_time
        self.__metric_repository = metric_repository

    def total_interrupt_per_delta_time(self):
        c_value = self.__metric_repository.current_value('kernel.all.intr', None)
        p_value = self.__metric_repository.previous_value('kernel.all.intr', None)
        if c_value is not None and p_value is not None:
            return float("%.2f"%(float(c_value - p_value)/self.delta_time))
        else:
            return None
    def total_interrupt_percpu_per_delta_time(self):
        return list(map((lambda cpuid: TotalCpuInterrupt(cpuid, self.__get_cpu_total_interrupt(cpuid), self.__metric_repository)), self.__cpus()))

    def __get_cpu_total_interrupt(self, instance):
        c_value = self.__metric_repository.current_value('kernel.percpu.cpu.intr', instance)
        p_value = self.__metric_repository.previous_value('kernel.percpu.cpu.intr', instance)
        if c_value is not None and p_value is not None:
            return float("%.2f"%(float(c_value - p_value)/self.delta_time))
        else:
            return None

    def __cpus(self):
        cpu_dict = self.__metric_repository.current_values('hinv.map.cpu_num')
        return sorted(cpu_dict.values())

class InterruptUsage:
    def __init__(self, delta_time, metric_repository, metric, instance):
        self.__name = metric.split('.')[-1]
        self.instance = instance
        self.metric = metric
        self.delta_time = delta_time
        self.__metric_repository = metric_repository

    def name(self):
        if self.__name.startswith("line"):
            return self.__name[4:]
        return self.__name

    def value(self):
        c_value = self.__metric_repository.current_value(self.metric, self.instance)
        p_value = self.__metric_repository.previous_value(self.metric, self.instance)
        if c_value is not None and p_value is not None:
            return float("%.2f"%((c_value - p_value)/self.delta_time))
        else:
            return None

class CpuInterrupts:
    def __init__(self, metric_repository, cpu_number, interrupts):
        self.metric_repository = metric_repository
        self.cpu_num = cpu_number
        self.interrupts = interrupts
    def cpu_number(self):
        return self.cpu_num
    def cpu_online(self):
        return self.metric_repository.current_value('hinv.cpu.online', self.cpu_num)

class HardInterruptUsage:
    def __init__(self, delta_time, metric_repository, interrupt_metrics):
        self.delta_time = delta_time
        self.metric_repository = metric_repository
        self.interrupt_metrics = interrupt_metrics

    def get_percpu_interrupts(self):
        return list(map((lambda cpuid: CpuInterrupts(self.metric_repository, cpuid, self.__get_all_interrupts_for_cpu(cpuid))), self.__cpus()))

    def __cpus(self):
        cpu_dict = self.metric_repository.current_values('hinv.map.cpu_num')
        return sorted(cpu_dict.values())

    def __get_all_interrupts_for_cpu(self, cpuid):
        return list(map((lambda metric: InterruptUsage(self.delta_time, self.metric_repository, metric, cpuid)), self.interrupt_metrics))

class SoftInterruptUsage:
    def __init__(self, delta_time, metric_repository, interrupt_metrics):
        self.delta_time = delta_time
        self.metric_repository = metric_repository
        self.interrupt_metrics = interrupt_metrics

    def get_percpu_interrupts(self):
        return list(map((lambda cpuid: CpuInterrupts(self.metric_repository, cpuid, self.__get_all_interrupts_for_cpu(cpuid))), self.__cpus()))

    def __cpus(self):
        cpu_dict = self.metric_repository.current_values('hinv.map.cpu_num')
        return sorted(cpu_dict.values())

    def __get_all_interrupts_for_cpu(self, cpuid):
        return list(map((lambda metric: InterruptUsage(self.delta_time, self.metric_repository, metric, cpuid)), self.interrupt_metrics))


class CpuFilter:
    def __init__(self, mpstat_options):
        self.mpstat_options = mpstat_options

    def filter_cpus(self, cpus):
        return list(filter(lambda c: self.__matches_cpu(c), cpus))

    def __matches_cpu(self, cpu):
        if self.mpstat_options.cpu_list == 'ALL':
            return True
        elif self.mpstat_options.cpu_list == 'ON':
            if cpu.cpu_online() == 1:
                return True
            else:
                return False
        elif self.mpstat_options.cpu_list is not None:
            if cpu.cpu_number() in self.mpstat_options.cpu_list:
                return True
            else:
                return False
        else:
            return True

class CpuUtilReporter:
    def __init__(self, cpu_filter, printer, mpstat_options):
        self.cpu_filter = cpu_filter
        self.printer = printer
        self.mpstat_options = mpstat_options
        self.header_print = True

    def print_report(self, cpu_utils, timestamp):
        if self.header_print:
            self.printer("\n%-10s\t%-3s\t%-5s\t%-6s\t%-5s\t%-8s\t%-5s\t%-6s\t%-7s\t%-7s\t%-6s\t%-6s"%("Timestamp","CPU","%usr","%nice","%sys","%iowait","%irq","%soft","%steal","%guest","%nice","%idle"))
            self.header_print = False
        if self.mpstat_options.cpu_list == "ALL" or self.mpstat_options.cpu_list == "ON":
            self.header_print = True
        if type(self.mpstat_options.cpu_list) != type([]):
            cpu_util = cpu_utils.get_totalcpu_util()
            self.printer("%-10s\t%-3s\t%-5s\t%-6s\t%-5s\t%-8s\t%-5s\t%-6s\t%-7s\t%-7s\t%-6s\t%-6s"%(timestamp,"all",
            cpu_util.user_time(), cpu_util.nice_time(), cpu_util.sys_time(), cpu_util.iowait_time(),
            cpu_util.irq_hard(), cpu_util.irq_soft(), cpu_util.steal(), cpu_util.guest_time(),
            cpu_util.guest_nice(), cpu_util.idle_time()))
        if self.mpstat_options.cpu_filter == True:
            cpu_util_list = self.cpu_filter.filter_cpus(cpu_utils.get_percpu_util())
            for cpu_util in cpu_util_list:
                self.printer("%-10s\t%-3s\t%-5s\t%-6s\t%-5s\t%-8s\t%-5s\t%-6s\t%-7s\t%-7s\t%-6s\t%-6s"%(timestamp,
                 cpu_util.cpu_number(), cpu_util.user_time(), cpu_util.nice_time(), cpu_util.sys_time(),
                 cpu_util.iowait_time(), cpu_util.irq_hard(), cpu_util.irq_soft(), cpu_util.steal(),
                 cpu_util.guest_time(), cpu_util.guest_nice(), cpu_util.idle_time()))

class TotalInterruptUsageReporter:
    def __init__(self, cpu_filter, printer, mpstat_options):
        self.cpu_filter = cpu_filter
        self.printer = printer
        self.mpstat_options = mpstat_options
        self.print_header = True

    def print_report(self, total_interrupt_usage, timestamp):
        self.total_interrupt_usage = total_interrupt_usage
        if self.print_header:
            self.printer("%-10s\t%-5s\t%-5s"%("\nTimestamp","CPU","intr/s"))
            self.print_header = False
        if self.mpstat_options.cpu_list == "ALL" or self.mpstat_options.cpu_list == "ON" or self.mpstat_options.interrupt_type == "ALL":
            self.print_header = True
        if type(self.mpstat_options.cpu_list) != type([]):
            self.printer("%-10s\t%-5s\t%-5s"%(timestamp,'all',self.total_interrupt_usage.total_interrupt_per_delta_time()))

        if self.mpstat_options.cpu_filter == True:
            percpu_total_interrupt_list = self.cpu_filter.filter_cpus(self.total_interrupt_usage.total_interrupt_percpu_per_delta_time())
            for total_cpu_interrupt in percpu_total_interrupt_list:
                self.printer("%-10s\t%-5s\t%-5s"%(timestamp, total_cpu_interrupt.cpu_number(), total_cpu_interrupt.value()))

class InterruptUsageReporter:
    def __init__(self, cpu_filter, printer, mpstat_options):
        self.cpu_filter = cpu_filter
        self.printer = printer
        self. mpstat_options = mpstat_options
        self.print_header = True

    def print_report(self, interrupt_usage, timestamp):
        self.interrupt_usage = interrupt_usage
        cpu_interrupts = self.interrupt_usage.get_percpu_interrupts()
        header_values = ("\nTimestamp","cpu")
        format_str = "%-10s\t%-4s\t"

        # use the first CPU in cpu_interrupts to get the interrupt names
        for interrupt in cpu_interrupts[0].interrupts:
            format_str += "%-"+str(len(interrupt.name())+2)+"s\t"
            header_values += (interrupt.name() + "/s",)
        if self.print_header:
            self.printer(format_str % header_values)
            self.print_header = False
        if self.mpstat_options.cpu_list == "ALL" or self.mpstat_options.cpu_list == "ON" or self.mpstat_options.interrupt_type == "ALL":
            self.print_header = True

        cpu_interrupts_list = self.cpu_filter.filter_cpus(cpu_interrupts)
        for cpu_interrupt in cpu_interrupts_list:
            values = (timestamp, cpu_interrupt.cpu_number()) + tuple(map((lambda interrupt: interrupt.value()), cpu_interrupt.interrupts))
            self.printer(format_str % values)

class NoneHandlingPrinterDecorator:
    def __init__(self, printer):
        self.printer = printer

    def Print(self, args):
        new_args = args.replace('None','?')
        self.printer(new_args)

class MpstatOptions(pmapi.pmOptions):
    cpu_list = None
    cpu_filter = False
    options_all = False
    interrupts_filter = False
    interrupt_type = None
    no_options = True

    def extraOptions(self, opt,optarg, index):
        MpstatOptions.no_options = False
        if opt == 'u':
            MpstatOptions.no_options = True
        elif opt == 'A':
            MpstatOptions.interrupts_filter = True
            MpstatOptions.interrupt_type = 'ALL'
            MpstatOptions.cpu_filter = True
            MpstatOptions.cpu_list = 'ALL'
            MpstatOptions.no_options = True
        elif opt == "I":
            MpstatOptions.interrupts_filter = True
            MpstatOptions.interrupt_type = optarg
        elif opt == 'P':
            if optarg == 'ALL' or optarg == 'ON':
                MpstatOptions.cpu_filter = True
                MpstatOptions.cpu_list = optarg
            else:
                MpstatOptions.cpu_filter = True
                try:
                    MpstatOptions.cpu_list = list(map(lambda x:int(x),optarg.split(',')))
                except ValueError as e:
                    print ("Invalid CPU List: use comma separated cpu nos without whitespaces")
                    sys.exit(1)

    def override(self, opt):
        """ Override standard PCP options to match mpstat(1) """
        if (opt == 'A'):
            return 1
        return 0

    def __init__(self):
        pmapi.pmOptions.__init__(self,"a:s:t:uAP:I:V?")
        self.pmSetOptionCallback(self.extraOptions)
        self.pmSetOverrideCallback(self.override)
        self.pmSetLongOptionHeader("General options")
        self.pmSetLongOptionArchive()
        self.pmSetLongOptionSamples()
        self.pmSetLongOptionInterval()
        self.pmSetLongOptionVersion()
        self.pmSetLongOptionHelp()
        self.pmSetLongOption("",0,"u","","Similar to no options")
        self.pmSetLongOption("",0,"A","","Similar to -P ALL -I ALL")
        self.pmSetLongOption("",1,"P","[1,3..|ON|ALL]","Filter or Show All/Online CPUs")
        self.pmSetLongOption("",1,"I","[SUM|CPU|SCPU|ALL]","Report Interrupt statistics")

class DisplayOptions:
    def __init__(self, mpstatoptions):
        self.mpstatoptions = mpstatoptions

    def display_cpu_usage_summary(self):
        return self.mpstatoptions.no_options or (self.mpstatoptions.cpu_filter and not self.mpstatoptions.interrupts_filter)

    def display_total_cpu_usage(self):
        return self.mpstatoptions.interrupts_filter and (self.mpstatoptions.interrupt_type == "SUM" or self.mpstatoptions.interrupt_type == "ALL")

    def display_hard_interrupt_usage(self):
        return self.mpstatoptions.interrupts_filter and (self.mpstatoptions.interrupt_type == "CPU" or self.mpstatoptions.interrupt_type == "ALL")

    def display_soft_interrupt_usage(self):
        return self.mpstatoptions.interrupts_filter and (self.mpstatoptions.interrupt_type == "SCPU" or self.mpstatoptions.interrupt_type == "ALL")

class MpstatReport(pmcc.MetricGroupPrinter):
    Machine_info_count = 0

    def __init__(self, cpu_util_reporter, total_interrupt_usage_reporter, soft_interrupt_usage_reporter, hard_interrupt_usage_reporter):
        self.cpu_util_reporter = cpu_util_reporter
        self.total_interrupt_usage_reporter = total_interrupt_usage_reporter
        self.soft_interrupt_usage_reporter = soft_interrupt_usage_reporter
        self.hard_interrupt_usage_reporter = hard_interrupt_usage_reporter

    def timeStampDelta(self, group):
        s = group.timestamp.tv_sec - group.prevTimestamp.tv_sec
        u = group.timestamp.tv_usec - group.prevTimestamp.tv_usec
        return (s + u / 1000000.0)

    def print_machine_info(self,group):
        machine_name = group['pmda.uname'].netValues[0][2]
        no_cpu =self.get_ncpu(group)
        print("%s\t(%s CPU)" % (machine_name,no_cpu))

    def get_ncpu(self,group):
        return group['hinv.ncpu'].netValues[0][2]

    def report(self,manager):
        group = manager['mpstat']
        if group['kernel.all.cpu.user'].netPrevValues == None:
            # need two fetches to report rate converted counter metrics
            return

        if self.Machine_info_count == 0:
            self.print_machine_info(group)
            self.Machine_info_count = 1

        timestamp = group.contextCache.pmCtime(int(group.timestamp)).rstrip().split()
        interval_in_seconds = self.timeStampDelta(group)
        metric_repository = MetricRepository(group)
        display_options = DisplayOptions(MpstatOptions)

        if display_options.display_cpu_usage_summary():
            cpu_util = CpuUtil(interval_in_seconds, metric_repository)
            self.cpu_util_reporter.print_report(cpu_util, timestamp[3])
        if display_options.display_total_cpu_usage():
            total_interrupt_usage = TotalInterruptUsage(interval_in_seconds, metric_repository)
            self.total_interrupt_usage_reporter.print_report(total_interrupt_usage, timestamp[3])
        if display_options.display_hard_interrupt_usage():
            hard_interrupt_usage = HardInterruptUsage(interval_in_seconds, metric_repository, interrupts_list)
            self.hard_interrupt_usage_reporter.print_report(hard_interrupt_usage,timestamp[3])
        if display_options.display_soft_interrupt_usage():
            soft_interrupt_usage = SoftInterruptUsage(interval_in_seconds, metric_repository, soft_interrupts_list)
            self.soft_interrupt_usage_reporter.print_report(soft_interrupt_usage, timestamp[3])




if __name__ == '__main__':

    stdout = StdoutPrinter()
    none_handler_printer = NoneHandlingPrinterDecorator(stdout.Print)
    cpu_filter = CpuFilter(MpstatOptions)
    cpu_util_reporter = CpuUtilReporter(cpu_filter, none_handler_printer.Print, MpstatOptions)
    total_interrupt_usage_reporter = TotalInterruptUsageReporter(cpu_filter, none_handler_printer.Print, MpstatOptions)
    soft_interrupt_usage_reporter = InterruptUsageReporter(cpu_filter, none_handler_printer.Print, MpstatOptions)
    hard_interrupt_usage_reporter = InterruptUsageReporter(cpu_filter, none_handler_printer.Print, MpstatOptions)

    try:
        opts = MpstatOptions()
        manager = pmcc.MetricGroupManager.builder(opts,sys.argv)
        interrupts_list = NamedInterrupts(manager, 'kernel.percpu.interrupts').get_all_named_interrupt_metrics()
        soft_interrupts_list = NamedInterrupts(manager, 'kernel.percpu.softirqs').get_all_named_interrupt_metrics()
        MPSTAT_METRICS += interrupts_list
        MPSTAT_METRICS += soft_interrupts_list
        manager['mpstat'] = MPSTAT_METRICS
        manager.printer = MpstatReport(cpu_util_reporter, total_interrupt_usage_reporter, soft_interrupt_usage_reporter, hard_interrupt_usage_reporter)
        sts = manager.run()
        sys.exit(sts)
    except pmapi.pmErr as pmerror:
        sys.stderr.write('%s: %s\n' % (pmerror.progname,pmerror.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except KeyboardInterrupt:
        pass
