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

import copy
from pcp import *
from ctypes import *


def check_code (code):
    if (code < 0):
        print pmErrStr(code)
        sys.exit(1)


# _pmsubsys ---------------------------------------------------------------


class _pmsubsys(object):
    def __init__(self):
        self.metrics = []
        self.metric_pmids = []
        self.metric_descs = []
        self.metric_values = []
        self.metrics_dict = {}
        self.old_metric_values = []
        super (_pmsubsys, self).__init__()

    def setup_metrics(self,pm):
        # remove any unsupported metrics
        for j in range(len(self.metrics)-1, -1, -1):
            try:

                (code, self.metric_pmids) = pm.pmLookupName(self.metrics[j])
            except pmErr, e:
                self.metrics.remove(self.metrics[j])

        self.metrics_dict=dict((i,self.metrics.index(i)) for i in self.metrics)
        (code, self.metric_pmids) = pm.pmLookupName(self.metrics)
        check_code (code)
        (code, self.metric_descs) = pm.pmLookupDesc(self.metric_pmids)
        check_code (code)
        self.metric_values = [0 for i in range(len(self.metrics))]
        self.old_metric_values = [0 for i in range(len(self.metrics))]
        if hasattr(super(_pmsubsys, self), 'setup_metrics'):
            super (_pmsubsys, self).setup_metrics(arg)

    def dump_metrics(self):
        metrics_string = ""
        for i in xrange(len(self.metrics)):
            metrics_string += self.metrics[i]
            metrics_string += " "
        if hasattr(super(_pmsubsys, self), 'dump_metrics'):
            super (_pmsubsys, self).dump_metrics(arg)
        return metrics_string

    def get_total(self):
        True
            
    def get_atom_value (self, metric, atom1, atom2, desc, first):
        if desc.contents.sem == pmapi.PM_SEM_DISCRETE or desc.contents.sem == pmapi.PM_SEM_INSTANT :
            # just use the absolute value as if it were the first value
            first = True

        # value conversion and diff, if required
        type = desc.contents.type
        if type == pmapi.PM_TYPE_32:
            if first:
                return atom1.l 
            else:
                return atom1.l - atom2.l
        elif type == pmapi.PM_TYPE_U32:
            if first:
                return atom1.ul 
            else:
                return atom1.ul - atom2.ul
        elif type == pmapi.PM_TYPE_64:
            if first:
                return atom1.ll 
            else:
                return atom1.ll - atom2.ll
        elif type == pmapi.PM_TYPE_U64:
            if first:
                return atom1.ull 
            else:
                return atom1.ull - atom2.ull
        elif type == pmapi.PM_TYPE_FLOAT:
            if first:
                return atom1.f 
            else:
                return atom1.f - atom2.f
        elif type == pmapi.PM_TYPE_DOUBLE:
            if first:
                return atom1.d 
            else:
                return atom1.d - atom2.d
        else:
            return 0

    def get_stats(self, pm):
        if len(self.metrics) <= 0:
            print "This subsystem is not implemented yet"
            return
    
        list_type = type([])

        try:
            (code, metric_result) = pm.pmFetch(self.metric_pmids)
            check_code (code)
        except pmErr, e:
            if str(e).find("PM_ERR_EOL") != -1:
                print "\nReached end of archive"
                sys.exit(1)

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
                    (code, atom) = pm.pmExtractValue(metric_result.contents.get_valfmt(j), metric_result.contents.get_vlist(j, k), self.metric_descs[j].contents.type, self.metric_descs[j].contents.type)
                    atomlist.append(atom)

                value = []
                # metric may require a diff to get a per interval value
                for k in xrange(metric_result.contents.get_numval(j)):
                    if type(self.old_metric_values[j]) == list_type:
                        old_val = self.old_metric_values[j][k]
                    else:
                        old_val = self.old_metric_values[j]
                    value.append (self.get_atom_value(self.metrics[i], atomlist[k], old_val, self.metric_descs[j], first))

                self.old_metric_values[j] = copy.copy(atomlist)
                if metric_result.contents.get_numval(j) == 1:
                    if len(value) == 1:
                        self.metric_values[j] = copy.copy(value[0])
                    else:
                        self.metric_values[j] = 0
                elif metric_result.contents.get_numval(j) > 1:
                    self.metric_values[j] = copy.copy(value)
                if hasattr(super(_pmsubsys, self), 'get_stats'):
                    super (_pmsubsys, self).get_stats(arg)


    def get_metric_value(self, idx):
        if idx in self.metrics:
            return self.metric_values[self.metrics_dict[idx]]
        else:
            return 0
        if hasattr(super(_pmsubsys, self), 'get_metric_value'):
            super (_pmsubsys, self).get_metric_value(arg)


# cpu  -----------------------------------------------------------------


