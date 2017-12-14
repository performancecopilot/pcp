#!/usr/bin/env pmpython
#
# Copyright (C) 2017 Red Hat
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

""" read from a prometheus end-point URL, write to a PCP archive """

import argparse
import errno
import time
import sys
from datetime import datetime, timedelta
import socket
import time
import math
import re
import os
import requests

# PCP Python PMAPI
from pcp import pmapi, pmi, pmconfig
from cpmapi import PM_CONTEXT_ARCHIVE, PM_CONTEXT_LOCAL
from cpmapi import PM_ERR_EOL, PM_IN_NULL, PM_DEBUG_APPL1
from cpmapi import PM_TYPE_FLOAT, PM_TYPE_DOUBLE, PM_TYPE_STRING
from cpmapi import PM_TIME_SEC
from cpmi import PMI_ERR_DUPINSTNAME

class Prometheus2PCP(object):
    """
        periodically http get from a Prometheus end-point URL and write
        to a PCP archive. Metric translation is automatic.
    """
    def __init__(self):
        opts = argparse.ArgumentParser(description='prometheus2pcp: read from an end-point URL, write to a PCP archive')
        opts.add_argument('-a', '--archive', type=str, default=None, help='output archive (must not exist)')
        opts.add_argument('-u', '--url', type=str, default=None, help='prometheus endpoint URL')
        opts.add_argument('-s', '--samples', type=int, default = -1, help='stop after this many samples')
        opts.add_argument('-t', '--interval', type=float, default=2.0, help='interval between URL fetches (seconds)')
        opts.add_argument('-H', '--host', type=str, default='localhost', help='host/url identifier for archive label')
        opts.add_argument('-v', '--verbose', help='verbose messages', action="store_true")
        opts.add_argument('-z', '--zone', help='timezone for archive label')
        self.args = opts.parse_args()

        self.pmi = None

    def openArchive(self, tz=None):
        if self.args.verbose:
            print("create new archive %s" % self.args.archive)
        if self.args.archive is None:
            raise pmapi.pmUsageErr()
        # TODO create path to archive if necessary
        self.pmi = pmi.pmiLogImport(self.args.archive)
        if self.args.host is not None:
            self.pmi.pmiSetHostname(self.args.host)
        if self.args.zone is not None:
            self.pmi.pmiSetTimezone(self.args.zone)

    def fetchURL(self):
        if self.args.verbose:
            print("fetch '%s'" % self.args.url)

    def run(self):
        # main loop
        samples = self.args.samples
        timestamp = time.time()
        while samples != 0:
            # Fetch the URL
            try:
                # TODO fetch the URL
                self.fetchURL()
            except pmapi.pmErr as error:
                if error.args[0] == PM_ERR_EOL:
                    break
                raise error

            # Report and sleep until next sample time
            self.report(timestamp)
            samples -= 1
            if samples != 0:
                latency = time.time() - timestamp
                if self.args.verbose:
                    print('Notice: fetch latency %.6f ms\n' % latency * 1000)
                if latency > self.args.interval:
                    print('Warning: fetch latency of %.4fs exceeds sampling interval of %.4f\n'
                        % (latency, self.args.interval))
                else:
                    time.sleep(self.args.interval - latency)
                timestamp = time.time()

    def report(self, timestamp):
        """ write out current values """
        timetuple = math.modf(timestamp)
        useconds = int(timetuple[0] * 1000000)
        seconds = int(timetuple[1])
        if self.pmi is not None:
            try:
                # this fails if there is nothing to write out yet
                self.pmi.pmiWrite(seconds, useconds)
            except pmi.pmiErr as error:
                if self.args.verbose:
                    print("   Warning: %s" % error)

    def finalize(self, status):
        """ Finalize and clean up """
        if self.pmi is not None:
            self.pmi.pmiEnd()
        sys.exit(status)

if __name__ == '__main__':
    try:
        p = Prometheus2PCP()
        p.openArchive()
        p.run()

    except pmapi.pmErr as error:
        sys.stderr.write('%s: %s\n' % (error.progname(), error.message()))
        p.finalize(1)
    except pmapi.pmUsageErr as usage:
        usage.message()
        p.finalize(1)
    except IOError as error:
        if error.errno != errno.EPIPE:
            sys.stderr.write("%s\n" % str(error))
            p.finalize(1)
    except KeyboardInterrupt:
        sys.stdout.write("interrupt\n")
        p.finalize(1)
    p.finalize(0)
