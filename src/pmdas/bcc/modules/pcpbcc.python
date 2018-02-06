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
""" PCP BCC PMDA module base class """

# pylint: disable=too-many-instance-attributes
class PCPBCCBase(object):
    """ PCP BCC Base Class """
    def __init__(self, module, config, log, err):
        """ Constructor """
        self._who = module
        self._log = log
        self._err = err

        self.bpf = None
        self.insts = {}
        self.items = []

        self.pmdaIndom = None # pylint: disable=invalid-name
        self.config = config
        self.debug = False

        for opt in self.config.options(self._who):
            if opt == 'debug':
                self.debug = self.config.getboolean(self._who, opt)

        if self.debug:
            self.log("Debug logging enabled.")

    def log(self, msg):
        """ Log a message """
        self._log(self._who + ": " + msg)

    def err(self, msg):
        """ Log an error """
        self._err(self._who + ": " + msg)

    def metrics(self):
        """ Get metric definitions """
        raise NotImplementedError

    def helpers(self, pmdaIndom): # pylint: disable=invalid-name
        """ Register helper function references """
        self.pmdaIndom = pmdaIndom

    def compile(self):
        """ Compile BPF """
        raise NotImplementedError

    def refresh(self):
        """ Refresh BPF data """
        raise NotImplementedError

    def bpfdata(self, item, inst):
        """ Return BPF data as PCP metric value """
        raise NotImplementedError

    def cleanup(self):
        """ Clean up at exit """
        if self.bpf is not None:
            self.bpf.cleanup()
        self.bpf = None
        self.log("BPF/kprobes detached.")
