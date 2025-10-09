#!/usr/bin/env pmpython
#
# Copyright (C) 2014-2018 Red Hat.
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
# pylint: disable=bad-continuation,consider-using-enumerate
#
""" Display NUMA memory allocation statistucs """

import os
import signal
import sys
import time

from pcp import pmapi
from pcp import pmcc
from cpmapi import PM_CONTEXT_ARCHIVE

if sys.version >= '3':
    long = int  # python2 to python3 portability (no long() in python3)
    xrange = range  # more back-compat (xrange() is range() in python3)

NUMA_METRICS = [
    "mem.numa.alloc.hit",
    "mem.numa.alloc.miss",
    "mem.numa.alloc.foreign",
    "mem.numa.alloc.interleave_hit",
    "mem.numa.alloc.local_node",
    "mem.numa.alloc.other_node",
]

MEM_METRICS = [
    "mem.numa.util.total",
    "mem.numa.util.free",
    "mem.numa.util.used",
    "mem.numa.util.active",
    "mem.numa.util.inactive",
    "mem.numa.util.active_anon",
    "mem.numa.util.inactive_anon",
    "mem.numa.util.active_file",
    "mem.numa.util.inactive_file",
    "mem.numa.util.unevictable",
    "mem.numa.util.mlocked",
    "mem.numa.util.dirty",
    "mem.numa.util.writeback",
    "mem.numa.util.filePages",
    "mem.numa.util.mapped",
    "mem.numa.util.anonpages",
    "mem.numa.util.shmem",
    "mem.numa.util.kernelStack",
    "mem.numa.util.pageTables",
    "mem.numa.util.NFS_Unstable",
    "mem.numa.util.bounce",
    "mem.numa.util.writebackTmp",
    "mem.numa.util.filehugepages",
    "mem.numa.util.filepmdmapped",
    "mem.numa.util.slab",
    "mem.numa.util.slabReclaimable",
    "mem.numa.util.slabUnreclaimable",
    "mem.numa.util.anonhugepages",
    "mem.numa.util.shmemhugepages",
    "mem.numa.util.shmempmdmapped",
    "mem.numa.util.hugepagesTotal",
    "mem.numa.util.hugepagesFree",
    "mem.numa.util.hugepagesSurp",
    "mem.numa.util.swapCached",
    "mem.numa.util.kreclaimable",
]

SYS_METRICS = [
    'kernel.uname.nodename',
    'kernel.uname.release',
    'kernel.uname.sysname',
    'kernel.uname.machine',
    'hinv.ncpu',
]

ALL_METRICS = NUMA_METRICS + MEM_METRICS

def prefix(metric):
    last_part = metric.split('.')[-1]
    result = last_part[0].upper() + last_part[1:]
    return result

class MetricRepository:
    def __init__(self, group):
        self.group = group
        self.current_cached_values = {}
        self.previous_cached_values = {}

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
    def current_values(self, metric_name):
        if self.group.get(metric_name, None) is None:
            return None
        if self.current_cached_values.get(metric_name, None) is None:
            self.current_cached_values[
                metric_name
            ] = self._fetch_current_values(metric_name, True)
        return self.current_cached_values.get(metric_name, None)

class NUMAStat:

    def __init__(self, group):
        self.group = group
        self.repo = MetricRepository(group)

    def resize(self, width):
        """ Find a suitable display width limit """
        if width == 0:
            if not sys.stdout.isatty():
                width = 1000000000        # mimic numastat(1) here
            else:
                # popen() is SAFE, command is a literal string
                (_, width) = os.popen('stty size', 'r').read().split()
                width = int(width)
            width = int(os.getenv('NUMASTAT_WIDTH', str(width)))
        return max(width, 32)

    def __format_table(self, width, nodes, data):
        null_output = False
        if not nodes:
            null_output = True
            nodes = [(0, 'Node ')]

        if "numastat" in data:
            metrics = NUMA_METRICS
            title = "NUMA memory allocation statistics (pages)"
        else:
            metrics = MEM_METRICS
            title = "Per-node system memory usage (KB)"

        total_w = max(42, int(width))
        print(title[:total_w])

        width = self.resize(width)
        maxnodes = int((width - 16) / 16)
        if maxnodes > len(nodes):        # just an initial header suffices
            header = '%30s' % ''
            for _, node in nodes:
                header += '%-12s' % node
            print(header)

        for m in metrics:
            if not null_output:
                vals = self.repo.current_values(m)
            done = 0  # reset for each metric

            # Loop through nodes in chunks of 'maxnodes'
            while done < len(nodes):
                header = '%-30s' % ''
                window = '%-20s : ' % prefix(m)

                # Slice the range we'll print in this batch
                chunk = nodes[done:done + maxnodes]

                for i, ( _, name) in enumerate(chunk):
                    header += '%-12s' % name
                    if not null_output:
                        window += '%12s' % vals[done + i]
                    else:
                        window += '%12s' % "NA"

                # Print header once per row group (not every metric)
                if done > maxnodes or maxnodes <= len(nodes):
                    print('%s\n%s' % (header, window))
                else:
                    print('%s' % window)
                done += maxnodes
        print()

    def print_mem(self, width, nodes, data):
        self.__format_table(width, nodes, data)

    def print_numa(self, width, nodes, data):
        self.__format_table(width, nodes, data)

