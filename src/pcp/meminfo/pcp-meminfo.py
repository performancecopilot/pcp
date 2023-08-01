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
from pcp import pmapi, pmcc

METRICS = ["mem.physmem",
            "mem.util.free",
            "mem.util.available",
            "mem.util.bufmem",
            "mem.util.cached",
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
    samples = 0

    def __init__(self, samples):
        self.samples = samples

    def getMetricName(self, idx):
        metric_name = ""
        units = ""
        if METRICS_DESC[idx][-6:] == "_NO_kb":
            metric_name = METRICS_DESC[idx][:-6]
        else:
            metric_name = METRICS_DESC[idx]
            units = "kB"
        return metric_name, units

    def report(self, manager):
        group = manager["meminfo"]

        opts.pmGetOptionSamples()

        t_s = group.contextCache.pmLocaltime(int(group.timestamp))
        time_string = time.strftime(MeminfoOptions.timefmt, t_s.struct_time())
        print(time_string)

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

class MeminfoOptions(pmapi.pmOptions):
    context = None
    timefmt = "%H:%M:%S"
    samples = 0

    def __init__(self):
        pmapi.pmOptions.__init__(self, "a:s:S:T:z:A:t:")
        self.pmSetLongOptionStart()
        self.pmSetLongOptionFinish()
        self.pmSetLongOptionHelp()

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
        mngr.printer = MeminfoReport(opts.samples)
        sts = mngr.run()
        sys.exit(sts)

    except pmapi.pmErr as error:
        sys.stderr.write(F"{error.progname()} {error.message()}")
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except KeyboardInterrupt:
        pass
