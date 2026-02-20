#!/usr/bin/pmpython
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
from typing import Dict
from typing import Union

from cpmapi import PM_CONTEXT_ARCHIVE
from cpmapi import PM_MODE_FORW
from pcp import pmapi
from pcp import pmcc

SYS_METRICS = [
    "kernel.uname.sysname",
    "kernel.uname.release",
    "kernel.uname.nodename",
    "kernel.uname.machine",
    "hinv.ncpu",
]
NFSIOSTAT_METRICS = [
    "nfsclient.mountpoint",
    "nfsclient.export",
    "nfsclient.age",
    "nfsclient.xprt.sends",
    "nfsclient.xprt.backlog_u",
    "nfsclient.ops.read.ops",
    "nfsclient.ops.read.errors",
    "nfsclient.ops.read.execute",
    "nfsclient.ops.read.rtt",
    "nfsclient.ops.read.queue",
    "nfsclient.ops.read.bytes_recv",
    "nfsclient.ops.read.bytes_sent",
    "nfsclient.ops.read.ntrans",
    "nfsclient.ops.write.ops",
    "nfsclient.ops.write.errors",
    "nfsclient.ops.write.execute",
    "nfsclient.ops.write.rtt",
    "nfsclient.ops.write.queue",
    "nfsclient.ops.write.bytes_recv",
    "nfsclient.ops.write.bytes_sent",
    "nfsclient.ops.write.ntrans",
]
ALL_METRICS = NFSIOSTAT_METRICS + SYS_METRICS


def adjust_length(name):
    return name.ljust(25)


class ReportingMetricRepository:
    def __init__(self, group):
        self.group = group
        self._current_cache = {}
        self._previous_cache = {}

    def _fetch_values(self, metric, use_previous=False):
        """Fetch values - always returns a dictionary."""
        if metric not in self.group:
            return {}
        attr = "netPrevValues" if use_previous else "netValues"
        values = getattr(self.group[metric], attr, [])
        return {x[0].inst: x[2] for x in values} if values else {}

    def _get_values_dict(self, metric, use_previous=False):
        """Get cached dictionary of all values for a metric."""
        cache = self._previous_cache if use_previous else self._current_cache
        if metric not in cache:
            cache[metric] = self._fetch_values(metric, use_previous)
        return cache[metric]

    def previous_value(self, metric, instance=None):
        """Get previous value. Returns single value if instance given, else returns dict."""
        values_dict = self._get_values_dict(metric, use_previous=True)
        if instance is not None:
            return values_dict.get(instance)
        return values_dict

    def current_value(self, metric, instance=None):
        """Get current value. Returns single value if instance given, else returns dict."""
        values_dict = self._get_values_dict(metric, use_previous=False)
        if instance is not None:
            return values_dict.get(instance)
        return values_dict


class NfsioStatUtil:
    def __init__(self, metrics_repository):
        self.__metric_repository = metrics_repository
        self.report = ReportingMetricRepository(self.__metric_repository)


