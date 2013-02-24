#!/usr/bin/python

#
# pmcollectl.py
#
# Copyright (C) 2013 Red Hat Inc.
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

"""System status collector using the libpcp Wrapper module

Additional Information:

Performance Co-Pilot Web Site
http://oss.sgi.com/projects/pcp
"""

##############################################################################
#
# imports
#

import pmapi
import time
import sys
from pcp import *
from ctypes import *
from pmsubsys import cpu, interrupt, disk, memory, net, proc, subsys

me = "pmcollectl"

def usage ():
    print "\nUsage:", sys.argv[0], "\n\t[-sSUBSYS] [-f|--filename FILE] [-p|--playback FILE]"
    print '''\t[-R|--runtime N] [-c|--count N] [-i|--interval N] [--verbose]
'''


# round  -----------------------------------------------------------------


def round (value, magnitude):
    return (value + (magnitude / 2)) / magnitude


# get_dimension  ---------------------------------------------------------


def get_dimension (value):
    if type(value) != type(int()) and type(value) != type(long()):
        dim = len(value)
    else:
        dim = 1
    return dim
        

# get_scalar_value  ------------------------------------------------------


def get_scalar_value (var, idx):

    if type(var) != type(int()) and type(var) != type(long()):
        return var[idx]
    else:
        return var


# record ---------------------------------------------------------------

def record (pm, config, duration, file):
    global me

    # -f saves the metrics in a directory
    if os.path.exists(file):
        print me + "playback directory %s already exists\n" % file
        sys.exit(1)
    os.mkdir (file)
    status = pm.pmRecordSetup (file + "/" + me + ".pcp", me, 0)
    check_code (status)
    (status, rhp) = pm.pmRecordAddHost ("localhost", 1, config)
    check_code (status)
    status = pm.pmRecordControl (0, pmapi.PM_REC_SETARG, "-T" + str(duration) + "sec")
    check_code (status)
    status = pm.pmRecordControl (0, pmapi.PM_REC_ON, "")
    check_code (status)
    time.sleep(duration)
    pm.pmRecordControl (0, pmapi.PM_REC_STATUS, "")
    status = pm.pmRecordControl (rhp, pmapi.PM_REC_OFF, "")
    if status < 0 and status != pmapi.PM_ERR_IPC:
        check_status (status)


# record_add_creator ------------------------------------------------------

def record_add_creator (fn):
    f = open (fn + "/" + me + ".pcp", "r+")
    args = ""
    for i in sys.argv:
        args = args + i + " "
    f.write("# Created by " + args)
    f.write("\n#\n")
    f.close()

# record_check_creator ------------------------------------------------------

def record_check_creator (fn, doc):
    f = open (fn + "/" + me + ".pcp", "r")
    line = f.readline()
    if line.find("# Created by ") == 0:
        print doc + line[13:]
    f.close()


# _collect_print ---------------------------------------------------------------


class _collect_print(object):
    def print_header1(self):
        if self.verbosity == "brief":
            self.print_header1_brief()
        elif self.verbosity == "detail":
            self.print_header1_detail()
        elif self.verbosity == "verbose":
            self.print_header1_verbose()
    def print_header2(self):
        if self.verbosity == "brief":
            self.print_header2_brief()
        elif self.verbosity == "detail":
            self.print_header2_detail()
        elif self.verbosity == "verbose":
            self.print_header2_verbose()
    def print_header1_brief(self):
        True
    def print_header2_brief(self):
        True
    def print_header1_detail(self):
        True
    def print_header2_detail(self):
        True
    def print_header1_verbose(self):
        True
    def print_header2_verbose(self):
        True
    def print_line(self):
        if self.verbosity == "brief":
            self.print_brief()
        elif self.verbosity == "detail":
            self.print_detail()
        elif self.verbosity == "verbose":
            self.print_verbose()
    def print_brief(self):
        True
    def print_verbose(self):
        True
    def print_detail(self):
        True
    def divide_check(self, dividend, divisor):
        if divisor == 0:
            return 0
        else:
            return dividend / divisor


