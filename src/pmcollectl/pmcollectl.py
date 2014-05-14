#!/usr/bin/python

#
# pmcollectl.py
#
# Copyright (C) 2012-2013 Red Hat Inc.
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
http://www.performancecopilot.org
"""

# ignore line too long, missing docstring, method could be a function,
#        too many public methods
# pylint: disable=C0301 
# pylint: disable=C0111 
# pylint: disable=R0201
# pylint: disable=R0904

##############################################################################
#
# imports
#

import os
import sys
import time
import cpmapi as c_api
import cpmgui as c_gui
from pcp import pmapi, pmgui
from pcp.pmsubsys import Subsystem

ME = "pmcollectl"

def usage ():
    print "\nUsage:", sys.argv[0], "\n\t[-h HOST] [-sSUBSYS] [-f|--filename FILE] [-p|--playback FILE]"
    print '''\t[-R|--runtime N] [-c|--count N] [-i|--interval N] [--verbose]
'''


# scale  -----------------------------------------------------------------


def scale (value, magnitude):
    return (value + (magnitude / 2)) / magnitude


# record ---------------------------------------------------------------

def record (context, config, interval, path, host):
    
    if os.path.exists(path):
        print ME + "archive %s already exists\n" % path
        sys.exit(1)
    status = context.pmRecordSetup (path, ME, 0) # pylint: disable=W0621
    (status, rhp) = context.pmRecordAddHost (host, 1, config) # just a filename
    status = context.pmRecordControl (0, c_gui.PM_REC_SETARG, "-T" + str(interval) + "sec")
    status = context.pmRecordControl (0, c_gui.PM_REC_ON, "")
    # sleep +1 to make sure pmlogger gets to the -T limit
    time.sleep(interval+1)
    # don't need to do anything else ... pmlogger will stop of it's own
    # once -T limit is reached, or pmcollectl exits, and in particular
    # calling
    # status = context.pmRecordControl (rhp, c_gui.PM_REC_OFF, "")
    # produces a popup dialog we don't want
    #

# record_add_creator ------------------------------------------------------

def record_add_creator (path):
    fdesc = open (path, "a+")
    args = ""
    for i in sys.argv:
        args = args + i + " "
    fdesc.write("# Created by " + args)
    fdesc.write("\n#\n")
    fdesc.close()

# _CollectPrint -------------------------------------------------------


class _CollectPrint(object):
    def __init__(self, ss):
        self.ss = ss
    def print_header1(self):
        if self.verbosity == "brief":
            self.print_header1_brief()
        elif self.verbosity == "detail":
            self.print_header1_detail()
        elif self.verbosity == "verbose":
            self.print_header1_verbose()
	sys.stdout.flush()
    def print_header2(self):
        if self.verbosity == "brief":
            self.print_header2_brief()
        elif self.verbosity == "detail":
            self.print_header2_detail()
        elif self.verbosity == "verbose":
            self.print_header2_verbose()
	sys.stdout.flush()
    def print_header1_brief(self):
        True                        # pylint: disable-msg=W0104
    def print_header2_brief(self):
        True                        # pylint: disable-msg=W0104
    def print_header1_detail(self):
        True                        # pylint: disable-msg=W0104
    def print_header2_detail(self):
        True                        # pylint: disable-msg=W0104
    def print_header1_verbose(self):
        True                        # pylint: disable-msg=W0104
    def print_header2_verbose(self):
        True                        # pylint: disable-msg=W0104
    def print_line(self):
        if self.verbosity == "brief":
            self.print_brief()
        elif self.verbosity == "detail":
            self.print_detail()
        elif self.verbosity == "verbose":
            self.print_verbose()
    def print_brief(self):
        True                        # pylint: disable-msg=W0104
    def print_verbose(self):
        True                        # pylint: disable-msg=W0104
    def print_detail(self):
        True                        # pylint: disable-msg=W0104
    def divide_check(self, dividend, divisor):
        if divisor == 0:
            return 0
        else:
            return dividend / divisor
    def set_verbosity(self, cpverbosity):
        self.verbosity = cpverbosity # pylint: disable-msg=W0201


# _cpuCollectPrint --------------------------------------------------


class _cpuCollectPrint(_CollectPrint):
    def print_header1_brief(self):
        sys.stdout.write('#<--------CPU-------->')
    def print_header1_detail(self):
        print '# SINGLE CPU STATISTICS'
    def print_header1_verbose(self):
        print '# CPU SUMMARY (INTR, CTXSW & PROC /sec)'

    def print_header2_brief(self):
        sys.stdout.write('#cpu sys inter  ctxsw')
    def print_header2_detail(self):
        print '#   Cpu  User Nice  Sys Wait IRQ  Soft Steal Idle'
    def print_header2_verbose(self):
        print '#User  Nice   Sys  Wait   IRQ  Soft Steal  Idle  CPUs  Intr  Ctxsw  Proc  RunQ   Run   Avg1  Avg5 Avg15 RunT BlkT'

    def print_brief(self):
        print "%4d" % (100 * (self.ss.get_metric_value('kernel.all.cpu.nice') +
                              self.ss.get_metric_value('kernel.all.cpu.user') +
                              self.ss.get_metric_value('kernel.all.cpu.intr') +
                              self.ss.get_metric_value('kernel.all.cpu.sys') +
                              self.ss.get_metric_value('kernel.all.cpu.steal') +
                              self.ss.get_metric_value('kernel.all.cpu.irq.hard') +
                              self.ss.get_metric_value('kernel.all.cpu.irq.soft')) /
                       ss.cpu_total),
        print "%3d" % (100 * (self.ss.get_metric_value('kernel.all.cpu.intr') +
                              self.ss.get_metric_value('kernel.all.cpu.sys') +
                              self.ss.get_metric_value('kernel.all.cpu.steal') +
                              self.ss.get_metric_value('kernel.all.cpu.irq.hard') +
                              self.ss.get_metric_value('kernel.all.cpu.irq.soft')) /
                       ss.cpu_total),
        print "%5d %6d" % (self.ss.get_metric_value('kernel.all.intr'),
                           self.ss.get_metric_value('kernel.all.pswitch')),
    def print_detail(self):
        for k in range(self.ss.get_len(self.ss.get_metric_value('kernel.percpu.cpu.user'))):
            print "    %3d  %4d %4d  %3d %4d %3d  %4d %5d %4d" % (
                k,
                (100 * (self.ss.get_scalar_value('kernel.percpu.cpu.nice',k) +
                        self.ss.get_scalar_value('kernel.percpu.cpu.user',k) +
                        self.ss.get_scalar_value('kernel.percpu.cpu.intr',k) +
                        self.ss.get_scalar_value('kernel.percpu.cpu.sys',k) +
                        self.ss.get_scalar_value('kernel.percpu.cpu.steal',k) +
                        self.ss.get_scalar_value('kernel.percpu.cpu.irq.hard',k) +
                        self.ss.get_scalar_value('kernel.percpu.cpu.irq.soft',k)) /
                ss.cpu_total),
            self.ss.get_scalar_value('kernel.percpu.cpu.nice',k),
            (100 * (self.ss.get_scalar_value('kernel.percpu.cpu.intr',k) +
                    self.ss.get_scalar_value('kernel.percpu.cpu.sys',k) +
                    self.ss.get_scalar_value('kernel.percpu.cpu.steal',k) +
                    self.ss.get_scalar_value('kernel.percpu.cpu.irq.hard',k) +
                    self.ss.get_scalar_value('kernel.percpu.cpu.irq.soft',k)) /
             ss.cpu_total),
            self.ss.get_scalar_value('kernel.percpu.cpu.wait.total',k),
            self.ss.get_scalar_value('kernel.percpu.cpu.irq.hard',k),
            self.ss.get_scalar_value('kernel.percpu.cpu.irq.soft',k),
            self.ss.get_scalar_value('kernel.percpu.cpu.steal',k),
            self.ss.get_scalar_value('kernel.percpu.cpu.idle',k) / 10)
    def print_verbose(self):
        ncpu = self.ss.get_metric_value('hinv.ncpu')
        print "%4d %6d %5d %4d %4d %5d " % (
            (100 * (self.ss.get_metric_value('kernel.all.cpu.nice') +
                    self.ss.get_metric_value('kernel.all.cpu.user') +
                    self.ss.get_metric_value('kernel.all.cpu.intr') +
                    self.ss.get_metric_value('kernel.all.cpu.sys') +
                    self.ss.get_metric_value('kernel.all.cpu.steal') +
                    self.ss.get_metric_value('kernel.all.cpu.irq.hard') +
                    self.ss.get_metric_value('kernel.all.cpu.irq.soft')) /
             ss.cpu_total),
            self.ss.get_metric_value('kernel.all.cpu.nice'),
            (100 * (self.ss.get_metric_value('kernel.all.cpu.intr') +
                    self.ss.get_metric_value('kernel.all.cpu.sys') +
                    self.ss.get_metric_value('kernel.all.cpu.steal') +
                    self.ss.get_metric_value('kernel.all.cpu.irq.hard') +
                    self.ss.get_metric_value('kernel.all.cpu.irq.soft')) /
             ss.cpu_total),
            self.ss.get_metric_value('kernel.all.cpu.wait.total'),
            self.ss.get_metric_value('kernel.all.cpu.irq.hard'),
            self.ss.get_metric_value('kernel.all.cpu.irq.soft')
            ),
        print "%6d %6d %5d %5d %6d" % (
            self.ss.get_metric_value('kernel.all.cpu.steal'),
            self.ss.get_metric_value('kernel.all.cpu.idle') / (10 * ncpu),
            ncpu,
            self.ss.get_metric_value('kernel.all.intr'),
            self.ss.get_metric_value('kernel.all.pswitch')
            ),
        print "%5d %5d %5d %5.2f %5.2f %5.2f %4d %4d" % (
            self.ss.get_metric_value('kernel.all.nprocs'),
            self.ss.get_metric_value('kernel.all.runnable'),
            self.ss.get_metric_value('proc.runq.runnable'),
            self.ss.get_metric_value('kernel.all.load')[0],
            self.ss.get_metric_value('kernel.all.load')[1],
            self.ss.get_metric_value('kernel.all.load')[2],
            self.ss.get_metric_value('kernel.all.runnable'),
            self.ss.get_metric_value('proc.runq.blocked'))


# _interruptCollectPrint ---------------------------------------------


class _interruptCollectPrint(_CollectPrint):
    def print_header1_brief(self):
        ndashes = (((self.ss.get_len(self.ss.get_metric_value('kernel.percpu.interrupts.THR'))) * 6) - 6) / 2
        hdr = "#<"
        for k in range(ndashes):
            hdr += "-"
        hdr += "Int"
        for k in range(ndashes):
            hdr += "-"
        hdr += ">"
        print hdr,
    def print_header1_detail(self):
        print '# INTERRUPT DETAILS'
        print '# Int    ',
        for k in range(self.ss.get_len(self.ss.get_metric_value('kernel.percpu.interrupts.THR'))):
            print 'Cpu%d ' % k,
        print 'Type            Device(s)'
    def print_header1_verbose(self):
        print '# INTERRUPT SUMMARY'
    def print_header2_brief(self):
        for k in range(self.ss.get_len(self.ss.get_metric_value('kernel.percpu.interrupts.THR'))):
            if k == 0:
                print '#Cpu%d ' % k,
            else:
                print 'Cpu%d ' % k,
    def print_header2_verbose(self):
        print '#    ',
        for k in range(self.ss.get_len(self.ss.get_metric_value('kernel.percpu.interrupts.THR'))):
            print 'Cpu%d ' % k,
        print
    def print_brief(self):
        int_count = []
        for k in range(self.ss.get_len(self.ss.get_metric_value('kernel.percpu.interrupts.THR'))):
            int_count.append(0)
            for j in ss.metrics:
                if j[0:24] == 'kernel.percpu.interrupts':
                    int_count[k] += self.ss.get_scalar_value(self.ss.metrics_dict[j], k)
                
        for k in range(self.ss.get_len(self.ss.get_metric_value('kernel.percpu.interrupts.THR'))):
            print "%4d " % (int_count[k]),
    def print_detail(self):
        for j in ss.metrics:
            if j[0:24] != 'kernel.percpu.interrupts':
                continue
            j_i = self.ss.metrics_dict[j]
            have_nonzero_value = False
            for k in range(self.ss.get_len(self.ss.get_metric_value('kernel.percpu.interrupts.THR'))):
                if self.ss.get_scalar_value(j_i, k) != 0:
                    have_nonzero_value = True
                if not have_nonzero_value:
                    continue
            if have_nonzero_value:
                # pcp does not give the interrupt # so print spaces
                print "%-8s" % self.ss.metrics[j_i].split(".")[3],
                for i in range(self.ss.get_len(self.ss.get_metric_value('kernel.percpu.interrupts.THR'))):
                    print "%4d " % (self.ss.get_scalar_value(j_i, i)),
                text = (pm.pmLookupText(self.ss.metric_pmids[j_i], c_api.PM_TEXT_ONELINE))
                print "%-18s %s" % (text[:(str.index(text," "))],
                                 text[(str.index(text," ")):])
    def print_verbose(self):
        print "     ",
        self.print_brief()
        print


# _diskCollectPrint --------------------------------------------------


class _diskCollectPrint(_CollectPrint):
    def print_header1_brief(self):
        sys.stdout.write('<----------Disks----------->')
    def print_header1_detail(self):
        print '# DISK STATISTICS (/sec)'
    def print_header1_verbose(self):
        print '\n\n# DISK SUMMARY (/sec)'
    def print_header2_brief(self):
        sys.stdout.write(' KBRead  Reads KBWrit Writes')
    def print_header2_detail(self):
        print '#          <---------reads---------><---------writes---------><--------averages--------> Pct'
        print '#Name       KBytes Merged  IOs Size  KBytes Merged  IOs Size  RWSize  QLen  Wait SvcTim Util'
    def print_header2_verbose(self):
        sys.stdout.write('#KBRead RMerged  Reads SizeKB  KBWrite WMerged Writes SizeKB\n')
    def print_brief(self):
        sys.stdout.write("%6d %6d %6d %6d" % (
            self.ss.get_metric_value('disk.all.read_bytes'),
            self.ss.get_metric_value('disk.all.read'),
            self.ss.get_metric_value('disk.all.write_bytes'),
            self.ss.get_metric_value('disk.all.write')))
    def print_detail(self):
        for j in xrange(len(self.ss.metric_pmids)):
            try:
		if (self.ss.metrics[j] == 'disk.dev.read'):
		    (inst, iname) = pm.pmGetInDom(self.ss.metric_descs[j])
		    break
            except pmapi.pmErr, e:
                iname = "X"

        # metric values may be scalars or arrays depending on # of disks
        for j in xrange(len(iname)):
            print "%-10s %6d %6d %4d %4d  %6d %6d %4d %4d  %6d %6d %4d %6d %4d" % (
                iname[j],
                self.ss.get_scalar_value ('disk.dev.read_bytes', j),
                self.ss.get_scalar_value ('disk.dev.read_merge', j),
                self.ss.get_scalar_value ('disk.dev.read', j),
                self.ss.get_scalar_value ('disk.dev.blkread', j),
                self.ss.get_scalar_value ('disk.dev.write_bytes', j),
                self.ss.get_scalar_value ('disk.dev.write_merge', j),
                self.ss.get_scalar_value ('disk.dev.write', j),
                self.ss.get_scalar_value ('disk.dev.blkwrite', j),
                0, 0, 0, 0, 0)
# ??? replace 0 with required fields

    def print_verbose(self):
        avgrdsz = avgwrsz = 0
        if self.ss.get_metric_value('disk.all.read') > 0:
            avgrdsz = self.ss.get_metric_value('disk.all.read_bytes')
            avgrdsz /= self.ss.get_metric_value('disk.all.read')
        if self.ss.get_metric_value('disk.all.write') > 0:
            avgwrsz = self.ss.get_metric_value('disk.all.write_bytes')
            avgwrsz /= self.ss.get_metric_value('disk.all.write')

        print '%6d %6d %6d %6d %7d %8d %6d %6d' % (
            avgrdsz,
            self.ss.get_metric_value('disk.all.read_merge'),
            self.ss.get_metric_value('disk.all.read'),
            0,
            avgwrsz,
            self.ss.get_metric_value('disk.all.write_merge'),
            self.ss.get_metric_value('disk.all.write'),
            0)


# _memoryCollectPrint ------------------------------------------------


class _memoryCollectPrint(_CollectPrint):
    def print_header1_brief(self):
        sys.stdout.write('#<-----------Memory----------->')
    def print_header1_verbose(self):
        print '# MEMORY SUMMARY'
    def print_header2_brief(self):
        print '#Free Buff Cach Inac Slab  Map'
    def print_header2_verbose(self): 
        print '#<-------------------------------Physical Memory--------------------------------------><-----------Swap------------><-------Paging------>'
        print '#   Total    Used    Free    Buff  Cached    Slab  Mapped    Anon  Commit  Locked Inact Total  Used  Free   In  Out Fault MajFt   In  Out'
    def print_brief(self):
        print "%4dM %3dM %3dM %3dM %3dM %3dM " % (
            scale(self.ss.get_metric_value('mem.freemem'), 1000),
            scale(self.ss.get_metric_value('mem.util.bufmem'), 1000),
            scale(self.ss.get_metric_value('mem.util.cached'), 1000),
            scale(self.ss.get_metric_value('mem.util.inactive'), 1000),
            scale(self.ss.get_metric_value('mem.util.slab'), 1000),
            scale(self.ss.get_metric_value('mem.util.mapped'), 1000)),
    def print_verbose(self):
        print "%8dM %6dM %6dM %6dM %6dM %6dM %6dM %6dM %6dM %6dM %5dM %5dM %5dM %5dM %6d %6d %6d %6d %6d %6d " % (
            scale(self.ss.get_metric_value('mem.physmem'), 1000),
            scale(self.ss.get_metric_value('mem.util.used'), 1000),
            scale(self.ss.get_metric_value('mem.freemem'), 1000),
            scale(self.ss.get_metric_value('mem.util.bufmem'), 1000),
            scale(self.ss.get_metric_value('mem.util.cached'), 1000),
            scale(self.ss.get_metric_value('mem.util.slab'), 1000),
            scale(self.ss.get_metric_value('mem.util.mapped'), 1000),
            scale(self.ss.get_metric_value('mem.util.anonpages'), 1000),
            scale(self.ss.get_metric_value('mem.util.committed_AS'), 1000),
            scale(self.ss.get_metric_value('mem.util.mlocked'), 1000),
            scale(self.ss.get_metric_value('mem.util.inactive'), 1000),
            scale(self.ss.get_metric_value('mem.util.swapTotal'), 1000),
            scale(self.ss.get_metric_value('swap.used'), 1000),
            scale(self.ss.get_metric_value('swap.free'), 1000),
            scale(self.ss.get_metric_value('swap.pagesin'), 1000),
            scale(self.ss.get_metric_value('swap.pagesout'), 1000),
            scale(self.ss.get_metric_value('mem.vmstat.pgfault') -
                  self.ss.get_metric_value('mem.vmstat.pgmajfault'), 1000),
            scale(self.ss.get_metric_value('mem.vmstat.pgmajfault'), 1000),
            scale(self.ss.get_metric_value('mem.vmstat.pgpgin'), 1000),
            scale(self.ss.get_metric_value('mem.vmstat.pgpgout'), 1000))


# _netCollectPrint --------------------------------------------------


class _netCollectPrint(_CollectPrint):
    def print_header1_brief(self):
        sys.stdout.write('<----------Network---------->')
    def print_header1_detail(self):
        print '# NETWORK STATISTICS (/sec)'
    def print_header1_verbose(self):
        print '\n\n# NETWORK SUMMARY (/sec)'
    def print_header2_brief(self):
        sys.stdout.write(' KBIn  PktIn  KBOut  PktOut')
    def print_header2_detail(self):
        print '#Num    Name   KBIn  PktIn SizeIn  MultI   CmpI  ErrsI  KBOut PktOut  SizeO   CmpO ErrsO'
    def print_header2_verbose(self):
        print '# KBIn  PktIn SizeIn  MultI   CmpI  ErrsI  KBOut PktOut  SizeO   CmpO  ErrsO'
    def print_brief(self):
        print "%5d %6d %6d %6d" % (
            sum(self.ss.get_metric_value('network.interface.in.bytes')) / 1024,
            sum(self.ss.get_metric_value('network.interface.in.packets')),
            sum(self.ss.get_metric_value('network.interface.out.bytes')) / 1024,
            sum(self.ss.get_metric_value('network.interface.out.packets'))),
    def average_packet_size(self, bytes, packets):
        # calculate mean packet size safely (note that divisor may be zero)
        result = 0
        bin = sum(self.ss.get_metric_value('network.interface.' + bytes))
        pin = sum(self.ss.get_metric_value('network.interface.' + packets))
        if pin > 0:
           result = bin / pin
        return result
    def print_verbose(self):
        # don't include loopback; TODO: pmDelProfile would be more appropriate
        self.ss.get_metric_value('network.interface.in.bytes')[0] = 0
        self.ss.get_metric_value('network.interface.out.bytes')[0] = 0
        print '%6d %5d %6d %6d %6d %6d %6d %6d %6d %6d %7d' % (
            sum(self.ss.get_metric_value('network.interface.in.bytes')) / 1024,
            sum(self.ss.get_metric_value('network.interface.in.packets')),
            self.average_packet_size('in.bytes', 'in.packets'),
            sum(self.ss.get_metric_value('network.interface.in.mcasts')),
            sum(self.ss.get_metric_value('network.interface.in.compressed')),
            sum(self.ss.get_metric_value('network.interface.in.errors')),
            sum(self.ss.get_metric_value('network.interface.out.bytes')) / 1024,
            sum(self.ss.get_metric_value('network.interface.out.packets')),
            self.average_packet_size('out.bytes', 'out.packets'),
            sum(self.ss.get_metric_value('network.interface.total.mcasts')),
            sum(self.ss.get_metric_value('network.interface.out.errors')))
    def print_detail(self):
        for j in xrange(len(self.ss.metric_pmids)):
            try:
		if (self.ss.metrics[j] == 'network.interface.in.bytes'):
		    (inst, iname) = pm.pmGetInDom(self.ss.metric_descs[j])
		    break
            except pmapi.pmErr, e: # pylint: disable-msg=C0103
                iname = "X"

        for j in xrange(len(iname)):
            print '%4d %-7s %6d %5d %6d %6d %6d %6d %6d %6d %6d %6d %7d' % (
                j, iname[j],
                self.ss.get_metric_value('network.interface.in.bytes')[j] / 1024,
                self.ss.get_metric_value('network.interface.in.packets')[j],
                self.divide_check (self.ss.get_metric_value('network.interface.in.bytes')[j],
                                   self.ss.get_metric_value('network.interface.in.packets')[j]),
                self.ss.get_metric_value('network.interface.in.mcasts')[j],
                self.ss.get_metric_value('network.interface.in.compressed')[j],
                self.ss.get_metric_value('network.interface.in.errors')[j],
                self.ss.get_metric_value('network.interface.in.packets')[j],
                self.ss.get_metric_value('network.interface.out.packets')[j],
                self.divide_check (self.ss.get_metric_value('network.interface.in.packets')[j],
                self.ss.get_metric_value('network.interface.out.packets')[j]) / 1024,
                    self.ss.get_metric_value('network.interface.total.mcasts')[j],
                    self.ss.get_metric_value(
                    'network.interface.out.compressed')[j])


# main -----------------------------------------------------------------


# ignore These are actually global names; ignore invalid name warning for now
# TODO move main into a def and enable
# pylint: disable-msg=C0103


if __name__ == '__main__':
    n_samples = 0
    subsys = list()
    verbosity = "brief"
    output_file = ""
    input_file = ""
    duration = 0
    interval_arg = 1
    duration_arg = 0
    create_archive = False
    replay_archive = False
    host = ""

    ss = Subsystem()
    ss.init_processor_metrics()
    ss.init_interrupt_metrics()
    ss.init_disk_metrics()
    ss.init_memory_metrics()
    ss.init_network_metrics()

    cpu = _cpuCollectPrint(ss)
    interrupt = _interruptCollectPrint(ss)
    disk = _diskCollectPrint(ss)
    memory = _memoryCollectPrint(ss)
    net = _netCollectPrint(ss)

    s_options = {"d":[disk, "brief"], "D":[disk, "detail"],
                 "c":[cpu, "brief"], "C":[cpu, "detail"],
                 "n":[net, "brief"], "N":[net, "detail"],
                 "j":[interrupt, "brief"], "J":[interrupt, "detail"],
#                 "b":[ss, "brief"], # "B":[ss, "detail"],
                 "m":[memory, "brief"], # "M":[ss, "detail"],
#                 "f":[ss, "brief"], "F":[ss, "detail"],
#                 "y":[ss, "brief"], "Y":[ss, "detail"],
#                 "z":[ss, "detail"], "Z":[ss, "detail"]
                 }

    argx = 1
    while argx < len(sys.argv):
        if (sys.argv[argx] == "-c" or sys.argv[argx] == "--count"):
            argx += 1
            n_samples = int(sys.argv[argx])
        elif (sys.argv[argx][:2] == "-c"):
            n_samples = int(sys.argv[argx][2:])
        elif (sys.argv[argx] == "-f" or sys.argv[argx] == "--filename"):
            argx += 1
            output_file = sys.argv[argx]
            create_archive = True
        elif (sys.argv[argx] == "-p" or sys.argv[argx] == "--playback"):
            argx += 1
            input_file = sys.argv[argx]
            replay_archive = True
        elif (sys.argv[argx] == "-R" or sys.argv[argx] == "--runtime"):
            argx += 1
            duration_arg = sys.argv[argx]
        elif (sys.argv[argx] == "-i" or sys.argv[argx] == "--interval"):
            argx += 1
            interval_arg = sys.argv[argx]
	# TODO: --subsys XYZ
        elif (sys.argv[argx][:2] == "-s"):
            for ssx in xrange(len(sys.argv[argx][2:])):
                subsys_arg = sys.argv[argx][ssx+2:ssx+3]
                try:
                    subsys.append(s_options[subsys_arg][0])
                except KeyError:
                    print sys.argv[0] + \
                    ": Unimplemented subsystem -s" + subsys_arg
                    sys.exit(1)
                if subsys_arg.isupper():
                    verbosity =  s_options[subsys_arg][1]
        elif (sys.argv[argx] == "--verbose"):
            if verbosity != "detail":
                verbosity = "verbose"
        elif (sys.argv[argx] == "--help"):
            usage()
            sys.exit(1)
        elif (sys.argv[argx] == "-h"):
            argx += 1
            host = sys.argv[argx]
        elif (sys.argv[argx][:1] == "-"):
            print sys.argv[0] + ": Unknown option ", sys.argv[argx]
            print "Try `" + sys.argv[0] + " --help' for more information."
            sys.exit(1)
        argx += 1

    if len(subsys) == 0:
        if create_archive:
            map( subsys.append, (cpu, disk, net, interrupt, memory))
        else:
            map( subsys.append, (cpu, disk, net) )

    if replay_archive:
        archive = input_file
        if not os.path.exists(input_file):
            print input_file, "does not exist"
            sys.exit(1)
        for line in open(input_file):
            if (line[:8] == "Archive:"):
                tokens = line[:-1].split()
                archive = os.path.join(os.path.dirname(input_file), tokens[2])
        try:
            pm = pmapi.pmContext(c_api.PM_CONTEXT_ARCHIVE, archive)
        except pmapi.pmErr, e:
            print "Cannot open PCP archive: %s" % archive
            sys.exit(1)
    else:
        if host == "":
            host = "local:"
        try:
            pm = pmapi.pmContext(target=host)
        except pmapi.pmErr, e:
            print "Cannot connect to pmcd on " + host
            sys.exit(1)

    # Find server-side pmcd host-name
    host = pm.pmGetContextHostName()

    if duration_arg != 0:
        (code, timeval, errmsg) = pm.pmParseInterval(duration_arg)
        if code < 0:
            print errmsg
            sys.exit(1)
        duration = timeval.tv_sec

    (delta, errmsg) = pm.pmParseInterval(str(interval_arg) + " seconds")

    if create_archive:
        configuration = "log mandatory on every " + \
            str(interval_arg) + " seconds { "
        configuration += ss.dump_metrics()
        configuration += "}"
        if duration == 0:
            if n_samples != 0:
                duration = n_samples * interval_arg
            else:
                duration = 10 * interval_arg
        record (pmgui.GuiClient(), configuration, duration, output_file, host)
        record_add_creator (output_file)
        sys.exit(0)

    try:
        ss.setup_metrics(pm)
        ss.get_stats(pm)
    except pmapi.pmErr, e:
        if replay_archive:
            import textwrap
            print str(cpu.metrics)
            print "One of the following metrics is required " + \
                "but absent in " + input_file + "\n" + \
                textwrap.fill(str(metrics))
        else:
            print "unable to setup metrics"
            sys.exit(1)

    for ssx in subsys:
        ssx.set_verbosity(verbosity)

    # brief headings for different subsystems are concatenated together
    if verbosity == "brief":
        for ssx in subsys:
            if ssx == 0:
                continue
            ssx.print_header1()
        print
        for ssx in subsys:
            if ssx == 0:
                continue
            ssx.print_header2()
        print

    try:
        i_samples = 0
        while (i_samples < n_samples) or (n_samples == 0):
            pm.pmtimevalSleep(delta)
            if verbosity != "brief" and len(subsys) > 1:
                print "\n### RECORD %d >>> %s <<< %s ###" % \
                (i_samples+1,  host, time.strftime("%a %b %d %H:%M:%S %Y"))

            try:
                ss.get_stats(pm)
                ss.get_total()
            except pmapi.pmErr, e:
                if str(e).find("PM_ERR_EOL") != -1:
                    print str(e)
                    sys.exit(1)
            for ssx in subsys:
                if ssx == 0:
                    continue
                if verbosity != "brief" and (len(subsys) > 1 or i_samples == 0):
                    print
                    ssx.print_header1()
                    ssx.print_header2()
                ssx.print_line()
            if verbosity == "brief":
                print

            i_samples += 1
    except KeyboardInterrupt:
        True                        # pylint: disable-msg=W0104
