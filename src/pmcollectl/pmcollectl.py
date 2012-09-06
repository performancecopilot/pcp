#!/usr/bin/python
#
# pmcollectl.py
#
# Copyright (C) 2012 Red Hat Inc.
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

import unittest
import pmapi
import time
import sys
import argparse
import copy
from pcp import *
from ctypes import *

me = "pmcollectl"

def check_code (code):
    if (code < 0):
        print pmErrStr(code)
        sys.exit(1)

def usage ():
    print "\nUsage:", sys.argv[0], "\n\t[-sSUBSYS] [-f|--filename FILE] [-p|--playback FILE]"
    print '''\t[-R|--runtime N] [-c|--count N] [-i|--interval N] [--verbose]

	Collect and display current system status.

Where:	-cN is number of cycles
	-sSUBSYS is one of:
	  d for disk
	  c for cpu
	  n for net
	  j for interrupt
	  m for memory
	-f, --filename FILE outputs the status to FILE instead of displaying
		current system data
	-p, --playback FILE reads the status from FILE instead of using current
		system data
	-R, --runtime N is the amount of time to sample data.  N may have a
		suffix of one of 'dhms'
	-i, --interval N is the number of seconds to wait between samples
'''

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
    def setup_metrics(self,pm):
        True


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

    def setup_metrics(self,pm):
        # remove any unsupported metrics
        for j in range(len(self.cpu_metrics)-1, -1, -1):
            try:

                (code, self.cpu_metric_name) = pm.pmLookupName(self.cpu_metrics[j])
            except pmErr as e:
                self.cpu_metrics.remove(self.cpu_metrics[j])

        self.cpu_metrics_dict=dict((i,self.cpu_metrics.index(i)) for i in self.cpu_metrics)
        (code, self.cpu_metric_name) = pm.pmLookupName(self.cpu_metrics)
        check_code (code)
        (code, self.cpu_metric_desc) = pm.pmLookupDesc(self.cpu_metric_name)
        check_code (code)
        self.cpu_metric_value = [0 for i in range(len(self.cpu_metrics))]
        self.old_cpu_metric_value = [0 for i in range(len(self.cpu_metrics))]

    def dump_metrics(self):
        metrics_string = ""
        for i in xrange(len(self.cpu_metrics)):
            metrics_string += self.cpu_metrics[i]
            metrics_string += " "
        return metrics_string
            
    def get_stats(self):
        get_stats (self.cpu_metrics, self.cpu_metric_name, self.cpu_metric_desc, self.cpu_metric_value, self.old_cpu_metric_value)

    def get_cpu_metric_value(self, idx):
        if idx in self.cpu_metrics:
            return self.cpu_metric_value[self.cpu_metrics_dict[idx]]
        else:
            return 0

    def get_total(self):
        self.cpu_total = (self.get_cpu_metric_value('kernel.all.cpu.nice') +
                          self.get_cpu_metric_value('kernel.all.cpu.user') +
                          self.get_cpu_metric_value('kernel.all.cpu.intr') +
                          self.get_cpu_metric_value('kernel.all.cpu.sys') +
                          self.get_cpu_metric_value('kernel.all.cpu.idle') +
                          self.get_cpu_metric_value('kernel.all.cpu.steal') +
                          self.get_cpu_metric_value('kernel.all.cpu.irq.hard') +
                          self.get_cpu_metric_value('kernel.all.cpu.irq.soft') )

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
        print "%4d" % (100 * (self.get_cpu_metric_value('kernel.all.cpu.nice') +
                              self.get_cpu_metric_value('kernel.all.cpu.user') +
                              self.get_cpu_metric_value('kernel.all.cpu.intr') +
                              self.get_cpu_metric_value('kernel.all.cpu.sys') +
                              self.get_cpu_metric_value('kernel.all.cpu.steal') +
                              self.get_cpu_metric_value('kernel.all.cpu.irq.hard') +
                              self.get_cpu_metric_value('kernel.all.cpu.irq.soft')) /
                       self.cpu_total),
        print "%3d" % (100 * (self.get_cpu_metric_value('kernel.all.cpu.intr') +
                              self.get_cpu_metric_value('kernel.all.cpu.sys') +
                              self.get_cpu_metric_value('kernel.all.cpu.steal') +
                              self.get_cpu_metric_value('kernel.all.cpu.irq.hard') +
                              self.get_cpu_metric_value('kernel.all.cpu.irq.soft')) /
                       self.cpu_total),
        print "%5d %6d" % (self.get_cpu_metric_value('kernel.all.intr'),
                           self.get_cpu_metric_value('kernel.all.pswitch')),
    def print_detail(self):
        for k in range(len(self.get_cpu_metric_value('kernel.percpu.cpu.user'))):
            print "    %3d  %4d %4d  %3d %4d %3d  %4d %5d %4d" % (
                k,
                (100 * (self.get_cpu_metric_value('kernel.percpu.cpu.nice')[k] +
                        self.get_cpu_metric_value('kernel.percpu.cpu.user')[k] +
                        self.get_cpu_metric_value('kernel.percpu.cpu.intr')[k] +
                        self.get_cpu_metric_value('kernel.percpu.cpu.sys')[k] +
                        self.get_cpu_metric_value('kernel.percpu.cpu.steal')[k] +
                        self.get_cpu_metric_value('kernel.percpu.cpu.irq.hard')[k] +
                        self.get_cpu_metric_value('kernel.percpu.cpu.irq.soft')[k]) /
             self.cpu_total),
            self.get_cpu_metric_value('kernel.percpu.cpu.nice')[k],
            (100 * (self.get_cpu_metric_value('kernel.percpu.cpu.intr')[k] +
                    self.get_cpu_metric_value('kernel.percpu.cpu.sys')[k] +
                    self.get_cpu_metric_value('kernel.percpu.cpu.steal')[k] +
                    self.get_cpu_metric_value('kernel.percpu.cpu.irq.hard')[k] +
                    self.get_cpu_metric_value('kernel.percpu.cpu.irq.soft')[k]) /
             self.cpu_total),
            self.get_cpu_metric_value('kernel.percpu.cpu.wait.total')[k],
            self.get_cpu_metric_value('kernel.percpu.cpu.irq.hard')[k],
            self.get_cpu_metric_value('kernel.percpu.cpu.irq.soft')[k],
            self.get_cpu_metric_value('kernel.percpu.cpu.steal')[k],
            self.get_cpu_metric_value('kernel.percpu.cpu.idle')[k] / 10)
    def print_verbose(self):
        ncpu = self.get_cpu_metric_value('hinv.ncpu')
        print "%4d %6d %5d %4d %4d %5d " % (
            (100 * (self.get_cpu_metric_value('kernel.all.cpu.nice') +
                    self.get_cpu_metric_value('kernel.all.cpu.user') +
                    self.get_cpu_metric_value('kernel.all.cpu.intr') +
                    self.get_cpu_metric_value('kernel.all.cpu.sys') +
                    self.get_cpu_metric_value('kernel.all.cpu.steal') +
                    self.get_cpu_metric_value('kernel.all.cpu.irq.hard') +
                    self.get_cpu_metric_value('kernel.all.cpu.irq.soft')) /
             self.cpu_total),
            self.get_cpu_metric_value('kernel.all.cpu.nice'),
            (100 * (self.get_cpu_metric_value('kernel.all.cpu.intr') +
                    self.get_cpu_metric_value('kernel.all.cpu.sys') +
                    self.get_cpu_metric_value('kernel.all.cpu.steal') +
                    self.get_cpu_metric_value('kernel.all.cpu.irq.hard') +
                    self.get_cpu_metric_value('kernel.all.cpu.irq.soft')) /
             self.cpu_total),
            self.get_cpu_metric_value('kernel.all.cpu.wait.total'),
            self.get_cpu_metric_value('kernel.all.cpu.irq.hard'),
            self.get_cpu_metric_value('kernel.all.cpu.irq.soft')
            ),
        print "%6d %6d %5d %5d %6d" % (
            self.get_cpu_metric_value('kernel.all.cpu.steal'),
            self.get_cpu_metric_value('kernel.all.cpu.idle') / (10 * ncpu),
            ncpu,
            self.get_cpu_metric_value('kernel.all.intr'),
            self.get_cpu_metric_value('kernel.all.pswitch')
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


    def setup_metrics(self,pm):
        # remove any unsupported metrics
        for j in range(len(self.interrupt_metrics)-1, -1, -1):
            try:

                (code, self.int_metric_name) = pm.pmLookupName(self.interrupt_metrics[j])
            except pmErr as e:
                self.interrupt_metrics.remove(self.interrupt_metrics[j])

        self.interrupt_metrics_dict=dict((i,self.interrupt_metrics.index(i)) for i in self.interrupt_metrics)
        (code, self.int_metric_name) = pm.pmLookupName(self.interrupt_metrics)
        check_code (code)
        (code, self.int_metric_desc) = pm.pmLookupDesc(self.int_metric_name)
        check_code (code)
        self.interrupt_metric_value = [0 for i in range(len(self.interrupt_metrics))]
        self.old_interrupt_metric_value = [0 for i in range(len(self.interrupt_metrics))]

    def dump_metrics(self):
        metrics_string = ""
        for i in xrange(len(self.interrupt_metrics)):
            metrics_string += self.interrupt_metrics[i]
            metrics_string += " "
        return metrics_string
            
    def get_stats(self):
        get_stats (self.interrupt_metrics, self.int_metric_name, self.int_metric_desc, self.interrupt_metric_value, self.old_interrupt_metric_value)

    def get_interrupt_metric_value(self, idx):
        if idx in self.interrupt_metrics:
            return self.interrupt_metric_value[self.interrupt_metrics_dict[idx]]
        else:
            return 0

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
        int_count = []
        _=self.interrupt_metrics_dict
        for k in range(len(self.get_interrupt_metric_value('kernel.percpu.interrupts.MCP'))):
            int_count.append(0)
            for j  in range(_['kernel.percpu.interrupts.MCP'], len(self.interrupt_metric_value)):
                int_count[k] += self.interrupt_metric_value[j][k]
                
        for k in range(len(self.get_interrupt_metric_value('kernel.percpu.interrupts.MCP'))):
            print "%4d " % (int_count[k]),
    def print_detail(self):
        _=self.interrupt_metrics_dict
        for j  in range(_['kernel.percpu.interrupts.MCP'], len(self.interrupt_metrics_dict)):
            for k in range(len(self.get_interrupt_metric_value('kernel.percpu.interrupts.MCP'))):
                have_nonzero_value = False
                if self.interrupt_metric_value[j][k] != 0:
                    have_nonzero_value = True
                if not have_nonzero_value:
                    continue
                # pcp does not give the interrupt # so print spaces
                print "        ",
                for k in range(len(self.get_interrupt_metric_value('kernel.percpu.interrupts.MCP'))):
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

    def setup_metrics(self,pm):
        # remove any unsupported metrics
        for j in range(len(self.process_metrics)-1, -1, -1):
            try:

                (code, self.process_metric_name) = pm.pmLookupName(self.process_metrics[j])
            except pmErr as e:
                self.process_metrics.remove(self.process_metrics[j])

        self.process_metrics_dict=dict((i,self.process_metrics.index(i)) for i in self.process_metrics)
        (code, self.process_metric_name) = pm.pmLookupName(self.process_metrics)
        check_code (code)
        (code, self.process_metric_desc) = pm.pmLookupDesc(self.process_metric_name)
        check_code (code)
        self.process_metric_value = [0 for i in range(len(self.process_metrics))]
        self.old_process_metric_value = [0 for i in range(len(self.process_metrics))]


    def dump_metrics(self):
        metrics_string = ""
        for i in xrange(len(self.process_metrics)):
            metrics_string += self.process_metrics[i]
            metrics_string += " "
        return metrics_string
            
    def get_stats(self):
        get_stats (self.process_metrics, self.process_metric_name, self.process_metric_desc, self.process_metric_value, self.old_process_metric_value)

    def get_process_metric_value(self, idx):
        if idx in self.process_metrics:
            return self.process_metric_value[self.process_metrics_dict[idx]]
        else:
            return 0

    def print_verbose(self):
        print "%5d %5d %5d %5.2f %5.2f %5.2f %4d %4d" % (
            self.get_process_metric_value('kernel.all.nprocs'),
            self.get_process_metric_value('kernel.all.runnable'),
            self.get_process_metric_value('proc.runq.runnable'),
            self.get_process_metric_value('kernel.all.load')[0],
            self.get_process_metric_value('kernel.all.load')[1],
            self.get_process_metric_value('kernel.all.load')[2],
            self.get_process_metric_value('kernel.all.runnable'),
            self.get_process_metric_value('proc.runq.blocked')),


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


    def setup_metrics(self,pm):
        # remove any unsupported metrics
        for j in range(len(self.disk_metrics)-1, -1, -1):
            try:

                (code, self.disk_metric_name) = pm.pmLookupName(self.disk_metrics[j])
            except pmErr as e:
                self.disk_metrics.remove(self.disk_metrics[j])

        self.disk_metrics_dict=dict((i,self.disk_metrics.index(i)) for i in self.disk_metrics)
        (code, self.disk_metric_name) = pm.pmLookupName(self.disk_metrics)
        check_code (code)
        (code, self.disk_metric_desc) = pm.pmLookupDesc(self.disk_metric_name)
        check_code (code)
        self.disk_metric_value = [0 for i in range(len(self.disk_metrics))]
        self.old_disk_metric_value = [0 for i in range(len(self.disk_metrics))]

    def dump_metrics(self):
        metrics_string = ""
        for i in xrange(len(self.disk_metrics)):
            metrics_string += self.disk_metrics[i]
            metrics_string += " "
        return metrics_string
            
    def get_stats(self):
        get_stats (self.disk_metrics, self.disk_metric_name, self.disk_metric_desc, self.disk_metric_value, self.old_disk_metric_value)

    def get_disk_metric_value(self, idx):
        if idx in self.disk_metrics:
            return self.disk_metric_value[self.disk_metrics_dict[idx]]
        else:
            return 0

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
        print "%6d %6d %6d %6d" % (
            self.get_disk_metric_value('disk.all.read_bytes') / 1024,
            self.get_disk_metric_value('disk.all.read'),
            self.get_disk_metric_value('disk.all.write_bytes') / 1024,
            self.get_disk_metric_value('disk.all.write')),
    def print_detail(self):
        for j in xrange(len(self.disk_metric_name)):
            try:
                (inst, iname) = pm.pmGetInDom(self.disk_metric_desc[j])
                break
            except pmErr as e:
                iname = iname = "X"

        # metric values may be scalars or arrays depending on # of disks
        for j in xrange(get_dimension(self.get_disk_metric_value('disk.dev.read_bytes'))):
            print "%-10s %6d %6d %4d %4d  %6d %6d %4d %4d  %6d %6d %4d %6d %4d" % (
                iname[j],
                get_scalar_value (self.get_disk_metric_value('disk.dev.read_bytes'), j),
                get_scalar_value (self.get_disk_metric_value('disk.dev.read_merge'), j),
                get_scalar_value (self.get_disk_metric_value('disk.dev.read'), j),
                get_scalar_value (self.get_disk_metric_value('disk.dev.blkread'), j),
                get_scalar_value (self.get_disk_metric_value('disk.dev.write_bytes'), j),
                get_scalar_value (self.get_disk_metric_value('disk.dev.write_merge'), j),
                get_scalar_value (self.get_disk_metric_value('disk.dev.write'), j),
                get_scalar_value (self.get_disk_metric_value('disk.dev.blkwrite'), j),
                0, 0, 0, 0, 0)
# ??? replace 0 with required fields

    def print_verbose(self):
        print '%6d %6d %6d %6d %7d %8d %6d %6d' % (
            0 if self.get_disk_metric_value('disk.all.read_bytes') == 0
              else self.get_disk_metric_value('disk.all.read_bytes')/
                   self.get_disk_metric_value('disk.all.read'),
            self.get_disk_metric_value('disk.all.read_merge'),
            self.get_disk_metric_value('disk.all.read'),
            0,
            0 if self.get_disk_metric_value('disk.all.write_bytes') == 0
              else self.get_disk_metric_value('disk.all.write_bytes')/
                   self.get_disk_metric_value('disk.all.write'),
            self.get_disk_metric_value('disk.all.write_merge'),
            self.get_disk_metric_value('disk.all.write'),
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



    def setup_metrics(self,pm):
        # remove any unsupported metrics
        for j in range(len(self.memory_metrics)-1, -1, -1):
            try:

                (code, self.memory_metric_name) = pm.pmLookupName(self.memory_metrics[j])
            except pmErr as e:
                self.memory_metrics.remove(self.memory_metrics[j])

        self.memory_metrics_dict=dict((i,self.memory_metrics.index(i)) for i in self.memory_metrics)
        (code, self.memory_metric_name) = pm.pmLookupName(self.memory_metrics)
        check_code (code)
        (code, self.memory_metric_desc) = pm.pmLookupDesc(self.memory_metric_name)
        check_code (code)
        self.memory_metric_value = [0 for i in range(len(self.memory_metrics))]
        self.old_memory_metric_value = [0 for i in range(len(self.memory_metrics))]

    def dump_metrics(self):
        metrics_string = ""
        for i in xrange(len(self.memory_metrics)):
            metrics_string += self.memory_metrics[i]
            metrics_string += " "
        return metrics_string
            
    def get_stats(self):
        get_stats (self.memory_metrics, self.memory_metric_name, self.memory_metric_desc, self.memory_metric_value, self.old_memory_metric_value)

    def get_memory_metric_value(self, idx):
        if idx in self.memory_metrics:
            return self.memory_metric_value[self.memory_metrics_dict[idx]]
        else:
            return 0

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
        print "%4dM %3dM %3dM %3dM %3dM %3dM " % (
            round(self.get_memory_metric_value('mem.freemem'), 1000),
            round(self.get_memory_metric_value('mem.util.bufmem'), 1000),
            round(self.get_memory_metric_value('mem.util.cached'), 1000),
            round(self.get_memory_metric_value('mem.util.inactive'), 1000),
            round(self.get_memory_metric_value('mem.util.slab'), 1000),
            round(self.get_memory_metric_value('mem.util.mapped'), 1000))
    def print_verbose(self):
        print "%8dM %6dM %6dM %6dM %6dM %6dM %6dM %6dM %6dM %6dM %5dM %5dM %5dM %5dM %6d %6d %6d %6d %6d %6d " % (
            round(self.get_memory_metric_value('mem.physmem'), 1000),
            round(self.get_memory_metric_value('mem.util.used'), 1000),
            round(self.get_memory_metric_value('mem.freemem'), 1000),
            round(self.get_memory_metric_value('mem.util.bufmem'), 1000),
            round(self.get_memory_metric_value('mem.util.cached'), 1000),
            round(self.get_memory_metric_value('mem.util.slab'), 1000),
            round(self.get_memory_metric_value('mem.util.mapped'), 1000),
            round(self.get_memory_metric_value('mem.util.anonpages'), 1000),
            round(self.get_memory_metric_value('mem.util.committed_AS'), 1000),
            round(self.get_memory_metric_value('mem.util.mlocked'), 1000),
            round(self.get_memory_metric_value('mem.util.inactive'), 1000),
            round(self.get_memory_metric_value('mem.util.swapTotal'), 1000),
            round(self.get_memory_metric_value('swap.used'), 1000),
            round(self.get_memory_metric_value('swap.free'), 1000),
            round(self.get_memory_metric_value('swap.pagesin'), 1000),
            round(self.get_memory_metric_value('swap.pagesout'), 1000),
            round(self.get_memory_metric_value('mem.vmstat.pgfault') -
                  self.get_memory_metric_value('mem.vmstat.pgmajfault'), 1000),
            round(self.get_memory_metric_value('mem.vmstat.pgmajfault'), 1000),
            round(self.get_memory_metric_value('mem.vmstat.pgpgin'), 1000),
            round(self.get_memory_metric_value('mem.vmstat.pgpgout'), 1000))


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


    def setup_metrics(self,pm):
        # remove any unsupported metrics
        for j in range(len(self.net_metrics)-1, -1, -1):
            try:

                (code, self.net_metric_name) = pm.pmLookupName(self.net_metrics[j])
            except pmErr as e:
                self.net_metrics.remove(self.net_metrics[j])

        self.net_metrics_dict=dict((i,self.net_metrics.index(i)) for i in self.net_metrics)
        (code, self.net_metric_name) = pm.pmLookupName(self.net_metrics)
        check_code (code)
        (code, self.net_metric_desc) = pm.pmLookupDesc(self.net_metric_name)
        check_code (code)
        self.net_metric_value = [0 for i in range(len(self.net_metrics))]
        self.old_net_metric_value = [0 for i in range(len(self.net_metrics))]

    def dump_metrics(self):
        metrics_string = ""
        for i in xrange(len(self.net_metrics)):
            metrics_string += self.net_metrics[i]
            metrics_string += " "
        return metrics_string
            
    def get_stats(self):
        get_stats (self.net_metrics, self.net_metric_name, self.net_metric_desc, self.net_metric_value, self.old_net_metric_value)

    def get_net_metric_value(self, idx):
        if idx in self.net_metrics:
            return self.net_metric_value[self.net_metrics_dict[idx]]
        else:
            return 0

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
            sum(self.get_net_metric_value('network.interface.in.bytes')) / 1024,
            sum(self.get_net_metric_value('network.interface.in.packets')),
            sum(self.get_net_metric_value('network.interface.out.bytes')) / 1024,
            sum(self.get_net_metric_value('network.interface.out.packets'))),
    def print_verbose(self):
        self.get_net_metric_value('network.interface.in.bytes')[0] = 0 # don't include loopback
        self.get_net_metric_value('network.interface.in.bytes')[1] = 0
        print '%6d %5d %6d %6d %6d %6d %6d %6d %6d %6d %7d' % (
            sum(self.get_net_metric_value('network.interface.in.bytes')) / 1024,
            sum(self.get_net_metric_value('network.interface.in.packets')),
            sum(self.get_net_metric_value('network.interface.in.bytes')) /
            sum(self.get_net_metric_value('network.interface.in.packets')),
            sum(self.get_net_metric_value('network.interface.in.mcasts')),
            sum(self.get_net_metric_value('network.interface.in.compressed')),
            sum(self.get_net_metric_value('network.interface.in.errors')),
            sum(self.get_net_metric_value('network.interface.out.bytes')) / 1024,
            sum(self.get_net_metric_value('network.interface.out.packets')),
            sum(self.get_net_metric_value('network.interface.out.bytes')) /
            sum(self.get_net_metric_value('network.interface.out.packets')),
            sum(self.get_net_metric_value('network.interface.total.mcasts')),
            sum(self.get_net_metric_value('network.interface.out.errors'))),
    def print_detail(self):
        for j in xrange(len(self.get_net_metric_value('network.interface.in.bytes'))):
            for k in xrange(len(self.net_metric_name)):
                try:
                    (inst, iname) = pm.pmGetInDom(self.net_metric_desc[k])
                    break
                except pmErr as e:
                    iname = "X"

            print '%4d %-7s %6d %5d %6d %6d %6d %6d %6d %6d %6d %6d %7d' % (
                j, iname[j],
                self.get_net_metric_value('network.interface.in.bytes')[j] / 1024,
                self.get_net_metric_value('network.interface.in.packets')[j],
                self.divide_check (self.get_net_metric_value('network.interface.in.bytes')[j],
                                   self.get_net_metric_value('network.interface.in.packets')[j]),
                self.get_net_metric_value('network.interface.in.mcasts')[j],
                self.get_net_metric_value('network.interface.in.compressed')[j],
                self.get_net_metric_value('network.interface.in.errors')[j],
                self.get_net_metric_value('network.interface.in.packets')[j],
                self.get_net_metric_value('network.interface.out.packets')[j],
                self.divide_check (self.get_net_metric_value('network.interface.in.packets')[j],
                                   self.get_net_metric_value('network.interface.out.packets')[j]) / 1024,
                    self.get_net_metric_value('network.interface.total.mcasts')[j],
                    self.get_net_metric_value('network.interface.out.compressed')[j])


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
    duration_arg = 0

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
            print sys.argv[0] + ": Unknown option ", sys.argv[i]
            print "Try `" + sys.argv[0] + "--help' for more information."
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
        if not os.path.exists(input_file):
            print input_file, "does not exist"
            sys.exit(1)
        if not os.path.isdir(input_file) or not os.path.exists(input_file + "/pmcollectl.pcp"):
            print input_file, "is not a", me, "playback directory"
            sys.exit(1)
        for line in open(input_file + "/" + me + ".pcp"):
            lol.append(line[:-1].split())
        archive = input_file + "/" + lol[len(lol)-1][2]

        pm = pmContext(pmapi.PM_CONTEXT_ARCHIVE, archive)
    if (pm < 0):
        print "PCP is not running"

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
