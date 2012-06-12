
##############################################################################
#
# pm-collectl.py
#
# Copyright (C) 2012 Red Hat Inc.
#
# This file is part of pcp, the python extensions for SGI's Performance
# Co-Pilot. Pcp is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Pcp is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
# more details. You should have received a copy of the GNU Lesser General
# Public License along with pcp. If not, see <http://www.gnu.org/licenses/>.
#

"""System status collector using the libpcp Wrapper module

Additional Information:

Performance Co-Pilot Web Site
http://oss.sgi.com/projects/pcp

Performance Co-Pilot Programmer's Guide
SGI Document 007-3434-005
http://techpubs.sgi.com
cf. Chapter 3. PMAPI - The Performance Metrics API
"""


##############################################################################
#
# imports
#

import unittest
import pmapi
import time
import sys
import argparse
import copy
from pcp import *
from ctypes import *

def check_code (code):
    if (code < 0):
        print pmErrStr(code)
        sys.exit(1)


metric_type= {
    'hinv.ncpu' : 'absolute',
    'kernel.all.runnable' : 'absolute',
    'proc.runq.runnable' : 'absolute',
    'proc.runq.blocked' : 'absolute',
    'kernel.all.load' : 'absolute'
    }


# get_atom_value  -----------------------------------------------------------


def get_atom_value (metric, atom1, atom2, type, first):
    if metric in metric_type:
        want_diff = False
    else:
        want_diff = True
    if type == pmapi.PM_TYPE_32:
        return atom1.l - (atom2.l if not first else 0)
    elif type == pmapi.PM_TYPE_U32:
        return atom1.ul - (atom2.ul if not first else 0)
    elif type == pmapi.PM_TYPE_64:
        return atom1.ll - (atom2.ll if not first else 0)
    elif type == pmapi.PM_TYPE_U64:
        return atom1.ull - (atom2.ull if not first else 0)
    elif type == pmapi.PM_TYPE_FLOAT:
        return atom1.f - (atom2.f if not first else 0)
    elif type == pmapi.PM_TYPE_DOUBLE:
        return atom1.d - (atom2.d if not first else 0)
    else:
        return 0


# get_stats  -----------------------------------------------------------------


def get_stats (metric, metric_name, metric_desc, metric_value, old_metric_value):
    
    global metric_type
    list_type = type([])

    (code, metric_result) = pm.pmFetch(metric_name)
    check_code (code)

    first = True if max(old_metric_value) == 0 else False
    for i in xrange(len(metric)):
        for j in xrange(metric_result.contents.numpmid):
            if (metric_result.contents.get_pmid(j) != metric_name[i]):
                continue
            atomlist = []
            for k in xrange(metric_result.contents.get_numval(j)):
                (code, atom) = pm.pmExtractValue(metric_result.contents.get_valfmt(j), metric_result.contents.get_vlist(j, k), metric_desc[j].contents.type, metric_desc[j].contents.type)
                atomlist.append(atom)

            value = []
            for k in xrange(metric_result.contents.get_numval(j)):
                if first:
                    old_val = 0
                elif type(old_metric_value[j]) == list_type:
                    old_val = old_metric_value[j][k]
                else:
                    old_val = old_metric_value[j]
                value.append (get_atom_value(metric[i], atomlist[k], old_val, metric_desc[j].contents.type, first))

            old_metric_value[j] = copy.copy(atomlist)
            if metric_result.contents.get_numval(j) == 1:
                metric_value[j] = copy.copy(value[0]) if len(value) == 1 else 0
            elif metric_result.contents.get_numval(j) > 1:
                metric_value[j] = copy.copy(value)


# _subsys ---------------------------------------------------------------


class _subsys(object):
    def get_stats(self):
        True
    def get_total(self):
        True
    def print_brief_header1(self):
        True
    def print_brief_header2(self):
        True
    def print_brief(self):
        True
    def print_verbose(self):
        True


# _cpu  -----------------------------------------------------------------


