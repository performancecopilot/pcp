#
# Performance Co-Pilot subsystem classes
#
# Copyright (C) 2013-2015 Red Hat.
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

"""Advanced System & Process Monitor using the libpcp Wrapper module

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

import copy
import cpmapi as c_api
from pcp.pmapi import pmErr, timeval
from ctypes import c_char_p

# python version information and compatibility
import sys
if sys.version > '3':
    integer_types = (int,)
else:
    integer_types = (int, long,)


# Subsystem  ---------------------------------------------------------------


class Subsystem(object):
    def __init__(self):
        self.metrics = []
        self._timestamp = timeval(0, 0)
        self.diff_metrics = []
        self.metric_pmids = []
        self.metric_descs = []
        self.metric_values = []
        self.metrics_dict = {}
        self._last_values = []

    def _R_timestamp(self):
        return self._timestamp

    timestamp = property(_R_timestamp, None, None, None)

    def setup_metrics(self, pcp):
        # remove any unsupported metrics
        name_pattern = self.metrics[0].split(".")[0] + ".*"
        for j in range(len(self.metrics)-1, -1, -1):
            try:
                self.metric_pmids = pcp.pmLookupName(self.metrics[j])
            except pmErr as e:
                self.metrics.remove(self.metrics[j])

        if (len(self.metrics) == 0):
            raise pmErr(c_api.PM_ERR_NAME, "", name_pattern)
        self.metrics_dict = dict((i, self.metrics.index(i)) for i in self.metrics)
        self.metric_pmids = pcp.pmLookupName(self.metrics)
        self.metric_descs = pcp.pmLookupDescs(self.metric_pmids)
        self.metric_values = [0 for i in range(len(self.metrics))]
        self._last_values = [0 for i in range(len(self.metrics))]

    def dump_metrics(self):
        metrics_string = ""
        for i in range(len(self.metrics)):
            metrics_string += self.metrics[i]
            metrics_string += " "
        return metrics_string

    def get_scalar_value(self, var, idx):
        if type(var) == type(u'') or type(var) == type(b''):
            value = self.get_metric_value(var)
        else:
            value = self.metric_values[var]
        if not isinstance(value, integer_types):
            return value[idx]
        else:
            return value

    def get_metric_value(self, idx):
        if idx in self.metrics:
            return self.metric_values[self.metrics_dict[idx]]
        else:
            return 0

    def get_old_scalar_value(self, var, idx):
        aidx = 0
        if var in self.metrics:
            aidx = self.metrics_dict[var]
            aval = self._last_values[aidx]
        else:
            return 0
        val = self.get_atom_value(aval[idx], None, self.metric_descs[aidx], False)
        if isinstance(val, integer_types):
            return val
        else:
            return val[idx]

    def get_len(self, var):
        if not isinstance(var, integer_types):
            return len(var)
        else:
            return 1

    def get_atom_value(self, atom1, atom2, desc, want_diff): # pylint: disable-msg=R0913
        # value conversion and diff, if required
        atom_type = desc.type
        if atom2 == None:
            want_diff = False
        if atom_type == c_api.PM_TYPE_32:
            if want_diff:
                return atom1.l - atom2.l
            else:
                return atom1.l 
        elif atom_type == c_api.PM_TYPE_U32:
            if want_diff:
                return atom1.ul - atom2.ul
            else:
                return atom1.ul 
        elif atom_type == c_api.PM_TYPE_64:
            if want_diff:
                return atom1.ll - atom2.ll
            else:
                return atom1.ll 
        elif atom_type == c_api.PM_TYPE_U64:
            if want_diff:
                return atom1.ull - atom2.ull
            else:
                return atom1.ull 
        elif atom_type == c_api.PM_TYPE_FLOAT:
            if want_diff:
                return atom1.f - atom2.f
            else:
                return atom1.f 
        elif atom_type == c_api.PM_TYPE_DOUBLE:
            if want_diff:
                return atom1.d - atom2.d
            else:
                return atom1.d 
        elif atom_type == c_api.PM_TYPE_STRING:
            atom_str = c_char_p(atom1.cp)
            return str(atom_str.value.decode())
        else:
            return 0

    def get_stats(self, pcp):
        if len(self.metrics) <= 0:
            raise pmErr
    
        list_type = type([])
        if self._timestamp.tv_sec == 0:
            first = True
        else:
            first = False

        try:
            metric_result = pcp.pmFetch(self.metric_pmids)
            self._timestamp = metric_result.contents.timestamp
        except pmErr as e:
            self._timestamp = timeval(0, 0)
            raise e

        # list of metric names
        for i in range(len(self.metrics)):
            # list of metric results, one per metric name
            for j in range(metric_result.contents.numpmid):
                if (metric_result.contents.get_pmid(j) != self.metric_pmids[i]):
                    continue
                atomlist = []
                # list of instances, one or more per metric.  e.g. there are many 
                # instances for network metrics, one per network interface
                for k in range(metric_result.contents.get_numval(j)):
                    atom = pcp.pmExtractValue(metric_result.contents.get_valfmt(j), metric_result.contents.get_vlist(j, k), self.metric_descs[j].type, self.metric_descs[j].type)
                    atomlist.append(atom)

                value = []
                # metric may require a diff to get a per interval value
                for k in range(metric_result.contents.get_numval(j)):
                    if type(self._last_values[j]) == list_type:
                        try:
                            lastval = self._last_values[j][k]
                        except IndexError:
                            lastval = None
                    else:
                        lastval = self._last_values[j]
                    if first:
                        want_diff = False
                    elif self.metrics[j] in self.diff_metrics:
                        want_diff = True
                    elif (self.metric_descs[j].sem == c_api.PM_SEM_DISCRETE
                          or self.metric_descs[j].sem == c_api.PM_SEM_INSTANT) :
                        want_diff = False
                    else:
                        want_diff = True
                    value.append(self.get_atom_value(atomlist[k], lastval, self.metric_descs[j], want_diff))

                self._last_values[j] = copy.copy(atomlist)
                if metric_result.contents.get_numval(j) == 1:
                    if len(value) == 1:
                        self.metric_values[j] = copy.copy(value[0])
                    else:
                        self.metric_values[j] = 0
                elif metric_result.contents.get_numval(j) > 1:
                    self.metric_values[j] = copy.copy(value)


# Processor  --------------------------------------------------------------

    def init_processor_metrics(self):
        self.cpu_total = 0
        self.metrics += ['hinv.ncpu', 'hinv.cpu.clock', 
                         'kernel.all.cpu.idle', 'kernel.all.cpu.intr',
                         'kernel.all.cpu.irq.hard', 'kernel.all.cpu.irq.soft',
                         'kernel.all.cpu.nice', 'kernel.all.cpu.steal',
                         'kernel.all.cpu.sys', 'kernel.all.cpu.user',
                         'kernel.all.cpu.wait.total', 'kernel.all.intr',
                         'kernel.all.load', 'kernel.all.pswitch',
                         'kernel.all.uptime', 'kernel.percpu.cpu.nice',
                         'kernel.percpu.cpu.user', 'kernel.percpu.cpu.intr',
                         'kernel.percpu.cpu.sys', 'kernel.percpu.cpu.steal',
                         'kernel.percpu.cpu.irq.hard',
                         'kernel.percpu.cpu.irq.soft',
                         'kernel.percpu.cpu.wait.total',
                         'kernel.percpu.cpu.idle', 'kernel.all.nprocs',
                         'kernel.all.runnable',
                         # multiple inheritance?
                         'proc.runq.blocked', 'proc.runq.defunct', 
                         'proc.runq.runnable', 'proc.runq.sleeping']
        self.diff_metrics = ['kernel.all.uptime']


    def get_total(self):
        self.cpu_total = (self.get_metric_value('kernel.all.cpu.nice') +
                          self.get_metric_value('kernel.all.cpu.user') +
                          self.get_metric_value('kernel.all.cpu.intr') +
                          self.get_metric_value('kernel.all.cpu.sys') +
                          self.get_metric_value('kernel.all.cpu.idle') +
                          self.get_metric_value('kernel.all.cpu.steal') +
                          self.get_metric_value('kernel.all.cpu.irq.hard') +
                          self.get_metric_value('kernel.all.cpu.irq.soft') )

# Disk  -----------------------------------------------------------------

    def init_disk_metrics(self):
        self.metrics += ['disk.all.read', 'disk.all.write',
                         'disk.all.read_bytes', 'disk.all.write_bytes',
                         'disk.all.read_merge', 'disk.all.write_merge',
                         'disk.dev.avactive',
                         'disk.dev.blkread', 'disk.dev.blkwrite',
                         'disk.dev.read', 'disk.dev.read_bytes',
                         'disk.dev.read_merge', 'disk.dev.total',
                         'disk.dev.write','disk.dev.write_bytes',
                         'disk.dev.write_merge',
                         'disk.partitions.blkread', 'disk.partitions.blkwrite',
                         'disk.partitions.read', 'disk.partitions.write',
                         'hinv.map.lvname'
                         ]

# Memory  -----------------------------------------------------------------

    def init_memory_metrics(self):
        self.metrics += ['mem.freemem', 'mem.physmem', 'mem.util.anonpages',
                         'mem.util.bufmem',
                         'mem.util.cached', 'mem.util.commitLimit',
                         'mem.util.committed_AS',
                         'mem.util.inactive',
                         'mem.util.inactive', 'mem.util.mapped',
                         'mem.util.mlocked',
                         'mem.util.shmem', 'mem.util.slab',
                         'mem.util.swapFree',
                         'mem.util.swapTotal', 'mem.util.used',
                         'mem.vmstat.allocstall', 'mem.vmstat.pgfault',
                         'mem.vmstat.pginodesteal',
                         'mem.vmstat.pgmajfault', 'mem.vmstat.pgpgin',
                         'mem.vmstat.pgpgout', 'mem.vmstat.pswpin',
                         'mem.vmstat.pswpout', 'mem.vmstat.slabs_scanned',
                         'swap.free', 'swap.pagesin',
                         'swap.pagesout', 'swap.used' ]


# Network  -----------------------------------------------------------------

    def init_network_metrics(self):
        self.metrics += ['network.interface.in.bytes',
                         'network.interface.in.packets',
                         'network.interface.out.bytes',
                         'network.interface.out.packets',
                         'network.interface.in.mcasts',
                         'network.interface.total.mcasts',
                         'network.interface.in.compressed',
                         'network.interface.out.compressed',
                         'network.interface.in.errors',
                         'network.interface.out.errors',
                         'network.icmp.inmsgs',
                         'network.icmp.outmsgs',
                         'network.ip.forwdatagrams', 'network.ip.indelivers',
                         'network.ip.inreceives', 'network.ip.outrequests',
                         'network.tcp.activeopens',
                         'network.tcp.insegs',
                         'network.tcp.outsegs',
                         'network.tcp.passiveopens',
                         'network.udp.indatagrams',
                         'network.udp.outdatagrams' ]

# Process  -----------------------------------------------------------------

    def init_process_metrics(self):
        self.metrics += ['proc.id.uid', 'proc.id.uid_nm',
                         'proc.memory.datrss', 'proc.memory.librss',
                         'proc.memory.textrss', 'proc.memory.vmstack',
                         'proc.nprocs', 'proc.psinfo.cmd',
                         'proc.psinfo.maj_flt', 'proc.psinfo.minflt',
                         'proc.psinfo.pid',
                         'proc.psinfo.rss', 'proc.psinfo.sname',
                         'proc.psinfo.stime', 'proc.psinfo.threads',
                         'proc.psinfo.utime', 'proc.psinfo.vsize',
                         'proc.runq.runnable', 'proc.runq.sleeping',
                         'proc.runq.blocked', 'proc.runq.defunct',
                         ]
        self.diff_metrics += ['proc.psinfo.rss', 'proc.psinfo.vsize']

# Interrupt  --------------------------------------------------------------

    def init_interrupt_metrics(self):
        self.metrics += ['kernel.percpu.interrupts.MCP',
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
                         'kernel.percpu.interrupts.line46',
                         'kernel.percpu.interrupts.line45',
                         'kernel.percpu.interrupts.line44',
                         'kernel.percpu.interrupts.line43',
                         'kernel.percpu.interrupts.line42',
                         'kernel.percpu.interrupts.line41',
                         'kernel.percpu.interrupts.line40',
                         'kernel.percpu.interrupts.line23',
                         'kernel.percpu.interrupts.line19',
                         'kernel.percpu.interrupts.line18',
                         'kernel.percpu.interrupts.line16',
                         'kernel.percpu.interrupts.line12',
                         'kernel.percpu.interrupts.line9',
                         'kernel.percpu.interrupts.line8',
                         'kernel.percpu.interrupts.line1',
                         'kernel.percpu.interrupts.line0',
                         ]