class NfsiostatReport(pmcc.MetricGroupPrinter):
    machine_info_count = 0

    def __init__(self, opts, group):
        self.opts = opts
        self.group = group
        self.samples = opts.samples
        self.context = opts.context

    def __get_ncpu(self, group):
        return group["hinv.ncpu"].netValues[0][2]

    def __print_machine_info(self, context):
        timestamp = self.group.pmLocaltime(context.timestamp.tv_sec)
        # Please check strftime(3) for different formatting options.
        # Also check TZ and LC_TIME environment variables for more
        # information on how to override the default formatting of
        # the date display in the header
        time_string = time.strftime(
            "%m/%d/%Y %H:%M:%S", timestamp.struct_time()
        )
        header_string = ""
        header_string += context["kernel.uname.sysname"].netValues[0][2] + "  "
        header_string += context["kernel.uname.release"].netValues[0][2] + "  "
        header_string += (
            "(" + context["kernel.uname.nodename"].netValues[0][2] + ")  "
        )
        header_string += time_string + "  "
        header_string += context["kernel.uname.machine"].netValues[0][2] + "  "
        print("%s  (%s CPU)" % (header_string, self.__get_ncpu(context)))

    def __collect(self, nfs):
        return {
            metric: nfs.report.current_value(metric)
            for metric in NFSIOSTAT_METRICS
        }

    def __delta(
        self,
        new: Dict[str, Dict[str, Union[float, str]]],
        nfs,
    ) -> Dict[str, Dict[str, Union[float, str]]]:
        delta: Dict[str, Dict[str, Union[float, str]]] = {}
        old = {
            metric: nfs.report.previous_value(metric)
            for metric in NFSIOSTAT_METRICS
        }

        for metric in new:
            delta[metric] = {}

            for inst in new[metric]:
                new_val = new[metric][inst]
                old_val = old.get(metric, {}).get(inst, 0)

                # If value is numeric → subtract
                if isinstance(new_val, (int, float)):
                    delta[metric][inst] = new_val - old_val
                else:
                    # If string → just copy (no subtraction)
                    delta[metric][inst] = new_val

        return delta

    def __print_values(self, timestamp, delta):
        sampletime = delta["nfsclient.age"]
        readops = delta["nfsclient.ops.read.ops"]
        writeops = delta["nfsclient.ops.write.ops"]
        readbytesrecv = delta["nfsclient.ops.read.bytes_recv"]
        writebytesrecv = delta["nfsclient.ops.write.bytes_recv"]
        mountpoint = delta["nfsclient.mountpoint"]
        mountshare = delta["nfsclient.export"]
        sends = delta["nfsclient.xprt.sends"]
        backlog = delta["nfsclient.xprt.backlog_u"]
        readerrors = delta["nfsclient.ops.read.errors"]
        readexecute = delta["nfsclient.ops.read.execute"]
        readrtt = delta["nfsclient.ops.read.rtt"]
        readqueue = delta["nfsclient.ops.read.queue"]
        readbytessent = delta["nfsclient.ops.read.bytes_sent"]
        readntrans = delta["nfsclient.ops.read.ntrans"]
        writeerrors = delta["nfsclient.ops.write.errors"]
        writeexecute = delta["nfsclient.ops.write.execute"]
        writertt = delta["nfsclient.ops.write.rtt"]
        writequeue = delta["nfsclient.ops.write.queue"]
        writebytessent = delta["nfsclient.ops.write.bytes_sent"]
        writentrans = delta["nfsclient.ops.write.ntrans"]

        print("%-18s:%s" % ("Timestamp", timestamp))
        print()

        for name in mountshare:
            # read
            r_kilobytes = (readbytessent[name] + readbytesrecv[name]) / 1024
            if sampletime[name] > 0:
                ops_per_sample = sends[name] / sampletime[name]
                ops_per_sample_read = readops[name] / sampletime[name]
                r_kilobytes_per_sample = r_kilobytes / sampletime[name]
            else:
                ops_per_sample = 0.0
                ops_per_sample_read = 0.0
                r_kilobytes_per_sample = 0.0

            r_retrans = readntrans[name] - readops[name]
            if readops[name] > 0:
                r_kilobytes_per_op = r_kilobytes / readops[name]
                r_retrans_percent = (r_retrans * 100) / readops[name]
                r_rtt_per_op = readrtt[name] / readops[name]
                r_exe_per_op = readexecute[name] / readops[name]
                r_queued_for_per_op = readqueue[name] / readops[name]
                r_errs_percent = (readerrors[name] * 100) / readops[name]
            else:
                r_kilobytes_per_op = 0.0
                r_retrans_percent = 0.0
                r_rtt_per_op = 0.0
                r_exe_per_op = 0.0
                r_queued_for_per_op = 0.0
                r_errs_percent = 0.0

            # write
            w_kilobytes = (writebytessent[name] + writebytesrecv[name]) / 1024
            if sampletime[name] > 0:
                ops_per_sample_write = writeops[name] / sampletime[name]
                w_kilobytes_per_sample = w_kilobytes / sampletime[name]
            else:
                ops_per_sample_write = 0.0
                w_kilobytes_per_sample = 0.0

            w_retrans = writentrans[name] - writeops[name]
            if writeops[name] > 0:
                w_kilobytes_per_op = w_kilobytes / writeops[name]
                w_retrans_percent = (w_retrans * 100) / writeops[name]
                w_rtt_per_op = writertt[name] / writeops[name]
                w_exe_per_op = writeexecute[name] / writeops[name]
                w_queued_for_per_op = writequeue[name] / writeops[name]
                w_errs_percent = (writeerrors[name] * 100) / writeops[name]
            else:
                w_kilobytes_per_op = 0.0
                w_retrans_percent = 0.0
                w_rtt_per_op = 0.0
                w_exe_per_op = 0.0
                w_queued_for_per_op = 0.0
                w_errs_percent = 0.0

            print(f"{mountshare[name]} mounted on {mountpoint[name]}:")

            print(f"{'':14}ops/s{'':7}rpc bklog")
            print(f"{ops_per_sample:19.3f}{backlog[name]:16.3f}")
            print()
            print(
                "read:              "
                "ops/s        kB/s        kB/op     retrans   "
                "avg RTT (ms)   avg exe (ms)   avg queue (ms)        errors"
            )
            print(
                f"{'':19}"
                f"{ops_per_sample_read:5.3f}"
                f"{r_kilobytes_per_sample:12.3f}"
                f"{r_kilobytes_per_op:13.3f}   "
                f"{int(r_retrans):2d} ({r_retrans_percent:2.1f}%)"
                f"{r_rtt_per_op:15.3f}"
                f"{r_exe_per_op:15.3f}"
                f"{r_queued_for_per_op:17.3f}   "
                f"{int(readerrors[name]):4d} ({r_errs_percent:2.1f}%)"
            )

            print(
                "write:             "
                "ops/s        kB/s        kB/op     retrans   "
                "avg RTT (ms)   avg exe (ms)   avg queue (ms)        errors"
            )

            print(
                f"{'':19}"
                f"{ops_per_sample_write:5.3f}"
                f"{w_kilobytes_per_sample:12.3f}"
                f"{w_kilobytes_per_op:13.3f}   "
                f"{int(w_retrans):2d} ({w_retrans_percent:2.1f}%)"
                f"{w_rtt_per_op:15.3f}"
                f"{w_exe_per_op:15.3f}"
                f"{w_queued_for_per_op:17.3f}   "
                f"{int(writeerrors[name]):4d} ({w_errs_percent:2.1f}%)"
            )
            print()

    def get_timestamp(self, group):
        t_s = group.contextCache.pmLocaltime(int(group.timestamp))
        timestamp = time.strftime(NfsiostatOptions.timefmt, t_s.struct_time())
        return timestamp

    def print_report(self, manager_nfsiostat, mgr):
        def __print_nfs_status():
            timestamp = self.get_timestamp(manager_nfsiostat)
            nfs = NfsioStatUtil(manager_nfsiostat)
            if nfs.report.current_value("nfsclient.export"):
                try:
                    if self.machine_info_count == 0 and not self.opts.uflag:
                        self.__print_machine_info(manager_nfsiostat)
                        self.machine_info_count = 1
                    current = self.__collect(nfs)
                    delta = self.__delta(current, nfs)
                    self.__print_values(timestamp, delta)

                except IndexError:
                    print("Incorrect machine info due to some missing metrics")
                return
            else:
                pass

        if self.context != PM_CONTEXT_ARCHIVE and self.samples is None:
            __print_nfs_status()
        elif self.context == PM_CONTEXT_ARCHIVE and self.samples is None:
            __print_nfs_status()
        elif self.samples >= 1:
            __print_nfs_status()
            self.samples -= 1
        else:
            pass

    def report(self, manager):
        self.samples = self.opts.pmGetOptionSamples()
        self.print_report(manager["nfsiostat"], manager)


