#!/usr/bin/pcp python
#
# Copyright (C) 2015 Red Hat.
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
""" Verify various aspects of a PCP collector installation """

import sys
from pcp import pmapi, pmcc
from cpmapi import PM_CONTEXT_ARCHIVE, PM_MODE_FORW

STATUS = 0	# exit code indicating success/failure

class Verifier(pmcc.MetricGroupPrinter):
    """ Common setup code used by each verification mode """
    _name = ''
    _metrics = []
    _verbose = 0

    def setup(self, manager, name, verbose):
        manager[name] = self._metrics
        self._verbose = verbose
        self._name = name
        return self

    """ Common extract code used by each verification mode """
    def current(self, group, metric):
        return dict(map(lambda x: (x[1], x[2]), group[metric].netValues))


class BasicReport(Verifier):
    """ Verifies high-level issues for a PCP collector """
    _metrics = [ 'pmcd.agent.status' ]

    def report(self, manager):
        global STATUS
        group = manager[self._name]
        status = self.current(group, 'pmcd.agent.status')
        for pmda in sorted(status):
            if self._verbose:
                print('** DEBUG: pmda%s(1), status=%x' % (pmda, status[pmda]))
            lowbits = status[pmda] & 0xff
            if lowbits == 0x1:
                print('== INFO: the pmda%s(1) agent is not yet ready.' % pmda)
            elif lowbits == 0x2:
                print('== WARN: the pmda%s(1) agent exited cleanly.' % pmda)
                STATUS = 1
            elif lowbits == 0x4:
                print('== FAIL: the pmda%s(1) failed to start up.' % pmda)
                STATUS = 2
            elif lowbits == 0x8:
                print('== FAIL: the pmda%s(1) was timed out by pmcd.' % pmda)
                STATUS = 2
        if self._verbose:
            print('** DEBUG: completed basic installation checks')


class SecureReport(Verifier):
    """ Verifies security protocol extensions for a PCP collector """
    _metrics = [ 'pmcd.feature.secure', 'pmcd.feature.authentication' ]

    def report(self, manager):
        global STATUS
        group = manager[self._name]
        status = self.current(group, 'pmcd.feature.secure')
        if status[''] != 1:
            print("== FAIL: pmcd(1) doesn't support secure sockets")
            print('\tCheck tutorials, verify NSS certificate database.')
            STATUS = 2
        status = self.current(group, 'pmcd.feature.authentication')
        if status[''] == 0:
            print("== FAIL: pmcd doesn't support user authentication")
            print('\tCheck tutorials, verify SASL configuration.')
            STATUS = 2
        if self._verbose:
            print('** DEBUG: completed secure install verification')


class ContainersReport(Verifier):
    """ Verifies the container setup for a PCP installation """
    _metrics = [ 'pmcd.agent.status', 'pmcd.agent.type',
                 'pmcd.feature.containers' ]

    def report(self, manager):
        global STATUS
        group = manager[self._name]
        pmdas = self.current(group, 'pmcd.agent.status')
        if 'linux' not in pmdas:
            print('== FAIL: the pmdalinux(1) agent is not installed.')
            print('\tCheck man page, Install PMDA from %s/pmdas/linux.' %
                  manager.pmGetConfig('PCP_PMDAS_DIR'))
            STATUS = 2
        elif self._verbose:
            print('** DEBUG: verified pmdalinux installed')
        if 'root' not in pmdas:
            print('== FAIL: the pmdaroot(1) agent is not installed.')
            print('\tCheck man page, Install PMDA from %s/pmdas/root.' %
                  manager.pmGetConfig('PCP_PMDAS_DIR'))
            STATUS = 2
        elif self._verbose:
            print('** DEBUG: verified pmdaroot installed')
        status = self.current(group, 'pmcd.agent.status')
        if status['root'] == 4:
            print('== FAIL: the pmdaroot(1) agent exited immediately.')
            print('\tPossible permissions issues on pmdaroot socket file?')
            print('\tCheck %s/pmcd/root.log for details, check selinux AVCs.' %
                  manager.pmGetConfig('PCP_LOG_DIR'))
            STATUS = 2
        elif self._verbose:
            print('** DEBUG: verified pmdaroot running')
        typeof = self.current(group, 'pmcd.agent.type')
        if typeof['linux'] == 0:
            print('== FAIL: the pmdalinux(1) is running as a DSO.')
            print('\tContainers namespace operations cannot function.')
            print('\tCheck man page, Install daemon PMDA %s/pmdas/linux.' %
                  manager.pmGetConfig('PCP_PMDAS_DIR'))
            STATUS = 2
        elif self._verbose:
            print('** DEBUG: verified pmdalinux non-DSO mode')
        status = self.current(group, 'pmcd.feature.containers')
        if status[''] != 1:
            print('== FAIL: libpcp or pmcd does not support containers.')
            print('\tThe running kernel or libc may not support setns(2).')
            print("value=%d\n", group['pmcd.feature.containers'].netValues)
            STATUS = 2
        if self._verbose:
            print('** DEBUG: completed containers verification')


class VerifyOptions(pmapi.pmOptions):
    def __init__(self):
        pmapi.pmOptions.__init__(self, "a:csD:h:vV?")
        self._mode = PM_MODE_FORW
        self.pmSetOptionSamples('1')	# one-shot
        self.pmSetOptionCallback(self.option)
        self.pmSetOverrideCallback(self.override)
        self.pmSetLongOptionHeader('General Options')
        self.pmSetLongOptionDebug()
        self.pmSetLongOptionHost()
        self.pmSetLongOptionArchive()
        self.pmSetLongOptionVersion()
        self.pmSetLongOption("verbose", 0, 'v', '', "increase check verbosity")
        self.pmSetLongOptionHelp()
        self.pmSetLongOptionHeader('Verification Modes')
        self.pmSetLongOption("containers", 0, 'c', '', "check containers setup")
        self.pmSetLongOption("secure", 0, 's', '', "check secure connections setup")
        self.verify = 'basic'	# default, basic verification checks
        self.verbose = 0	# be quiet by default

    def override(self, opt):
        """ Override any few standard PCP options we use here """
        if (opt == 's'):
            return 1
        return 0

    def option(self, opt, optarg, index):
        """ Perform setup for an individual command line option """
        # pylint: disable=W0613
        if opt == 'c':
            self.verify = 'containers'
        elif opt == 's':
            self.verify = 'secure'
        elif opt == 'v':
            self.verbose = 1

if __name__ == '__main__':
    try:
        options = VerifyOptions()
        manager = pmcc.MetricGroupManager.builder(options, sys.argv)
        verbose = options.verbose
        report = options.verify
        if report == 'containers':
            manager.printer = ContainersReport().setup(manager, report, verbose)
        elif report == 'secure':
            manager.printer = SecureReport().setup(manager, report, verbose)
        else:
            manager.printer = BasicReport().setup(manager, report, verbose)
        sts = manager.run() 
        if sts != 0:
            sys.exit(sts)
        sys.exit(STATUS)
    except pmapi.pmErr as error:
        print('%s: %s\n' % (error.progname(), error.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
    except KeyboardInterrupt:
        pass