# _cpu_collect_print --------------------------------------------------


class _cpu_collect_print(cpu, _collect_print):
    def print_header1_brief(self):
        print '#<--------CPU-------->',
    def print_header1_detail(self):
        print '# SINGLE CPU STATISTICS'
    def print_header1_verbose(self):
        print '# CPU SUMMARY (INTR, CTXSW & PROC /sec)'

    def print_header2_brief(self):
        print '#cpu sys inter  ctxsw',
    def print_header2_detail(self):
        print '#   Cpu  User Nice  Sys Wait IRQ  Soft Steal Idle'
    def print_header2_verbose(self):
        print '#User  Nice   Sys  Wait   IRQ  Soft Steal  Idle  CPUs  Intr  Ctxsw  Proc  RunQ   Run   Avg1  Avg5 Avg15 RunT BlkT'

    def print_brief(self):
        print "%4d" % (100 * (self.get_metric_value('kernel.all.cpu.nice') +
                              self.get_metric_value('kernel.all.cpu.user') +
                              self.get_metric_value('kernel.all.cpu.intr') +
                              self.get_metric_value('kernel.all.cpu.sys') +
                              self.get_metric_value('kernel.all.cpu.steal') +
                              self.get_metric_value('kernel.all.cpu.irq.hard') +
                              self.get_metric_value('kernel.all.cpu.irq.soft')) /
                       self.cpu_total),
        print "%3d" % (100 * (self.get_metric_value('kernel.all.cpu.intr') +
                              self.get_metric_value('kernel.all.cpu.sys') +
                              self.get_metric_value('kernel.all.cpu.steal') +
                              self.get_metric_value('kernel.all.cpu.irq.hard') +
                              self.get_metric_value('kernel.all.cpu.irq.soft')) /
                       self.cpu_total),
        print "%5d %6d" % (self.get_metric_value('kernel.all.intr'),
                           self.get_metric_value('kernel.all.pswitch')),
    def print_detail(self):
        for k in range(len(self.get_metric_value('kernel.percpu.cpu.user'))):
            print "    %3d  %4d %4d  %3d %4d %3d  %4d %5d %4d" % (
                k,
                (100 * (self.get_metric_value('kernel.percpu.cpu.nice')[k] +
                        self.get_metric_value('kernel.percpu.cpu.user')[k] +
                        self.get_metric_value('kernel.percpu.cpu.intr')[k] +
                        self.get_metric_value('kernel.percpu.cpu.sys')[k] +
                        self.get_metric_value('kernel.percpu.cpu.steal')[k] +
                        self.get_metric_value('kernel.percpu.cpu.irq.hard')[k] +
                        self.get_metric_value('kernel.percpu.cpu.irq.soft')[k]) /
             self.cpu_total),
            self.get_metric_value('kernel.percpu.cpu.nice')[k],
            (100 * (self.get_metric_value('kernel.percpu.cpu.intr')[k] +
                    self.get_metric_value('kernel.percpu.cpu.sys')[k] +
                    self.get_metric_value('kernel.percpu.cpu.steal')[k] +
                    self.get_metric_value('kernel.percpu.cpu.irq.hard')[k] +
                    self.get_metric_value('kernel.percpu.cpu.irq.soft')[k]) /
             self.cpu_total),
            self.get_metric_value('kernel.percpu.cpu.wait.total')[k],
            self.get_metric_value('kernel.percpu.cpu.irq.hard')[k],
            self.get_metric_value('kernel.percpu.cpu.irq.soft')[k],
            self.get_metric_value('kernel.percpu.cpu.steal')[k],
            self.get_metric_value('kernel.percpu.cpu.idle')[k] / 10)
    def print_verbose(self):
        ncpu = self.get_metric_value('hinv.ncpu')
        print "%4d %6d %5d %4d %4d %5d " % (
            (100 * (self.get_metric_value('kernel.all.cpu.nice') +
                    self.get_metric_value('kernel.all.cpu.user') +
                    self.get_metric_value('kernel.all.cpu.intr') +
                    self.get_metric_value('kernel.all.cpu.sys') +
                    self.get_metric_value('kernel.all.cpu.steal') +
                    self.get_metric_value('kernel.all.cpu.irq.hard') +
                    self.get_metric_value('kernel.all.cpu.irq.soft')) /
             self.cpu_total),
            self.get_metric_value('kernel.all.cpu.nice'),
            (100 * (self.get_metric_value('kernel.all.cpu.intr') +
                    self.get_metric_value('kernel.all.cpu.sys') +
                    self.get_metric_value('kernel.all.cpu.steal') +
                    self.get_metric_value('kernel.all.cpu.irq.hard') +
                    self.get_metric_value('kernel.all.cpu.irq.soft')) /
             self.cpu_total),
            self.get_metric_value('kernel.all.cpu.wait.total'),
            self.get_metric_value('kernel.all.cpu.irq.hard'),
            self.get_metric_value('kernel.all.cpu.irq.soft')
            ),
        print "%6d %6d %5d %5d %6d" % (
            self.get_metric_value('kernel.all.cpu.steal'),
            self.get_metric_value('kernel.all.cpu.idle') / (10 * ncpu),
            ncpu,
            self.get_metric_value('kernel.all.intr'),
            self.get_metric_value('kernel.all.pswitch')
            ),
        print "%5d %5d %5d %5.2f %5.2f %5.2f %4d %4d" % (
            self.get_metric_value('kernel.all.nprocs'),
            self.get_metric_value('kernel.all.runnable'),
            self.get_metric_value('proc.runq.runnable'),
            self.get_metric_value('kernel.all.load')[0],
            self.get_metric_value('kernel.all.load')[1],
            self.get_metric_value('kernel.all.load')[2],
            self.get_metric_value('kernel.all.runnable'),
            self.get_metric_value('proc.runq.blocked'))


