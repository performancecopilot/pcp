#
# Copyright (C) 2016 Sasha Goldshtein
# Copyright (C) 2018-2019 Marko Myllynen <myllynen@redhat.com>
# Based on the ustat BCC tool by Sasha Goldshtein:
# https://github.com/iovisor/bcc/blob/master/tools/lib/ustat.py
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
""" PCP BCC PMDA ustat module """

# pylint: disable=invalid-name, too-few-public-methods, too-many-instance-attributes

from ctypes import c_int

from bcc import BPF, USDT, USDTException, utils

from pcp.pmapi import pmUnits
from cpmapi import PM_TYPE_U64, PM_SEM_COUNTER, PM_COUNT_ONE
from cpmapi import PM_ERR_PMID
from cpmda import PMDA_FETCH_NOVALUES

from modules.pcpbcc import PCPBCCBase

#
# PCP BCC PMDA constants
#
MODULE = 'ustat'
BASENS = 'proc.ustat.'
units_count = pmUnits(0, 0, 1, 0, 0, PM_COUNT_ONE)

#
# Helper classes
#
class Category(object):
    """ Category """
    THREAD = "THREAD"
    METHOD = "METHOD"
    OBJNEW = "OBJNEW"
    CLOAD = "CLOAD"
    EXCP = "EXCP"
    GC = "GC"

class Probe(object):
    """ Probe """
    def __init__(self, language, procnames, events):
        """ Initialize new probe object """
        self.language = language
        self.procnames = procnames
        self.events = events
        self.targets = {}
        self.usdts = []

    def _find_targets(self, pids):
        """ Find targets """
        for pid in pids:
            self.targets[pid] = 1

    def _enable_probes(self):
        """ Enable probes """
        for pid in self.targets:
            try:
                usdt = USDT(pid=pid)
            except USDTException:
                continue
            for event in self.events:
                try:
                    usdt.enable_probe(event, "%s_%s" % (self.language, event))
                except Exception: # pylint: disable=broad-except
                    pass
            self.usdts.append(usdt)

    def _generate_tables(self):
        """ Generate tables """
        text = "BPF_HASH(%s_%s_counts, u32, u64);"
        return str.join('', [text % (self.language, event)
                             for event in self.events])

    def _generate_functions(self):
        """ Generate functions """
        text = """
int %s_%s(void *ctx) {
    u64 *valp, zero = 0;
    u32 tgid = bpf_get_current_pid_tgid() >> 32;
    valp = %s_%s_counts.lookup_or_init(&tgid, &zero);
    ++(*valp);
    return 0;
}
        """
        lang = self.language
        return str.join('', [text % (lang, event, lang, event)
                             for event in self.events])

    def get_program(self, pids):
        """ Get program text """
        self._find_targets(pids)
        self._enable_probes()
        return self._generate_tables() + self._generate_functions()

    def get_usdts(self):
        """ Get USDTs """
        return self.usdts

    def get_counts(self, bpf):
        """ Return a map of event counts per process """
        events = {category: 0 for category in self.events.values()}
        result = {pid: events.copy() for pid in self.targets}
        for event, category in self.events.items():
            counts = bpf["%s_%s_counts" % (self.language, event)]
            for pid, count in counts.items():
                try:
                    result[pid.value][category] = count.value
                except Exception: # pylint: disable=broad-except
                    pass
            counts.clear()
        return result

    def cleanup(self):
        """ Clean up """
        self.usdts = None

