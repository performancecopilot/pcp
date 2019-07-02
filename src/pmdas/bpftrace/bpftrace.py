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
from cpmapi import PM_SEM_INSTANT, PM_SEM_COUNTER


class BPFtraceError(Exception):
    """BPFtrace general error"""

class BPFtraceState: # pylint: disable=too-few-public-methods
    """BPFtrace state"""
    def __init__(self, status='idle', pid=None, exit_code=None, output='', probes=None, maps=None): # pylint: disable=too-many-arguments
        self.status = status # idle|running|exited
        self.pid = pid
        self.exit_code = exit_code
        self.output = output
        self.probes = probes
        self.maps = maps or {}

class BPFtraceVarDef: # pylint: disable=too-few-public-methods
    """BPFtrace variable definitions"""
    def __init__(self, single, semantics):
        self.single = single
        self.semantics = semantics

class BPFtrace:
    """class for interacting with bpftrace"""
    def __init__(self, log, script):
        self.log = log
        self.script = script # contains the modified script (added continuous output)

        self.var_defs = {}
        self.lock = Lock()
        self._state = BPFtraceState()

    def state(self):
        """returns latest state"""
        with self.lock:
            return deepcopy(self._state)

    @property
    def status(self):
        """returns the status of bpftrace"""
        with self.lock:
            return self._state.status

    def process_output_obj(self, obj):
        """process a single JSON object from bpftrace output"""
        with self.lock:
            if obj['type'] == 'attached_probes':
                self._state.probes = obj['probes']
            elif obj['type'] == 'map':
                self._state.maps.update(obj['data'])
            elif obj['type'] == 'hist':
                for k, v in obj['data'].items():
                    self._state.maps[k] = {
                        '{}-{}'.format(bucket.get('min', 'inf'), bucket.get('max', 'inf'))
                        :bucket['count']
                        for bucket in v
                    }

    def process_output_objs(self, json_objs):
        """process multiple JSON objects"""
        if not json_objs:
            return

        for json_obj in json_objs:
            json_obj = json_obj.strip()
            if json_obj:
                try:
                    obj = json.loads(json_obj)
                    self.process_output_obj(obj)
                except ValueError:
                    with self.lock:
                        self._state.output += json_obj

    def process_output(self):
        """process stdout and stderr of running bpftrace process"""
        buf = ''
        for line in self.process.stdout:
            buf += line
            json_objs = buf.split('\n\n')
            buf = json_objs.pop()
            self.process_output_objs(json_objs)

        # process has exited, set returncode
        self.process.poll()
        with self.lock:
            self._state.status = 'exited'
            self._state.exit_code = self.process.returncode
            self._state.output += buf

    def parse_script(self):
        """parse bpftrace script (read variable semantics, add continuous output)"""
        variables = re.findall(r'(@.*?)(\[.+?\])?\s*=\s*(count|hist)?', self.script)
        if variables:
            print_st = ' '.join(['print({});'.format(var) for var, key, func in variables])
            self.script = self.script + ' interval:s:1 {{ {} }}'.format(print_st)

            self.var_defs = {}
            for var, key, func in variables:
                vardef = BPFtraceVarDef(single=True, semantics=PM_SEM_INSTANT)
                if func in ['hist', 'lhist']:
                    vardef.single = False
                elif key:
                    vardef.single = False
                if func == 'count':
                    vardef.semantics = PM_SEM_COUNTER
                self.var_defs[var] = vardef
        else:
            raise BPFtraceError("no global bpftrace variable found, please include "
                                "at least one global variable in your script")

    def start(self):
        """starts bpftrace in the background, waits until first data arrives or an error occurs"""
        if self.status != 'idle':
            raise Exception("bpftrace already started")

        self.parse_script()
        self.process = subprocess.Popen(['bpftrace', '-f', 'json', '-e', self.script],
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                        encoding='utf8')
        with self.lock:
            self._state.status = 'running'
            self._state.pid = self.process.pid

        # with daemon=False, atexit doesn't work
        self.process_output_thread = Thread(target=self.process_output, daemon=True)
        self.process_output_thread.start()

        self.log("started bpftrace -e '{}', PID: {}".format(self.script, self.process.pid))

    def stop(self, wait=False):
        """stop bpftrace process"""
        if self.status != 'running':
            return

        self.log("send stop signal to bpftrace process {}, "
                 "wait for termination: {}".format(self.process.pid, wait))
        self.process.send_signal(signal.SIGINT)

        if wait:
            self.process.communicate()
        else:
            self.process.poll()

        with self.lock:
            self._state.status = 'exited'
            self._state.exit_code = self.process.returncode