class _cpu(_subsys):
    def __init__(self):
        self.cpu_metrics = ['kernel.all.cpu.nice', 'kernel.all.cpu.user', 'kernel.all.cpu.intr', 'kernel.all.cpu.sys', 'kernel.all.cpu.idle', 'kernel.all.cpu.steal', 'kernel.all.cpu.irq.hard', 'kernel.all.cpu.irq.soft', 'kernel.all.cpu.wait.total', 'hinv.ncpu']
        self.cpu_metric_value = [0 for i in range(len(self.cpu_metrics))]
        self.old_cpu_metric_value = [0 for i in range(len(self.cpu_metrics))]
        (code, self.cpu_metric_name) = pm.pmLookupName(self.cpu_metrics)
        check_code (code)
        (code, self.cpu_metric_desc) = pm.pmLookupDesc(self.cpu_metric_name)
        check_code (code)
    def get_stats(self):
        get_stats (self.cpu_metrics, self.cpu_metric_name, self.cpu_metric_desc, self.cpu_metric_value, self.old_cpu_metric_value)
    def get_total(self):
        self.cpu_total = (self.cpu_metric_value[0]+self.cpu_metric_value[1]+self.cpu_metric_value[2]+self.cpu_metric_value[3]+self.cpu_metric_value[4]+self.cpu_metric_value[5]+self.cpu_metric_value[6]+self.cpu_metric_value[7] )
    def print_brief_header1(self):
        print '#<--------CPU-------->',
    def print_brief_header2(self):
        print '#cpu sys inter  ctxsw',
    def print_brief(self):
        print "%4d" % (100*(self.cpu_metric_value[0]+self.cpu_metric_value[1]+self.cpu_metric_value[2]+self.cpu_metric_value[3]+self.cpu_metric_value[5]+self.cpu_metric_value[6]+self.cpu_metric_value[7]) / self.cpu_total),
        print "%4d" % (100*(self.cpu_metric_value[2]+self.cpu_metric_value[3]+self.cpu_metric_value[5]+self.cpu_metric_value[6]+self.cpu_metric_value[7]) / self.cpu_total),
    def print_verbose(self):
        print '# CPU SUMMARY (INTR, CTXSW & PROC /sec)'
        print '#User  Nice   Sys  Wait   IRQ  Soft Steal  Idle  CPUs  Intr  Ctxsw  Proc  RunQ   Run   Avg1  Avg5 Avg15 RunT BlkT'
        print "%4d %6d %5d %4d %4d %5d %6d %6d %5d" % ((100*(self.cpu_metric_value[0]+self.cpu_metric_value[1]+self.cpu_metric_value[2]+self.cpu_metric_value[3]+self.cpu_metric_value[5]+self.cpu_metric_value[6]+self.cpu_metric_value[7]) / self.cpu_total), self.cpu_metric_value[0], (100*(self.cpu_metric_value[2]+self.cpu_metric_value[3]+self.cpu_metric_value[5]+self.cpu_metric_value[6]+self.cpu_metric_value[7]) / self.cpu_total), self.cpu_metric_value[8], self.cpu_metric_value[6], self.cpu_metric_value[7], self.cpu_metric_value[5], self.cpu_metric_value[4], self.cpu_metric_value[9]),


# _interrupt  -----------------------------------------------------------------


class _interrupt(_subsys):
    def __init__(self):
        self.interrupt_metrics = ['kernel.all.intr', 'kernel.all.pswitch']
        self.interrupt_metric_value = [0 for i in range(len(self.interrupt_metrics))]
        self.old_interrupt_metric_value = [0 for i in range(len(self.interrupt_metrics))]
        (code, self.int_metric_name) = pm.pmLookupName(self.interrupt_metrics)
        check_code (code)
        (code, self.int_metric_desc) = pm.pmLookupDesc(self.int_metric_name)
        check_code (code)
    def get_stats(self):
        get_stats (self.interrupt_metrics, self.int_metric_name, self.int_metric_desc, self.interrupt_metric_value, self.old_interrupt_metric_value)
    def print_brief(self):
        print "%4d %6d" % (self.interrupt_metric_value[0], self.interrupt_metric_value[1]),