#
# PCP BCC Module
#
class PCPBCCModule(PCPBCCBase):
    """ PCP BCC ustat module """
    def __init__(self, config, log, err, proc_refresh):
        """ Constructor """
        PCPBCCBase.__init__(self, MODULE, config, log, err)

        self.pids = []
        self.proc_filter = None
        self.proc_refresh = proc_refresh

        self.cache = None
        self.lang = None
        self.probes = None

        self.langs = {
            "java"   : 1,
            "node"   : 1,
            "perl"   : 1,
            "php"    : 1,
            "python" : 1,
            "ruby"   : 1,
            "tcl"    : 1,
        }

        for opt in self.config.options(MODULE):
            if opt == 'language':
                self.lang = self.config.get(MODULE, opt)
            if opt == 'process':
                self.proc_filter = self.config.get(MODULE, opt)
                self.update_pids(self.get_proc_info(self.proc_filter))

        if not self.proc_filter:
            # https://github.com/iovisor/bcc/issues/1774
            raise RuntimeError("Process filter is mandatory.")

        if not self.lang:
            if not self.pids:
                raise RuntimeError("Language must be set when no process found on startup!")
            self.lang = utils.detect_language(self.langs.keys(), self.pids[0])
            self.log("Language not set, detected: %s." % str(self.lang))
        if self.lang not in self.langs.keys():
            raise RuntimeError("Language must be one of: %s." % str(self.langs.keys()))

        self.log("Initialized.")

    def metrics(self):
        """ Get metric definitions """
        name = BASENS
        self.items = (
            # Name - reserved - type - semantics - units - help
            (name + 'thread', None, PM_TYPE_U64, PM_SEM_COUNTER, units_count, 'threads started'),
            (name + 'method', None, PM_TYPE_U64, PM_SEM_COUNTER, units_count, 'method call count'),
            (name + 'object', None, PM_TYPE_U64, PM_SEM_COUNTER, units_count, 'objects created'),
            (name + 'class', None, PM_TYPE_U64, PM_SEM_COUNTER, units_count, 'class load count'),
            (name + 'except', None, PM_TYPE_U64, PM_SEM_COUNTER, units_count, 'exception count'),
            (name + 'gc', None, PM_TYPE_U64, PM_SEM_COUNTER, units_count, 'gc count'),
        )
        return True, self.items

    def reset_cache(self):
        """ Reset internal cache """
        self.cache = {Category.THREAD: {}, Category.METHOD: {}, Category.OBJNEW: {},
                      Category.CLOAD: {}, Category.EXCP: {}, Category.GC: {}}

    def undef_cache(self):
        """ Undefine internal cache """
        self.cache = None

    def create_probes(self):
        """ Create probes """
        probes_by_lang = {
            "java": Probe("java", ["java"], {
                "gc__begin": Category.GC,
                "mem__pool__gc__begin": Category.GC,
                "thread__start": Category.THREAD,
                "class__loaded": Category.CLOAD,
                "object__alloc": Category.OBJNEW,
                "method__entry": Category.METHOD,
                "ExceptionOccurred__entry": Category.EXCP
                }),
            "node": Probe("node", ["node"], {
                "gc__start": Category.GC
                }),
            "perl": Probe("perl", ["perl"], {
                "sub__entry": Category.METHOD
                }),
            "php": Probe("php", ["php"], {
                "function__entry": Category.METHOD,
                "compile__file__entry": Category.CLOAD,
                "exception__thrown": Category.EXCP
                }),
            "python": Probe("python", ["python"], {
                "function__entry": Category.METHOD,
                "gc__start": Category.GC
                }),
            "ruby": Probe("ruby", ["ruby", "irb"], {
                "method__entry": Category.METHOD,
                "cmethod__entry": Category.METHOD,
                "gc__mark__begin": Category.GC,
                "gc__sweep__begin": Category.GC,
                "object__create": Category.OBJNEW,
                "hash__create": Category.OBJNEW,
                "string__create": Category.OBJNEW,
                "array__create": Category.OBJNEW,
                "require__entry": Category.CLOAD,
                "load__entry": Category.CLOAD,
                "raise": Category.EXCP
                }),
            "tcl": Probe("tcl", ["tclsh", "wish"], {
                "proc__entry": Category.METHOD,
                "obj__create": Category.OBJNEW
                }),
        }
        self.probes = [probes_by_lang[self.lang]]

    def compile(self):
        """ Compile BPF """
        try:
            if not self.pids and self.proc_filter and not self.proc_refresh:
                # https://github.com/iovisor/bcc/issues/1774
                raise RuntimeError("No process to attach found.")

            if not self.pids and self.proc_filter and self.proc_refresh:
                self.log("No process to attach found, activation postponed.")
                return

            self.create_probes()
            self.bpf_text = str.join("\n", [probe.get_program(self.pids) for probe in self.probes])

            if self.debug:
                self.log("BPF to be compiled:\n" + self.bpf_text.strip())

            self.reset_cache()
            self.bpf = BPF(text=self.bpf_text)
            usdts = [usdt for probe in self.probes for usdt in probe.get_usdts()]
            uprobes = {(path, func, addr) for usdt in usdts
                       for path, func, addr, _ in usdt.enumerate_active_probes()}
            for path, func, addr in uprobes:
                try:
                    # Compat: bcc < 0.6.0
                    if self.bcc_version() == "0.5.0":
                        func = func.decode()
                        path = path.decode()
                    self.bpf.attach_uprobe(name=path, fn_name=func, addr=addr, pid=-1)
                except Exception: # pylint: disable=broad-except
                    self.err("USDT instrumentation for a process failed.")
                    self.err("Results may be incomplete.")
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

        self.insts = {}

        counts = {}
        stale_pids = set()
        for probe in self.probes:
            counts.update(probe.get_counts(self.bpf))
        for pid, stats in counts.items():
            if pid in stale_pids or not self.pid_alive(pid):
                stale_pids.add(pid)
                continue
            for cat in Category.THREAD, Category.METHOD, Category.OBJNEW, \
                       Category.CLOAD, Category.EXCP, Category.GC:
                for key in self.cache:
                    if pid not in self.cache[key]:
                        self.cache[key][pid] = 0
                self.cache[cat][pid] += stats.get(cat, 0)
            self.insts[str(pid)] = c_int(1)

        return self.insts

    # pylint: disable=too-many-return-statements
    def bpfdata(self, item, inst):
        """ Return BPF data as PCP metric value """
        try:
            key = int(self.pmdaIndom.inst_name_lookup(inst))
            if item == 0:
                return [self.cache[Category.THREAD][key], 1]
            elif item == 1:
                return [self.cache[Category.METHOD][key], 1]
            elif item == 2:
                return [self.cache[Category.OBJNEW][key], 1]
            elif item == 3:
                return [self.cache[Category.CLOAD][key], 1]
            elif item == 4:
                return [self.cache[Category.EXCP][key], 1]
            elif item == 5:
                return [self.cache[Category.GC][key], 1]
            else:
                return [PM_ERR_PMID, 0]
        except Exception: # pylint: disable=broad-except
            return [PMDA_FETCH_NOVALUES, 0]

    def cleanup(self):
        """ Clean up """
        if self.probes:
            for probe in self.probes:
                probe.cleanup()
            self.probes = []
        super(PCPBCCModule, self).cleanup()
