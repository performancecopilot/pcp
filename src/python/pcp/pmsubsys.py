#
# Performance Co-Pilot subsystem classes
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

"""Advanced System & Process Monitor using the libpcp Wrapper module

Additional Information:

Performance Co-Pilot Web Site
http://oss.sgi.com/projects/pcp
"""

# ignore line too long, missing docstring, method could be a function,
#        too many public methods
# pylint: disable=C0301 
# pylint: disable=C0111 
# pylint: disable=R0201
# pylint: disable=R0904

import copy
import cpmapi as c_api
from pcp.pmapi import pmErr
from ctypes import c_char_p


# _pmsubsys ---------------------------------------------------------------


class _pmsubsys(object):
    def __init__(self):
        self.metrics = []
        self.metric_pmids = []
        self.metric_descs = []
        self.metric_values = []
        self.metrics_dict = {}
        self.old_metric_values = []
        super(_pmsubsys, self).__init__()

    def init_metrics(self, pcp):
        pass

    def setup_metrics(self, pcp):
        # remove any unsupported metrics
        for j in range(len(self.metrics)-1, -1, -1):
            try:
                self.metric_pmids = pcp.pmLookupName(self.metrics[j])
            except pmErr, e:
                self.metrics.remove(self.metrics[j])

        self.metrics_dict = dict((i, self.metrics.index(i)) for i in self.metrics)
        self.metric_pmids = pcp.pmLookupName(self.metrics)
        self.metric_descs = pcp.pmLookupDescs(self.metric_pmids)
        self.metric_values = [0 for i in range(len(self.metrics))]
        self.old_metric_values = [0 for i in range(len(self.metrics))]
        if hasattr(super(_pmsubsys, self), 'setup_metrics'):
            super (_pmsubsys, self).setup_metrics()

    def dump_metrics(self):
        metrics_string = ""
        for i in xrange(len(self.metrics)):
            metrics_string += self.metrics[i]
            metrics_string += " "
        if hasattr(super(_pmsubsys, self), 'dump_metrics'):
            super (_pmsubsys, self).dump_metrics()
        return metrics_string

    def get_total(self):
        True                        # pylint: disable-msg=W0104
            
    def get_scalar_value(self, var, idx):
        value = self.get_metric_value(var)
        if type(value) != type(int()) and type(value) != type(long()):
            return value[idx]
        else:
            return value

    def get_len(self, var):
        if type(var) != type(int()) and type(var) != type(long()):
            return len(var)
        else:
            return 1

    def get_atom_value(self, metric, atom1, atom2, desc, first): # pylint: disable-msg=R0913
        if desc.contents.sem == c_api.PM_SEM_DISCRETE or desc.contents.sem == c_api.PM_SEM_INSTANT :
            # just use the absolute value as if it were the first value
            first = True

        # value conversion and diff, if required
        atom_type = desc.contents.type
        if atom_type == c_api.PM_TYPE_32:
            if first:
                return atom1.l 
            else:
                return atom1.l - atom2.l
        elif atom_type == c_api.PM_TYPE_U32:
            if first:
                return atom1.ul 
            else:
                return atom1.ul - atom2.ul
        elif atom_type == c_api.PM_TYPE_64:
            if first:
                return atom1.ll 
            else:
                return atom1.ll - atom2.ll
        elif atom_type == c_api.PM_TYPE_U64:
            if first:
                return atom1.ull 
            else:
                return atom1.ull - atom2.ull
        elif atom_type == c_api.PM_TYPE_FLOAT:
            if first:
                return atom1.f 
            else:
                return atom1.f - atom2.f
        elif atom_type == c_api.PM_TYPE_DOUBLE:
            if first:
                return atom1.d 
            else:
                return atom1.d - atom2.d
        elif atom_type == c_api.PM_TYPE_STRING:
            atom_str = c_char_p(atom1.cp)
            return str(atom_str.value)
        else:
            return 0

    def get_stats(self, pcp):
        if len(self.metrics) <= 0:
            raise pmErr
    
        list_type = type([])

        metric_result = pcp.pmFetch(self.metric_pmids)

        if max(self.old_metric_values) == 0:
            first = True
        else:
            first =  False
        # list of metric names
        for i in xrange(len(self.metrics)):
            # list of metric results, one per metric name
            for j in xrange(metric_result.contents.numpmid):
                if (metric_result.contents.get_pmid(j) != self.metric_pmids[i]):
                    continue
                atomlist = []
                # list of instances, one or more per metric.  e.g. there are many 
                # instances for network metrics, one per network interface
                for k in xrange(metric_result.contents.get_numval(j)):
                    atom = pcp.pmExtractValue(metric_result.contents.get_valfmt(j), metric_result.contents.get_vlist(j, k), self.metric_descs[j].contents.type, self.metric_descs[j].contents.type)
                    atomlist.append(atom)

                value = []
                # metric may require a diff to get a per interval value
                for k in xrange(metric_result.contents.get_numval(j)):
                    if type(self.old_metric_values[j]) == list_type:
                        old_val = self.old_metric_values[j][k]
                    else:
                        old_val = self.old_metric_values[j]
                    value.append(self.get_atom_value(self.metrics[i], atomlist[k], old_val, self.metric_descs[j], first))

                self.old_metric_values[j] = copy.copy(atomlist)
                if metric_result.contents.get_numval(j) == 1:
                    if len(value) == 1:
                        self.metric_values[j] = copy.copy(value[0])
                    else:
                        self.metric_values[j] = 0
                elif metric_result.contents.get_numval(j) > 1:
                    self.metric_values[j] = copy.copy(value)
                if hasattr(super(_pmsubsys, self), 'get_stats'):
                    super(_pmsubsys, self).get_stats()


    def get_metric_value(self, idx):
        if idx in self.metrics:
            return self.metric_values[self.metrics_dict[idx]]
        else:
            return 0
        if hasattr(super(_pmsubsys, self), 'get_metric_value'):
            super(_pmsubsys, self).get_metric_value()