class cpu(_pmsubsys):
    def __init__(self):
        super(cpu, self).__init__()
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
                         'proc.runq.runnable', 'proc.runq.blocked',
                         'proc.runq.defunct']

    def get_total(self):
        self.cpu_total = (self.get_metric_value('kernel.all.cpu.nice') +
                          self.get_metric_value('kernel.all.cpu.user') +
                          self.get_metric_value('kernel.all.cpu.intr') +
                          self.get_metric_value('kernel.all.cpu.sys') +
                          self.get_metric_value('kernel.all.cpu.idle') +
                          self.get_metric_value('kernel.all.cpu.steal') +
                          self.get_metric_value('kernel.all.cpu.irq.hard') +
                          self.get_metric_value('kernel.all.cpu.irq.soft') )


# interrupt  -----------------------------------------------------------------


class interrupt(_pmsubsys):
    def __init__(self):
        super(interrupt, self).__init__()


    def setup_metrics(self,pm):
        int_list = pm.pmGetChildren("kernel.percpu.interrupts")
        for i in xrange(len(int_list)):
            self.metrics.append('kernel.percpu.interrupts.' + int_list[i])
        super(interrupt, self).setup_metrics(pm)


# disk  -----------------------------------------------------------------


class disk(_pmsubsys):
    def __init__(self):
        super(disk, self).__init__()
        self.metrics += ['disk.all.read', 'disk.all.write',
                         'disk.all.read_bytes', 'disk.all.write_bytes',
                         'disk.all.read_merge', 'disk.all.write_merge',
                         'disk.dev.avactive', 'disk.dev.aveq',
                         'disk.dev.blkread', 'disk.dev.blkwrite',
                         'disk.dev.read', 'disk.dev.read_bytes',
                         'disk.dev.read_merge',
                         'disk.dev.write','disk.dev.write_bytes',
                         'disk.dev.write_merge',
                         ]


# memory  -----------------------------------------------------------------


class memory(_pmsubsys):
    def __init__(self):
        super(memory, self).__init__()
        self.metrics += ['mem.freemem', 'mem.physmem', 'mem.util.anonpages',
                         'mem.util.buffers', 'mem.util.bufmem',
                         'mem.util.cached', 'mem.util.commitLimit',
                         'mem.util.committed_AS', 'mem.util.dirty',
                         'mem.util.free', 'mem.util.inactive',
                         'mem.util.inactive', 'mem.util.mapped',
                         'mem.util.mlocked', 'mem.util.other',
                         'mem.util.shared', 'mem.util.slab',
                         'mem.util.slabReclaimable', 'mem.util.swapFree',
                         'mem.util.swapTotal', 'mem.util.used',
                         'mem.vmstat.allocstall', 'mem.vmstat.pgfault',
                         'mem.vmstat.pginodesteal',
                         'mem.vmstat.pgmajfault', 'mem.vmstat.pgpgin',
                         'mem.vmstat.pgpgout', 'mem.vmstat.pswpin',
                         'mem.vmstat.pswpout', 'mem.vmstat.slabs_scanned',
                         'swap.free', 'swap.pagesin',
                         'swap.pagesout', 'swap.used' ]


# net  -----------------------------------------------------------------


class net(_pmsubsys):
    def __init__(self):
        super(net, self).__init__()
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
                         'network.ip.inreceives', 'network.tcp.activeopens',
                         'network.tcp.inerrs', 'network.tcp.insegs',
                         'network.tcp.outrsts', 'network.tcp.outsegs',
                         'network.tcp.passiveopens', 'network.tcp.retranssegs',
                         'network.udp.inerrors', 'network.udp.indatagrams',
                         'network.udp.outdatagrams', 'network.udp.noports' ]


# proc  -----------------------------------------------------------------


class proc(_pmsubsys):
    def __init__(self):
        super(proc, self).__init__()
        self.metrics += ['proc.id.egid', 'proc.id.euid', 'proc.id.fsgid',
                         'proc.id.fsuid', 'proc.id.gid', 'proc.id.sgid',
                         'proc.id.suid', 'proc.id.uid', 'proc.io.write_bytes',
                         'proc.memory.rss', 'proc.memory.textrss',
                         'proc.memory.vmdata', 'proc.memory.vmlib',
                         'proc.memory.vmsize', 'proc.memory.vmstack',
                         'proc.nprocs', 'proc.psinfo.cmd',
                         'proc.psinfo.exit_signal', 'proc.psinfo.flags',
                         'proc.psinfo.minflt', 'proc.psinfo.nice',
                         'proc.psinfo.pid', 'proc.psinfo.ppid',
                         'proc.psinfo.priority', 'proc.psinfo.processor',
                         'proc.psinfo.rss', 'proc.psinfo.start_time',
                         'proc.runq.runnable', 'proc.runq.sleeping',
                         'proc.runq.blocked', 'proc.runq.defunct',
                         'proc.schedstat.cpu_time',
                         'process.interface.out.errors']


# subsys  -----------------------------------------------------------------


class subsys(_pmsubsys):
    True