# _interrupt_collect_print --------------------------------------------------


class _interrupt_collect_print(interrupt, _collect_print):
    def print_header1_brief(self):
            ndashes = (((len(self.metric_value[0])) * 6) - 6) / 2
            h = "#<"
            for k in range(ndashes):
                h += "-"
            h += "Int"
            for k in range(ndashes):
                h += "-"
            h += ">"
            print h,
    def print_header1_detail(self):
            print '# INTERRUPT DETAILS'
            print '# Int    ',
            for k in range(len(self.metric_value[0])):
                print 'Cpu%d ' % k,
            print 'Type            Device(s)'
    def print_header1_verbose(self):
            print '# INTERRUPT SUMMARY'
    def print_header2_brief(self):
            for k in range(len(self.metric_value[0])):
                if k == 0:
                    print '#Cpu%d ' % k,
                else:
                    print 'Cpu%d ' % k,
    def print_header2_verbose(self):
            print '#    ',
            for k in range(len(self.metric_value[0])):
                print 'Cpu%d ' % k,
            print
    def print_brief(self):
        int_count = []
        for k in range(len(self.metric_value[0])):
            int_count.append(0)
            for j  in range(0, len(self.metric_value)):
                int_count[k] += self.metric_value[j][k]
                
        for k in range(len(self.metric_value[0])):
            print "%4d " % (int_count[k]),
    def print_detail(self):
        for j  in range(0, len(self.metrics_dict)):
            for k in range(len(self.metric_value[0])):
                have_nonzero_value = False
                if self.metric_value[j][k] != 0:
                    have_nonzero_value = True
                if not have_nonzero_value:
                    continue
                # pcp does not give the interrupt # so print spaces
                print "%-8s" % self.metrics[j].split(".")[3],
                for k in range(len(self.metric_value[0])):
                    print "%4d " % (self.metric_value[j][k]),
                text = (pm.pmLookupText(self.metric_name[j], pmapi.PM_TEXT_ONELINE))
                print "%-18s %s" % (text[:(str.index(text," "))],
                                 text[(str.index(text," ")):])
    def print_verbose(self):
        print "     ",
        self.print_brief()
        print


