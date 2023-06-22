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

import sys
import time
from pcp import pmapi
from pcp import pmcc
from cpmapi import PM_CONTEXT_ARCHIVE

METRICS = ["mem.physmem",
            "mem.util.free",
            "mem.util.available",
            "mem.util.bufmem",
            "mem.util.cached"
            "mem.util.swapCached",
            "mem.util.active",
            "mem.util.inactive",
            "mem.util.active_anon",
            "mem.util.inactive_anon",
            "mem.util.active_file",
            "mem.util.inactive_file",
            "mem.util.unevictable",
            "mem.util.mlocked",
            "mem.util.swapTotal",
            "mem.util.swapFree",
            "mem.util.dirty",
            "mem.util.writeback",
            "mem.util.anonpages",
            "mem.util.mapped",
            "mem.util.shared",
            "mem.util.slab",
            "mem.util.slabReclaimable",
            "mem.util.slabUnreclaimable",
            "mem.util.kernelStack",
            "mem.util.pageTables",
            "mem.util.NFS_Unstable",
            "mem.util.bounce",
            "mem.vmstat.nr_writeback_temp",
            "mem.util.commitLimit",
            "mem.util.committed_AS",
            "mem.util.vmallocTotal",
            "mem.util.vmallocUsed",
            "mem.util.vmallocChunk",
            "mem.util.corrupthardware",
            "mem.util.anonhugepages",
            "mem.vmstat.nr_shmem_hugepages",
            "mem.vmstat.nr_shmem_pmdmapped",
            "mem.zoneinfo.nr_free_cma",
            "mem.util.hugepagesTotal",
            "mem.util.hugepagesFree",
            "mem.util.hugepagesRsvd",
            "mem.util.hugepagesSurp",
            "hinv.hugepagesize",
            "mem.util.directMap4k",
            "mem.util.directMap2M",
            "mem.util.directMap1G"]

METRICS_DESC = ["MemTotal",
                "MemFree",
                "MemAvailable",
                "Buffers",
                "Cached",
                "SwapCached",
                "Active",
                "Inactive",
                "Active(anon)",
                "Inactive(anon)",
                "Active(file)",
                "Inactive(file)",
                "Unevictable",
                "Mlocked",
                "SwapTotal",
                "SwapFree",
                "Dirty",
                "Writeback",
                "AnonPages",
                "Mapped",
                "Shmem",
                "Slab",
                "SReclaimable",
                "SUnreclaim",
                "KernelStack",
                "PageTables",
                "NFS_Unstable",
                "Bounce",
                "WritebackTmp",
                "CommitLimit",
                "Committed_AS",
                "VmallocTotal",
                "VmallocUsed",
                "VmallocChunk",
                "HardwareCorrupted",
                "AnonHugePages",
                "ShmemHugePages",
                "ShmemPmdMapped",
                "CmaFree",
                "HugePages_Total_NO_kb",
                "HugePages_Free_NO_kb",
                "HugePages_Rsvd_NO_kb",
                "HugePages_Surp_NO_kb",
                "Hugepagesize",
                "DirectMap4k",
                "DirectMap2M",
                "DirectMap1G"]

class MeminfoReport(pmcc.MetricGroupPrinter):

    def getMetricName(self, idx):
        metric_name = ""
        units = ""
        if METRICS_DESC[idx].__contains__("_NO_kb"):
            metric_name = METRICS_DESC[idx][:-6]
        else:
            metric_name = METRICS_DESC[idx]
            units = "kB"
        return metric_name, units

    def report(self, manager):
        group = manager["meminfo"]
        t_s = group.contextCache.pmLocaltime(int(group.timestamp))
        time_string = time.strftime(MeminfoOptions.timefmt, t_s.struct_time())
        print(time_string)

        if MeminfoOptions.samples == 0 and MeminfoOptions.context is PM_CONTEXT_ARCHIVE:
            MeminfoOptions.samples = sys.maxsize

        idx = 0
        for metric in METRICS:
            try:
                val = group[metric].netValues[0][2]
            except IndexError:
                metric_name, units = self.getMetricName(idx)
                print(F"{metric_name:17} : NA")

                idx += 1
                continue

            metric_name, units = self.getMetricName(idx)
            print(F"{metric_name:17} : {val} {units}")

            idx += 1
        print()

        MeminfoOptions.samples -= 1

        if MeminfoOptions.samples <= 0:
            sys.exit(0)

class MeminfoOptions(pmapi.pmOptions):
    context = None
    timefmt = "%H:%M:%S"
    samples = 0

    def __init__(self):
        pmapi.pmOptions.__init__(self, "a:s:S:T:z:P:A:R:t:T:x:")
        self.pmSetLongOption("archive", 1, "a", "FILENAME","Fetch /proc/meminfo for a specified archive file")
        self.pmSetLongOption("samples", 1, "s", "count","Get the meminfo for specified number of samples count")
        self.pmSetLongOption("start_time", 1, "S", "TIME","Filter the samples from the archive from the given time")
        self.pmSetLongOption("end_time", 1, "T", "TIME","Filter the samples from the archive till the given time")
        self.pmSetOptionCallback(self.options)
        self.pmSetOverrideCallback(self.override)
        self.pmSetLongOptionHelp()

    def override(self, opt):
        if opt == 's':
            return 1
        return 0

    def options(self,opt,optarg,index):
        if opt == 's':
            MeminfoOptions.samples = int(optarg)

if __name__ == '__main__':
    try:
        opts = MeminfoOptions()
        mngr = pmcc.MetricGroupManager.builder(opts,sys.argv)
        MeminfoOptions.context = mngr.type

        missing = mngr.checkMissingMetrics(METRICS)
        if missing is not None:
            sys.stderr.write(F"Error:Metric is {missing} missing\n")
            sys.exit(1)

        mngr["meminfo"] = METRICS
        mngr.printer = MeminfoReport()
        sts = mngr.run()
        sys.exit(sts)

    except pmapi.pmErr as error:
        sys.stderr.write(F"{error.progname()} {error.message()}")
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except KeyboardInterrupt:
        pass
