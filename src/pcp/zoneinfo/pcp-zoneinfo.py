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
from cpmapi import PM_CONTEXT_HOST

# By default we are assuming we are running on the latest pcp version
# Later will figure out the version and will change the newVersionFlag

newVersionFlag = True

SYS_METRICS = ["kernel.uname.sysname", "kernel.uname.release",
               "kernel.uname.nodename", "kernel.uname.machine", "hinv.ncpu"]

ZONESTAT_METRICS = ["mem.zoneinfo.managed", "mem.zoneinfo.nr_mlock", "mem.zoneinfo.present",
                    "mem.zoneinfo.scanned", "mem.zoneinfo.nr_unstable", "mem.zoneinfo.nr_page_table_pages",
                    "mem.zoneinfo.nr_shmem", "mem.zoneinfo.nr_free_pages", "mem.zoneinfo.nr_active_file",
                    "mem.zoneinfo.nr_dirty", "mem.zoneinfo.nr_writeback", "mem.zoneinfo.free",
                    "mem.zoneinfo.nr_unevictable", "mem.zoneinfo.nr_alloc_batch", "mem.zoneinfo.low",
                    "mem.zoneinfo.nr_slab_reclaimable", "mem.zoneinfo.nr_kernel_stack", "mem.zoneinfo.numa_interleave",
                    "mem.zoneinfo.workingset_nodereclaim", "mem.zoneinfo.nr_isolated_anon", "mem.zoneinfo.nr_bounce",
                    "mem.zoneinfo.numa_other", "mem.zoneinfo.nr_file_pages", "mem.zoneinfo.numa_hit",
                    "mem.zoneinfo.nr_isolated_file", "mem.zoneinfo.nr_anon_transparent_hugepages",
                    "mem.zoneinfo.nr_inactive_file", "mem.zoneinfo.spanned", "mem.zoneinfo.nr_written",
                    "mem.zoneinfo.numa_foreign", "mem.zoneinfo.nr_vmscan_write", "mem.zoneinfo.nr_free_cma",
                    "mem.zoneinfo.nr_writeback_temp", "mem.zoneinfo.nr_slab_unreclaimable",
                    "mem.zoneinfo.nr_vmscan_immediate_reclaim", "mem.zoneinfo.nr_mapped", "mem.zoneinfo.nr_anon_pages",
                    "mem.zoneinfo.high", "mem.zoneinfo.protection", "mem.zoneinfo.numa_local", "mem.zoneinfo.numa_miss",
                    "mem.zoneinfo.nr_active_anon", "mem.zoneinfo.workingset_refault", "mem.zoneinfo.nr_dirtied",
                    "mem.zoneinfo.workingset_activate", "mem.zoneinfo.nr_inactive_anon", "mem.zoneinfo.min"]

ZONESTAT_NEW_METRICS = ["mem.zoneinfo.nr_foll_pin_acquired", "mem.zoneinfo.nr_foll_pin_released",
                        "mem.zoneinfo.nr_zspages", "mem.zoneinfo.workingset_restore_anon",
                        "mem.zoneinfo.workingset_refault_anon", "mem.zoneinfo.nr_zone_active_anon",
                        "mem.zoneinfo.nr_zone_inactive_file", "mem.zoneinfo.nr_swapcached",
                        "mem.zoneinfo.nr_file_pmdmapped", "mem.zoneinfo.workingset_active_file",
                        "mem.zoneinfo.workingset_refault_file", "mem.zoneinfo.cma",
                        "mem.zoneinfo.workingset_active_anon", "mem.zoneinfo.nr_zone_write_pending",
                        "mem.zoneinfo.nr_shmem_pmdmapped", "mem.zoneinfo.nr_shmem_hugepages",
                        "mem.zoneinfo.nr_kernel_misc_reclaimable", "mem.zoneinfo.nr_zone_active_file",
                        "mem.zoneinfo.nr_file_hugepages", "mem.zoneinfo.nr_zone_unevictable",
                        "mem.zoneinfo.nr_zone_inactive_anon", "mem.zoneinfo.workingset_restore_file"]

