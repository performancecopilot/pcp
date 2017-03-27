#!/usr/bin/env pmpython
# Copyright (C) 2014-2016 Red Hat.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# tapestat Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# pylint: disable=C0103,R0914,R0902
""" Display tape I/O statistics """

import re
import sys
import signal
from pcp import pmapi, pmcc
from cpmapi import PM_TYPE_U64, PM_CONTEXT_ARCHIVE, PM_SPACE_KBYTE, PM_MODE_FORW

# use default SIGPIPE handler to avoid broken pipe exceptions
signal.signal(signal.SIGPIPE, signal.SIG_DFL)

# Tape: r/s w/s kB_read/s kB_wrtn/s %Rd %Wr %Oa Rs/s Ot/s
# These are the metrics tapestat (sysstat) shows and we will do the same
# for starters


#       [root@nkshirsa sysstat]# ls /sys/class/scsi_tape/st0/stats/ -l
#       total 0
#       in_flight
#       io_ns
#       other_cnt
#       read_byte_cnt
#       read_cnt
#       read_ns
#       resid_cnt
#       write_byte_cnt
#       write_cnt
#       write_ns


TAPESTAT_METRICS = [ 'tape.dev.in_flight', 'tape.dev.io_ns',
                 'tape.dev.other_cnt', 'tape.dev.read_byte_cnt',
                 'tape.dev.read_cnt', 'tape.dev.read_ns',
                 'tape.dev.resid_cnt', 'tape.dev.write_byte_cnt',
                 'tape.dev.write_cnt', 'tape.dev.write_ns']


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

