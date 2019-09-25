#
# Copyright (c) 2019 Red Hat.
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
"""bpftrace"""

import subprocess
import signal
import re
from threading import Thread, Lock
from copy import deepcopy
import json
from cpmapi import PM_SEM_INSTANT, PM_SEM_COUNTER, PM_TYPE_U64, PM_TYPE_STRING


class BPFtraceError(Exception):
    """BPFtrace general error"""

class BPFtraceMessageType: # pylint: disable=too-few-public-methods
    """BPFtrace JSON output types"""
    AttachedProbes = 'attached_probes'
    Map = 'map'
    Hist = 'hist'
    Printf = 'printf'
    Time = 'time'

class BPFtraceState: # pylint: disable=too-few-public-methods
    """BPFtrace state"""
    class Status:
        """BPFtrace status"""
        Stopped = 'stopped'
        Starting = 'starting'
        Started = 'started'
        Stopping = 'stopping'

    def __init__(self):
        self.status = BPFtraceState.Status.Stopped
        self.reset()

    def reset(self):
        """reset state"""
        self.pid = -1
        self.exit_code = 0
        self.output = ''
        self.probes = 0
        self.maps = {}

    def __str__(self):
        return str(self.__dict__)

class BPFtraceVarDef: # pylint: disable=too-few-public-methods
    """BPFtrace variable definitions"""
    class MetricType:
        """BPFtrace variable types"""
        Control = 'control'
        Histogram = 'histogram'
        Stacks = 'stacks'
        Output = 'output'

    def __init__(self, single, semantics, datatype, metrictype):
        self.single = single
        self.semantics = semantics
        self.datatype = datatype
        self.metrictype = metrictype

    def __str__(self):
        return str(self.__dict__)

class ScriptMetadata: # pylint: disable=too-few-public-methods
    """metadata for bpftrace scripts"""
    def __init__(self):
        self.name = None
        self.include = None
        self.table_retain_lines = None

