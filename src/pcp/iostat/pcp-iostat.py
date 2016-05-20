#!/usr/bin/env pmpython
# Copyright (C) 2014-2016 Red Hat.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Iostat Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# pylint: disable=C0103,R0914,R0902
""" Display disk and device-mapper I/O statistics """

import re
import sys
import signal
from pcp import pmapi, pmcc
from cpmapi import PM_TYPE_U64, PM_CONTEXT_ARCHIVE, PM_SPACE_KBYTE, PM_MODE_FORW

# use default SIGPIPE handler to avoid broken pipe exceptions
signal.signal(signal.SIGPIPE, signal.SIG_DFL)

IOSTAT_SD_METRICS = [ 'disk.dev.read', 'disk.dev.read_bytes',
                 'disk.dev.write', 'disk.dev.write_bytes',
                 'disk.dev.read_merge', 'disk.dev.write_merge',
                 'disk.dev.blkread', 'disk.dev.blkwrite',
                 'disk.dev.read_rawactive', 'disk.dev.write_rawactive',
                 'disk.dev.avactive']

IOSTAT_DM_METRICS = [ 'disk.dm.read', 'disk.dm.read_bytes',
                 'disk.dm.write', 'disk.dm.write_bytes',
                 'disk.dm.read_merge', 'disk.dm.write_merge',
                 'disk.dm.blkread', 'disk.dm.blkwrite',
                 'disk.dm.read_rawactive', 'disk.dm.write_rawactive',
                 'disk.dm.avactive']

def aggregate(method, aggr_value, value):
    if method == 'sum':
        aggr_value += value
    elif method == 'avg':
        aggr_value += value
    elif method == 'min':
        if aggr_value > value:
            aggr_value = value
    elif method == 'max':
        if aggr_value < value:
            aggr_value = value
    else:
       raise pmapi.pmUsageErr
    return aggr_value

