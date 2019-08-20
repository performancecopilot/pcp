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
        self.pid = None
        self.exit_code = None
        self.output = ''
        self.probes = 0
        self.maps = {}

    def __str__(self):
        return str(self.__dict__)

class BPFtraceVarDef: # pylint: disable=too-few-public-methods
    """BPFtrace variable definitions"""
    class MetricType:
        """BPFtrace variable types"""
        Histogram = 'histogram'
        Output = 'output'
        Control = 'control'

    def __init__(self, single, semantics, datatype, metrictype):
        self.single = single
        self.semantics = semantics
        self.datatype = datatype
        self.metrictype = metrictype

    def __str__(self):
        return str(self.__dict__)

class BPFtrace:
    """class for interacting with bpftrace"""
    def __init__(self, bpftrace_path, log, script):
        self.bpftrace_path = bpftrace_path
        self.log = log
        self.script = script # contains the modified script (added continuous output)
        self.process = None

        self.metadata = {}
        self.var_defs = {}
        self.lock = Lock()
        self._state = BPFtraceState()

        self.parse_script()

    def state(self):
        """returns latest state"""
        with self.lock:
            return deepcopy(self._state)

    def process_output_obj(self, obj):
        """process a single JSON object from bpftrace output"""
        with self.lock:
            if self._state.status == BPFtraceState.Status.Starting:
                self._state.status = BPFtraceState.Status.Started

            if obj['type'] == BPFtraceMessageType.AttachedProbes:
                self._state.probes = obj['probes']
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

    def process_output(self):
        """process stdout and stderr of running bpftrace process"""
        for line in self.process.stdout:
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

    def parse_script(self):
        """parse bpftrace script (read variable semantics, add continuous output)"""
        metadata = re.findall(r'^// (\w+): (.+)$', self.script, re.MULTILINE)
        for key, val in metadata:
            if key == 'name':
                if re.match(r'^[a-zA-Z_]\w+$', val):
                    self.metadata['name'] = val
                else:
                    raise BPFtraceError("invalid value '{}' for script name: must contain only "
                                        "alphanumeric characters and start with a letter".format(
                                            val))
            elif key == 'include':
                self.metadata['include'] = val.split(',')

        self.var_defs = {}
        variables = re.findall(r'(@.*?)(\[.+?\])?\s*=\s*(count|hist)?', self.script)
        if variables:
            for var, key, func in variables:
                if 'include' in self.metadata and var not in self.metadata['include']:
                    continue

                vardef = BPFtraceVarDef(single=True, semantics=PM_SEM_INSTANT, datatype=PM_TYPE_U64,
                                        metrictype=None)
                if func in ['hist', 'lhist']:
                    vardef.single = False
                    vardef.semantics = PM_SEM_COUNTER
                    vardef.metrictype = BPFtraceVarDef.MetricType.Histogram
                if func == 'count':
                    vardef.semantics = PM_SEM_COUNTER
                if key:
                    vardef.single = False
                    if vardef.metrictype == BPFtraceVarDef.MetricType.Histogram:
                        raise BPFtraceError("every histogram needs to be in a separate variable")
                self.var_defs[var] = vardef

            print_st = ' '.join(['print({});'.format(var) for var in self.var_defs])
            self.script = self.script + ' interval:s:1 {{ {} }}'.format(print_st)

        output_fns = re.search(r'(printf|time)\s*\(', self.script)
        if output_fns and ('include' not in self.metadata or '@output' in self.metadata['include']):
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

        self.process = subprocess.Popen([self.bpftrace_path, '-f', 'json', '-e', self.script],
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                        encoding='utf8')

        # with daemon=False, atexit doesn't work
        self.process_output_thread = Thread(target=self.process_output, daemon=True)
        self.process_output_thread.start()

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
