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
from pcp import pmapi, pmcc
from cpmapi import PM_CONTEXT_ARCHIVE

SYS_METRICS= ["kernel.uname.sysname","kernel.uname.release",
               "kernel.uname.nodename","kernel.uname.machine","hinv.ncpu"]
NFSIOSTAT_METRICS = ["nfsclient.mountpoint","nfsclient.export","nfsclient.age",
                     "nfsclient.xprt.sends","nfsclient.xprt.backlog_u","nfsclient.ops.read.ops",
                     "nfsclient.ops.read.errors","nfsclient.ops.read.execute","nfsclient.ops.read.rtt",
                     "nfsclient.ops.read.queue","nfsclient.ops.read.bytes_recv","nfsclient.ops.read.bytes_sent",
                     "nfsclient.ops.read.ntrans","nfsclient.ops.write.ops","nfsclient.ops.write.errors",
                     "nfsclient.ops.write.execute","nfsclient.ops.write.rtt","nfsclient.ops.write.queue",
                     "nfsclient.ops.write.bytes_recv","nfsclient.ops.write.bytes_sent","nfsclient.ops.write.ntrans"]
ALL_METRICS = NFSIOSTAT_METRICS + SYS_METRICS

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

class NfsioStatUtil:
    def __init__(self,metrics_repository):
        self.__metric_repository=metrics_repository
        self.report=ReportingMetricRepository(self.__metric_repository)

    def mount_point(self):
        return self.report.current_value('nfsclient.mountpoint')

    def mount_share(self):
        return self.report.current_value('nfsclient.export')

    def mount_share_keys(self):
        data = self.report.current_value('nfsclient.export')
        return data.keys()

    def sample_time(self):
        return self.report.current_value('nfsclient.age')

    def xprt_sends(self):
        return self.report.current_value('nfsclient.xprt.sends')

    def xprt_backlog(self):
        return self.report.current_value('nfsclient.xprt.backlog_u')

    def readops(self):
        return self.report.current_value('nfsclient.ops.read.ops')

    def readerrors(self):
        return self.report.current_value('nfsclient.ops.read.errors')

    def readexecute(self):
        return self.report.current_value('nfsclient.ops.read.execute')

    def readrtt(self):
        return self.report.current_value('nfsclient.ops.read.rtt')

    def readqueue(self):
        return self.report.current_value('nfsclient.ops.read.queue')

    def readbytesrecv(self):
        return self.report.current_value('nfsclient.ops.read.bytes_recv')

    def readbytessent(self):
        return self.report.current_value('nfsclient.ops.read.bytes_sent')

    def readntrans(self):
        return self.report.current_value('nfsclient.ops.read.ntrans')

    def writeops(self):
        return self.report.current_value('nfsclient.ops.write.ops')

    def writeerrors(self):
        return self.report.current_value('nfsclient.ops.write.errors')

    def writeexecute(self):
        return self.report.current_value('nfsclient.ops.write.execute')

    def writertt(self):
        return self.report.current_value('nfsclient.ops.write.rtt')

    def writequeue(self):
        return self.report.current_value('nfsclient.ops.write.queue')

    def writebytesrecv(self):
        return self.report.current_value('nfsclient.ops.write.bytes_recv')

    def writebytessent(self):
        return self.report.current_value('nfsclient.ops.write.bytes_sent')

    def writentrans(self):
        return self.report.current_value('nfsclient.ops.write.ntrans')

class NfsiostatReport(pmcc.MetricGroupPrinter):
    def __init__(self,opts,group):
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
        time_string = time.strftime("%m/%d/%Y %H:%M:%S", timestamp.struct_time())
        header_string = ''
        header_string += context['kernel.uname.sysname'].netValues[0][2] + '  '
        header_string += context['kernel.uname.release'].netValues[0][2] + '  '
        header_string += '(' + context['kernel.uname.nodename'].netValues[0][2] + ')  '
        header_string += time_string + '  '
        header_string += context['kernel.uname.machine'].netValues[0][2] + '  '
        print("%s  (%s CPU)" % (header_string, self.__get_ncpu(context)))

    def __print_values(self,timestamp, nfsstatus):
        n_shares = nfsstatus.mount_share_keys()
        mountshare = nfsstatus.mount_share()
        mountpoint = nfsstatus.mount_point()
        sampletime = nfsstatus.sample_time()
        sends = nfsstatus.xprt_sends()
        backlog = nfsstatus.xprt_backlog()
        readops = nfsstatus.readops()
        readerrors = nfsstatus.readerrors()
        readexecute = nfsstatus.readexecute()
        readrtt = nfsstatus.readrtt()
        readqueue = nfsstatus.readqueue()
        readbytesrecv = nfsstatus.readbytesrecv()
        readbytessent = nfsstatus.readbytessent()
        readntrans = nfsstatus.readntrans()
        writeops = nfsstatus.writeops()
        writeerrors = nfsstatus.writeerrors()
        writeexecute = nfsstatus.writeexecute()
        writertt = nfsstatus.writertt()
        writequeue = nfsstatus.writequeue()
        writebytesrecv = nfsstatus.writebytesrecv()
        writebytessent = nfsstatus.writebytessent()
        writentrans = nfsstatus.writentrans()

        print("%-18s:%s"%("Timestamp", timestamp))
        print()

        for name in n_shares:
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

    def print_report(self,group,timestamp, manager_nfsiostat):
        def __print_nfs_status():
            nfsstatus = NfsioStatUtil(manager_nfsiostat)
            if nfsstatus.mount_share():
                try:
                    self.__print_machine_info(group)
                    self.__print_values(timestamp, nfsstatus)
                except IndexError:
                    print("Incorrect machine info due to some missing metrics")
                return
            else:
                pass

        if self.context != PM_CONTEXT_ARCHIVE and self.samples is None:
            __print_nfs_status()
            sys.exit(0)
        elif self.context == PM_CONTEXT_ARCHIVE and self.samples is None:
            __print_nfs_status()
        elif self.samples >=1:
            __print_nfs_status()
            self.samples-=1
        else:
            pass

    def report(self, manager):
        group = manager["sysinfo"]
        self.samples = self.opts.pmGetOptionSamples()
        t_s = group.contextCache.pmLocaltime(int(group.timestamp))
        timestamp = time.strftime(NfsiostatOptions.timefmt, t_s.struct_time())
        self.print_report(group,timestamp,manager['nfsiostat'])

class NfsiostatOptions(pmapi.pmOptions):
    timefmt = "%m/%d/%Y %H:%M:%S"
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
        opts = NfsiostatOptions()
        mngr = pmcc.MetricGroupManager.builder(opts,sys.argv)
        opts.context=mngr.type
        missing = mngr.checkMissingMetrics(ALL_METRICS)
        if missing is not None:
            sys.stderr.write('Error: not all required metrics are available\nMissing %s\n' % missing)
            sys.exit(1)
        mngr["nfsiostat"] = ALL_METRICS
        mngr["sysinfo"] = SYS_METRICS
        mngr.printer = NfsiostatReport(opts,mngr)
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