# _disk_collect_print --------------------------------------------------


class _disk_collect_print(disk, _collect_print):
    def print_header1_brief(self):
            print '<----------Disks----------->',
    def print_header1_detail(self):
            print '# DISK STATISTICS (/sec)'
    def print_header1_verbose(self):
            print '\n\n# DISK SUMMARY (/sec)'
    def print_header2_brief(self):
            print ' KBRead  Reads KBWrit Writes',
    def print_header2_detail(self):
            print '#          <---------reads---------><---------writes---------><--------averages--------> Pct'
            print '#Name       KBytes Merged  IOs Size  KBytes Merged  IOs Size  RWSize  QLen  Wait SvcTim Util'
    def print_header2_verbose(self):
            print '#KBRead RMerged  Reads SizeKB  KBWrite WMerged Writes SizeKB\n',
    def print_brief(self):
        print "%6d %6d %6d %6d" % (
            self.get_metric_value('disk.all.read_bytes') / 1024,
            self.get_metric_value('disk.all.read'),
            self.get_metric_value('disk.all.write_bytes') / 1024,
            self.get_metric_value('disk.all.write')),
    def print_detail(self):
        for j in xrange(len(self.metric_name)):
            try:
                (inst, iname) = pm.pmGetInDom(self.metric_desc[j])
                break
            except pmErr, e:
                iname = iname = "X"

        # metric values may be scalars or arrays depending on # of disks
        for j in xrange(get_dimension(self.get_metric_value('disk.dev.read_bytes'))):
            print "%-10s %6d %6d %4d %4d  %6d %6d %4d %4d  %6d %6d %4d %6d %4d" % (
                iname[j],
                get_scalar_value (self.get_metric_value('disk.dev.read_bytes'), j),
                get_scalar_value (self.get_metric_value('disk.dev.read_merge'), j),
                get_scalar_value (self.get_metric_value('disk.dev.read'), j),
                get_scalar_value (self.get_metric_value('disk.dev.blkread'), j),
                get_scalar_value (self.get_metric_value('disk.dev.write_bytes'), j),
                get_scalar_value (self.get_metric_value('disk.dev.write_merge'), j),
                get_scalar_value (self.get_metric_value('disk.dev.write'), j),
                get_scalar_value (self.get_metric_value('disk.dev.blkwrite'), j),
                0, 0, 0, 0, 0)
# ??? replace 0 with required fields

    def print_verbose(self):
        avgrdsz = avgwrsz = 0
        if self.get_metric_value('disk.all.read') > 0:
            avgrdsz = self.get_metric_value('disk.all.read_bytes')
            avgrdsz /= self.get_metric_value('disk.all.read')
        if self.get_metric_value('disk.all.write') > 0:
            avgwrsz = self.get_metric_value('disk.all.write_bytes')
            avgwrsz /= self.get_metric_value('disk.all.write')

        print '%6d %6d %6d %6d %7d %8d %6d %6d' % (
            avgrdsz,
            self.get_metric_value('disk.all.read_merge'),
            self.get_metric_value('disk.all.read'),
            0,
            avgwrsz,
            self.get_metric_value('disk.all.write_merge'),
            self.get_metric_value('disk.all.write'),
            0)


# _memory_collect_print --------------------------------------------------


