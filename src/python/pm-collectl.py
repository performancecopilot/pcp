
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

me = "pm-collectl"

def check_code (code):
    if (code < 0):
        print pmErrStr(code)
        sys.exit(1)

def usage ():
    print sys.argv[0]," [-sSUBSYS] [-cN] [--verbose]"
    print "Where: N is number of cycles"
    print "       SUSBYS is one of:"
    print "              d for disk"
    print "              c for cpu"
    print "              n for net"
    print "              j for interrupt"
    print "              m for memory"
#    print "              b for ss"
#    print "              f for ss"
#    print "              y for ss"
#    print "              Z for ss"


# get_atom_value  -----------------------------------------------------------


def get_atom_value (metric, atom1, atom2, desc, first):
    if desc.contents.sem == pmapi.PM_SEM_DISCRETE or desc.contents.sem == pmapi.PM_SEM_INSTANT :
        # just use the absolute value as if it were the first value
        first = True

    # value conversion and diff, if required
    type = desc.contents.type
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
    
    list_type = type([])

    try:
        (code, metric_result) = pm.pmFetch(metric_name)
        check_code (code)
    except pmErr as e:
        if str(e).find("PM_ERR_EOL") != -1:
            print "\nReached end of archive"
            sys.exit(1)

    first = True if max(old_metric_value) == 0 else False
    # list of metric names
    for i in xrange(len(metric)):
        # list of metric results, one per metric name
        for j in xrange(metric_result.contents.numpmid):
            if (metric_result.contents.get_pmid(j) != metric_name[i]):
                continue
            atomlist = []
            # list of instances, one or more per metric.  e.g. there are many 
            # instances for network metrics, one per network interface
            for k in xrange(metric_result.contents.get_numval(j)):
                (code, atom) = pm.pmExtractValue(metric_result.contents.get_valfmt(j), metric_result.contents.get_vlist(j, k), metric_desc[j].contents.type, metric_desc[j].contents.type)
                atomlist.append(atom)

            value = []
            # metric may require a diff to get a per interval value
            for k in xrange(metric_result.contents.get_numval(j)):
                if first:
                    old_val = 0
                elif type(old_metric_value[j]) == list_type:
                    old_val = old_metric_value[j][k]
                else:
                    old_val = old_metric_value[j]
                value.append (get_atom_value(metric[i], atomlist[k], old_val, metric_desc[j], first))

            old_metric_value[j] = copy.copy(atomlist)
            if metric_result.contents.get_numval(j) == 1:
                metric_value[j] = copy.copy(value[0]) if len(value) == 1 else 0
            elif metric_result.contents.get_numval(j) > 1:
                metric_value[j] = copy.copy(value)


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

    if os.path.exists(file):
        print "Directory %s already exists\n" % file
        sys.exit(1)
    os.mkdir (file)
    status = pm.pmRecordSetup (file + "/" + me + ".pcp", me, 0)
    check_code (status)
    (status, rhp) = pm.pmRecordAddHost ("localhost", 1, configuration)
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


# _subsys ---------------------------------------------------------------


class _subsys(object):
    def __init__(self):
        self.notimpl_warned = False
    def set_verbosity(self, verbosity):
        self.verbosity = verbosity
    def get_stats(self):
        if not self.notimpl_warned:
            print "This subsystem is not implemented yet"
            self.notimpl_warned = True
        True
    def get_total(self):
        True
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


# _cpu  -----------------------------------------------------------------