# _process  -----------------------------------------------------------------


class _process(_subsys):
    def __init__(self):
        self.process_metrics = ['kernel.all.nprocs', 'kernel.all.runnable', 'proc.runq.runnable', 'kernel.all.load', 'proc.runq.blocked']
        self.process_metric_value = [0 for i in range(len(self.process_metrics))]
        self.old_process_metric_value = [0 for i in range(len(self.process_metrics))]
        (code, self.process_metric_name) = pm.pmLookupName(self.process_metrics)
        check_code (code)
        (code, self.process_metric_desc) = pm.pmLookupDesc(self.process_metric_name)
        check_code (code)
    def get_stats(self):
        get_stats (self.process_metrics, self.process_metric_name, self.process_metric_desc, self.process_metric_value, self.old_process_metric_value)
    def print_verbose(self):
        print "%6d %5d %5d %5.2f %5.2f %5.2f %4d %4d" % (self.process_metric_value[0], self.process_metric_value[1], self.process_metric_value[2], self.process_metric_value[3][0],self.process_metric_value[3][1],self.process_metric_value[3][2], self.process_metric_value[1], self.process_metric_value[4]),


# _disk  -----------------------------------------------------------------


class _disk(_subsys):
    def __init__(self):
        self.disk_metrics = ['disk.all.read_bytes', 'disk.all.read', 'disk.all.write_bytes', 'disk.all.write', 'disk.all.read_merge', 'disk.all.write_merge']
        self.disk_metric_value = [0 for i in range(len(self.disk_metrics))]
        self.old_disk_metric_value = [0 for i in range(len(self.disk_metrics))]
        (code, self.disk_metric_name) = pm.pmLookupName(self.disk_metrics)
        check_code (code)
        (code, self.disk_metric_desc) = pm.pmLookupDesc(self.disk_metric_name)
        check_code (code)
    def get_stats(self):
        get_stats (self.disk_metrics, self.disk_metric_name, self.disk_metric_desc, self.disk_metric_value, self.old_disk_metric_value)
    def print_brief_header1(self):
        print '<----------Disks----------->',
    def print_brief_header2(self):
        print 'KBRead  Reads KBWrit Writes',
    def print_brief(self):
        print "%4d %6d %6d %6d" % (self.disk_metric_value[0], self.disk_metric_value[1], self.disk_metric_value[2], self.disk_metric_value[3]),
    def print_verbose(self):
        print '\n\n# DISK SUMMARY (/sec)'
        print '#KBRead RMerged  Reads SizeKB  KBWrite WMerged Writes SizeKB\n'
        print '%6d %6d %6d %6d %7d %8d %6d %6d' % (0 if self.disk_metric_value[0] == 0 else self.disk_metric_value[0]/self.disk_metric_value[1], self.disk_metric_value[4], self.disk_metric_value[1], 0, 0 if self.disk_metric_value[3] == 0 else self.disk_metric_value[2]/self.disk_metric_value[3], self.disk_metric_value[5], self.disk_metric_value[3], 0)


# _net  -----------------------------------------------------------------


