#!/usr/bin/env pmpython
#
# Copyright (C) 2015-2017 Marko Myllynen <myllynen@redhat.com>
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

# [zbxsend] Copyright (C) 2014 Sergey Kirillov <sergey.kirillov@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# pylint: disable=superfluous-parens
# pylint: disable=invalid-name, line-too-long, no-self-use
# pylint: disable=too-many-boolean-expressions, too-many-statements
# pylint: disable=too-many-instance-attributes, too-many-locals
# pylint: disable=too-many-branches, too-many-nested-blocks
# pylint: disable=bare-except, broad-except

""" PCP to Zabbix Bridge """

# Common imports
from collections import OrderedDict
import errno
import time
import sys

# Our imports
try:
    import json
except:
    import simplejson as json
import socket
import struct

# PCP Python PMAPI
from pcp import pmapi, pmconfig
from cpmapi import PM_CONTEXT_ARCHIVE, PM_ERR_EOL, PM_IN_NULL, PM_DEBUG_APPL0, PM_DEBUG_APPL1
from cpmapi import PM_TIME_SEC

if sys.version_info[0] >= 3:
    long = int # pylint: disable=redefined-builtin

# Default config
DEFAULT_CONFIG = ["./pcp2zabbix.conf", "$HOME/.pcp2zabbix.conf", "$HOME/.pcp/pcp2zabbix.conf", "$PCP_SYSCONF_DIR/pcp2zabbix.conf"]

# Defaults
CONFVER = 1
ZBXSERVER = "localhost"
ZBXPORT = 10051
ZBXPREFIX = "pcp."

class ZabbixMetric(object): # pylint: disable=too-few-public-methods
    """ A Zabbix metric """
    def __init__(self, host, key, value, clock):
        self.host = host
        self.key = key
        self.value = value
        self.clock = clock

    def __repr__(self):
        return 'Metric(%r, %r, %r, %r)' % (self.host, self.key, self.value, self.clock)