class _cpu(_subsys):
    def __init__(self):
        self.cpu_metrics = ['kernel.all.cpu.nice', 'kernel.all.cpu.user',
                            'kernel.all.cpu.intr', 'kernel.all.cpu.sys',
                            'kernel.all.cpu.idle', 'kernel.all.cpu.steal',
                            'kernel.all.cpu.irq.hard', 'kernel.all.cpu.irq.soft',
                            'kernel.all.cpu.wait.total', 'hinv.ncpu', 
                            'kernel.all.intr', 'kernel.all.pswitch',
                            'kernel.percpu.cpu.nice', 'kernel.percpu.cpu.user',
                            'kernel.percpu.cpu.intr', 'kernel.percpu.cpu.sys',
                            'kernel.percpu.cpu.steal', 'kernel.percpu.cpu.irq.hard',
                            'kernel.percpu.cpu.irq.soft', 'kernel.percpu.cpu.wait.total',
                            'kernel.percpu.cpu.idle']

        self.cpu_metrics_dict={i:self.cpu_metrics.index(i) for i in self.cpu_metrics}
        self.cpu_metric_value = [0 for i in range(len(self.cpu_metrics))]
        self.old_cpu_metric_value = [0 for i in range(len(self.cpu_metrics))]

    def setup_metrics(self,pm):
        (code, self.cpu_metric_name) = pm.pmLookupName(self.cpu_metrics)
        check_code (code)
        (code, self.cpu_metric_desc) = pm.pmLookupDesc(self.cpu_metric_name)
        check_code (code)

    def dump_metrics(self):
        metrics_string = ""
        for i in xrange(len(self.cpu_metrics)):
            metrics_string += self.cpu_metrics[i]
            metrics_string += " "
        return metrics_string
            
    def get_stats(self):
        get_stats (self.cpu_metrics, self.cpu_metric_name, self.cpu_metric_desc, self.cpu_metric_value, self.old_cpu_metric_value)
    def get_total(self):
        _=self.cpu_metrics_dict
        self.cpu_total = (self.cpu_metric_value[_['kernel.all.cpu.nice']] +
                          self.cpu_metric_value[_['kernel.all.cpu.user']] +
                          self.cpu_metric_value[_['kernel.all.cpu.intr']] +
                          self.cpu_metric_value[_['kernel.all.cpu.sys']] +
                          self.cpu_metric_value[_['kernel.all.cpu.idle']] +
                          self.cpu_metric_value[_['kernel.all.cpu.steal']] +
                          self.cpu_metric_value[_['kernel.all.cpu.irq.hard']] +
                          self.cpu_metric_value[_['kernel.all.cpu.irq.soft']] )

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
        _=self.cpu_metrics_dict
        print "%4d" % (100 * (self.cpu_metric_value[_['kernel.all.cpu.nice']] +
                              self.cpu_metric_value[_['kernel.all.cpu.user']] +
                              self.cpu_metric_value[_['kernel.all.cpu.intr']] +
                              self.cpu_metric_value[_['kernel.all.cpu.sys']] +
                              self.cpu_metric_value[_['kernel.all.cpu.steal']] +
                              self.cpu_metric_value[_['kernel.all.cpu.irq.hard']] +
                              self.cpu_metric_value[_['kernel.all.cpu.irq.soft']]) /
                       self.cpu_total),
        print "%3d" % (100 * (self.cpu_metric_value[_['kernel.all.cpu.intr']] +
                              self.cpu_metric_value[_['kernel.all.cpu.sys']] +
                              self.cpu_metric_value[_['kernel.all.cpu.steal']] +
                              self.cpu_metric_value[_['kernel.all.cpu.irq.hard']] +
                              self.cpu_metric_value[_['kernel.all.cpu.irq.soft']]) /
                       self.cpu_total),
        print "%5d %6d" % (self.cpu_metric_value[_['kernel.all.intr']],
                           self.cpu_metric_value[_['kernel.all.pswitch']]),
    def print_detail(self):
        _=self.cpu_metrics_dict
        for k in range(len(self.cpu_metric_value[_['kernel.percpu.cpu.user']])):
            print "    %3d  %4d %4d  %3d %4d %3d  %4d %5d %4d" % (
                k,
                (100 * (self.cpu_metric_value[_['kernel.percpu.cpu.nice']][k] +
                        self.cpu_metric_value[_['kernel.percpu.cpu.user']][k] +
                        self.cpu_metric_value[_['kernel.percpu.cpu.intr']][k] +
                        self.cpu_metric_value[_['kernel.percpu.cpu.sys']][k] +
                        self.cpu_metric_value[_['kernel.percpu.cpu.steal']][k] +
                        self.cpu_metric_value[_['kernel.percpu.cpu.irq.hard']][k] +
                        self.cpu_metric_value[_['kernel.percpu.cpu.irq.soft']][k]) /
             self.cpu_total),
            self.cpu_metric_value[_['kernel.percpu.cpu.nice']][k],
            (100 * (self.cpu_metric_value[_['kernel.percpu.cpu.intr']][k] +
                    self.cpu_metric_value[_['kernel.percpu.cpu.sys']][k] +
                    self.cpu_metric_value[_['kernel.percpu.cpu.steal']][k] +
                    self.cpu_metric_value[_['kernel.percpu.cpu.irq.hard']][k] +
                    self.cpu_metric_value[_['kernel.percpu.cpu.irq.soft']][k]) /
             self.cpu_total),
            self.cpu_metric_value[_['kernel.percpu.cpu.wait.total']][k],
            self.cpu_metric_value[_['kernel.percpu.cpu.irq.hard']][k],
            self.cpu_metric_value[_['kernel.percpu.cpu.irq.soft']][k],
            self.cpu_metric_value[_['kernel.percpu.cpu.steal']][k],
            self.cpu_metric_value[_['kernel.percpu.cpu.idle']][k] / 10)
    def print_verbose(self):
        _=self.cpu_metrics_dict
        ncpu = self.cpu_metric_value[_['hinv.ncpu']]
        print "%4d %6d %5d %4d %4d %5d " % (
            (100 * (self.cpu_metric_value[_['kernel.all.cpu.nice']] +
                    self.cpu_metric_value[_['kernel.all.cpu.user']] +
                    self.cpu_metric_value[_['kernel.all.cpu.intr']] +
                    self.cpu_metric_value[_['kernel.all.cpu.sys']] +
                    self.cpu_metric_value[_['kernel.all.cpu.steal']] +
                    self.cpu_metric_value[_['kernel.all.cpu.irq.hard']] +
                    self.cpu_metric_value[_['kernel.all.cpu.irq.soft']]) /
             self.cpu_total),
            self.cpu_metric_value[_['kernel.all.cpu.nice']],
            (100 * (self.cpu_metric_value[_['kernel.all.cpu.intr']] +
                    self.cpu_metric_value[_['kernel.all.cpu.sys']] +
                    self.cpu_metric_value[_['kernel.all.cpu.steal']] +
                    self.cpu_metric_value[_['kernel.all.cpu.irq.hard']] +
                    self.cpu_metric_value[_['kernel.all.cpu.irq.soft']]) /
             self.cpu_total),
            self.cpu_metric_value[_['kernel.all.cpu.wait.total']],
            self.cpu_metric_value[_['kernel.all.cpu.irq.hard']],
            self.cpu_metric_value[_['kernel.all.cpu.irq.soft']]
            ),
        print "%6d %6d %5d %5d %6d" % (
            self.cpu_metric_value[_['kernel.all.cpu.steal']],
            self.cpu_metric_value[_['kernel.all.cpu.idle']] / (10 * ncpu),
            ncpu,
            self.cpu_metric_value[_['kernel.all.intr']],
            self.cpu_metric_value[_['kernel.all.pswitch']]
            ),