class BPFtrace:
    """class for interacting with bpftrace"""
    BPFTRACE_PATH = ''
    TRACEPOINTS = ''
    VERSION = ()
    VERSION_LATEST = (999, 999, 999) # simplifies version checks, assuming latest version

    def __init__(self, log, script):
        self.log = log
        self.script = script # contains the modified script (added continuous output)
        self.process = None

        self.metadata = ScriptMetadata()
        self.var_defs = {}
        self.lock = Lock()
        self._state = BPFtraceState()

        self.parse_script()

    @classmethod
    def parse_version(cls, version):
        """parse bpftrace version"""
        re_released_version = re.match(r'bpftrace v(\d+)\.(\d+)\.(\d+)', version)
        if re_released_version:
            # regex enforces exactly 3 integers
            return tuple(map(int, re_released_version.groups())) # (major, minor, patch)
        return cls.VERSION_LATEST

    @classmethod
    def configure(cls, path):
        """configure BPFtrace class"""
        cls.BPFTRACE_PATH = path
        version = subprocess.check_output([cls.BPFTRACE_PATH, '--version'], encoding='utf8').strip()
        cls.VERSION = cls.parse_version(version)
        cls.TRACEPOINTS = subprocess.check_output([cls.BPFTRACE_PATH, '-l'],
                                                  encoding='utf8').strip()

    def state(self):
        """returns latest state"""
        with self.lock:
            return deepcopy(self._state)

    def table_retain_lines(self):
        """cut table lines and keep first line (header)"""
        output = self._state.maps['@output']
        newlines = []
        for i, c in enumerate(output):
            if c == '\n':
                newlines.append(i + 1)

        # e.g. if we found one NL, we have 2 lines
        if len(newlines) + 1 > 1 + self.metadata.table_retain_lines:
            # special handling if last character is a NL
            ignore_last_nl = 1 if output[-1] == '\n' else 0
            # pylint: disable=invalid-unary-operand-type
            start_content_at = newlines[-self.metadata.table_retain_lines - ignore_last_nl]
            self._state.maps['@output'] = output[:newlines[0]] + output[start_content_at:]

    def process_output_obj(self, obj):
        """process a single JSON object from bpftrace output"""
        with self.lock:
            if self._state.status == BPFtraceState.Status.Starting:
                self._state.status = BPFtraceState.Status.Started

            if obj['type'] == BPFtraceMessageType.AttachedProbes:
                if self.VERSION <= (0, 9, 2):
                    self._state.probes = obj['probes']
                else:
                    # https://github.com/iovisor/bpftrace/commit/9d1269b
                    self._state.probes = obj['data']['probes']
            elif obj['type'] == BPFtraceMessageType.Map:
                self._state.maps.update(obj['data'])
            elif obj['type'] == BPFtraceMessageType.Hist:
                for k, v in obj['data'].items():
                    self._state.maps[k] = {
                        '{}-{}'.format(bucket.get('min', '-inf'), bucket.get('max', 'inf'))
                        :bucket['count']
                        for bucket in v
                    }
            elif obj['type'] in [BPFtraceMessageType.Printf, BPFtraceMessageType.Time]:
                self._state.maps['@output'] = self._state.maps.get('@output', '') + obj['msg']
                if self.metadata.table_retain_lines is not None:
                    self.table_retain_lines()

    def process_stdout(self):
        """process stdout of running bpftrace process"""
        for line in self.process.stdout:
            if self.VERSION <= (0, 9, 2) and '": }' in line:
                # invalid JSON, fixed in https://github.com/iovisor/bpftrace/commit/348975b
                continue

            if not line or line.isspace():
                continue
            try:
                obj = json.loads(line)
                self.process_output_obj(obj)
            except ValueError:
                with self.lock:
                    self._state.output += line

        # process has exited, set returncode
        self.process.poll()
        with self.lock:
            self._state.status = BPFtraceState.Status.Stopped
            self._state.exit_code = self.process.returncode

    def process_stderr(self):
        """process stderr of running bpftrace process"""
        for line in self.process.stderr:
            with self.lock:
                self._state.output += line

    def parse_script(self):
        """parse bpftrace script (read variable semantics, add continuous output)"""
        found_metadata = re.findall(r'^//\s*([\w\-]+)' + # key
                                    r'(?:\s*:\s*(.+?)\s*)?$', # optional value
                                    self.script, re.MULTILINE)
        for key, val in found_metadata:
            if key == 'name':
                if re.match(r'^[a-zA-Z_]\w+$', val):
                    self.metadata.name = val
                else:
                    raise BPFtraceError("invalid value '{}' for {}: "
                                        "must contain only alphanumeric characters and "
                                        "start with a letter".format(val, key))
            elif key == 'include':
                self.metadata.include = val.split(',')
            elif key == 'table-retain-lines':
                if val.isdigit():
                    self.metadata.table_retain_lines = int(val)
                else:
                    raise BPFtraceError("invalid value '{}' for {}, "
                                        "must be numeric".format(val, key))

        variables = re.findall(r'(@.*?)' + # variable
                               r'(\[.+?\])?' + # optional map key
                               r'\s*=\s*' + # assignment
                               r'(?:([a-zA-Z]\w*)\s*\()?', # optional function
                               self.script)
        if variables:
            for var, key, func in variables:
                if self.metadata.include is not None and var not in self.metadata.include:
                    continue

                vardef = BPFtraceVarDef(single=True, semantics=PM_SEM_INSTANT, datatype=PM_TYPE_U64,
                                        metrictype=None)
                if func in ['hist', 'lhist']:
                    if key:
                        raise BPFtraceError("every histogram needs to be in a separate variable")
                    vardef.single = False
                    vardef.semantics = PM_SEM_COUNTER
                    vardef.metrictype = BPFtraceVarDef.MetricType.Histogram
                elif func == 'count':
                    vardef.single = not bool(key)
                    vardef.semantics = PM_SEM_COUNTER
                    if key in ['ustack', 'kstack']:
                        vardef.metrictype = BPFtraceVarDef.MetricType.Stacks
                else:
                    vardef.single = not bool(key)
                    vardef.semantics = PM_SEM_INSTANT
                self.var_defs[var] = vardef

            print_st = ' '.join(['print({});'.format(var) for var in self.var_defs])
            self.script = self.script + '\ninterval:s:1 {{ {} }}'.format(print_st)

        output_fns = re.search(r'(printf|time)\s*\(', self.script)
        if output_fns and (self.metadata.include is None or '@output' in self.metadata.include):
            if '@output' in self.var_defs:
                raise BPFtraceError("output from printf(), time() etc. will be stored in @output. "
                                    "please rename the existing @output variable or remove any "
                                    "calls to any function which produces output")
            self.var_defs['@output'] = BPFtraceVarDef(single=True, semantics=PM_SEM_INSTANT,
                                                      datatype=PM_TYPE_STRING,
                                                      metrictype=BPFtraceVarDef.MetricType.Output)

        if not self.var_defs:
            raise BPFtraceError("no bpftrace variables or printf statements found, please include "
                                "at least one variable or print statement in your script")

    def start(self):
        """starts bpftrace in the background and reads its stdout in a new thread"""
        with self.lock:
            if self._state.status != BPFtraceState.Status.Stopped:
                raise BPFtraceError("cannot start bpftrace, current status: {}".format(
                    self._state.status))
            self._state.reset()
            self._state.status = BPFtraceState.Status.Starting

        self.process = subprocess.Popen([self.BPFTRACE_PATH, '-f', 'json', '-e', self.script],
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                        encoding='utf8')

        # with daemon=False, atexit doesn't work
        self.process_stdout_thread = Thread(target=self.process_stdout, daemon=True)
        self.process_stdout_thread.start()
        self.process_stderr_thread = Thread(target=self.process_stderr, daemon=True)
        self.process_stderr_thread.start()

        self.log("started {}".format(self))
        with self.lock:
            # status will be set to 'started' once the first data arrives
            self._state.pid = self.process.pid

    def stop(self, wait=False):
        """stop bpftrace process"""
        with self.lock:
            if self._state.status != BPFtraceState.Status.Started:
                raise BPFtraceError("cannot stop bpftrace, current status: {}".format(
                    self._state.status))
            self._state.status = BPFtraceState.Status.Stopping

        self.log("stopped {}, wait for termination: {}".format(self, wait))
        self.process.send_signal(signal.SIGINT)

        if wait:
            self.process.communicate()
        else:
            self.process.poll()

        with self.lock:
            self._state.status = BPFtraceState.Status.Stopped
            self._state.exit_code = self.process.returncode

    def __str__(self):
        script_output = self.script
        if len(script_output) > 80:
            script_output = script_output[:80-6] + ' [...]'
        script_output = script_output.replace('\n', '\\n')
        pid = self.process.pid if self.process else None
        return "BPFtrace (script='{}', PID={})".format(script_output, pid)