class _memory_collect_print(memory, _collect_print):
    def print_header1_brief(self):
        print '#<-----------Memory----------->'
    def print_header1_verbose(self):
        print '# MEMORY SUMMARY'
    def print_header2_brief(self):
        print '#Free Buff Cach Inac Slab  Map'
    def print_header2_verbose(self):
        print '#<-------------------------------Physical Memory--------------------------------------><-----------Swap------------><-------Paging------>'
        print '#   Total    Used    Free    Buff  Cached    Slab  Mapped    Anon  Commit  Locked Inact Total  Used  Free   In  Out Fault MajFt   In  Out'
    def print_brief(self):
        print "%4dM %3dM %3dM %3dM %3dM %3dM " % (
            round(self.get_metric_value('mem.freemem'), 1000),
            round(self.get_metric_value('mem.util.bufmem'), 1000),
            round(self.get_metric_value('mem.util.cached'), 1000),
            round(self.get_metric_value('mem.util.inactive'), 1000),
            round(self.get_metric_value('mem.util.slab'), 1000),
            round(self.get_metric_value('mem.util.mapped'), 1000)),
    def print_verbose(self):
        print "%8dM %6dM %6dM %6dM %6dM %6dM %6dM %6dM %6dM %6dM %5dM %5dM %5dM %5dM %6d %6d %6d %6d %6d %6d " % (
            round(self.get_metric_value('mem.physmem'), 1000),
            round(self.get_metric_value('mem.util.used'), 1000),
            round(self.get_metric_value('mem.freemem'), 1000),
            round(self.get_metric_value('mem.util.bufmem'), 1000),
            round(self.get_metric_value('mem.util.cached'), 1000),
            round(self.get_metric_value('mem.util.slab'), 1000),
            round(self.get_metric_value('mem.util.mapped'), 1000),
            round(self.get_metric_value('mem.util.anonpages'), 1000),
            round(self.get_metric_value('mem.util.committed_AS'), 1000),
            round(self.get_metric_value('mem.util.mlocked'), 1000),
            round(self.get_metric_value('mem.util.inactive'), 1000),
            round(self.get_metric_value('mem.util.swapTotal'), 1000),
            round(self.get_metric_value('swap.used'), 1000),
            round(self.get_metric_value('swap.free'), 1000),
            round(self.get_metric_value('swap.pagesin'), 1000),
            round(self.get_metric_value('swap.pagesout'), 1000),
            round(self.get_metric_value('mem.vmstat.pgfault') -
                  self.get_metric_value('mem.vmstat.pgmajfault'), 1000),
            round(self.get_metric_value('mem.vmstat.pgmajfault'), 1000),
            round(self.get_metric_value('mem.vmstat.pgpgin'), 1000),
            round(self.get_metric_value('mem.vmstat.pgpgout'), 1000))


# _net_collect_print --------------------------------------------------