# _interrupt  -----------------------------------------------------------------


class _interrupt(_subsys):
    def __init__(self):
        self.interrupt_metrics = ['kernel.percpu.interrupts.MCP',
                                  'kernel.percpu.interrupts.MCE',
                                  'kernel.percpu.interrupts.THR',
                                  'kernel.percpu.interrupts.TRM',
                                  'kernel.percpu.interrupts.TLB',
                                  'kernel.percpu.interrupts.CAL',
                                  'kernel.percpu.interrupts.RES',
                                  'kernel.percpu.interrupts.RTR',
                                  'kernel.percpu.interrupts.IWI',
                                  'kernel.percpu.interrupts.PMI',
                                  'kernel.percpu.interrupts.SPU',
                                  'kernel.percpu.interrupts.LOC',
                                  'kernel.percpu.interrupts.line48',
                                  'kernel.percpu.interrupts.line47',
                                  'kernel.percpu.interrupts.line46',
                                  'kernel.percpu.interrupts.line45',
                                  'kernel.percpu.interrupts.line44',
                                  'kernel.percpu.interrupts.line43',
                                  'kernel.percpu.interrupts.line42',
                                  'kernel.percpu.interrupts.line41',
                                  'kernel.percpu.interrupts.line40',
                                  'kernel.percpu.interrupts.line23',
                                  'kernel.percpu.interrupts.line22',
                                  'kernel.percpu.interrupts.line21',
                                  'kernel.percpu.interrupts.line20',
                                  'kernel.percpu.interrupts.line19',
                                  'kernel.percpu.interrupts.line18',
                                  'kernel.percpu.interrupts.line17',
                                  'kernel.percpu.interrupts.line16',
                                  'kernel.percpu.interrupts.line12',
                                  'kernel.percpu.interrupts.line9',
                                  'kernel.percpu.interrupts.line8',
                                  'kernel.percpu.interrupts.line1',
                                  'kernel.percpu.interrupts.line0']

        self.interrupt_metrics_dict={i:self.interrupt_metrics.index(i) for i in self.interrupt_metrics}
        self.interrupt_metric_value = [0 for i in range(len(self.interrupt_metrics))]
        self.old_interrupt_metric_value = [0 for i in range(len(self.interrupt_metrics))]

    def setup_metrics(self,pm):
        # remove any unsupported metrics
        for j in range(len(self.interrupt_metrics)-1, -1, -1):
            try:

                (code, self.int_metric_name) = pm.pmLookupName(self.interrupt_metrics[j])
            except pmErr as e:
                self.interrupt_metrics.remove(self.interrupt_metrics[j])

        (code, self.int_metric_name) = pm.pmLookupName(self.interrupt_metrics)
        check_code (code)
        (code, self.int_metric_desc) = pm.pmLookupDesc(self.int_metric_name)
        check_code (code)

    def dump_metrics(self):
        metrics_string = ""
        for i in xrange(len(self.interrupt_metrics)):
            metrics_string += self.interrupt_metrics[i]
            metrics_string += " "
        return metrics_string
            
    def get_stats(self):
        get_stats (self.interrupt_metrics, self.int_metric_name, self.int_metric_desc, self.interrupt_metric_value, self.old_interrupt_metric_value)
    def print_header1_brief(self):
            print '#<--Int--->'
    def print_header1_detail(self):
            print '# INTERRUPT DETAILS'
            print '# Int    Cpu0   Cpu1   Type            Device(s)'
    def print_header1_verbose(self):
            print '# INTERRUPT SUMMARY'
    def print_header2_brief(self):
            print '#Cpu0 Cpu1'
    def print_header2_verbose(self):
            print '#    Cpu0   Cpu1'
    def print_brief(self):
        _=self.interrupt_metrics_dict
        int_count = []
        for k in range(len(self.interrupt_metric_value[_['kernel.percpu.interrupts.MCP']])):
            int_count.append(0)
            for j  in range(_['kernel.percpu.interrupts.MCP'], len(self.interrupt_metrics_dict)):
                int_count[k] += self.interrupt_metric_value[j][k]
                
        for k in range(len(self.interrupt_metric_value[_['kernel.percpu.interrupts.MCP']])):
            print "%4d " % (int_count[k]),
    def print_detail(self):
        _=self.interrupt_metrics_dict
        for j  in range(_['kernel.percpu.interrupts.MCP'], len(self.interrupt_metrics_dict)):
            for k in range(len(self.interrupt_metric_value[_['kernel.percpu.interrupts.MCP']])):
                have_nonzero_value = False
                if self.interrupt_metric_value[j][k] != 0:
                    have_nonzero_value = True
                if not have_nonzero_value:
                    continue
                # pcp does not give the interrupt # so print spaces
                print "        ",
                for k in range(len(self.interrupt_metric_value[_['kernel.percpu.interrupts.MCP']])):
                    print "%4d " % (self.interrupt_metric_value[j][k]),
                text = (pm.pmLookupText(self.int_metric_name[j], pmapi.PM_TEXT_ONELINE))
                print "%-18s %s" % (text[:(str.index(text," "))],
                                 text[(str.index(text," ")):])
    def print_verbose(self):
        print "     ",
        self.print_brief()