class PCP2Zabbix(object):
    """ PCP to Zabbix """
    def __init__(self):
        """ Construct object, prepare for command line handling """
        self.context = None
        self.daemonize = 0
        self.pmconfig = pmconfig.pmConfig(self)
        self.opts = self.options()

        # Configuration directives
        self.keys = ('source', 'output', 'derived', 'header', 'globals',
                     'samples', 'interval', 'type', 'precision', 'daemonize',
                     'zabbix_server', 'zabbix_port', 'zabbix_host', 'zabbix_interval', 'zabbix_prefix',
                     'count_scale', 'space_scale', 'time_scale', 'version',
                     'speclocal', 'instances', 'ignore_incompat', 'omit_flat')

        # The order of preference for options (as present):
        # 1 - command line options
        # 2 - options from configuration file(s)
        # 3 - built-in defaults defined below
        self.check = 0
        self.version = CONFVER
        self.source = "local:"
        self.output = None # For pmrep conf file compat only
        self.speclocal = None
        self.derived = None
        self.header = 1
        self.globals = 1
        self.samples = None # forever
        self.interval = pmapi.timeval(60)      # 60 sec
        self.opts.pmSetOptionInterval(str(60)) # 60 sec
        self.delay = 0
        self.type = 0
        self.ignore_incompat = 0
        self.instances = []
        self.omit_flat = 0
        self.precision = 3 # .3f
        self.timefmt = "%H:%M:%S" # For compat only
        self.interpol = 0
        self.count_scale = None
        self.space_scale = None
        self.time_scale = None

        self.zabbix_server = ZBXSERVER
        self.zabbix_port = ZBXPORT
        self.zabbix_host = None
        self.zabbix_interval = None
        self.zabbix_prefix = ZBXPREFIX

        # Internal
        self.runtime = -1

        self.zabbix_prevsend = None
        self.zabbix_metrics = []

        # Performance metrics store
        # key - metric name
        # values - 0:label, 1:instance(s), 2:unit/scale, 3:type, 4:width, 5:pmfg item
        self.metrics = OrderedDict()
        self.pmfg = None
        self.pmfg_ts = None

        # Read configuration and prepare to connect
        self.config = self.pmconfig.set_config_file(DEFAULT_CONFIG)
        self.pmconfig.read_options()
        self.pmconfig.read_cmd_line()
        self.pmconfig.prepare_metrics()

    def options(self):
        """ Setup default command line argument option handling """
        opts = pmapi.pmOptions()
        opts.pmSetOptionCallback(self.option)
        opts.pmSetOverrideCallback(self.option_override)
        opts.pmSetShortOptions("a:h:LK:c:Ce:D:V?HGA:S:T:O:s:t:rIi:vP:q:b:y:g:p:X:E:x:")
        opts.pmSetShortUsage("[option...] metricspec [...]")

        opts.pmSetLongOptionHeader("General options")
        opts.pmSetLongOptionArchive()      # -a/--archive
        opts.pmSetLongOptionArchiveFolio() # --archive-folio
        opts.pmSetLongOptionContainer()    # --container
        opts.pmSetLongOptionHost()         # -h/--host
        opts.pmSetLongOptionLocalPMDA()    # -L/--local-PMDA
        opts.pmSetLongOptionSpecLocal()    # -K/--spec-local
        opts.pmSetLongOption("config", 1, "c", "FILE", "config file path")
        opts.pmSetLongOption("check", 0, "C", "", "check config and metrics and exit")
        opts.pmSetLongOption("derived", 1, "e", "FILE|DFNT", "derived metrics definitions")
        self.daemonize = opts.pmSetLongOption("daemonize", 0, "", "", "daemonize on startup") # > 1
        opts.pmSetLongOptionDebug()        # -D/--debug
        opts.pmSetLongOptionVersion()      # -V/--version
        opts.pmSetLongOptionHelp()         # -?/--help

        opts.pmSetLongOptionHeader("Reporting options")
        opts.pmSetLongOption("no-header", 0, "H", "", "omit headers")
        opts.pmSetLongOption("no-globals", 0, "G", "", "omit global metrics")
        opts.pmSetLongOptionAlign()        # -A/--align
        opts.pmSetLongOptionStart()        # -S/--start
        opts.pmSetLongOptionFinish()       # -T/--finish
        opts.pmSetLongOptionOrigin()       # -O/--origin
        opts.pmSetLongOptionSamples()      # -s/--samples
        opts.pmSetLongOptionInterval()     # -t/--interval
        opts.pmSetLongOption("raw", 0, "r", "", "output raw counter values (no rate conversion)")
        opts.pmSetLongOption("ignore-incompat", 0, "I", "", "ignore incompatible instances (default: abort)")
        opts.pmSetLongOption("instances", 1, "i", "STR", "instances to report (default: all current)")
        opts.pmSetLongOption("omit-flat", 0, "v", "", "omit single-valued metrics with -i (default: include)")
        opts.pmSetLongOption("precision", 1, "P", "N", "N digits after the decimal separator (default: 3)")
        opts.pmSetLongOption("count-scale", 1, "q", "SCALE", "default count unit")
        opts.pmSetLongOption("space-scale", 1, "b", "SCALE", "default space unit")
        opts.pmSetLongOption("time-scale", 1, "y", "SCALE", "default time unit")

        opts.pmSetLongOption("zabbix-server", 1, "g", "SERVER", "zabbix server (default: " + ZBXSERVER + ")")
        opts.pmSetLongOption("zabbix-port", 1, "p", "PORT", "zabbix port (default: " + str(ZBXPORT) + ")")
        opts.pmSetLongOption("zabbix-host", 1, "X", "HOSTID", "zabbix host-id for measurements")
        opts.pmSetLongOption("zabbix-interval", 1, "E", "INTERVAL", "interval to send collected metrics")
        opts.pmSetLongOption("zabbix-prefix", 1, "x", "PREFIX", "prefix for metric names (default: " + ZBXPREFIX + ")")

        return opts

    def option_override(self, opt):
        """ Override standard PCP options """
        if opt == 'H' or opt == 'K' or opt == 'g':
            return 1
        return 0

    def option(self, opt, optarg, index):
        """ Perform setup for an individual command line option """
        if index == self.daemonize and opt == '':
            self.daemonize = 1
            return
        if opt == 'K':
            if not self.speclocal or not self.speclocal.startswith("K:"):
                self.speclocal = "K:" + optarg
            else:
                self.speclocal = self.speclocal + "|" + optarg
        elif opt == 'c':
            self.config = optarg
        elif opt == 'C':
            self.check = 1
        elif opt == 'e':
            self.derived = optarg
        elif opt == 'H':
            self.header = 0
        elif opt == 'G':
            self.globals = 0
        elif opt == 'r':
            self.type = 1
        elif opt == 'I':
            self.ignore_incompat = 1
        elif opt == 'i':
            self.instances = self.instances + self.pmconfig.parse_instances(optarg)
        elif opt == 'v':
            self.omit_flat = 1
        elif opt == 'P':
            try:
                self.precision = int(optarg)
            except:
                sys.stderr.write("Error while parsing options: Integer expected.\n")
                sys.exit(1)
        elif opt == 'q':
            self.count_scale = optarg
        elif opt == 'b':
            self.space_scale = optarg
        elif opt == 'y':
            self.time_scale = optarg
        elif opt == 'g':
            self.zabbix_server = optarg
        elif opt == 'p':
            self.zabbix_port = optarg
        elif opt == 'X':
            self.zabbix_host = optarg
        elif opt == 'E':
            self.zabbix_interval = optarg
        elif opt == 'x':
            self.zabbix_prefix = optarg
        else:
            raise pmapi.pmUsageErr()

    def connect(self):
        """ Establish a PMAPI context """
        context, self.source = pmapi.pmContext.set_connect_options(self.opts, self.source, self.speclocal)

        self.pmfg = pmapi.fetchgroup(context, self.source)
        self.pmfg_ts = self.pmfg.extend_timestamp()
        self.context = self.pmfg.get_context()

        if pmapi.c_api.pmSetContextOptions(self.context.ctx, self.opts.mode, self.opts.delta):
            raise pmapi.pmUsageErr()

        self.pmconfig.validate_metrics()

    def validate_config(self):
        """ Validate configuration options """
        if self.version != CONFVER:
            sys.stderr.write("Incompatible configuration file version (read v%s, need v%d).\n" % (self.version, CONFVER))
            sys.exit(1)

        if self.zabbix_host is None:
            self.zabbix_host = self.context.pmGetContextHostName()

        self.pmconfig.finalize_options()

        # Adjust interval
        if self.zabbix_interval:
            self.zabbix_interval = float(pmapi.timeval.fromInterval(self.zabbix_interval))
            if self.zabbix_interval < float(self.interval):
                self.zabbix_interval = float(self.interval)
        else:
            self.zabbix_interval = float(self.interval)

    def execute(self):
        """ Fetch and report """
        # Debug
        if self.context.pmDebug(PM_DEBUG_APPL1):
            sys.stdout.write("Known config file keywords: " + str(self.keys) + "\n")
            sys.stdout.write("Known metric spec keywords: " + str(self.pmconfig.metricspec) + "\n")

        # Set delay mode, interpolation
        if self.context.type != PM_CONTEXT_ARCHIVE:
            self.delay = 1
            self.interpol = 1

        # Common preparations
        self.context.prepare_execute(self.opts, False, self.interpol, self.interval)

        # Headers
        if self.header == 1:
            self.header = 0
            self.write_header()

        # Just checking
        if self.check == 1:
            return

        # Daemonize when requested
        if self.daemonize == 1:
            self.opts.daemonize()

        # Align poll interval to host clock
        if self.context.type != PM_CONTEXT_ARCHIVE and self.opts.pmGetOptionAlignment():
            align = float(self.opts.pmGetOptionAlignment()) - (time.time() % float(self.opts.pmGetOptionAlignment()))
            time.sleep(align)

        # Main loop
        while self.samples != 0:
            # Fetch values
            try:
                self.pmfg.fetch()
            except pmapi.pmErr as error:
                if error.args[0] == PM_ERR_EOL:
                    break
                raise error

            # Watch for endtime in uninterpolated mode
            if not self.interpol:
                if float(self.pmfg_ts().strftime('%s')) > float(self.opts.pmGetOptionFinish()):
                    break

            # Report and prepare for the next round
            self.report(self.pmfg_ts())
            if self.samples and self.samples > 0:
                self.samples -= 1
            if self.delay and self.interpol and self.samples != 0:
                self.context.pmtimevalSleep(self.interval)

        # Allow to flush buffered values / say goodbye
        self.report(None)

    def report(self, tstamp):
        """ Report the metric values """
        if tstamp != None:
            tstamp = tstamp.strftime(self.timefmt)

        self.write_zabbix(tstamp)

    def write_header(self):
        """ Write info header """
        if self.context.type == PM_CONTEXT_ARCHIVE:
            self.zabbix_interval = 250 # See zabbix_sender(8)
            sys.stdout.write("Sending %d archived metrics to Zabbix server %s...\n(Ctrl-C to stop)\n" % (len(self.metrics), self.zabbix_server))
            return

        sys.stdout.write("Sending %d metrics to Zabbix server %s every %d sec" % (len(self.metrics), self.zabbix_server, self.zabbix_interval))
        if self.runtime != -1:
            sys.stdout.write(":\n%s samples(s) with %.1f sec interval ~ %d sec runtime.\n" % (self.samples, float(self.interval), self.runtime))
        elif self.samples:
            duration = (self.samples - 1) * float(self.interval)
            sys.stdout.write(":\n%s samples(s) with %.1f sec interval ~ %d sec runtime.\n" % (self.samples, float(self.interval), duration))
        else:
            sys.stdout.write("...\n(Ctrl-C to stop)\n")

    def recv_from_zabbix(self, sock, count):
        """ Receive a response from a Zabbix server. """
        buf = b''
        while len(buf) < count:
            chunk = sock.recv(count - len(buf))
            if not chunk:
                return buf
            buf += chunk
        return buf

    def send_to_zabbix(self, metrics, zabbix_host, zabbix_port, timeout=15):
        """ Send a set of metrics to a Zabbix server. """
        j = json.dumps
        # Zabbix has a very fragile JSON parser, so we cannot use json to
        # dump the whole packet
        metrics_data = []
        for m in metrics:
            clock = m.clock or time.time()
            metrics_data.append(('\t\t{\n'
                                 '\t\t\t"host":%s,\n'
                                 '\t\t\t"key":%s,\n'
                                 '\t\t\t"value":%s,\n'
                                 '\t\t\t"clock":%d}') % (j(m.host), j(m.key), j(m.value), clock))
        json_data = ('{\n'
                     '\t"request":"sender data",\n'
                     '\t"data":[\n%s]\n'
                     '}') % (',\n'.join(metrics_data))

        data_len = struct.pack('<Q', len(json_data))
        packet = b'ZBXD\1' + data_len + json_data.encode('utf-8')
        try:
            # NB: Zabbix trapper protocol (as of Zabbix 3.4) supports only one
            # transaction per connection, so we can't use a long-lived socket.
            zabbix = socket.socket()
            zabbix.connect((zabbix_host, zabbix_port))
            zabbix.settimeout(timeout)
            # send metrics to zabbix
            zabbix.sendall(packet)
            # get response header from zabbix
            resp_hdr = self.recv_from_zabbix(zabbix, 13)
            if not bytes.decode(resp_hdr).startswith('ZBXD\1') or len(resp_hdr) != 13:
                if self.context.pmDebug(PM_DEBUG_APPL0):
                    print('Invalid Zabbix response len=%d' % len(resp_hdr))
                return False
            resp_body_len = struct.unpack('<Q', resp_hdr[5:])[0]
            # get response body from zabbix
            resp_body = zabbix.recv(resp_body_len)
            resp = json.loads(bytes.decode(resp_body))
            if self.context.pmDebug(PM_DEBUG_APPL0):
                print('Got response from Zabbix: %s' % resp)
            if resp.get('response') != 'success':
                sys.stderr.write('Error response from Zabbix: %s', resp)
                sys.stderr.flush()
                return False
            return True
        except socket.timeout as err:
            sys.stderr.write("Zabbix connection timed out: " + str(err))
            sys.stderr.flush()
            return False
        finally:
            zabbix.close()

    def write_zabbix(self, timestamp):
        """ Write (send) metrics to a Zabbix server """
        if timestamp is None:
            # Send any remaining buffered values
            if self.zabbix_metrics:
                self.send_to_zabbix(self.zabbix_metrics, self.zabbix_server, self.zabbix_port)
                self.zabbix_metrics = []
            return

        ts = self.context.datetime_to_secs(self.pmfg_ts(), PM_TIME_SEC)

        if self.zabbix_prevsend is None:
            self.zabbix_prevsend = ts

        # Collect the results
        for metric in self.metrics:
            try:
                for inst, name, val in self.metrics[metric][5](): # pylint: disable=unused-variable
                    key = self.zabbix_prefix + metric
                    if name:
                        key += "[" + name + "]"
                    if inst != PM_IN_NULL and not name:
                        continue
                    try:
                        value = val()
                        fmt = "." + str(self.precision) + "f"
                        value = format(value, fmt) if isinstance(value, float) else str(value)
                        self.zabbix_metrics.append(ZabbixMetric(self.zabbix_host, key, value, ts))
                    except:
                        pass
            except:
                pass

        # Send when needed
        if self.context.type == PM_CONTEXT_ARCHIVE:
            if len(self.zabbix_metrics) >= self.zabbix_interval:
                self.send_to_zabbix(self.zabbix_metrics, self.zabbix_server, self.zabbix_port)
                self.zabbix_metrics = []
        elif ts - self.zabbix_prevsend > self.zabbix_interval:
            self.send_to_zabbix(self.zabbix_metrics, self.zabbix_server, self.zabbix_port)
            self.zabbix_metrics = []
            self.zabbix_prevsend = ts

    def finalize(self):
        """ Finalize and clean up """
        if self.zabbix_metrics:
            self.send_to_zabbix(self.zabbix_metrics, self.zabbix_server, self.zabbix_port)
            self.zabbix_metrics = []

if __name__ == '__main__':
    try:
        P = PCP2Zabbix()
        P.connect()
        P.validate_config()
        P.execute()
        P.finalize()

    except pmapi.pmErr as error:
        sys.stderr.write('%s: %s\n' % (error.progname(), error.message()))
        sys.exit(1)
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except IOError as error:
        if error.errno != errno.EPIPE:
            sys.stderr.write("%s\n" % str(error))
            sys.exit(1)
    except KeyboardInterrupt:
        sys.stdout.write("\n")
        P.finalize()