class _net_collect_print(net, _collect_print):
    def print_header1_brief(self):
            print '<----------Network--------->',
    def print_header1_detail(self):
            print '# NETWORK STATISTICS (/sec)'
    def print_header1_verbose(self):
            print '\n\n# NETWORK SUMMARY (/sec)'
    def print_header2_brief(self):
            print ' KBIn  PktIn  KBOut  PktOut',
    def print_header2_detail(self):
            print '#Num    Name   KBIn  PktIn SizeIn  MultI   CmpI  ErrsI  KBOut PktOut  SizeO   CmpO ErrsO'
    def print_header2_verbose(self):
            print '# KBIn  PktIn SizeIn  MultI   CmpI  ErrsI  KBOut PktOut  SizeO   CmpO  ErrsO'
    def print_brief(self):
        print "%5d %6d %6d %6d" % (
            sum(self.get_metric_value('network.interface.in.bytes')) / 1024,
            sum(self.get_metric_value('network.interface.in.packets')),
            sum(self.get_metric_value('network.interface.out.bytes')) / 1024,
            sum(self.get_metric_value('network.interface.out.packets'))),
    def print_verbose(self):
        self.get_metric_value('network.interface.in.bytes')[0] = 0 # don't include loopback
        self.get_metric_value('network.interface.in.bytes')[1] = 0
        print '%6d %5d %6d %6d %6d %6d %6d %6d %6d %6d %7d' % (
            sum(self.get_metric_value('network.interface.in.bytes')) / 1024,
            sum(self.get_metric_value('network.interface.in.packets')),
            sum(self.get_metric_value('network.interface.in.bytes')) /
            sum(self.get_metric_value('network.interface.in.packets')),
            sum(self.get_metric_value('network.interface.in.mcasts')),
            sum(self.get_metric_value('network.interface.in.compressed')),
            sum(self.get_metric_value('network.interface.in.errors')),
            sum(self.get_metric_value('network.interface.out.bytes')) / 1024,
            sum(self.get_metric_value('network.interface.out.packets')),
            sum(self.get_metric_value('network.interface.out.bytes')) /
            sum(self.get_metric_value('network.interface.out.packets')),
            sum(self.get_metric_value('network.interface.total.mcasts')),
            sum(self.get_metric_value('network.interface.out.errors')))
    def print_detail(self):
        for j in xrange(len(self.get_metric_value('network.interface.in.bytes'))):
            for k in xrange(len(self.metric_name)):
                try:
                    (inst, iname) = pm.pmGetInDom(self.metric_desc[k])
                    break
                except pmErr, e:
                    iname = "X"

            print '%4d %-7s %6d %5d %6d %6d %6d %6d %6d %6d %6d %6d %7d' % (
                j, iname[j],
                self.get_metric_value('network.interface.in.bytes')[j] / 1024,
                self.get_metric_value('network.interface.in.packets')[j],
                self.divide_check (self.get_metric_value('network.interface.in.bytes')[j],
                                   self.get_metric_value('network.interface.in.packets')[j]),
                self.get_metric_value('network.interface.in.mcasts')[j],
                self.get_metric_value('network.interface.in.compressed')[j],
                self.get_metric_value('network.interface.in.errors')[j],
                self.get_metric_value('network.interface.in.packets')[j],
                self.get_metric_value('network.interface.out.packets')[j],
                self.divide_check (self.get_metric_value('network.interface.in.packets')[j],
                                   self.get_metric_value('network.interface.out.packets')[j]) / 1024,
                    self.get_metric_value('network.interface.total.mcasts')[j],
                    self.get_metric_value('network.interface.out.compressed')[j])


# _generic_collect_print --------------------------------------------------


class _generic_collect_print(subsys, _collect_print):
    True


# main ----------------------------------------------------------------------