# _process  -----------------------------------------------------------------


class _process(_subsys):
    def __init__(self):
        self.process_metrics = ['kernel.all.nprocs', 'kernel.all.runnable', 'proc.runq.runnable', 'kernel.all.load', 'proc.runq.blocked']

        self.process_metrics_dict={i:self.process_metrics.index(i) for i in self.process_metrics}
        self.process_metric_value = [0 for i in range(len(self.process_metrics))]
        self.old_process_metric_value = [0 for i in range(len(self.process_metrics))]

    def setup_metrics(self,pm):
        (code, self.process_metric_name) = pm.pmLookupName(self.process_metrics)
        check_code (code)
        (code, self.process_metric_desc) = pm.pmLookupDesc(self.process_metric_name)
        check_code (code)

    def dump_metrics(self):
        metrics_string = ""
        for i in xrange(len(self.process_metrics)):
            metrics_string += self.process_metrics[i]
            metrics_string += " "
        return metrics_string
            
    def get_stats(self):
        get_stats (self.process_metrics, self.process_metric_name, self.process_metric_desc, self.process_metric_value, self.old_process_metric_value)
    def print_verbose(self):
        _=self.process_metrics_dict
        print "%5d %5d %5d %5.2f %5.2f %5.2f %4d %4d" % (
            self.process_metric_value[_['kernel.all.nprocs']],
            self.process_metric_value[_['kernel.all.runnable']],
            self.process_metric_value[_['proc.runq.runnable']],
            self.process_metric_value[_['kernel.all.load']][0],
            self.process_metric_value[_['kernel.all.load']][1],
            self.process_metric_value[_['kernel.all.load']][2],
            self.process_metric_value[_['kernel.all.runnable']],
            self.process_metric_value[_['proc.runq.blocked']]),