class NfsiostatOptions(pmapi.pmOptions):
    timefmt = "%m/%d/%Y %H:%M:%S"
    uflag = False

    def checkOptions(self, manager):
        if NfsiostatOptions.uflag:
            if manager._options.pmGetOptionInterval():  # pylint: disable=protected-access
                print("Error: -t incompatible with -u")
                return False
            if manager.type != PM_CONTEXT_ARCHIVE:
                print("Error: -u can only be specified with -a archive")
                return False
        return True

    def extraOptions(self, opt, optarg, index):
        if opt == "u":
            NfsiostatOptions.uflag = True

    def __init__(self):
        pmapi.pmOptions.__init__(self, "a:s:Z:t:uzV?")
        self.pmSetOptionCallback(self.extraOptions)
        self.pmSetLongOptionHeader("General options")
        self.pmSetLongOptionHostZone()
        self.pmSetLongOptionTimeZone()
        self.pmSetLongOptionArchive()
        self.pmSetLongOptionSamples()
        self.pmSetLongOptionInterval()
        self.pmSetLongOption(
            "no-interpolation",
            0,
            "u",
            "",
            "disable interpolation mode with archives",
        )
        self.pmSetLongOptionHelp()
        self.pmSetLongOptionVersion()
        self.context = None
        self.samples = None


if __name__ == "__main__":
    try:
        opts = NfsiostatOptions()
        mngr = pmcc.MetricGroupManager.builder(opts, sys.argv)
        opts.context = mngr.type
        if not opts.checkOptions(mngr):
            raise pmapi.pmUsageErr

        if NfsiostatOptions.uflag:
            # -u turns off interpolation
            mngr.pmSetMode(
                PM_MODE_FORW, mngr._options.pmGetOptionOrigin(), None   # pylint: disable=protected-access
            )

        missing = mngr.checkMissingMetrics(ALL_METRICS)
        if missing is not None:
            sys.stderr.write(
                "Error: not all required metrics are available\nMissing %s\n"
                % missing
            )
            sys.exit(1)
        mngr["nfsiostat"] = ALL_METRICS
        mngr.printer = NfsiostatReport(opts, mngr)
        sts = mngr.run()
        sys.exit(sts)
    except pmapi.pmErr as error:
        sys.stderr.write("%s\n" % (error.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except IOError:
        signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    except KeyboardInterrupt:
        pass
