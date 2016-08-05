#!/usr/bin/env pmpython
#
# Copyright (C) 2016 fujitsu (wulm.fnst@cn.fujitsu.com).
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
""" Provide information on IPC facilities """

import sys
from pcp import pmapi
from cpmapi import PM_TYPE_U32

class Ipcs(object):
    """ provides information on the inter-process communication 
        facilities for which the calling process has read access.
    """

    def __init__(self):
        """ Construct object - prepare for command line handling """
        self.show_limit = 0
        self.show_summary = 0
        self.opts = self.options()
        self.context = None

    def options(self):
        """ Setup default command line argument option handling """
        opts = pmapi.pmOptions()
        opts.pmSetOptionCallback(self.option)
        opts.pmSetShortOptions("luV?")
        opts.pmSetLongOptionHeader("Options")
        # other options will to do(wulm.fnst@cn.fujitsu.com)
        opts.pmSetLongOption("limits", 0, 'l', '', "show resource limits")
        opts.pmSetLongOption("summary", 0, 'u', '', "show status summary")
        opts.pmSetLongOptionVersion()
        opts.pmSetLongOptionHelp()
        return opts

    def option(self, opt, optarg, index):
        """ Perform setup for an individual command line option """
        if opt == 'l':
            self.show_limit = 1
        elif opt == 'u':
            self.show_summary = 1

    def extract(self, descs, result):
        """ Extract the set of metric values from a given pmResult """
        values = []
        for index in range(len(descs)):
            if result.contents.get_numval(index) > 0:
                atom = self.context.pmExtractValue(
                                result.contents.get_valfmt(index),
                                result.contents.get_vlist(index, 0),
                                descs[index].contents.type, PM_TYPE_U32)
                values.append(int(atom.ul))
            else:
                values.append(int(0))
        return values

    def execute(self):
        """ Using a PMAPI context (could be either host or archive),
            fetch and report a fixed set of values related to ipc.
        """
        metrics_limit = ('ipc.msg.max_msgqid',  'ipc.msg.max_msgsz',
                         'ipc.msg.max_defmsgq', 'ipc.shm.max_seg',
                         'ipc.shm.max_segsz',   'ipc.shm.max_shmsys',
                         'ipc.shm.min_segsz',   'ipc.sem.max_semid',
                         'ipc.sem.max_perid',   'ipc.sem.num_undo',
                         'ipc.sem.max_ops',     'ipc.sem.max_semval')

        metrics_summary = ('ipc.msg.used_queues', 'ipc.msg.tot_msg',
                           'ipc.msg.tot_bytes', 'ipc.shm.used_ids',
                           'ipc.shm.tot',       'ipc.shm.rss',
                           'ipc.shm.swp',       'ipc.shm.swap_attempts',
                           'ipc.shm.swap_successes',
                           'ipc.sem.used_sem',  'ipc.sem.tot_sem')
        pmids_limit = self.context.pmLookupName(metrics_limit)
        descs_limit = self.context.pmLookupDescs(pmids_limit)

        pmids_summary = self.context.pmLookupName(metrics_summary)
        descs_summary = self.context.pmLookupDescs(pmids_summary)

        if self.show_limit == 1:
            result_limit = self.context.pmFetch(pmids_limit)
            values_limit = self.extract(descs_limit, result_limit)
            self.context.pmFreeResult(result_limit)
            self.report(values_limit)

        elif self.show_summary == 1:
            result_summary = self.context.pmFetch(pmids_summary)
            values_summary = self.extract(descs_summary, result_summary)
            self.context.pmFreeResult(result_summary)
            self.report(values_summary)

    def get_pagesize(self):
        metric_pagesize = ("hinv.pagesize")
        pmid_pagesize = self.context.pmLookupName(metric_pagesize)
        desc_pagesize = self.context.pmLookupDescs(pmid_pagesize)
        result = self.context.pmFetch(pmid_pagesize)
        pagesize = self.extract(desc_pagesize, result)
        return pagesize[0]

    def report(self, values):
        kb = 1024
        pagesize = self.get_pagesize() 
        pagesize_kb = pagesize / kb
        if self.show_limit == 1:
            print("\n------ Messages Limits --------")
            print("max queues system wide = %u" %values[0])
            print("max size of message (bytes) = %u" %values[1])
            print("default max size of queue (bytes) = %u" %values[2])
            print("\n------ Shared Memory Limits --------")
            print("max number of segments = %u" %values[3])
            print("max seg size (kbytes) = %u" %(values[4] / kb))
            print("max total shared memory (kbytes) = %u" %(values[5] * pagesize_kb))
            print("min seg size (bytes) = %u" %values[6])
            print("\n------ Semaphore Limits --------")
            print("max number of arrays = %u" %values[7])
            print("max semaphores per array = %u" %values[8])
            print("max semaphores system wide = %u" %values[9])
            print("max ops per semop call = %u" %values[10])
            print("semaphore max value = %u" %values[11])
        elif self.show_summary == 1:
            print("\n------ Messages Status --------")
            print("allocated queues = %u" %values[0])
            print("used headers = %u" %values[1])
            print("used space = %u  bytes" %values[2])
            print("\n------ Shared Memory Status --------")
            print("segments allocated   %u" %values[3])
            print("pages allocated    %u" %(values[4] / pagesize))
            print("pages resident   %u" %(values[5] / pagesize))
            print("pages swapped   %u" %values[6])
            print("Swap performance: %u attempts     %u successes" %(values[7], values[8]))
            print("\n------ Semaphore Status --------")
            print("used arrays = %u" %values[9])
            print("allocated semaphores = %u" %values[10])

    def connect(self):
        """ Establish a PMAPI context to archive, host or local, via args """
        self.context = pmapi.pmContext.fromOptions(self.opts, sys.argv)

if __name__ == '__main__':
    try:
        IPCS = Ipcs()
        IPCS.connect()
        IPCS.execute()
    except pmapi.pmErr as error:
        print("%s: %s" % (error.progname(), error.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
    except KeyboardInterrupt:
        pass