ZONEINFO_PER_NODE = {
    "nr_inactive_anon"              :   "mem.zoneinfo.nr_inactive_anon",
    "nr_active_anon"                :   "mem.zoneinfo.nr_active_anon",
    "nr_inactive_file"              :   "mem.zoneinfo.nr_inactive_file",
    "nr_active_file"                :   "mem.zoneinfo.nr_active_file",
    "nr_unevictable"                :   "mem.zoneinfo.nr_unevictable",
    "nr_slab_reclaimable"           :   "mem.zoneinfo.nr_slab_reclaimable",
    "nr_slab_unreclaimable"         :   "mem.zoneinfo.nr_slab_unreclaimable",
    "nr_isolated_anon"              :   "mem.zoneinfo.nr_isolated_anon",
    "nr_isolated_file"              :   "mem.zoneinfo.nr_isolated_file",
    "nr_anon_pages"                 :   "mem.zoneinfo.nr_anon_pages",
    "nr_mapped"                     :   "mem.zoneinfo.nr_mapped",
    "nr_file_pages"                 :   "mem.zoneinfo.nr_file_pages",
    "nr_dirty"                      :   "mem.zoneinfo.nr_dirty",
    "nr_writeback"                  :   "mem.zoneinfo.nr_writeback",
    "nr_writeback_temp"             :   "mem.zoneinfo.nr_writeback_temp",
    "nr_shmem"                      :   "mem.zoneinfo.nr_shmem",
    "nr_anon_transparent_hugepages" :   "mem.zoneinfo.nr_anon_transparent_hugepages",
    "nr_unstable"                   :   "mem.zoneinfo.nr_unstable",
    "nr_vmscan_write"               :   "mem.zoneinfo.nr_vmscan_write",
    "nr_vmscan_immediate_reclaim"   :   "mem.zoneinfo.nr_vmscan_immediate_reclaim",
    "nr_dirtied"                    :   "mem.zoneinfo.nr_dirtied",
    "nr_written"                    :   "mem.zoneinfo.nr_written",
    "nr_shmem_hugepages"            :   "mem.zoneinfo.nr_shmem_hugepages",
    "nr_shmem_pmdmapped"            :   "mem.zoneinfo.nr_shmem_pmdmapped",
    "nr_file_hugepages"             :   "mem.zoneinfo.nr_file_hugepages",
    "nr_file_pmdmapped"             :   "mem.zoneinfo.nr_file_pmdmapped",
    "nr_kernel_misc_reclaimable"    :   "mem.zoneinfo.nr_kernel_misc_reclaimable"
}

ZONEINFO_PAGE_INFO = [
        ("pages free"   ,   "mem.zoneinfo.free"),
        ("      min"    ,   "mem.zoneinfo.min"),
        ("      low"    ,   "mem.zoneinfo.low"),
        ("      high"   ,   "mem.zoneinfo.high"),
        ("      spanned",   "mem.zoneinfo.spanned"),
        ("      present",   "mem.zoneinfo.present"),
        ("      managed",   "mem.zoneinfo.managed")
]

ZONEINFO_NUMBER_ZONE    =   {
    "nr_free_pages"         :   "mem.zoneinfo.nr_free_pages",
    "nr_mlock"              :   "mem.zoneinfo.nr_mlock",
    "nr_page_table_pages"   :   "mem.zoneinfo.nr_page_table_pages",
    "nr_kernel_stack"       :   "mem.zoneinfo.nr_kernel_stack",
    "nr_bounce"             :   "mem.zoneinfo.nr_bounce",
    "nr_free_cma"           :   "mem.zoneinfo.nr_free_cma",
    "numa_hit"              :   "mem.zoneinfo.numa_hit",
    "numa_miss"             :   "mem.zoneinfo.numa_miss",
    "numa_foreign"          :   "mem.zoneinfo.numa_foreign",
    "numa_interleave"       :   "mem.zoneinfo.numa_interleave",
    "numa_other"            :   "mem.zoneinfo.numa_other",
    "nr_zone_active_anon"   :   "mem.zoneinfo.nr_zone_active_anon",
    "nr_zone_inactive_file" :    "mem.zoneinfo.nr_zone_inactive_file",
    "nr_zone_active_file"   :   "mem.zoneinfo.nr_zone_active_file",
    "nr_zone_unevictable"   :   "mem.zoneinfo.nr_zone_unevictable",
    "nr_zone_write_pending" :   "mem.zoneinfo.nr_zone_write_pending",
    "nr_zspages"            :   "mem.zoneinfo.nr_zspages",
    "numa_local"            :   "mem.zoneinfo.numa_local",
    "nr_zone_inactive_anon" :   "mem.zoneinfo.nr_zone_inactive_anon"
}


class ReportingMetricRepository:

    def __init__(self, group):
        self.group = group
        self.current_cached_values = {}

    def __sorted(self, data):
        return dict(sorted(data.items(), key=lambda item: item[0].lower()))

    def __fetch_current_value(self, metric):
        val = dict(map(lambda x: (x[1], x[2]), self.group[metric].netValues))
        val = self.__sorted(val)
        return dict(val)

    def current_value(self, metric):
        if metric not in self.group:
            return None
        if self.current_cached_values.get(metric) is None:
            first_value = self.__fetch_current_value(metric)
            self.current_cached_values[metric] = first_value
        return self.current_cached_values[metric]