# _disk  -----------------------------------------------------------------


class _disk(_subsys):
    def __init__(self):
        self.disk_metrics = ['disk.all.read', 'disk.all.write',
                             'disk.all.read_bytes', 'disk.all.write_bytes',
                             'disk.all.read_merge', 'disk.all.write_merge',
                             'disk.dev.read', 'disk.dev.write',
                             'disk.dev.read_bytes', 'disk.dev.write_bytes',
                             'disk.dev.read_merge', 'disk.dev.write_merge',
                             'disk.dev.blkread', 'disk.dev.blkwrite']

        self.disk_metrics_dict={i:self.disk_metrics.index(i) for i in self.disk_metrics}
        self.disk_metric_value = [0 for i in range(len(self.disk_metrics))]
        self.old_disk_metric_value = [0 for i in range(len(self.disk_metrics))]

    def setup_metrics(self,pm):
        (code, self.disk_metric_name) = pm.pmLookupName(self.disk_metrics)
        check_code (code)
        (code, self.disk_metric_desc) = pm.pmLookupDesc(self.disk_metric_name)
        check_code (code)


    def dump_metrics(self):
        metrics_string = ""
        for i in xrange(len(self.disk_metrics)):
            metrics_string += self.disk_metrics[i]
            metrics_string += " "
        return metrics_string
            
    def get_stats(self):
        get_stats (self.disk_metrics, self.disk_metric_name, self.disk_metric_desc, self.disk_metric_value, self.old_disk_metric_value)
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
            print '#KBRead RMerged  Reads SizeKB  KBWrite WMerged Writes SizeKB\n'
    def print_brief(self):
        _=self.disk_metrics_dict
        print "%6d %6d %6d %6d" % (
            self.disk_metric_value[_['disk.all.read_bytes']] / 1024,
            self.disk_metric_value[_['disk.all.read']],
            self.disk_metric_value[_['disk.all.write_bytes']] / 1024,
            self.disk_metric_value[_['disk.all.write']]),
    def print_detail(self):
        _=self.disk_metrics_dict
        for j in xrange(len(self.disk_metric_name)):
            try:
                (inst, iname) = pm.pmGetInDom(self.disk_metric_desc[j])
                break
            except pmErr as e:
                iname = iname = "X"

        # metric values may be scalars or arrays depending on # of disks
        for j in xrange(get_dimension(self.disk_metric_value[_['disk.dev.read_bytes']])):
            print "%-10s %6d %6d %4d %4d  %6d %6d %4d %4d  %6d %6d %4d %6d %4d" % (
                iname[j],
                get_scalar_value (self.disk_metric_value[_['disk.dev.read_bytes']], j),
                get_scalar_value (self.disk_metric_value[_['disk.dev.read_merge']], j),
                get_scalar_value (self.disk_metric_value[_['disk.dev.read']], j),
                get_scalar_value (self.disk_metric_value[_['disk.dev.blkread']], j),
                get_scalar_value (self.disk_metric_value[_['disk.dev.write_bytes']], j),
                get_scalar_value (self.disk_metric_value[_['disk.dev.write_merge']], j),
                get_scalar_value (self.disk_metric_value[_['disk.dev.write']], j),
                get_scalar_value (self.disk_metric_value[_['disk.dev.blkwrite']], j),
                0, 0, 0, 0, 0)
