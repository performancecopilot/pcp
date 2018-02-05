#
# Copyright (C) 2017-2018 Marko Myllynen <myllynen@redhat.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
""" PCP BCC PMDA biolatency module """

# Configuration options
# Name - type - default
#
# queued - boolean - False : include OS queued time in I/O time

# pylint: disable=invalid-name, line-too-long

from ctypes import c_int
from bcc import BPF

from modules.pcpbcc import PCPBCCBase
from pcp.pmapi import pmUnits
from cpmapi import PM_TYPE_U64, PM_SEM_COUNTER, PM_COUNT_ONE
from cpmapi import PM_ERR_AGAIN

#
# BPF program
#
bpf_src = "modules/biolatency.bpf"

#
# PCP BCC PMDA constants
#
MODULE = 'biolatency'
METRIC = 'disk.all.latency'
units_count = pmUnits(0, 0, 1, 0, 0, PM_COUNT_ONE)

#
# PCP BCC Module
#
class PCPBCCModule(PCPBCCBase):
    """ PCP BCC biolatency module """
    def __init__(self, config, log, err):
        """ Constructor """
        PCPBCCBase.__init__(self, MODULE, config, log, err)

        self.cache = {}
        self.queued = False

        for opt in self.config.options(MODULE):
            if opt == 'queued':
                self.queued = self.config.getboolean(MODULE, opt)

        if self.queued:
            self.log("Including OS queued time in I/O time.")
        else:
            self.log("Excluding OS queued time from I/O time.")

        self.log("Initialized.")

    def metrics(self):
        """ Get metric definitions """
        name = METRIC
        self.items.append(
            # Name - reserved - type - semantics - units - help
            (name, None, PM_TYPE_U64, PM_SEM_COUNTER, units_count, 'block io latency distribution'),
        )
        return True, self.items

    def compile(self):
        """ Compile BPF """
        try:
            self.bpf = BPF(src_file=bpf_src)
            if self.queued:
                self.bpf.attach_kprobe(event="blk_start_request", fn_name="trace_req_start")
                self.bpf.attach_kprobe(event="blk_mq_start_request", fn_name="trace_req_start")
            else:
                self.bpf.attach_kprobe(event="blk_account_io_start", fn_name="trace_req_start")
            self.bpf.attach_kprobe(event="blk_account_io_completion", fn_name="trace_req_completion")
            self.log("Compiled.")
        except Exception as error: # pylint: disable=broad-except
            self.err(str(error))
            self.err("Module NOT active!")
            self.bpf = None

    def refresh(self):
        """ Refresh BPF data """
        if self.bpf is None:
            return

        dist = self.bpf.get_table("dist")

        for k, v in dist.items():
            if k.value == 0:
                continue
            low = (1 << k.value) >> 1
            high = (1 << k.value) - 1
            if low == high:
                low -= 1
            key = str(low) + "-" + str(high)
            if key not in self.cache:
                self.cache[key] = 0
            self.cache[key] += v.value
            self.insts[key] = c_int(1)

        dist.clear()

        return self.insts

    def bpfdata(self, item, inst):
        """ Return BPF data as PCP metric value """
        try:
            key = self.pmdaIndom.inst_name_lookup(inst)
            return [self.cache[key], 1]
        except Exception: # pylint: disable=broad-except
            return [PM_ERR_AGAIN, 0]