class NumaStatOption(pmapi.pmOptions):
    context = None
    timefmt = "%m/%d/%Y %H:%M:%S"
    width = 0
    mem_out = False
    numa_out = False

    def override(self,opt):
        """ Override standard PCP options to match numastat(1) """
        if opt == 'n':
            return True
        return False

    def __init__(self):
        pmapi.pmOptions.__init__(self)
        self.pmSetShortOptions("w:mV?:n")
        self.pmSetOptionCallback(self.extraOptions)
        self.pmSetOverrideCallback(self.override)
        self.pmSetLongOptionHeader("Numastat options")
        self.pmSetLongOption("width", 1, 'w', "n", "limit the display width")
        # Map long options to our non-conflicting short letters
        self.pmSetLongOption("meminfo", 0, 'm', "", "show meminfo-like system-wide memory usage")
        self.pmSetLongOption("numastat", 0, 'n', "", "show the numastat statistics info")
        self.pmSetLongOptionVersion()
        self.pmSetLongOptionHelp()

    def extraOptions(self, opt, optarg, index):
        if opt == 'w':
            self.width = int(optarg)
        elif opt == "m":
            self.mem_out = True
        elif opt == "n":
            self.numa_out = True
        elif opt == "V":
            pass
        else:
            raise pmapi.pmUsageErr()
        return True

    def checkoptions(self):
        if (not self.mem_out) and (not self.numa_out) and (self.width == 0):
            self.numa_out = True
        if self.width < 0:
            return False
        return True

class NumaStatReport(pmcc.MetricGroupPrinter):
    machine_info_count = 0

    def __init__(self, options):
        self.options = options
        self.timestamp = None

    def __get_timestamp(self, group):
        ts = group.contextCache.pmLocaltime(int(group.timestamp))
        self.timestamp = time.strftime(NumaStatOption.timefmt, ts.struct_time())
        return self.timestamp

    def __get_ncpu(self, group):
        return group['hinv.ncpu'].netValues[0][2]

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
        print("%s  (%s CPU)" % (header_string, self.__get_ncpu(group)))

    def __discover_nodes(self, group, name):
        # Build list of online nodes (instance id, instance name)
        nodes = []
        try:
            for ent in group[name].netValues:
                inst_id = ent[0].inst
                inst_name = ent[1]           # usually "node0", "node1", ...
                online = int(ent[2]) != 0
                if online:
                    nodes.append((inst_id, inst_name))
        except Exception:
            pass
        # Sort by instance id (node number)
        nodes.sort(key=lambda t: t[0])
        return nodes

    def report(self, manager):
        # Print in a stable order
        group = manager["sys_info"]
        try:
            if not self.machine_info_count:
                self.print_machine_info(group, manager)
                self.machine_info_count = 1
        except IndexError:
            return

        output_numa = (
            self.options.numa_out
            or (not self.options.mem_out and not self.options.numa_out)
        )
        output_mem = self.options.mem_out
        group = manager["numastat"]
        nodes = self.__discover_nodes(group, "mem.numa.util.total")
        timestamp = self.__get_timestamp(group)
        print("%-20s : %s"%("Timestamp", timestamp))
        if output_mem:
            NUMAStat(group).print_mem(self.options.width, nodes, "meminfo")
        if output_numa:
            NUMAStat(group).print_numa(self.options.width, nodes, "numastat")

        if (
            NumaStatOption.context is not PM_CONTEXT_ARCHIVE
            and self.options.pmGetOptionSamples() is None
        ):
            sys.exit(0)

if __name__ == '__main__':
    try:
        opts = NumaStatOption()
        mngr = pmcc.MetricGroupManager.builder(opts, sys.argv)
        if not opts.checkoptions():
            print("Invalid options from command line")
            raise pmapi.pmUsageErr()
        NumaStatOption.context = mngr.type

        missing = mngr.checkMissingMetrics(ALL_METRICS)
        if missing is not None:
            sys.stderr.write('Error: not all required metrics are available\nMissing: %s\n' % (missing))
            sys.exit(1)

        mngr["numastat"] = ALL_METRICS
        mngr["sys_info"] = SYS_METRICS
        mngr.printer = NumaStatReport(opts)
        sts = mngr.run()
        sys.exit(sts)
    except IOError:
        signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    except pmapi.pmErr as error:
        sys.stderr.write("%s %s\n" % (error.progname(), error.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except KeyboardInterrupt:
        pass
