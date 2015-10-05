#!/usr/bin/pcp python
#
# Copyright (C) 2014-2015 Red Hat.
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
# pylint: disable=C0103,R0914,R0902
""" Display NUMA memory allocation statistucs """

import os
import sys
from pcp import pmapi
from cpmapi import PM_TYPE_U64

if sys.version >= '3':
    long = int  # python2 to python3 portability (no long() in python3)

class NUMAStat(object):
    """ Gives a short summary of per-node NUMA memory information.

        Knows about some of the default PCP arguments - can function
        using remote hosts or historical data, using the timezone of
        the metric source, at an offset within an archive, and so on.
    """

    def __init__(self):
        """ Construct object - prepare for command line handling """
        self.opts = self.options()
        self.context = None
        self.width = 0

    def resize(self):
        """ Find a suitable display width limit """
        if self.width == 0:
            if not sys.stdout.isatty():
                self.width = 1000000000        # mimic numastat(1) here
            else:
                (rows, width) = os.popen('stty size', 'r').read().split()
                self.width = int(width)
            self.width = int(os.getenv('NUMASTAT_WIDTH', self.width))
        if self.width < 32:
            self.width = 32

    def option(self, opt, optarg, index):
        """ Perform setup for an individual command line option """
        if (opt == 'w'):
            self.width = int(optarg)

    def options(self):
        """ Setup default command line argument option handling """
        opts = pmapi.pmOptions()
        opts.pmSetOptionCallback(self.option)
        opts.pmSetShortOptions("w:V?")
        opts.pmSetLongOptionHeader("Options")
        opts.pmSetLongOption("width", 1, 'w', "N", "limit the display width")
        opts.pmSetLongOptionVersion()
        opts.pmSetLongOptionHelp()
        return opts

    def extract(self, descs, insts, result):
        """ Extract the set of metric values from a given pmResult """
        values = [[]]
        for metrics in range(len(descs)):
            values.append([])
            for nodes in range(len(insts)):
                if result.contents.get_numval(metrics) > 0:
                    atom = self.context.pmExtractValue(
                                result.contents.get_valfmt(metrics),
                                result.contents.get_vlist(metrics, nodes),
                                descs[metrics].contents.type, PM_TYPE_U64)
                    values[metrics].append(long(atom.ull))
                else:
                    values[metrics].append(long(0))
        return values

    def execute(self):
        """ Using a PMAPI context (could be either host or archive),
            fetch and report per-node values related to NUMA memory.
        """
        metrics = ('mem.numa.alloc.hit', 'mem.numa.alloc.miss',
                   'mem.numa.alloc.foreign', 'mem.numa.alloc.interleave_hit',
                   'mem.numa.alloc.local_node',        'mem.numa.alloc.other_node')

        pmids = self.context.pmLookupName(metrics)
        descs = self.context.pmLookupDescs(pmids)
        (insts, nodes) = self.context.pmGetInDom(descs[0])
        result = self.context.pmFetch(pmids)
        values = self.extract(descs, insts, result)
        self.context.pmFreeResult(result)
        self.report(metrics, nodes, values)

    def report(self, metrics, nodes, values):
        """ Given per-node metric names and values, dump 'em like numastat(1)
            Nodes is a list of strings, values is a list of lists of values.
        """
        columns = len(nodes) * 16
        if (columns == 0):
            print("No NUMA nodes found, exiting")
            sys.exit(1)
        self.resize()
        maxnodes = int((self.width - 16) / 16)
        if maxnodes > len(nodes):        # just an initial header suffices
            header = '%-16s' % ''
            for node in nodes:
                header += '%16s' % node
            print(header)
        for index in range(len(metrics)):
            title = self.prefix(metrics[index])
            self.metric(title, nodes, values[index], maxnodes)

    def metric(self, prefix, nodes, values, maxnodes):
        """ Given one metric and its per-node values, produce one or more
            lines of output with the values, each line node-name prefixed
            and with a new node header for each.
        """
        done = 0
        while done < len(nodes):
            header = '%-16s' % ''
            window = '%-16s' % prefix
            for index in range(maxnodes):
                current = done + index
                if current < len(nodes):
                    header += '%16s' % (nodes[current])
                    window += '%16d' % (values[current])
            if done > maxnodes or maxnodes <= len(nodes):
                print('%s\n%s' % (header, window))
            else:
                print('%s' % window)
            done += maxnodes

    def prefix(self, metric):
        """ Transform the PCP metric names into the reported sub-headings """
        title = metric[15:]
        if '_' not in title:
            title = 'numa_' + title
        return title

    def connect(self):
        """ Establish a PMAPI context to archive, host or local, via args """
        self.context = pmapi.pmContext.fromOptions(self.opts, sys.argv)

if __name__ == '__main__':
    try:
        NUMASTAT = NUMAStat()
        NUMASTAT.connect()
        NUMASTAT.execute()
    except pmapi.pmErr as error:
        print("numastat:",  error.message())
    except pmapi.pmUsageErr as usage:
        usage.message()
    except KeyboardInterrupt:
        pass