# ??? replace 0 with required fields

    def print_verbose(self):
        _=self.disk_metrics_dict
        print '%6d %6d %6d %6d %7d %8d %6d %6d' % (
            0 if self.disk_metric_value[_['disk.all.read_bytes']] == 0
              else self.disk_metric_value[_['disk.all.read_bytes']]/
                   self.disk_metric_value[_['disk.all.read']],
            self.disk_metric_value[_['disk.all.read_merge']],
            self.disk_metric_value[_['disk.all.read']],
            0,
            0 if self.disk_metric_value[_['disk.all.write_bytes']] == 0
              else self.disk_metric_value[_['disk.all.write_bytes']]/
                   self.disk_metric_value[_['disk.all.write']],
            self.disk_metric_value[_['disk.all.write_merge']],
            self.disk_metric_value[_['disk.all.write']],
            0),


# _memory  -----------------------------------------------------------------


class _memory(_subsys):
    def __init__(self):
        self.memory_metrics = ['mem.freemem',
                               'mem.util.bufmem',
                               'mem.util.cached',
                               'mem.util.inactive',
                               'mem.util.slab',
                               'mem.util.mapped',
                               'mem.physmem',
                               'mem.util.used',
                               'mem.freemem',
                               'mem.util.anonpages',
                               'mem.util.committed_AS',
                               'mem.util.mlocked',
                               'mem.util.inactive',
                               'mem.util.swapTotal',
                               'swap.used',
                               'swap.free',
                               'swap.pagesin',
                               'swap.pagesout',
                               'mem.vmstat.pgfault',
                               'mem.vmstat.pgmajfault',
                               'mem.vmstat.pgpgin',
                               'mem.vmstat.pgpgout'
                               ]


        self.memory_metrics_dict={i:self.memory_metrics.index(i) for i in self.memory_metrics}
        self.memory_metric_value = [0 for i in range(len(self.memory_metrics))]
        self.old_memory_metric_value = [0 for i in range(len(self.memory_metrics))]

    def setup_metrics(self,pm):
        (code, self.memory_metric_name) = pm.pmLookupName(self.memory_metrics)
        check_code (code)
        (code, self.memory_metric_desc) = pm.pmLookupDesc(self.memory_metric_name)
        check_code (code)

    def dump_metrics(self):
        metrics_string = ""
        for i in xrange(len(self.memory_metrics)):
            metrics_string += self.memory_metrics[i]
            metrics_string += " "
        return metrics_string
            
    def get_stats(self):
        get_stats (self.memory_metrics, self.memory_metric_name, self.memory_metric_desc, self.memory_metric_value, self.old_memory_metric_value)
    def print_header1_brief(self):
            print '#<--Int--->'
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
        _=self.memory_metrics_dict
        print "%4dM %3dM %3dM %3dM %3dM %3dM " % (
            round(self.memory_metric_value[_['mem.freemem']], 1000),
            round(self.memory_metric_value[_['mem.util.bufmem']], 1000),
            round(self.memory_metric_value[_['mem.util.cached']], 1000),
            round(self.memory_metric_value[_['mem.util.inactive']], 1000),
            round(self.memory_metric_value[_['mem.util.slab']], 1000),
            round(self.memory_metric_value[_['mem.util.mapped']], 1000))
    def print_verbose(self):
        _=self.memory_metrics_dict
        print "%8dM %6dM %6dM %6dM %6dM %6dM %6dM %6dM %6dM %6dM %5dM %5dM %5dM %5dM %6d %6d %6d %6d %6d %6d " % (
            round(self.memory_metric_value[_['mem.physmem']], 1000),
            round(self.memory_metric_value[_['mem.util.used']], 1000),
            round(self.memory_metric_value[_['mem.freemem']], 1000),
            round(self.memory_metric_value[_['mem.util.bufmem']], 1000),
            round(self.memory_metric_value[_['mem.util.cached']], 1000),
            round(self.memory_metric_value[_['mem.util.slab']], 1000),
            round(self.memory_metric_value[_['mem.util.mapped']], 1000),
            round(self.memory_metric_value[_['mem.util.anonpages']], 1000),
            round(self.memory_metric_value[_['mem.util.committed_AS']], 1000),
            round(self.memory_metric_value[_['mem.util.mlocked']], 1000),
            round(self.memory_metric_value[_['mem.util.inactive']], 1000),
            round(self.memory_metric_value[_['mem.util.swapTotal']], 1000),
            round(self.memory_metric_value[_['swap.used']], 1000),
            round(self.memory_metric_value[_['swap.free']], 1000),
            round(self.memory_metric_value[_['swap.pagesin']], 1000),
            round(self.memory_metric_value[_['swap.pagesout']], 1000),
            round(self.memory_metric_value[_['mem.vmstat.pgfault']] -
                  self.memory_metric_value[_['mem.vmstat.pgmajfault']], 1000),
            round(self.memory_metric_value[_['mem.vmstat.pgmajfault']], 1000),
            round(self.memory_metric_value[_['mem.vmstat.pgpgin']], 1000),
            round(self.memory_metric_value[_['mem.vmstat.pgpgout']], 1000))