# Cpu  -----------------------------------------------------------------


class Cpu(_pmsubsys):
    def __init__(self):
        super(Cpu, self).__init__()
        self.cpu_total = 0
        self.metrics += ['hinv.ncpu', 'kernel.all.cpu.guest',
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


    def get_total(self):
        self.cpu_total = (self.get_metric_value('kernel.all.cpu.nice') +
                          self.get_metric_value('kernel.all.cpu.user') +
                          self.get_metric_value('kernel.all.cpu.intr') +
                          self.get_metric_value('kernel.all.cpu.sys') +
                          self.get_metric_value('kernel.all.cpu.idle') +
                          self.get_metric_value('kernel.all.cpu.steal') +
                          self.get_metric_value('kernel.all.cpu.irq.hard') +
                          self.get_metric_value('kernel.all.cpu.irq.soft') )


# Interrupt  -----------------------------------------------------------------


class Interrupt(_pmsubsys):
    def __init__(self):
        super(Interrupt, self).__init__()


    def init_metrics(self, pcp):
        int_list = pcp.pmGetChildren("kernel.percpu.interrupts")
        for i in xrange(len(int_list)):
            self.metrics.append('kernel.percpu.interrupts.' + int_list[i])


# Disk  -----------------------------------------------------------------


class Disk(_pmsubsys):
    def __init__(self):
        super(Disk, self).__init__()
        self.metrics += ['disk.all.read', 'disk.all.write',
                         'disk.all.read_bytes', 'disk.all.write_bytes',
                         'disk.all.read_merge', 'disk.all.write_merge',
                         'disk.dev.avactive', 'disk.dev.aveq',
                         'disk.dev.blkread', 'disk.dev.blkwrite',
                         'disk.dev.read', 'disk.dev.read_bytes',
                         'disk.dev.read_merge',
                         'disk.dev.write','disk.dev.write_bytes',
                         'disk.dev.write_merge',
                         'disk.partitions.read', 'disk.partitions.write',
                         'disk.partitions.read_bytes',
                         'disk.partitions.write_bytes'
                         ]


# Memory  -----------------------------------------------------------------


class Memory(_pmsubsys):
    def __init__(self):
        super(Memory, self).__init__()
        self.metrics += ['mem.freemem', 'mem.physmem', 'mem.util.anonpages',
                         'mem.util.bufmem',
                         'mem.util.cached', 'mem.util.commitLimit',
                         'mem.util.committed_AS', 'mem.util.dirty',
                         'mem.util.free', 'mem.util.inactive',
                         'mem.util.inactive', 'mem.util.mapped',
                         'mem.util.mlocked', 'mem.util.other',
                         'mem.util.shared', 'mem.util.shmem', 'mem.util.slab',
                         'mem.util.slabReclaimable', 'mem.util.swapFree',
                         'mem.util.swapTotal', 'mem.util.used',
                         'mem.vmstat.allocstall', 'mem.vmstat.pgfault',
                         'mem.vmstat.pginodesteal',
                         'mem.vmstat.pgmajfault', 'mem.vmstat.pgpgin',
                         'mem.vmstat.pgpgout', 'mem.vmstat.pswpin',
                         'mem.vmstat.pswpout', 'mem.vmstat.slabs_scanned',
                         'swap.free', 'swap.pagesin',
                         'swap.pagesout', 'swap.used' ]


# Net  -----------------------------------------------------------------


class Net(_pmsubsys):
    def __init__(self):
        super(Net, self).__init__()
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
                         'network.icmp.inerrors', 'network.icmp.inmsgs',
                         'network.icmp.outmsgs', 'network.interface.collisions',
                         'network.interface.in.drops',
                         'network.interface.out.drops',
                         'network.ip.forwdatagrams', 'network.ip.indelivers',
                         'network.ip.inreceives', 'network.ip.outrequests',
                         'network.tcp.activeopens',
                         'network.tcp.inerrs', 'network.tcp.insegs',
                         'network.tcp.outrsts', 'network.tcp.outsegs',
                         'network.tcp.passiveopens', 'network.tcp.retranssegs',
                         'network.udp.inerrors', 'network.udp.indatagrams',
                         'network.udp.outdatagrams', 'network.udp.noports' ]