class _net(_subsys):
    def __init__(self):
        self.net_metrics = ['network.interface.in.bytes', 'network.interface.in.packets', 'network.interface.out.bytes', 'network.interface.out.packets', 'network.interface.in.mcasts', 'network.interface.total.mcasts', 'network.interface.in.compressed', 'network.interface.out.compressed', 'network.interface.in.errors', 'network.interface.out.errors']
        self.net_metric_value = [0 for i in range(len(self.net_metrics))]
        self.old_net_metric_value = [0 for i in range(len(self.net_metrics))]
        (code, self.net_metric_name) = pm.pmLookupName(self.net_metrics)
        check_code (code)
        (code, self.net_metric_desc) = pm.pmLookupDesc(self.net_metric_name)
        check_code (code)
    def get_stats(self):
        get_stats (self.net_metrics, self.net_metric_name, self.net_metric_desc, self.net_metric_value, self.old_net_metric_value)
    def print_brief_header1(self):
        print '<----------Network--------->',
    def print_brief_header2(self):
        print ' KBIn  PktIn  KBOut  PktOut',
    def print_brief(self):
        print "%8d %6d %6d %6d" % (sum(self.net_metric_value[0]), sum(self.net_metric_value[1]), sum(self.net_metric_value[2]), sum(self.net_metric_value[3])),
    def print_verbose(self):
        print '\n\n# NETWORK SUMMARY (/sec)'
        print '# KBIn  PktIn SizeIn  MultI   CmpI  ErrsI  KBOut PktOut  SizeO   CmpO  ErrsO'
        self.net_metric_value[0][0] = 0 # don't include loopback
        self.net_metric_value[0][1] = 0
        print '%6d %5d %6d %6d %6d %6d %6d %6d %6d %6d %7d' % (sum(self.net_metric_value[0]) / 1000, sum(self.net_metric_value[1]), sum(self.net_metric_value[0]) / sum(self.net_metric_value[1]), sum(self.net_metric_value[4]), sum(self.net_metric_value[6]), sum(self.net_metric_value[8]),sum(self.net_metric_value[2]), sum(self.net_metric_value[3]), sum(self.net_metric_value[2]) / sum(self.net_metric_value[3]) / 1000, sum(self.net_metric_value[5]), sum(self.net_metric_value[7]))


# main ----------------------------------------------------------------------


if __name__ == '__main__':

    n_samples = 1
    i = 1
    options = set()
    verbose = False

    while i < len(sys.argv):
        if (sys.argv[i] == "-c"):
            i += 1
            n_samples = int(sys.argv[i])
        elif sys.argv[i] == "-sd":
            options.add("disk")
        elif sys.argv[i] == "-sc":
            map( lambda x: options.add(x) , ("cpu", "interrupt") )
        elif sys.argv[i] == "-sj":
            options.add("interrupt")
        elif sys.argv[i] == "-sn":
            options.add("network")
        elif sys.argv[i] == "-sD":
            options.add("disk","detail")
        elif sys.argv[i] == "-sC":
            map( lambda x: options.add(x) , ("cpu", "process", "detail") )
        elif sys.argv[i] == "-sJ":
            map( lambda x: options.add(x) , ("interrupt", "detail") )
        elif sys.argv[i] == "-sN":
            map( lambda x: options.add(x) , ("network", "detail") )
        elif (sys.argv[i] == "--verbose"):
            verbose = True
        i += 1

    if len(options) == 0:
        map( lambda x: options.add(x) , ("interrupt", "disk", "cpu", "process", "network") )
    if verbose:
        options.add("verbose")
    else:
        options.add("brief")

    pm = pmContext()
    # pm = pmContext(pmapi.PM_CONTEXT_HOST,"localhost")
    if (pm < 0):
        print "PCP is not running"

    (code, delta, errmsg) = pm.pmParseInterval("1 seconds")

    subsystem = _subsys()
    cpu = _cpu()
    cpu.get_stats()
    interrupt = _interrupt()
    interrupt.get_stats()
    proc = _process()
    proc.get_stats()
    disk = _disk()
    disk.get_stats()
    net = _net()
    net.get_stats()


    subsys = [subsystem for i in range(6)]
    for n in options:
        if n == "cpu":
            subsys[1] = cpu
        elif n == "interrupt":
            subsys[2] = interrupt
        elif n == "process":
            subsys[3] = proc
        elif n == "disk":
            subsys[4] = disk
        elif n == "network":
            subsys[5] = net
        
    if "brief" in options:
        for s in subsys:
            s.print_brief_header1()
        print
        for s in subsys:
            s.print_brief_header2()
        print

    for n in xrange(n_samples):
        pm.pmtimevalSleep(delta)

        for s in subsys:
            s.get_stats()
            s.get_total()
            if "brief" in options:
                s.print_brief()
            elif verbose:
                s.print_verbose()

        print