class TapestatReport(pmcc.MetricGroupPrinter):
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
        regex = TapestatOptions.Rflag
        if regex == '':
            regex = '.*'

        aggr = TapestatOptions.Gflag
        if aggr and aggr not in ('sum', 'avg', 'min', 'max'):
           print("Error, -G aggregation method must be one of 'sum', 'avg', 'min' or 'max'")
           raise pmapi.pmUsageErr

        precision = TapestatOptions.Pflag
        if precision < 0 or precision > 10 :
           print("Precision value must be between 0 and 10")
           raise pmapi.pmUsageErr

        subtree = 'tape.dev'
        group = manager["tapestat"]

        if group[subtree + '.read_cnt'].netPrevValues == None:
            # need two fetches to report rate converted counter metrics
            #print "found none, returning"
            return
        instlist = self.instlist(group, subtree + '.read_cnt')
        dt = self.timeStampDelta(group)
        timestamp = group.contextCache.pmCtime(int(group.timestamp)).rstrip()

        c_r = self.curVals(group, subtree + '.read_cnt')
        p_r = self.prevVals(group, subtree + '.read_cnt')

        c_w = self.curVals(group, subtree + '.write_cnt')
        p_w = self.prevVals(group, subtree + '.write_cnt')

        c_rkb = self.curVals(group, subtree + '.read_byte_cnt')
        p_rkb = self.prevVals(group, subtree + '.read_byte_cnt')
        c_wkb = self.curVals(group, subtree + '.write_byte_cnt')
        p_wkb = self.prevVals(group, subtree + '.write_byte_cnt')
 
        # calculate the percentage waits
        c_rpw = self.curVals(group, subtree + '.read_ns')
        p_rpw = self.prevVals(group, subtree + '.read_ns')
        c_wpw = self.curVals(group, subtree + '.write_ns')
        p_wpw = self.prevVals(group, subtree + '.write_ns')
        c_opw =  self.curVals(group, subtree + '.io_ns')
        p_opw = self.prevVals(group, subtree + '.io_ns')

        p_resid = self.prevVals(group, subtree + '.resid_cnt')
        c_resid = self.curVals(group, subtree + '.resid_cnt')
        p_other = self.prevVals(group, subtree + '.other_cnt')
        c_other = self.curVals(group, subtree + '.other_cnt')



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

        if "t" in TapestatOptions.xflag:
            headfmt = "%-24s %-12s  %*s %*s %*s %*s %*s %*s %*s %*s %*s"
            valfmt = "%-24s %-12s %*.*f %*.*f %*d %*d %*.*f %*.*f %*.*f %*.*f %*.*f"
        else:
            headfmt = "%-12s %*s %*s %*s %*s %*s %*s %*s %*s %*s"
            valfmt = "%-12s %*.*f %*.*f %*d %*d %*.*f %*.*f %*.*f %*.*f %*.*f"


        tmpspace = precision+5
 
        if precision == 0 :
          tmpspace = tmpspace  +1

        if "h" not in TapestatOptions.xflag:
            self.Hcount += 1
            if self.Hcount == 24:
                self.Hcount = 1
            if self.Hcount == 1:
                if "t" in TapestatOptions.xflag:
                    heading = ('# Timestamp', 'Device',tmpspace - 1, 'r/s',tmpspace, 'w/s',tmpspace, 'kb_r/s',tmpspace, 'kb_w/s', tmpspace, 'r_pct',tmpspace,'w_pct',tmpspace, 'o_pct',tmpspace,'Rs/s',tmpspace,'o_cnt')
                else:
                    heading = ('# Device',tmpspace, 'r/s',tmpspace, 'w/s',tmpspace, 'kb_r/s',tmpspace, 'kb_w/s', tmpspace, 'r_pct',tmpspace,'w_pct',tmpspace, 'o_pct',tmpspace,'Rs/s',tmpspace,'o_cnt')
                print(headfmt % heading)

        if p_r == {} or p_w == {} or p_rkb == {} or p_wkb == {} or \
           p_rpw == {} or p_wpw == {} or p_opw == {} or \
           p_resid == {} or p_other == {}:
            # no values for some metric (e.g. near start of archive)
            if "t" in TapestatOptions.xflag:
                print(headfmt % (timestamp, 'NODATA',rrqmspace -1, '?',wrqmspace, '?',precision+5, '?',precision+5, '?',precision+5,\
               '?',precision+5, '?',headfmtavgspace -1 , '?',headfmtquspace -1, '?', precision+5, '?'))
            return
    
            #don't we need an else here to print ?'s for non timestamp flag too ?

        try:
            if TapestatOptions.Gflag:
                aggr_r = aggr_w = aggr_rkb = aggr_wkb = aggr_actual_rpw = aggr_actual_wpw =  0.0
                aggr_actual_opw = aggr_resid_cnt = aggr_o_cnt = 0.0
                aggr_count = 0

            for inst in sorted(instlist):
                # basic stats
                r = (c_r[inst] - p_r[inst]) / dt
                w = (c_w[inst] - p_w[inst]) / dt
                rkb = (c_rkb[inst] - p_rkb[inst]) / dt / 1000.0 # bytes to kb
                wkb = (c_wkb[inst] - p_wkb[inst]) / dt / 1000.0
               
                
                #calculate and convert from nano seconds
                rpw = 100 * (c_rpw[inst] - p_rpw[inst])/ 10.0**9 / dt
                wpw = 100 * (c_wpw[inst] - p_wpw[inst]) / 10.0**9 / dt
   
                actual_rpw = rpw 
                actual_wpw = wpw 

                opw = 100 * (c_opw[inst] - p_opw[inst]) / 10.0**9 / dt
                actual_opw = opw 
 
                resid_cnt = (c_resid[inst] - p_resid[inst]) / dt / 1000.0
 
                #The  number of I/Os, expressed as the number per second averaged over the interval, that were included as "other" is o_cnt
                o_cnt = (c_other[inst] - p_other[inst]) / dt / 1000.0


                device = inst   # prepare name for printing
                badcounters = r < 0 or w < 0 or rkb < 0 or wkb < 0 or rpw < 0 or wpw < 0 or opw < 0 or resid_cnt < 0 or o_cnt < 0

                if "t" in TapestatOptions.xflag:
                    if badcounters:
                        print(headfmt % (timestamp, device,rrqmspace, '?',wrqmspace, '?',precision+5, '?',precision+4, '?',precision+6,\
                        '?',precision+6, '?',headfmtavgspace, '?',headfmtquspace, '?', precision+5, '?'))

                    else:
                        if TapestatOptions.Rflag and re.search(regex,device) == None:
                            continue

                        if TapestatOptions.Gflag:
                            aggr_count += 1

                        if "noidle" in TapestatOptions.xflag:
                            if rkb == 0 and wkb == 0 and r == 0 and w == 0 :
                                continue

                        if not TapestatOptions.Gflag:
                            print(valfmt % (timestamp, device,tmpspace,precision,r,tmpspace, precision, w,tmpspace,rkb,tmpspace, wkb, tmpspace,precision,actual_rpw,tmpspace,precision,actual_wpw, tmpspace,precision,actual_opw,tmpspace,precision,resid_cnt,tmpspace,precision,o_cnt))
                else:
                    if badcounters:
                        print(headfmt % (device,rrqmspace, '?',wrqmspace, '?',precision+5, '?',precision+4, '?',precision+6,\
                        '?',precision+6, '?',headfmtavgspace, '?',headfmtquspace, '?', precision+5, '?',awaitspace, '?',\
                        awaitspace, '?',utilspace, '?'))

                    else:
                        if TapestatOptions.Rflag and re.search(regex,device) == None:
                            continue

                        if TapestatOptions.Gflag:
                            aggr_count += 1

                        if "noidle" in TapestatOptions.xflag:
                            if rkb == 0 and wkb == 0 and r == 0 and w == 0 :
                                continue

                        if not TapestatOptions.Gflag:
#                           print(valfmt % (device,tmpspace, precision, aggr_r,tmpspace,precision, aggr_w,tmpspace, aggr_rkb,tmpspace, aggr_wkb, tmpspace,precision,actual_rpw,tmpspace,precision,actual_wpw))
                            print(valfmt % (device,tmpspace,precision,r,tmpspace, precision, w,tmpspace,rkb,tmpspace, wkb, tmpspace,precision,actual_rpw,tmpspace,precision,actual_wpw, tmpspace,precision,actual_opw,tmpspace,precision,resid_cnt,tmpspace,precision,o_cnt))

                if TapestatOptions.Gflag and not badcounters:
                    aggr_r = aggregate(aggr, aggr_r, r)
                    aggr_w = aggregate(aggr, aggr_w, w)
                    aggr_rkb = aggregate(aggr, aggr_rkb, rkb)
                    aggr_wkb = aggregate(aggr, aggr_wkb, wkb)
                    aggr_actual_rpw = aggregate(aggr, aggr_actual_rpw, actual_rpw)
                    aggr_actual_wpw = aggregate(aggr, aggr_actual_wpw, actual_wpw)
                    aggr_actual_opw = aggregate(aggr, aggr_actual_opw, actual_opw)
                    aggr_resid_cnt = aggregate(aggr, aggr_resid_cnt, resid_cnt)
                    aggr_o_cnt = aggregate(aggr, aggr_o_cnt, o_cnt)
            # end of loop

            if TapestatOptions.Gflag:
                if TapestatOptions.Gflag == 'avg' and aggr_count > 0:
                    aggr_r /= aggr_count
                    aggr_w /= aggr_count
                    aggr_rkb /= aggr_count
                    aggr_wkb /= aggr_count
                    aggr_actual_rpw /= aggr_count
                    aggr_actual_wpw /= aggr_count
                    aggr_actual_opw /= aggr_count
                    aggr_resid_cnt /= aggr_count
                    aggr_o_cnt /= aggr_count


                # report aggregate values - the 'device' here is reported as the regex used for the aggregation
                device = '%s(%s)' % (aggr, regex)
                if "t" in TapestatOptions.xflag:
                    print(valfmt % (timestamp, device,tmpspace, precision, aggr_r,tmpspace,precision, aggr_w,tmpspace, aggr_rkb,tmpspace, aggr_wkb, tmpspace,precision,aggr_actual_rpw,tmpspace,precision,aggr_actual_wpw,tmpspace,precision,aggr_actual_opw , tmpspace,precision,aggr_resid_cnt,tmpspace,precision,aggr_o_cnt))
                else:
                    print(valfmt % (device,tmpspace, precision, aggr_r,tmpspace,precision, aggr_w,tmpspace, aggr_rkb,tmpspace, aggr_wkb, tmpspace,precision,aggr_actual_rpw,tmpspace,precision,aggr_actual_wpw,tmpspace,precision,aggr_actual_opw , tmpspace,precision,aggr_resid_cnt,tmpspace,precision,aggr_o_cnt))

        except KeyError:
            # instance missing from previous sample
            pass

class TapestatOptions(pmapi.pmOptions):
    # class attributes
    xflag = []
    uflag = None
    Pflag = 2
    Rflag = ""
    Gflag = ""
    def checkOptions(self, manager):
        if TapestatOptions.uflag:
            if manager._options.pmGetOptionInterval():
                print("Error: -t incompatible with -u")
                return False
            if manager.type != PM_CONTEXT_ARCHIVE:
                print("Error: -u can only be specified with -a archive")
                return False
        return True

    def extraOptions(self, opt, optarg, index):
        if opt == "x":
            TapestatOptions.xflag += optarg.replace(',', ' ').split(' ')
        elif opt == "u":
            TapestatOptions.uflag = True
        elif opt == "P":
            TapestatOptions.Pflag = int(optarg)
        elif opt == "R":
            TapestatOptions.Rflag = optarg
        elif opt == "G":
            TapestatOptions.Gflag = optarg

#   all this stuff needs changes but leaving it as it is for now, anyway we will first get
#   it working without any arguments

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
        self.pmSetLongOption("", 1, 'x', "LIST", "comma separated extended options: [[t],[h],[noidle]]")
        self.pmSetLongOptionText("\t\tt\tprecede every line with a timestamp in ctime format");
        self.pmSetLongOptionText("\t\th\tsuppress headings");
        self.pmSetLongOptionText("\t\tnoidle\tdo not display idle devices");

if __name__ == '__main__':
    try:
        opts = TapestatOptions()
        manager = pmcc.MetricGroupManager.builder(opts, sys.argv)
        if not opts.checkOptions(manager):
            raise pmapi.pmUsageErr

        if TapestatOptions.uflag:
            # -u turns off interpolation
            manager.pmSetMode(PM_MODE_FORW, manager._options.pmGetOptionOrigin(), 0)

        manager["tapestat"] = TAPESTAT_METRICS
        #print "calling TapestatReport()"
        manager.printer = TapestatReport()
        sts = manager.run()
        sys.exit(sts)
    except pmapi.pmErr as error:
        sys.stderr.write('%s: %s\n' % (error.progname(), error.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except KeyboardInterrupt:
        pass