# _net  -----------------------------------------------------------------


class _net(_subsys):
    def __init__(self):
        self.net_metrics = ['network.interface.in.bytes',
                            'network.interface.in.packets',
                            'network.interface.out.bytes',
                            'network.interface.out.packets',
                            'network.interface.in.mcasts',
                            'network.interface.total.mcasts',
                            'network.interface.in.compressed',
                            'network.interface.out.compressed',
                            'network.interface.in.errors',
                            'network.interface.out.errors']

        self.net_metrics_dict={i:self.net_metrics.index(i) for i in self.net_metrics}
        self.net_metric_value = [0 for i in range(len(self.net_metrics))]
        self.old_net_metric_value = [0 for i in range(len(self.net_metrics))]

    def setup_metrics(self,pm):
        (code, self.net_metric_name) = pm.pmLookupName(self.net_metrics)
        check_code (code)
        (code, self.net_metric_desc) = pm.pmLookupDesc(self.net_metric_name)
        check_code (code)

    def dump_metrics(self):
        metrics_string = ""
        for i in xrange(len(self.net_metrics)):
            metrics_string += self.net_metrics[i]
            metrics_string += " "
        return metrics_string
            
    def get_stats(self):
        get_stats (self.net_metrics, self.net_metric_name, self.net_metric_desc, self.net_metric_value, self.old_net_metric_value)
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
        _=self.net_metrics_dict
        print "%5d %6d %6d %6d" % (
            sum(self.net_metric_value[_['network.interface.in.bytes']]) / 1024,
            sum(self.net_metric_value[_['network.interface.in.packets']]),
            sum(self.net_metric_value[_['network.interface.out.bytes']]) / 1024,
            sum(self.net_metric_value[_['network.interface.out.packets']])),
    def print_verbose(self):
        _=self.net_metrics_dict
        self.net_metric_value[_['network.interface.in.bytes']][0] = 0 # don't include loopback
        self.net_metric_value[_['network.interface.in.bytes']][1] = 0
        print '%6d %5d %6d %6d %6d %6d %6d %6d %6d %6d %7d' % (
            sum(self.net_metric_value[_['network.interface.in.bytes']]) / 1024,
            sum(self.net_metric_value[_['network.interface.in.packets']]),
            sum(self.net_metric_value[_['network.interface.in.bytes']]) /
            sum(self.net_metric_value[_['network.interface.in.packets']]),
            sum(self.net_metric_value[_['network.interface.in.mcasts']]),
            sum(self.net_metric_value[_['network.interface.in.compressed']]),
            sum(self.net_metric_value[_['network.interface.in.errors']]),
            sum(self.net_metric_value[_['network.interface.out.bytes']]) / 1024,
            sum(self.net_metric_value[_['network.interface.out.packets']]),
            sum(self.net_metric_value[_['network.interface.out.bytes']]) /
            sum(self.net_metric_value[_['network.interface.out.packets']]),
            sum(self.net_metric_value[_['network.interface.total.mcasts']]),
            sum(self.net_metric_value[_['network.interface.out.errors']])),
    def print_detail(self):
        _=self.net_metrics_dict
        for j in xrange(len(self.net_metric_value[_['network.interface.in.bytes']])):
            for k in xrange(len(self.net_metric_name)):
                try:
                    (inst, iname) = pm.pmGetInDom(self.net_metric_desc[k])
                    break
                except pmErr as e:
                    iname = "X"

            print '%4d %-7s %6d %5d %6d %6d %6d %6d %6d %6d %6d %6d %7d' % (
                j, iname[j],
                self.net_metric_value[_['network.interface.in.bytes']][j] / 1024,
                self.net_metric_value[_['network.interface.in.packets']][j],
                self.divide_check (self.net_metric_value[_['network.interface.in.bytes']][j],
                                   self.net_metric_value[_['network.interface.in.packets']][j]),
                self.net_metric_value[_['network.interface.in.mcasts']][j],
                self.net_metric_value[_['network.interface.in.compressed']][j],
                self.net_metric_value[_['network.interface.in.errors']][j],
                self.net_metric_value[_['network.interface.in.packets']][j],
                self.net_metric_value[_['network.interface.out.packets']][j],
                self.divide_check (self.net_metric_value[_['network.interface.in.packets']][j],
                                   self.net_metric_value[_['network.interface.out.packets']][j]) / 1024,
                    self.net_metric_value[_['network.interface.total.mcasts']][j],
                    self.net_metric_value[_['network.interface.out.compressed']][j])