if __name__ == '__main__':

    n_samples = 0
    i = 1
    subsys = list()
    verbosity = "brief"
    output_file = ""
    input_file = ""
    duration = 0
    interval_arg = 1
    duration_arg = 0

    ss = _generic_collect_print()
    cpu = _cpu_collect_print()
    interrupt = _interrupt_collect_print()
    disk = _disk_collect_print()
    memory = _memory_collect_print()
    net = _net_collect_print()

    s_options = {"d":[disk,"brief"],"D":[disk,"detail"],
                 "c":[cpu,"brief"],"C":[cpu,"detail"],
                 "n":[net,"brief"],"N":[net,"detail"],
                 "j":[interrupt,"brief"],"J":[interrupt,"detail"],
                 "b":[ss,"brief"],"B":[ss,"detail"],
                 "m":[memory,"brief"],"M":[ss,"detail"],
                 "f":[ss,"brief"],"F":[ss,"detail"],
                 "y":[ss,"brief"],"Y":[ss,"detail"],
                 "Z":[ss,"detail"]
                 }

    while i < len(sys.argv):
        if (sys.argv[i] == "-c" or sys.argv[i] == "--count"):
            i += 1
            n_samples = int(sys.argv[i])
        elif (sys.argv[i][:2] == "-c"):
            n_samples = int(sys.argv[i][2:])
        elif (sys.argv[i] == "-f" or sys.argv[i] == "--filename"):
            i += 1
            output_file = sys.argv[i]
        elif (sys.argv[i] == "-p" or sys.argv[i] == "--playback"):
            i += 1
            input_file = sys.argv[i]
        elif (sys.argv[i] == "-R" or sys.argv[i] == "--runtime"):
            i += 1
            duration_arg = sys.argv[i]
        elif (sys.argv[i] == "-i" or sys.argv[i] == "--interval"):
            i += 1
            interval_arg = sys.argv[i]
	# TODO: --subsys XYZ
        elif (sys.argv[i][:2] == "-s"):
            for j in xrange(len(sys.argv[i][2:])):
                subsys_arg = sys.argv[i][j+2:j+3]
                subsys.append(s_options[subsys_arg][0])
                if subsys_arg.isupper():
                    verbosity =  s_options[subsys_arg][1]
        elif (sys.argv[i] == "--verbose"):
            if verbosity != "detail":
                verbosity = "verbose"
        elif (sys.argv[i] == "--help" or sys.argv[i] == "-h"):
            usage()
            sys.exit(1)
        elif (sys.argv[i][:1] == "-"):
            print sys.argv[0] + ": Unknown option ", sys.argv[i]
            print "Try `" + sys.argv[0] + "--help' for more information."
            sys.exit(1)
        i += 1

    if len(subsys) == 0:
        map( lambda x: subsys.append(x) , (cpu, disk, net) )

    if input_file == "":
        try:
            pm = pmContext()
        except pmErr, e:
            print "Cannot connect to pmcd on %s" % "localhost"
            sys.exit(1)
    else:
        # -f saves the metrics in a directory, so get the archive basename
        lol = []
        if not os.path.exists(input_file):
            print input_file, "does not exist"
            sys.exit(1)
        if not os.path.isdir(input_file) or not os.path.exists(input_file + "/pmcollectl.pcp"):
            print input_file, "is not a", me, "playback directory"
            sys.exit(1)
        for line in open(input_file + "/" + me + ".pcp"):
            lol.append(line[:-1].split())
        archive = input_file + "/" + lol[len(lol)-1][2]
        try:
            pm = pmContext(pmapi.PM_CONTEXT_ARCHIVE, archive)
        except pmErr, e:
            print "Cannot open PCP archive: %s" % archive
            sys.exit(1)

    if duration_arg != 0:
        (code, timeval, errmsg) = pm.pmParseInterval(duration_arg)
        if code < 0:
            print errmsg
            sys.exit(1)
        duration = timeval.tv_sec

    (code, delta, errmsg) = pm.pmParseInterval(str(interval_arg) + " seconds")

    if output_file != "":
        configuration = "log mandatory on every " + str(interval_arg) + " seconds { "
        for s in subsys:
            configuration += s.dump_metrics()
        configuration += "}"
        if duration == 0:
            if n_samples != 0:
                duration = n_samples * interval_arg
            else:
                duration = 10 * interval_arg
        record (pm, configuration, duration, output_file)
        record_add_creator (output_file)
        sys.exit(0)

    for s in subsys:
        try:
            s.setup_metrics(pm)
        except:
            if input_file != "":
                args = ""
                for i in sys.argv:
                    args = args + " " + i
                print "Argument mismatch between invocation arguments:\n" + args
                record_check_creator(input_file, "and arguments used to create the playback directory\n ")
                sys.exit(1)
        s.set_verbosity(verbosity)
        s.get_stats(pm)

    # brief headings for different subsystems are concatenated together
    if verbosity == "brief":
        for s in subsys:
            if s == 0: continue
            s.print_header1()
        print
        for s in subsys:
            if s == 0: continue
            s.print_header2()
        print

    n = 0

    host = pm.pmGetContextHostName()
    if host == "localhost":
        host = os.uname()[1]

    try:
        while (n < n_samples) or (n_samples == 0):
            pm.pmtimevalSleep(delta)
            if verbosity != "brief" and len(subsys) > 1:
                print "\n### RECORD %d >>> %s <<< %s ###" % (n+1,  host, time.strftime("%a %b %d %H:%M:%S %Y"))

            for s in subsys:
                if s == 0: continue
                if verbosity != "brief" and (len(subsys) > 1 or n == 0):
                    print
                    s.print_header1()
                    s.print_header2()
                s.get_stats(pm)
                s.get_total()
                s.print_line()
            if verbosity == "brief":
                print

            n += 1
    except KeyboardInterrupt:
        True