class IostatReport(pmcc.MetricGroupPrinter):
    Hcount = 0
    def timeStampDelta(self, group):
        s = group.timestamp.tv_sec - group.prevTimestamp.tv_sec
        u = group.timestamp.tv_usec - group.prevTimestamp.tv_usec
        # u may be negative here, calculation is still correct.
        return (s + u / 1000000.0)

    def instlist(self, group, name):
        return dict(map(lambda x: (x[1], x[2]), group[name].netValues)).keys()

    def curVals(self, group, name):
        return dict(map(lambda x: (x[1], x[2]), group[name].netValues))

    def prevVals(self, group, name):
        return dict(map(lambda x: (x[1], x[2]), group[name].netPrevValues))

    def report(self, manager):
        regex = IostatOptions.Rflag
        if regex == '':
            regex = '.*'

        aggr = IostatOptions.Gflag
        if aggr and aggr not in ('sum', 'avg', 'min', 'max'):
           print("Error, -G aggregation method must be one of 'sum', 'avg', 'min' or 'max'")
           raise pmapi.pmUsageErr

        precision = IostatOptions.Pflag
        if precision < 0 or precision > 10 :
           print("Precision value must be between 0 and 10")
           raise pmapi.pmUsageErr

        if 'dm' in IostatOptions.xflag:
            subtree = 'disk.dm'
        else:
            subtree = 'disk.dev'
        group = manager["iostat"]

        if group[subtree + '.read_merge'].netPrevValues == None:
            # need two fetches to report rate converted counter metrics
            return

        instlist = self.instlist(group, subtree + '.read')
        dt = self.timeStampDelta(group)
        timestamp = group.contextCache.pmCtime(int(group.timestamp)).rstrip()

        c_rrqm = self.curVals(group, subtree + '.read_merge')
        p_rrqm = self.prevVals(group, subtree + '.read_merge')

        c_wrqm = self.curVals(group, subtree + '.write_merge')
        p_wrqm = self.prevVals(group, subtree + '.write_merge')

        c_r = self.curVals(group, subtree + '.read')
        p_r = self.prevVals(group, subtree + '.read')

        c_w = self.curVals(group, subtree + '.write')
        p_w = self.prevVals(group, subtree + '.write')

        c_rkb = self.curVals(group, subtree + '.read_bytes')
        p_rkb = self.prevVals(group, subtree + '.read_bytes')

        c_wkb = self.curVals(group, subtree + '.write_bytes')
        p_wkb = self.prevVals(group, subtree + '.write_bytes')

        c_ractive = self.curVals(group, subtree + '.read_rawactive')
        p_ractive = self.prevVals(group, subtree + '.read_rawactive')

        c_wactive = self.curVals(group, subtree + '.write_rawactive')
        p_wactive = self.prevVals(group, subtree + '.write_rawactive')

        c_avactive = self.curVals(group, subtree + '.avactive')
        p_avactive = self.prevVals(group, subtree + '.avactive')
        
        if precision == 1:
           utilspace=precision+5
           avgrqszspace=precision+7
           awaitspace=precision+6
           rrqmspace=precision+5
           wrqmspace=precision+5
           headfmtavgspace=precision+7
           headfmtquspace=precision+7
        elif precision == 0:
           utilspace=precision+5
           avgrqszspace=precision+8
           awaitspace=precision+7
           rrqmspace=precision+6
           wrqmspace=precision+6
           headfmtavgspace=avgrqszspace
           headfmtquspace=precision+8
        else:
           utilspace=precision+5
           avgrqszspace=precision+6
           awaitspace=precision+5
           rrqmspace=precision+5
           wrqmspace=precision+5
           headfmtavgspace=avgrqszspace
           headfmtquspace=precision+6

        if "t" in IostatOptions.xflag:
            headfmt = "%-24s %-12s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s"
            valfmt = "%-24s %-12s %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f"
        else:
            headfmt = "%-12s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s"
            valfmt = "%-12s %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f"

        if "h" not in IostatOptions.xflag:
            self.Hcount += 1
            if self.Hcount == 24:
                self.Hcount = 1
            if self.Hcount == 1:
                if "t" in IostatOptions.xflag:
                    heading = ('# Timestamp', 'Device',rrqmspace, 'rrqm/s',wrqmspace, 'wrqm/s',precision+5, 'r/s',precision+4,\
                    'w/s',precision+6, 'rkB/s',precision+6, 'wkB/s', avgrqszspace,'avgrq-sz',precision+6, 'avgqu-sz',precision+5, \
                    'await',precision+5, 'r_await', precision+5,'w_await',utilspace, '%util')
                else:
                    heading = ('# Device',rrqmspace, 'rrqm/s',wrqmspace, 'wrqm/s',precision+5, 'r/s',precision+4, 'w/s'\
                    ,precision+6, 'rkB/s',precision+6, 'wkB/s', avgrqszspace,'avgrq-sz',precision+6, 'avgqu-sz',precision+5,\
                    'await',awaitspace, 'r_await',awaitspace, 'w_await',utilspace, '%util')
                print(headfmt % heading)

        if p_rrqm == {} or p_wrqm == {} or p_r == {} or p_w == {} or \
           p_ractive == {} or p_wactive == {} or p_avactive == {} or \
           p_rkb == {} or p_wkb == {}:
            # no values for some metric (e.g. near start of archive)
            if "t" in IostatOptions.xflag:
                print(headfmt % (timestamp, 'NODATA',rrqmspace, '?',wrqmspace, '?',precision+5, '?',precision+4, '?',precision+6,\
               '?',precision+6, '?',headfmtavgspace, '?',headfmtquspace, '?', precision+5, '?',awaitspace, '?',awaitspace, '?',\
                utilspace, '?'))
            return

        try:
            if IostatOptions.Gflag:
                aggr_rrqm = aggr_wrqm = aggr_r = aggr_w = aggr_rkb = aggr_wkb = aggr_avgrqsz = 0.0
                aggr_avgqsz = aggr_await = aggr_r_await = aggr_w_await = aggr_util = 0.0
                aggr_count = 0

            for inst in sorted(instlist):
                # basic stats
                rrqm = (c_rrqm[inst] - p_rrqm[inst]) / dt
                wrqm = (c_wrqm[inst] - p_wrqm[inst]) / dt
                r = (c_r[inst] - p_r[inst]) / dt
                w = (c_w[inst] - p_w[inst]) / dt
                rkb = (c_rkb[inst] - p_rkb[inst]) / dt
                wkb = (c_wkb[inst] - p_wkb[inst]) / dt

                # totals
                tot_rios = (float)(c_r[inst] - p_r[inst])
                tot_wios = (float)(c_w[inst] - p_w[inst])
                tot_ios = (float)(tot_rios + tot_wios)

                # total active time in seconds (same units as dt)
                tot_active = (float)(c_avactive[inst] - p_avactive[inst]) / 1000.0

                avgrqsz = avgqsz = await = r_await = w_await = util = 0.0

                # average request size units are KB (sysstat reports in units of sectors)
                if tot_ios:
                    avgrqsz = (float)((c_rkb[inst] - p_rkb[inst]) + (c_wkb[inst] - p_wkb[inst])) / tot_ios

                # average queue length
                avgqsz = (float)((c_ractive[inst] - p_ractive[inst]) + (c_wactive[inst] - p_wactive[inst])) / dt / 1000.0

                # await, r_await, w_await
                if tot_ios:
                    await = ((c_ractive[inst] - p_ractive[inst]) + (c_wactive[inst] - p_wactive[inst])) / tot_ios

                if tot_rios:
                    r_await = (c_ractive[inst] - p_ractive[inst]) / tot_rios

                if tot_wios:
                    w_await = (c_wactive[inst] - p_wactive[inst]) / tot_wios

                # device utilization (percentage of active time / interval)
                if tot_active:
                    util = 100.0 * tot_active / dt

                device = inst	# prepare name for printing
                badcounters = rrqm < 0 or wrqm < 0 or r < 0 or w < 0 or await < 0 or avgrqsz < 0 or avgqsz < 0 or util < 0

                if "t" in IostatOptions.xflag:
                    if badcounters:
                        print(headfmt % (timestamp, device,rrqmspace, '?',wrqmspace, '?',precision+5, '?',precision+4, '?',precision+6,\
                        '?',precision+6, '?',headfmtavgspace, '?',headfmtquspace, '?', precision+5, '?',awaitspace, '?',\
                        awaitspace, '?',utilspace, '?'))
                    else:
                        if IostatOptions.Rflag and re.search(regex,device) == None: 
                            continue  

                        if IostatOptions.Gflag:
                            aggr_count += 1

                        if "noidle" in IostatOptions.xflag:
                            if rrqm == 0 and wrqm == 0 and r == 0 and w == 0 :
                                continue

                        if not IostatOptions.Gflag:
                            print(valfmt % (timestamp, device,rrqmspace, precision, rrqm,wrqmspace,precision, wrqm,precision+5,precision,\
                            r,precision+4,precision, w,precision+6,precision, rkb,precision+6,precision, wkb, avgrqszspace,precision+1 ,avgrqsz,\
                            avgrqszspace,precision+1, avgqsz,precision+5,precision, await,awaitspace,precision, r_await,awaitspace,precision,\
                            w_await,utilspace,precision, util))
                else:
                    if badcounters:
                        print(headfmt % (device,rrqmspace, '?',wrqmspace, '?',precision+5, '?',precision+4, '?',precision+6, '?',precision+6,\
                        '?',headfmtavgspace, '?',headfmtquspace, '?', precision+5, '?',awaitspace, '?',awaitspace, '?',utilspace, '?'))
                    else:
                        if IostatOptions.Rflag and re.search(regex,device) == None: 
                            continue  

                        if IostatOptions.Gflag:
                            aggr_count += 1

                        if "noidle" in IostatOptions.xflag:
                            if rrqm == 0 and wrqm == 0 and r == 0 and w == 0 :
                                continue

                        if not IostatOptions.Gflag:
                            print(valfmt % (device,rrqmspace, precision, rrqm,wrqmspace,precision, wrqm,precision+5,precision, r,precision+4,\
                            precision, w,precision+6,precision, rkb,precision+6,precision, wkb,\
                            avgrqszspace,precision+1 ,avgrqsz,avgrqszspace,precision+1, avgqsz,precision+5,precision, await,awaitspace,precision,\
                            r_await,awaitspace,precision, w_await,utilspace,precision, util))

                if IostatOptions.Gflag and not badcounters:
                    aggr_rrqm = aggregate(aggr, aggr_rrqm, rrqm)
                    aggr_wrqm = aggregate(aggr, aggr_wrqm, wrqm)
                    aggr_r = aggregate(aggr, aggr_r, r)
                    aggr_w = aggregate(aggr, aggr_w, w)
                    aggr_rkb = aggregate(aggr, aggr_rkb, rkb)
                    aggr_wkb = aggregate(aggr, aggr_wkb, wkb)
                    aggr_avgrqsz = aggregate(aggr, aggr_avgrqsz, avgrqsz)
                    aggr_avgqsz = aggregate(aggr, aggr_avgqsz, avgqsz)
                    aggr_await = aggregate(aggr, aggr_await, await)
                    aggr_r_await = aggregate(aggr, aggr_r_await, r_await)
                    aggr_w_await = aggregate(aggr, aggr_w_await, w_await)
                    aggr_util = aggregate(aggr, aggr_util, util)
            # end of loop

            if IostatOptions.Gflag:
                if IostatOptions.Gflag == 'avg' and aggr_count > 0:
                    aggr_rrqm /= aggr_count
                    aggr_wrqm /= aggr_count
                    aggr_r /= aggr_count
                    aggr_w /= aggr_count
                    aggr_rkb /= aggr_count
                    aggr_wkb /= aggr_count
                    aggr_avgrqsz /= aggr_count
                    aggr_avgqsz /= aggr_count
                    aggr_await /= aggr_count
                    aggr_r_await /= aggr_count
                    aggr_w_await /= aggr_count
                    aggr_util /= aggr_count


                # report aggregate values - the 'device' here is reported as the regex used for the aggregation
                device = '%s(%s)' % (aggr, regex)
                if "t" in IostatOptions.xflag:
                    print(valfmt % (timestamp, device,rrqmspace, precision, aggr_rrqm,wrqmspace,precision, aggr_wrqm,precision+5,precision,\
                    aggr_r,precision+4,precision, aggr_w,precision+6,precision, aggr_rkb,precision+6,precision, aggr_wkb, avgrqszspace,precision+1 ,aggr_avgrqsz,\
                    avgrqszspace,precision+1, aggr_avgqsz,precision+5,precision, aggr_await,awaitspace,precision, aggr_r_await,awaitspace,precision,\
                    aggr_w_await,utilspace,precision, aggr_util))
                else:
                    print(valfmt % (device,rrqmspace, precision, aggr_rrqm,wrqmspace,precision, aggr_wrqm,precision+5,precision, aggr_r,precision+4,\
                    precision, aggr_w,precision+6,precision, aggr_rkb,precision+6,precision, aggr_wkb,\
                    avgrqszspace,precision+1 ,aggr_avgrqsz,avgrqszspace,precision+1, aggr_avgqsz,precision+5,precision, aggr_await,awaitspace,precision,\
                    aggr_r_await,awaitspace,precision, aggr_w_await,utilspace,precision, aggr_util))

        except KeyError:
            # instance missing from previous sample
            pass