# main ----------------------------------------------------------------------


if __name__ == '__main__':

    n_samples = 0
    i = 1
    subsys = set()
    verbosity = "brief"
    output_file = ""
    input_file = ""
    duration = 0
    interval_arg = 1

    ss = _subsys()
    cpu = _cpu()
    interrupt = _interrupt()
    proc = _process()
    disk = _disk()
    memory = _memory()
    net = _net()

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
        if (sys.argv[i] == "-c"):
            i += 1
            n_samples = int(sys.argv[i])
        elif (sys.argv[i] == "-f" or sys.argv[i] == "--filename"):
            i += 1
            output_file = sys.argv[i]
        elif (sys.argv[i] == "-p" or sys.argv[i] == "--playback"):
            i += 1
            input_file = sys.argv[i]
        elif (sys.argv[i] == "-R" or sys.argv[i] == "--runtime"):
            i += 1
            duration = sys.argv[i]
            # Nwdhms
        elif (sys.argv[i] == "-i" or sys.argv[i] == "--interval"):
            i += 1
            interval_arg = sys.argv[i]
            # Nwdhms
        elif (sys.argv[i][:2] == "-c"):
            n_samples = int(sys.argv[i][2:])
        elif (sys.argv[i][:2] == "-s"):
            for j in xrange(len(sys.argv[i][2:])):
                subsys_arg = sys.argv[i][j+2:j+3]
                subsys.add(s_options[subsys_arg][0])
                if subsys_arg.isupper():
                    verbosity =  s_options[subsys_arg][1]
        elif (sys.argv[i] == "--verbose"):
            if verbosity != "detail":
                verbosity = "verbose"
        elif (sys.argv[i] == "--help" or sys.argv[i] == "-h"):
            usage()
            sys.exit(1)
        elif (sys.argv[i][:1] == "-"):
            print "Unknown option ", sys.argv[i]
            sys.exit(1)
        i += 1

    if len(subsys) == 0:
        map( lambda x: subsys.add(x) , (disk, cpu, proc, net) )
    elif cpu in subsys:
        subsys.add(proc)

    if input_file == "":
        pm = pmContext()
    else:
        # -f saves the metrics in a directory, so get the archive basename
        lol = []
        if not os.path.isdir(input_file):
            print input_file,"is not a directory"
            sys.exit(1)
        for line in open(input_file + "/" + me + ".pcp"):
            lol.append(line[:-1].split())
        archive = input_file + "/" + lol[len(lol)-1][2]

        pm = pmContext(pmapi.PM_CONTEXT_ARCHIVE, archive)
    if (pm < 0):
        print "PCP is not running"

    (code, delta, errmsg) = pm.pmParseInterval(str(interval_arg) + " seconds")

    if output_file != "":
        # ??? fix every
        configuration = "log mandatory on every 1 seconds { "
        for s in subsys:
            configuration += s.dump_metrics()
        configuration += "}"
        duration = n_samples if n_samples != 0 else 10 * interval_arg
        record (pm, configuration, duration, output_file)
        sys.exit(0)

    for s in subsys:
        s.setup_metrics(pm)
        s.set_verbosity(verbosity)
        s.get_stats()

    # brief headings for different subsystems are concatenated together
    for s in subsys:
        if s == 0: continue
        s.print_header1()
    print
    for s in subsys:
        if s == 0: continue
        s.print_header2()
    print

    n = 0

    try:
        print_header = True
        while (n < n_samples) or (n_samples == 0):
            pm.pmtimevalSleep(delta)

            for s in subsys:
                if s == 0: continue
                s.get_stats()
                s.get_total()
                s.print_line()
            print
            n += 1
    except KeyboardInterrupt:
        True