class ZoneStatUtil:
    def __init__(self, metrics_repository, all_metrics_repository):
        self.__metric_repository = metrics_repository
        self.__all_metrics_repository = all_metrics_repository
        self.report = ReportingMetricRepository(self.__metric_repository)

    def names(self):
        data = self.report.current_value('mem.zoneinfo.present')
        return sorted(data.keys())

    def metric_value(self, metric, node):
        if metric in self.__all_metrics_repository:
            data = self.report.current_value(metric)
            data = data.get(node, "0")
            if data != "0":
                data = data/4
            return int(data)
        return None

    # creating the list of protection values for particular node
    def protection(self, node):
        data = self.report.current_value("mem.zoneinfo.protection")
        return [value // 4 for key, value in data.items() if key.startswith(node)]

    def protection_names(self, node_name):
        data = self.report.current_value("mem.zoneinfo.protection")
        return {
            key.split("::" + node_name)[0] + "::" + node_name
            for key in data.keys()
            if node_name in key
        } or None


class ZoneinfoReport(pmcc.MetricGroupPrinter):
    def __init__(self, opts, group):
        self.opts = opts
        self.group = group
        self.samples = opts.samples
        self.context = opts.context

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

    # Function to format the node name as per the /proc/zoneinfo naming convention
    def __format_node_name(self, input_str):
        parts = input_str.split("::")
        if len(parts) != 2 or not parts[1].startswith("node"):
            return "Invalid input format"
        node_num = parts[1][4:]
        return "Node {}, zone    {}".format(node_num, parts[0])

    def __print_old_version_values(self, manager):
        try:
            total_nodes = manager.names()
            total_nodes = sorted(total_nodes, key=lambda
                                 x: ((x.split('::')[0] != 'DMA')*2,  # Move 'DMA' entries to the front
                                     x.split('::')[0]))
            for node in total_nodes:
                print(self.__format_node_name(str(node)))
                print("\tper-node status")
                # print(ZONEINFO_PER_NODE)
                for key, value in ZONEINFO_PER_NODE.items():
                    print("\t{} {}".format(key, manager.metric_value(value, node)))
                for key, value in ZONEINFO_PAGE_INFO:
                    print("\t{} {}".format(key, manager.metric_value(value, node)))
                print(" " * 14 + "protection " + str([int(i) for i in sorted(manager.protection(node))]))
                for key, value in ZONEINFO_NUMBER_ZONE.items():
                    print("\t{} {}".format(key, manager.metric_value(value, node)))
        except IndexError:
            print("Got some error while printing values for old version zoneinfo")

    def __print_values(self, manager):
        total_nodes = manager.names()
        node_names = {key.split('::')[1] for key in total_nodes}
        # sort the node names in decreasing order
        node_names = sorted(node_names, key=lambda x: int(x[4:]) if x.startswith('node') else float('inf'))
        try:
            # lopping through all the available nodes
            for node_name in node_names:
                # get all the types available for the current node on which i'm
                node_types = manager.protection_names(node_name)
                if node_types is None:
                    return
                nodes = set(node_types).intersection(total_nodes)
                nodes = sorted(nodes, key=lambda x: ((x.split('::')[0] != 'DMA')*2,  # Move 'DMA' entries to the front
                                                     x.split('::')[0]))
                print("NODE {:>2}, per-node status".format(node_name[4:]))
                for key, value in ZONEINFO_PER_NODE.items():
                    print("\t{} {}".format(key, manager.metric_value(value, node_name)))
                for node in nodes:
                    print(self.__format_node_name(node))
                    for key, value in ZONEINFO_PAGE_INFO:
                        print("\t{} {}".format(key, manager.metric_value(value, node)))
                    print(" " * 14 + "protection " + str([int(i) for i in manager.protection(node)]))
                    for key, value in ZONEINFO_NUMBER_ZONE.items():
                        print("\t{} {}".format(key, manager.metric_value(value, node)))
                # finding the remaining type of nodes data which i haven't printed so far
                # these nodes will have only pages information for them so just printing them out
                remaining_nodes = set(node_types) - set(nodes)
                if remaining_nodes:
                    for node in sorted(remaining_nodes):
                        print(self.__format_node_name(node))
                        for key, value in ZONEINFO_PAGE_INFO:
                            print("\t", key, manager.metric_value(value, node))
                        print(" " * 14 + "protection " + str([int(i) for i in manager.protection(node)]))
                else:
                    continue
        except IndexError:
            print("Got some error while printing values for zoneinfo")

    def print_report(self, group, timestamp, manager_zoneinfo, manager_all):
        def __print_zone_status():
            zonestatus = ZoneStatUtil(manager_zoneinfo, manager_all)
            if zonestatus.names():
                try:
                    self.__print_machine_info(group)
                    print("TimeStamp = {}".format(timestamp))
                    if newVersionFlag:
                        self.__print_values(zonestatus)
                    else:
                        self.__print_old_version_values(zonestatus)
                except IndexError:
                    print("Incorrect machine info due to some missing metrics")
                return
            else:
                return

        if self.context != PM_CONTEXT_ARCHIVE and self.samples is None:
            __print_zone_status()
            sys.exit(0)
        elif self.context == PM_CONTEXT_ARCHIVE and self.samples is None:
            __print_zone_status()
        elif self.samples >= 1:
            __print_zone_status()
            self.samples -= 1
        else:
            return

    def report(self, manager):
        group = manager["sysinfo"]
        self.samples = self.opts.pmGetOptionSamples()
        t_s = group.contextCache.pmLocaltime(int(group.timestamp))
        timestamp = time.strftime(ZoneinfoOptions.timefmt, t_s.struct_time())
        self.print_report(group, timestamp, manager['zoneinfo'], manager['allinfo'])


class ZoneinfoOptions(pmapi.pmOptions):
    timefmt = "%H:%M:%S"

    def __init__(self):
        pmapi.pmOptions.__init__(self, "a:s:Z:zV?")
        self.pmSetLongOptionHeader("General options")
        self.pmSetLongOptionHostZone()
        self.pmSetLongOptionTimeZone()
        self.pmSetLongOptionHelp()
        self.pmSetLongOptionSamples()
        self.pmSetLongOptionVersion()
        self.samples = None
        self.context = None


def updateZoneinfoDataToBeFetched():
    ZoneinfoNumberZoneElementsToBeRemoved = {
        "nr_zone_active_anon"           :   "mem.zoneinfo.nr_zone_active_anon",
        "nr_zone_inactive_file"         :    "mem.zoneinfo.nr_zone_inactive_file",
        "nr_zone_active_file"           :   "mem.zoneinfo.nr_zone_active_file",
        "nr_zone_unevictable"           :   "mem.zoneinfo.nr_zone_unevictable",
        "nr_zone_write_pending"         :   "mem.zoneinfo.nr_zone_write_pending",
        "nr_zspages"                    :   "mem.zoneinfo.nr_zspages",
        "numa_local"                    :   "mem.zoneinfo.numa_local",
        "nr_zone_inactive_anon"         :   "mem.zoneinfo.nr_zone_inactive_anon"
    }

    ZoneinfoPerNodeElementsToBeRemoved = {
        "nr_shmem_hugepages"            :   "mem.zoneinfo.nr_shmem_hugepages",
        "nr_shmem_pmdmapped"            :   "mem.zoneinfo.nr_shmem_pmdmapped",
        "nr_file_hugepages"             :   "mem.zoneinfo.nr_file_hugepages",
        "nr_file_pmdmapped"             :   "mem.zoneinfo.nr_file_pmdmapped",
        "nr_kernel_misc_reclaimable"    :   "mem.zoneinfo.nr_kernel_misc_reclaimable"
    }

    for key in ZoneinfoNumberZoneElementsToBeRemoved:
        del ZONEINFO_NUMBER_ZONE[key]

    for key in ZoneinfoPerNodeElementsToBeRemoved:
        del ZONEINFO_PER_NODE[key]


if __name__ == '__main__':
    try:
        opts = ZoneinfoOptions()
        mngr = pmcc.MetricGroupManager.builder(opts, sys.argv)
        opts.context = mngr.type
        ZONESTAT_METRICS = ZONESTAT_METRICS + ZONESTAT_NEW_METRICS

        if opts.context == PM_CONTEXT_HOST:
            context = pmapi.pmContext(opts.context)
        elif opts.context == PM_CONTEXT_ARCHIVE:
            context = pmapi.pmContext.fromOptions(opts,sys.argv)
        try:
            metric_ids = context.pmLookupName(ZONESTAT_METRICS)
            descs= context.pmLookupDescs(metric_ids)
        except pmapi.pmErr as error:
            newVersionFlag = False
            updateZoneinfoDataToBeFetched()
            ZONESTAT_METRICS = [x for x in ZONESTAT_METRICS if x not in ZONESTAT_NEW_METRICS]

        ALL_METRICS = ZONESTAT_METRICS + SYS_METRICS
        mngr["zoneinfo"] = ZONESTAT_METRICS
        mngr["sysinfo"] = SYS_METRICS
        mngr["allinfo"] = ALL_METRICS
        mngr.printer = ZoneinfoReport(opts, mngr)
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