class IostatOptions(pmapi.pmOptions):
    # class attributes
    xflag = [] 
    uflag = None
    Pflag = 2
    Rflag = ""
    Gflag = ""
    def checkOptions(self, manager):
        if IostatOptions.uflag:
            if manager._options.pmGetOptionInterval():
                print("Error: -t incompatible with -u")
                return False
            if manager.type != PM_CONTEXT_ARCHIVE:
                print("Error: -u can only be specified with -a archive")
                return False
        return True

    def extraOptions(self, opt, optarg, index):
        if opt == "x":
            IostatOptions.xflag += optarg.replace(',', ' ').split(' ')
        elif opt == "u":
            IostatOptions.uflag = True
        elif opt == "P":
            IostatOptions.Pflag = int(optarg)
        elif opt == "R":
            IostatOptions.Rflag = optarg
        elif opt == "G":
            IostatOptions.Gflag = optarg

    def __init__(self):
        pmapi.pmOptions.__init__(self, "A:a:D:G:h:O:P:R:S:s:T:t:uVZ:z?x:")
        self.pmSetOptionCallback(self.extraOptions)
        self.pmSetLongOptionHeader("General options")
        self.pmSetLongOptionAlign()
        self.pmSetLongOptionArchive()
        self.pmSetLongOptionDebug()
        self.pmSetLongOption("aggregate", 1, "G", "method", "aggregate values for devices matching -R regex using 'method' (sum, avg, min or max)")
        self.pmSetLongOptionHost()
        self.pmSetLongOptionOrigin()
        self.pmSetLongOption("precision", 1, "P", "N", "N digits after the decimal separator")
        self.pmSetLongOption("regex", 1, "R", "pattern", "only report for devices names matching pattern, e.g. 'sd[a-zA-Z]+'. See also -G.")
        self.pmSetLongOptionStart()
        self.pmSetLongOptionSamples()
        self.pmSetLongOptionFinish()
        self.pmSetLongOptionInterval()
        self.pmSetLongOption("no-interpolation", 0, "u", "", "disable interpolation mode with archives")
        self.pmSetLongOptionVersion()
        self.pmSetLongOptionTimeZone()
        self.pmSetLongOptionHostZone()
        self.pmSetLongOptionHelp()
        self.pmSetLongOptionHeader("Extended options")
        self.pmSetLongOption("", 1, 'x', "LIST", "comma separated extended options: [[dm],[t],[h],[noidle]]")
        self.pmSetLongOptionText("\t\tdm\tshow device-mapper statistics (default is sd devices)")
        self.pmSetLongOptionText("\t\tt\tprecede every line with a timestamp in ctime format");
        self.pmSetLongOptionText("\t\th\tsuppress headings");
        self.pmSetLongOptionText("\t\tnoidle\tdo not display idle devices");

if __name__ == '__main__':
    try:
        opts = IostatOptions()
        manager = pmcc.MetricGroupManager.builder(opts, sys.argv)
        if not opts.checkOptions(manager):
            raise pmapi.pmUsageErr

        if IostatOptions.uflag:
            # -u turns off interpolation
            manager.pmSetMode(PM_MODE_FORW, manager._options.pmGetOptionOrigin(), 0)

        if "dm" in IostatOptions.xflag :
            manager["iostat"] = IOSTAT_DM_METRICS
        else:
            manager["iostat"] = IOSTAT_SD_METRICS
        manager.printer = IostatReport()
        sts = manager.run()
        sys.exit(sts)
    except pmapi.pmErr as error:
        sys.stderr.write('%s: %s\n' % (error.progname(), error.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except KeyboardInterrupt:
        pass