# Proc  -----------------------------------------------------------------


class Proc(_pmsubsys):
    def __init__(self):
        super(Proc, self).__init__()
        self.metrics += ['proc.id.egid', 'proc.id.euid', 'proc.id.fsgid',
                         'proc.id.fsuid', 'proc.id.gid', 'proc.id.sgid',
                         'proc.id.suid', 'proc.id.uid', 'proc.io.write_bytes',
                         'proc.memory.rss', 'proc.memory.textrss',
                         'proc.memory.vmdata', 'proc.memory.vmlib',
                         'proc.memory.vmsize', 'proc.memory.vmstack',
                         'proc.nprocs', 'proc.psinfo.cmd',
                         'proc.psinfo.exit_signal', 'proc.psinfo.flags',
                         'proc.psinfo.maj_flt',
                         'proc.psinfo.minflt', 'proc.psinfo.nice',
                         'proc.psinfo.nswap',
                         'proc.psinfo.pid', 'proc.psinfo.ppid',
                         'proc.psinfo.priority', 'proc.psinfo.processor',
                         'proc.psinfo.rss', 'proc.psinfo.start_time',
                         'proc.psinfo.stime','proc.psinfo.utime',
                         'proc.runq.runnable', 'proc.runq.sleeping',
                         'proc.runq.blocked', 'proc.runq.defunct',
                         'proc.schedstat.cpu_time',
                         'process.interface.out.errors']


# subsys  -----------------------------------------------------------------


class Subsys(_pmsubsys):
    True                        # pylint: disable-msg=W0104
