#
# Copyright (C) 2018 Marko Myllynen <myllynen@redhat.com>
# Based on the ucalls BCC tool by Sasha Goldshtein:
# https://github.com/iovisor/bcc/blob/master/tools/lib/ucalls.py
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
""" PCP BCC PMDA ucalls module """

# pylint: disable=invalid-name, too-many-instance-attributes

from ctypes import c_int
from os import path

from bcc import BPF, USDT, utils

from pcp.pmapi import pmUnits
from cpmapi import PM_TYPE_U64, PM_SEM_COUNTER, PM_SEM_INSTANT, PM_COUNT_ONE, PM_TIME_USEC
from cpmapi import PM_ERR_PMID
from cpmda import PMDA_FETCH_NOVALUES

from modules.pcpbcc import PCPBCCBase

#
# BPF program
#
bpf_src = "modules/ucalls.bpf"

#
# PCP BCC PMDA constants
#
MODULE = 'ucalls'
BASENS = 'proc.ucall.'
units_count = pmUnits(0, 0, 1, 0, 0, PM_COUNT_ONE)
units_usecs = pmUnits(0, 1, 0, 0, PM_TIME_USEC, 0)

#
# PCP BCC Module
#
class PCPBCCModule(PCPBCCBase):
    """ PCP BCC ucalls module """
    def __init__(self, config, log, err, proc_refresh):
        """ Constructor """
        PCPBCCBase.__init__(self, MODULE, config, log, err)

        self.pid = None
        self.proc_filter = None
        self.proc_refresh = proc_refresh

        self.cnt_cache = None
        self.avg_cache = None
        self.cml_cache = None
        self.lang = None
        self.latency = False
        self.usdt_contexts = []

        self.methods = {
            "java"   : ["bpf_usdt_readarg(2, ctx, &clazz);", "bpf_usdt_readarg(4, ctx, &method);"],
            "perl"   : ["bpf_usdt_readarg(2, ctx, &clazz);", "bpf_usdt_readarg(1, ctx, &method);"],
            "php"    : ["bpf_usdt_readarg(4, ctx, &clazz);", "bpf_usdt_readarg(1, ctx, &method);"],
            "python" : ["bpf_usdt_readarg(1, ctx, &clazz);", "bpf_usdt_readarg(2, ctx, &method);"],
            "ruby"   : ["bpf_usdt_readarg(1, ctx, &clazz);", "bpf_usdt_readarg(2, ctx, &method);"],
            "tcl"    : ["", "bpf_usdt_readarg(1, ctx, &method);"],
        }

        for opt in self.config.options(MODULE):
            if opt == 'language':
                self.lang = self.config.get(MODULE, opt)
            if opt == 'latency':
                self.latency = self.config.getboolean(MODULE, opt)
            if opt == 'process':
                self.proc_filter = self.config.get(MODULE, opt)
                self.update_pids(self.get_proc_info(self.proc_filter))

        if not self.proc_filter:
            # https://github.com/iovisor/bcc/issues/1774
            raise RuntimeError("Process filter is mandatory.")

        if not self.lang:
            if not self.pid:
                raise RuntimeError("Language must be set when no process found on startup!")
            self.lang = utils.detect_language(self.methods.keys(), self.pid)
            self.log("Language not set, detected: %s." % str(self.lang))
        if self.lang not in self.methods.keys():
            raise RuntimeError("Language must be one of: %s." % str(self.methods.keys()))

        self.log("Initialized.")

    def metrics(self):
        """ Get metric definitions """
        name = BASENS
        self.items = (
            # Name - reserved - type - semantics - units - help
            (name + 'count', None, PM_TYPE_U64, PM_SEM_COUNTER, units_count, 'ucall count'),
            (name + 'latency.avg', None, PM_TYPE_U64, PM_SEM_INSTANT, units_usecs, 'ucall avg'
                                                                                   'latency'),
            (name + 'latency.cml', None, PM_TYPE_U64, PM_SEM_COUNTER, units_usecs, 'ucall cml'
                                                                                   'latency'),
        )
        return True, self.items

    def reset_cache(self):
        """ Reset internal cache """
        self.cnt_cache = {}
        self.avg_cache = {}
        self.cml_cache = {}

    def undef_cache(self):
        """ Undefine internal cache """
        self.cnt_cache = None
        self.avg_cache = None
        self.cml_cache = None

    def compile(self):
        """ Compile BPF """
        try:
            if not self.pid and self.proc_filter and not self.proc_refresh:
                # https://github.com/iovisor/bcc/issues/1774
                raise RuntimeError("No process to attach found.")

            if not self.bpf_text:
                with open(path.dirname(__file__) + '/../' + bpf_src) as src:
                    self.bpf_text = src.read()

                lat = "#define LATENCY" if self.latency else ""
                self.bpf_text = self.bpf_text.replace("DEFINE_LATENCY", lat)
                self.bpf_text = self.bpf_text.replace("READ_CLASS", self.methods[self.lang][0])
                self.bpf_text = self.bpf_text.replace("READ_METHOD", self.methods[self.lang][1])

            if not self.pid and self.proc_filter and self.proc_refresh:
                self.log("No process to attach found, activation postponed.")
                return

            # Set the language specific tracepoint
            if self.lang in ("java", "ruby"):
                entry_probe = "method__entry"
                return_probe = "method__return"
            elif self.lang in ("perl",):
                entry_probe = "sub__entry"
                return_probe = "sub__return"
            elif self.lang in ("tcl",):
                entry_probe = "proc__entry"
                return_probe = "proc__return"
            else:
                entry_probe = "function__entry"
                return_probe = "function__return"
            extra_entry, extra_return = None, None
            if self.lang == "ruby":
                extra_entry = "cmethod__entry"
                extra_return = "cmethod__return"

            self.usdt_contexts = []
            usdt = USDT(pid=self.pid)
            usdt.enable_probe(entry_probe, "trace_entry")
            if extra_entry:
                usdt.enable_probe(extra_entry, "trace_entry")
            if self.latency:
                usdt.enable_probe(return_probe, "trace_return")
                if extra_return:
                    usdt.enable_probe(extra_return, "trace_return")
            self.usdt_contexts.append(usdt)

            bpf_text = self.apply_pid_filter(self.bpf_text, [self.pid])

            if self.debug:
                self.log("BPF to be compiled:\n" + bpf_text.strip())

            self.reset_cache()
            self.bpf = BPF(text=bpf_text, usdt_contexts=self.usdt_contexts)
            self.log("Compiled.")
        except Exception as error: # pylint: disable=broad-except
            self.bpf = None
            self.undef_cache()
            self.err(str(error))
            self.err("Module NOT active!")
            raise

    def refresh(self):
        """ Refresh BPF data """
        if self.bpf is None:
            return None

        def get_key(s):
            """ Helper to read key for dict """
            return self.parse_inst_name(s.clazz.decode("ASCII", "replace") + "." + \
                                        s.method.decode("ASCII", "replace"))

        if not self.latency:
            for k, v in self.bpf["counts"].items():
                key = get_key(k)
                self.cnt_cache[key] = v.value
                self.insts[key] = c_int(1)
        else:
            for k, v in self.bpf["times"].items():
                key = get_key(k)
                self.cnt_cache[key] = v.num_calls
                self.cml_cache[key] = v.total_us
                value = v.total_us if key not in self.avg_cache else int(v.total_us / v.num_calls)
                self.avg_cache[key] = value
                self.insts[key] = c_int(1)

        return self.insts

    def bpfdata(self, item, inst):
        """ Return BPF data as PCP metric value """
        try:
            key = self.pmdaIndom.inst_name_lookup(inst)
            if item == 0:
                return [self.cnt_cache[key], 1]
            elif item == 1:
                return [self.avg_cache[key], 1]
            elif item == 2:
                return [self.cml_cache[key], 1]
            else:
                return [PM_ERR_PMID, 0]
        except Exception: # pylint: disable=broad-except
            return [PMDA_FETCH_NOVALUES, 0]
