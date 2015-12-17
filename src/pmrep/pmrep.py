#!/usr/bin/pcp python
#
# Copyright (C) 2015 Marko Myllynen <myllynen@redhat.com>
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

# pylint: disable=fixme, line-too-long, bad-whitespace, invalid-name
# pylint: disable=superfluous-parens
""" Performance Metrics Reporter """

from collections import OrderedDict
from datetime import datetime
try:
    import ConfigParser
except ImportError:
    import configparser as ConfigParser
try:
    import json
except:
    import simplejson as json
import socket
import struct
import time
import copy
import sys
import os
import re

from pcp import pmapi, pmi
from cpmapi import PM_CONTEXT_ARCHIVE, PM_CONTEXT_HOST, PM_CONTEXT_LOCAL, PM_MODE_FORW, PM_MODE_INTERP, PM_ERR_TYPE, PM_ERR_EOL, PM_ERR_NAME, PM_IN_NULL, PM_SEM_COUNTER, PM_TIME_MSEC, PM_TIME_SEC, PM_XTB_SET
from cpmapi import PM_TYPE_32, PM_TYPE_U32, PM_TYPE_64, PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE, PM_TYPE_STRING
from cpmi import PMI_ERR_DUPINSTNAME

if sys.version_info[0] >= 3:
    long = int

# Default config
DEFAULT_CONFIG = "./pmrep.conf"

# Default field separators, config/time formats, missing/truncated values
CSVSEP  = ","
CSVTIME = "%Y-%m-%d %H:%M:%S"
OUTSEP  = "  "
OUTTIME = "%H:%M:%S"
ZBXPORT = 10051
ZBXPRFX = "pcp."
NO_VAL  = "N/A"
TRUNC   = "xxx"
VERSION = 1

# Output targets
OUTPUT_ARCHIVE = "archive"
OUTPUT_CSV     = "csv"
OUTPUT_STDOUT  = "stdout"
OUTPUT_ZABBIX  = "zabbix"

class ZabbixMetric(object):
    """ A Zabbix metric """
    def __init__(self, host, key, value, clock):
        self.host = host
        self.key = key
        self.value = value
        self.clock = clock

    def __repr__(self):
        return 'Metric(%r, %r, %r, %r)' % (self.host, self.key, self.value, self.clock)

def recv_from_zabbix(sock, count):
    """ Receive a response from a Zabbix server. """
    buf = b''
    while len(buf) < count:
        chunk = sock.recv(count - len(buf))
        if not chunk:
            return buf
        buf += chunk
    return buf

def send_to_zabbix(metrics, zabbix_host, zabbix_port, timeout=15):
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
                             '\t\t\t"clock":%.5f}') % (j(m.host), j(m.key), j(m.value), clock))
    json_data = ('{\n'
                 '\t"request":"sender data",\n'
                 '\t"data":[\n%s]\n'
                 '}') % (',\n'.join(metrics_data))

    data_len = struct.pack('<Q', len(json_data))
    packet = b'ZBXD\1' + data_len + json_data.encode('utf-8')
    try:
        zabbix = socket.socket()
        zabbix.connect((zabbix_host, zabbix_port))
        zabbix.settimeout(timeout)
        # send metrics to zabbix
        zabbix.sendall(packet)
        # get response header from zabbix
        resp_hdr = recv_from_zabbix(zabbix, 13)
        if not bytes.decode(resp_hdr).startswith('ZBXD\1') or len(resp_hdr) != 13:
            # debug: write('Invalid Zabbix response len=%d' % len(resp_hdr))
            return False
        resp_body_len = struct.unpack('<Q', resp_hdr[5:])[0]
        # get response body from zabbix
        resp_body = zabbix.recv(resp_body_len)
        resp = json.loads(bytes.decode(resp_body))
        # debug: write('Got response from Zabbix: %s' % resp)
        if resp.get('response') != 'success':
            sys.stderr.write('Error response from Zabbix: %s', resp)
            return False
        return True
    except socket.timeout as err:
        sys.stderr.write("Zabbix connection timed out: " + str(err))
        return False
    finally:
        zabbix.close()

class PMReporter(object):
    """ Report PCP metrics """

    def __init__(self):
        """ Construct object, prepare for command line handling """
        self.context = None
        self.check = 0
        self.format = None # output format
        self.opts = self.options()
        pmapi.c_api.pmSetOptionFlags(pmapi.c_api.PM_OPTFLAG_POSIX) # RHBZ#1289912

        # Configuration directives
        self.keys = ('source', 'output', 'derived', 'header', 'unitinfo',
                     'globals', 'timestamp', 'samples', 'interval',
                     'delay', 'type', 'width', 'precision', 'delimiter',
                     'extheader', 'repeat_header', 'timefmt', 'interpol',
                     'count_scale', 'space_scale', 'time_scale', 'version',
                     'zabbix_server', 'zabbix_port', 'zabbix_host', 'zabbix_interval')

        # Special command line switches
        self.argless = ('-C', '--check', '-L', '--local-PMDA', '-H', '--no-header', '-U', '--no-unit-info', '-G', '--no-globals', '-p', '--timestamps', '-d', '--delay', '-r', '--raw', '-x', '--extended-header', '-u', '--no-interpol', '-z', '--hostzone')
        self.arghelp = ('-?', '--help', '-V', '--version')

        # The order of preference for parameters (as present):
        # 1 - command line parameters
        # 2 - parameters from configuration file(s)
        # 3 - built-in defaults defined below
        self.config = self.set_config_file()
        self.version = VERSION
        self.source = "local:"
        self.output = OUTPUT_STDOUT
        self.archive = None # output archive
        self.log = None # pmi handle
        self.derived = None
        self.header = 1
        self.unitinfo = 1
        self.globals = 1
        self.timestamp = 0
        self.samples = None # forever
        self.interval = pmapi.timeval(1) # 1 sec
        self.opts.pmSetOptionInterval(str(1))
        self.runtime = -1
        self.delay = 0
        self.type = 0
        self.width = 0
        self.precision = 3 # .3f
        self.delimiter = None
        self.extheader = 0
        self.repeat_header = 0
        self.timefmt = None
        self.interpol = 1
        self.count_scale = None
        self.space_scale = None
        self.time_scale = None
        self.can_scale = None # PCP 3.9 compat

        # Performance metrics store
        # key - metric name
        # values - 0:label, 1:instance(s), 2:unit/scale, 3:type, 4:width
        self.metrics = OrderedDict()

        # Corresponding config file metric specifiers
        self.metricspec = ('label', 'instance', 'unit', 'type', 'width', 'formula')

        self.prevvals = None
        self.currvals = None
        self.ptstamp = 0
        self.ctstamp = 0
        self.pmids = []
        self.descs = []
        self.insts = []

        # Zabbix integration
        self.zabbix_server = None
        self.zabbix_port = ZBXPORT
        self.zabbix_host = None
        self.zabbix_interval = None
        self.zabbix_prevsend = None
        self.zabbix_metrics = []

    def set_config_file(self):
        """ Set configuration file """
        config = DEFAULT_CONFIG

        # Possibly override the built-in default config file before
        # parsing the rest of the command line parameters
        args = iter(sys.argv[1:])
        for arg in args:
            if arg in self.arghelp:
                return None
            if arg == '-c' or arg == '--config':
                try:
                    config = next(args)
                    if not os.path.isfile(config) or not os.access(config, os.R_OK):
                        raise IOError("Failed to read configuration file '%s'." % config)
                except StopIteration:
                    break
        return config

    def set_attr(self, name, value):
        """ Helper to apply config file settings properly """
        if value in ('true', 'True', 'y', 'yes', 'Yes'):
            value = 1
        if value in ('false', 'False', 'n', 'no', 'No'):
            value = 0
        if name == 'source':
            try: # RHBZ#1270176 / PCP < 3.10.8
                if '/' in value:
                    self.opts.pmSetOptionArchive(value)
                else:
                    self.opts.pmSetOptionHost(value) # RHBZ#1289911
            except:
                sys.stderr.write("PCP 3.10.8 or later required for the 'source' directive.\n")
                sys.exit(1)
        elif name == 'samples':
            self.opts.pmSetOptionSamples(value)
            self.samples = self.opts.pmGetOptionSamples()
        elif name == 'interval':
            self.opts.pmSetOptionInterval(value)
            self.interval = self.opts.pmGetOptionInterval()
        elif name == 'type':
            if value == 'raw':
                self.type = 1
            else:
                self.type = 0
        else:
            try:
                setattr(self, name, int(value))
            except ValueError:
                setattr(self, name, value)

    def read_config(self):
        """ Read options from configuration file """
        if self.config is None:
            return
        config = ConfigParser.SafeConfigParser()
        config.read(self.config)
        if not config.has_section('options'):
            return
        for opt in config.options('options'):
            if opt in self.keys:
                self.set_attr(opt, config.get('options', opt))
            else:
                sys.stderr.write("Invalid directive '%s' in %s.\n" % (opt, self.config))
                sys.exit(1)

    def options(self):
        """ Setup default command line argument option handling """
        opts = pmapi.pmOptions()
        opts.pmSetOptionCallback(self.option)
        opts.pmSetOverrideCallback(self.option_override)
        opts.pmSetShortOptions("a:h:LK:c:Co:F:e:D:V?HUGpA:S:T:O:s:t:Z:zdrw:P:l:xE:f:uq:b:y:")
        opts.pmSetShortUsage("[option...] metricspec [...]")

        opts.pmSetLongOptionHeader("General options")
        opts.pmSetLongOptionArchive()      # -a/--archive
        opts.pmSetLongOptionArchiveFolio() # --archive-folio
        opts.pmSetLongOptionHost()         # -h/--host
        opts.pmSetLongOptionLocalPMDA()    # -L/--local-PMDA
        opts.pmSetLongOptionSpecLocal()    # -K/--spec-local
        opts.pmSetLongOption("config", 1, "c", "FILE", "config file path")
        opts.pmSetLongOption("check", 0, "C", "", "check config and metrics and exit")
        opts.pmSetLongOption("output", 1, "o", "OUTPUT", "output target: archive, csv, stdout (default), or zabbix")
        opts.pmSetLongOption("output-archive", 1, "F", "ARCHIVE", "output archive (with -o archive)")
        opts.pmSetLongOption("derived", 1, "e", "FILE|DFNT", "derived metrics definitions")
        #opts.pmSetLongOptionGuiMode()     # -g/--guimode # RHBZ#1289910
        opts.pmSetLongOptionDebug()        # -D/--debug
        opts.pmSetLongOptionVersion()      # -V/--version
        opts.pmSetLongOptionHelp()         # -?/--help

        opts.pmSetLongOptionHeader("Reporting options")
        opts.pmSetLongOption("no-header", 0, "H", "", "omit headers")
        opts.pmSetLongOption("no-unit-info", 0, "U", "", "omit unit info from headers")
        opts.pmSetLongOption("no-globals", 0, "G", "", "omit global metrics")
        opts.pmSetLongOption("timestamps", 0, "p", "", "print timestamps")
        opts.pmSetLongOptionAlign()        # -A/--align
        opts.pmSetLongOptionStart()        # -S/--start
        opts.pmSetLongOptionFinish()       # -T/--finish
        opts.pmSetLongOptionOrigin()       # -O/--origin
        opts.pmSetLongOptionSamples()      # -s/--samples
        opts.pmSetLongOptionInterval()     # -t/--interval
        opts.pmSetLongOptionTimeZone()     # -Z/--timezone
        opts.pmSetLongOptionHostZone()     # -z/--hostzone
        opts.pmSetLongOption("delay", 0, "d", "", "delay, pause between updates for archive replay")
        opts.pmSetLongOption("raw", 0, "r", "", "output raw counter values (no rate conversion)")
        opts.pmSetLongOption("width", 1, "w", "N", "default column width")
        opts.pmSetLongOption("precision", 1, "P", "N", "N digits after the decimal separator (if width enough)")
        opts.pmSetLongOption("delimiter", 1, "l", "STR", "delimiter to separate csv/stdout columns")
        opts.pmSetLongOption("extended-header", 0, "x", "", "display extended header")
        opts.pmSetLongOption("repeat-header", 1, "E", "N", "repeat stdout headers every N lines")
        opts.pmSetLongOption("timestamp-format", 1, "f", "STR", "strftime string for timestamp format")
        opts.pmSetLongOption("no-interpol", 0, "u", "", "disable interpolation mode with archives")
        opts.pmSetLongOption("count-scale", 1, "q", "SCALE", "default count unit")
        opts.pmSetLongOption("space-scale", 1, "b", "SCALE", "default space unit")
        opts.pmSetLongOption("time-scale", 1, "y", "SCALE", "default time unit")

        return opts

    def option_override(self, opt):
        """ Override a few standard PCP options """
        if opt == 'H' or opt == 'p':
            return 1
        return 0

    def option(self, opt, optarg, index):
        """ Perform setup for an individual command line option """
        if opt == 'c':
            self.config = optarg
        elif opt == 'C':
            self.check = 1
        elif opt == 'o':
            if optarg == OUTPUT_ARCHIVE:
                self.output = OUTPUT_ARCHIVE
            elif optarg == OUTPUT_CSV:
                self.output = OUTPUT_CSV
            elif optarg == OUTPUT_STDOUT:
                self.output = OUTPUT_STDOUT
            elif optarg == OUTPUT_ZABBIX:
                self.output = OUTPUT_ZABBIX
            else:
                sys.stderr.write("Invalid output target %s specified.\n" % optarg)
                sys.exit(1)
        elif opt == 'F':
            if os.path.exists(optarg + ".index"):
                sys.stderr.write("Archive %s already exists.\n" % optarg)
                sys.exit(1)
            self.archive = optarg
        elif opt == 'e':
            self.derived = optarg
        elif opt == 'H':
            self.header = 0
        elif opt == 'U':
            self.unitinfo = 0
        elif opt == 'G':
            self.globals = 0
        elif opt == 'p':
            self.timestamp = 1
        elif opt == 'd':
            self.delay = 1
        elif opt == 'r':
            self.type = 1
        elif opt == 'w':
            self.width = int(optarg)
        elif opt == 'P':
            self.precision = int(optarg)
        elif opt == 'l':
            self.delimiter = optarg
        elif opt == 'x':
            self.extheader = 1
        elif opt == 'E':
            self.repeat_header = int(optarg)
        elif opt == 'f':
            self.timefmt = optarg
        elif opt == 'u':
            self.interpol = 0
        elif opt == 'q':
            self.count_scale = optarg
        elif opt == 'b':
            self.space_scale = optarg
        elif opt == 'y':
            self.time_scale = optarg
        else:
            raise pmapi.pmUsageErr()

    def get_cmd_line_metrics(self):
        """ Get metric set specifications from the command line """
        for arg in sys.argv[1:]:
            if arg in self.arghelp:
                return 0
        metrics = []
        for arg in reversed(sys.argv[1:]):
            if arg.startswith('-'):
                if len(metrics):
                    if arg not in self.argless and '=' not in arg:
                        del metrics[-1]
                break
            metrics.append(arg)
        metrics.reverse()
        return metrics

    def parse_metric_info(self, metrics, key, value):
        """ Parse metric information """
        # NB. Uses the config key, not the metric, as the dict key
        if ',' in value:
            # Compact / one-line definition
            metrics[key] = (key + "," + value).split(",")
        else:
            # Verbose / multi-line definition
            if not '.' in key or key.rsplit(".", 1)[1] not in self.metricspec:
                # New metric
                metrics[key] = value.split()
                for index in range(0, 6):
                    if len(metrics[key]) <= index:
                        metrics[key].append(None)
            else:
                # Additional info
                key, spec = key.rsplit(".", 1)
                if key not in metrics:
                    sys.stderr.write("Undeclared metric key %s.\n" % key)
                    sys.exit(1)
                if spec == "formula":
                    if self.derived == None:
                        self.derived = metrics[key][0] + "=" + value
                    else:
                        self.derived += "," + metrics[key][0] + "=" + value
                else:
                    metrics[key][self.metricspec.index(spec)+1] = value

    def prepare_metrics(self):
        """ Construct and prepare the initial metrics set """
        # Get direct and/or sets of metrics from the command line
        metrics = self.get_cmd_line_metrics()
        if metrics == 0:
            return
        if not metrics:
            sys.stderr.write("No metrics specified.\n")
            raise pmapi.pmUsageErr()

        # Don't rely on what get_cmd_line_metrics() might do
        if '-G' in sys.argv:
            self.globals = 0

        # Read config
        config = ConfigParser.SafeConfigParser()
        config.read(self.config)

        # First read global metrics (if not disabled already)
        globmet = OrderedDict()
        if self.globals == 1:
            if config.has_section('global'):
                parsemet = OrderedDict()
                for key in config.options('global'):
                    self.parse_metric_info(parsemet, key, config.get('global', key))
                for metric in parsemet:
                    name = parsemet[metric][:1][0]
                    globmet[name] = parsemet[metric][1:]

        # Add command line and configuration file metric sets
        tempmet = OrderedDict()
        for metric in metrics:
            if metric.startswith(":"):
                tempmet[metric[1:]] = None
            else:
                m = metric.split(",")
                tempmet[m[0]] = m[1:]

        # Get config and set details for configuration file metric sets
        confmet = OrderedDict()
        for spec in tempmet:
            if tempmet[spec] == None:
                if config.has_section(spec):
                    parsemet = OrderedDict()
                    for key in config.options(spec):
                        if key in self.keys:
                            self.set_attr(key, config.get(spec, key))
                        else:
                            self.parse_metric_info(parsemet, key, config.get(spec, key))
                            for metric in parsemet:
                                name = parsemet[metric][:1][0]
                                confmet[name] = parsemet[metric][1:]
                            tempmet[spec] = confmet
                else:
                    raise IOError("Metric set definition '%s' not found." % metric)

        # Create the combined metrics set
        if self.globals == 1:
            for metric in globmet:
                self.metrics[metric] = globmet[metric]
        for metric in tempmet:
            if type(tempmet[metric]) is list:
                self.metrics[metric] = tempmet[metric]
            else:
                for m in tempmet[metric]:
                    self.metrics[m] = confmet[m]

    def check_metric(self, metric):
        """ Validate individual metric and get its details """
        try:
            pmid = self.context.pmLookupName(metric)[0]
            desc = self.context.pmLookupDescs(pmid)[0]
            try:
                if self.context.type == PM_CONTEXT_ARCHIVE:
                    inst = self.context.pmGetInDomArchive(desc)
                else:
                    inst = self.context.pmGetInDom(desc) # disk.dev.read
                if not inst[0]:
                    inst = ([PM_IN_NULL], [None])        # pmcd.pmie.logfile
            except pmapi.pmErr:
                inst = ([PM_IN_NULL], [None])            # mem.util.free
            # Reject unsupported types
            mtype = desc.contents.type
            if not (mtype == PM_TYPE_32 or
                    mtype == PM_TYPE_U32 or
                    mtype == PM_TYPE_64 or
                    mtype == PM_TYPE_U64 or
                    mtype == PM_TYPE_FLOAT or
                    mtype == PM_TYPE_DOUBLE or
                    mtype == PM_TYPE_STRING):
                raise pmapi.pmErr(PM_ERR_TYPE)
            self.pmids.append(pmid)
            self.descs.append(desc)
            self.insts.append(inst)
        except pmapi.pmErr as error:
            sys.stderr.write("Invalid metric %s (%s).\n" % (metric, str(error)))
            sys.exit(1)

    def validate_config(self):
        """ Validate configuration parameters """
        if self.version != VERSION:
            sys.stderr.write("Incompatible configuration file version (read v%s, need v%d).\n" % (self.version, VERSION))
            sys.exit(1)

        if self.context.type == PM_CONTEXT_ARCHIVE:
            self.source = self.opts.pmGetOptionArchives()[0] # RHBZ#1262723
        if self.context.type == PM_CONTEXT_HOST:
            self.source = self.context.pmGetContextHostName()
        if self.context.type == PM_CONTEXT_LOCAL:
            self.source = "@" # PCPIntro(1), RHBZ#1289911

        if self.output == OUTPUT_ARCHIVE and not self.archive:
            sys.stderr.write("Archive must be defined with archive output.\n")
            sys.exit(1)

        if self.output == OUTPUT_ZABBIX and (not self.zabbix_server or \
           not self.zabbix_port or not self.zabbix_host):
            sys.stderr.write("zabbix_server, zabbix_port, and zabbix_host must be defined with Zabbix.\n")
            sys.exit(1)

        # Runtime overrides samples/interval
        if self.opts.pmGetOptionFinishOptarg():
            self.runtime = int(float(self.opts.pmGetOptionFinish()) - float(self.opts.pmGetOptionStart()))
            if self.opts.pmGetOptionSamples():
                self.samples = self.opts.pmGetOptionSamples()
                if self.samples < 2:
                    self.samples = 2
                self.interval = float(self.runtime) / (self.samples - 1)
                self.opts.pmSetOptionInterval(str(self.interval))
                self.interval = self.opts.pmGetOptionInterval()
            else:
                self.interval = self.opts.pmGetOptionInterval()
                if int(self.interval) == 0:
                    sys.stderr.write("Interval can't be less than 1 second.\n")
                    sys.exit(1)
                self.samples = self.runtime / int(self.interval) + 1
            if int(self.interval) > self.runtime:
                sys.stderr.write("Interval can't be longer than runtime.\n")
                sys.exit(1)
        else:
            self.samples = self.opts.pmGetOptionSamples()
            self.interval = self.opts.pmGetOptionInterval()

        if self.output == OUTPUT_ZABBIX:
            if self.zabbix_interval:
                self.zabbix_interval = int(pmapi.timeval.fromInterval(self.zabbix_interval))
                if self.zabbix_interval < int(self.interval):
                    self.zabbix_interval = int(self.interval)
            else:
                self.zabbix_interval = int(self.interval)

        self.can_scale = "pmParseUnitsStr" in dir(self.context)

    def validate_metrics(self):
        """ Validate the metrics set """
        # Check the metrics against PMNS, resolve non-leaf metrics
        if self.derived:
            if self.derived.startswith("/") or self.derived.startswith("."):
                try:
                    self.context.pmLoadDerivedConfig(self.derived)
                except pmapi.pmErr as error:
                    sys.stderr.write("Failed to register derived metric: %s.\n" % str(error))
                    sys.exit(1)
            else:
                for definition in self.derived.split(","):
                    err = ""
                    try:
                        name, expr = definition.split("=")
                        self.context.pmLookupName(name.strip())
                    except pmapi.pmErr as error:
                        if error.args[0] == PM_ERR_NAME:
                            self.context.pmRegisterDerived(name.strip(), expr.strip())
                            continue
                        err = error.message()
                    except ValueError as error:
                        err = "Invalid syntax (expected metric=expression)"
                    except Exception as error:
                        #err = self.context.pmDerivedErrStr() # RHBZ#1286733
                        err = "Unknown reason"
                    finally:
                        if err:
                            sys.stderr.write("Failed to register derived metric: %s.\n" % err)
                            sys.exit(1)
        # Prepare for non-leaf metrics
        metrics = self.metrics
        self.metrics = OrderedDict()
        for metric in metrics:
            try:
                l = len(self.pmids)
                self.context.pmTraversePMNS(metric, self.check_metric)
                if len(self.pmids) == l + 1:
                    # Leaf
                    if metric == self.context.pmNameID(self.pmids[l]):
                        self.metrics[metric] = metrics[metric]
                    else:
                        # But handle single non-leaf case in an archive
                        self.metrics[self.context.pmNameID(self.pmids[l])] = []
                else:
                    # Non-leaf
                    for i in range(l, len(self.pmids)):
                        name = self.context.pmNameID(self.pmids[i])
                        # We ignore specs like disk.dm,,,MB on purpose, for now
                        self.metrics[name] = []
            except pmapi.pmErr as error:
                sys.stderr.write("Invalid metric %s (%s).\n" % (metric, str(error)))
                sys.exit(1)

        # Finalize the metrics set
        for i, metric in enumerate(self.metrics):
            # Fill in all fields for easier checking later
            for index in range(0, 5):
                if len(self.metrics[metric]) <= index:
                    self.metrics[metric].append(None)

            # Label
            if not self.metrics[metric][0]:
                # mem.util.free -> m.u.free
                name = ""
                for m in metric.split("."):
                    name += m[0] + "."
                self.metrics[metric][0] = name[:-2] + m

            # Rawness
            if self.metrics[metric][3] == 'raw' or self.type == 1:
                self.metrics[metric][3] = 1
            else:
                self.metrics[metric][3] = 0

            # Unit/scale
            unitstr = str(self.descs[i].contents.units)
            # Set default unit if not specified on per-metric basis
            if not self.metrics[metric][2]:
                done = 0
                unit = self.descs[i].contents.units
                if self.count_scale and \
                   unit.dimCount == 1 and ( \
                   unit.dimSpace == 0 and
                   unit.dimTime  == 0):
                    self.metrics[metric][2] = self.count_scale
                    done = 1
                if self.space_scale and \
                   unit.dimSpace == 1 and ( \
                   unit.dimCount == 0 and
                   unit.dimTime  == 0):
                    self.metrics[metric][2] = self.space_scale
                    done = 1
                if self.time_scale and \
                   unit.dimTime  == 1 and ( \
                   unit.dimCount == 0 and
                   unit.dimSpace == 0):
                    self.metrics[metric][2] = self.time_scale
                    done = 1
                if not done:
                    self.metrics[metric][2] = unitstr
            # Set unit/scale for non-raw numeric metrics
            try:
                if self.metrics[metric][3] == 0 and self.can_scale and \
                   self.descs[i].contents.type != PM_TYPE_STRING:
                    (unitstr, mult) = self.context.pmParseUnitsStr(self.metrics[metric][2])
                    label = self.metrics[metric][2]
                    if self.descs[i].sem == PM_SEM_COUNTER:
                        label += "/s"
                        if self.descs[i].contents.units.dimTime == 1:
                            label = "util"
                    self.metrics[metric][2] = (label, unitstr, mult)
                else:
                    self.metrics[metric][2] = (unitstr, unitstr, 1)
            except pmapi.pmErr as error:
                sys.stderr.write("%s: %s.\n" % (str(error), self.metrics[metric][2]))
                sys.exit(1)

            # Set default width if not specified on per-metric basis
            if self.metrics[metric][4]:
                self.metrics[metric][4] = int(self.metrics[metric][4])
            elif self.width != 0:
                self.metrics[metric][4] = self.width
            else:
                self.metrics[metric][4] = len(self.metrics[metric][0])
            if self.metrics[metric][4] < len(TRUNC):
                self.metrics[metric][4] = len(TRUNC) # Forced minimum

    # RHBZ#1264147
    def pmids_to_ctypes(self, pmids):
        """ Convert a Python list of pmids (numbers) to
            a ctypes LP_c_uint (a C array of uints).
        """
        from ctypes import c_uint
        pmidA = (c_uint * len(pmids))()
        for i, p in enumerate(pmids):
            pmidA[i] = c_uint(p)
        return pmidA

    def get_mode_step(self):
        """ Get mode and step for pmSetMode """
        if not self.interpol or self.output == OUTPUT_ARCHIVE:
            mode = PM_MODE_FORW
            step = 0
        else:
            mode = PM_MODE_INTERP
            secs_in_24_days = 2073600
            if self.interval.tv_sec > secs_in_24_days:
                step = self.interval.tv_sec
                mode |= PM_XTB_SET(PM_TIME_SEC)
            else:
                step = self.interval.tv_sec*1000 + self.interval.tv_usec/1000
                mode |= PM_XTB_SET(PM_TIME_MSEC)
        return (mode, int(step))

    def execute(self):
        """ Using a PMAPI context (could be either host or archive),
            fetch and report the requested set of values on stdout.
        """
        # Set output primitives
        if self.delimiter == None:
            if self.output == OUTPUT_CSV:
                self.delimiter = CSVSEP
            else:
                self.delimiter = OUTSEP

        # Time
        if self.opts.pmGetOptionTimezone():
            os.environ['TZ'] = self.opts.pmGetOptionTimezone()
            time.tzset()
            self.context.pmNewZone(self.opts.pmGetOptionTimezone())

        if self.timefmt == None:
            if self.output == OUTPUT_CSV:
                self.timefmt = CSVTIME
            else:
                self.timefmt = OUTTIME
        if not self.timefmt:
            self.timestamp = 0

        if self.context.type != PM_CONTEXT_ARCHIVE:
            self.delay = 1
            self.interpol = 1

        # DBG_TRACE_APPL1 == 4096
        if "pmDebug" in dir(self.context) and self.context.pmDebug(4096):
            print("Known config file keywords: " + str(self.keys))
            print("Known metric spec keywords: " + str(self.metricspec))

        # Printing format and headers
        if self.format == None:
            self.define_format()

        if self.extheader == 1:
            self.extheader = 0
            self.write_ext_header()

        if self.header == 1:
            self.header = 0
            self.write_header()
        else:
            self.repeat_header = 0

        # Just checking
        if self.check == 1:
            return

        # Archive fetching mode
        if self.context.type == PM_CONTEXT_ARCHIVE:
            (mode, step) = self.get_mode_step()
            self.context.pmSetMode(mode, self.opts.pmGetOptionOrigin(), step)

        lines = 0
        while self.samples != 0:
            if self.output == OUTPUT_STDOUT:
                if lines > 1 and self.repeat_header == lines:
                    self.write_header()
                    lines = 0
                lines += 1

            try:
                result = self.context.pmFetch(self.pmids_to_ctypes(self.pmids))
            except pmapi.pmErr as error:
                if error.args[0] == PM_ERR_EOL:
                    self.samples = 0
                    continue
                raise error
            self.context.pmSortInstances(result) # XXX Is this really needed?
            values = self.extract(result)
            if self.ctstamp == 0:
                self.ctstamp = copy.copy(result.contents.timestamp)
            self.ptstamp = self.ctstamp
            self.ctstamp = copy.copy(result.contents.timestamp)

            if self.context.type == PM_CONTEXT_ARCHIVE:
                if float(self.ctstamp) < float(self.opts.pmGetOptionStart()):
                    self.context.pmFreeResult(result)
                    continue
                if float(self.ctstamp) > float(self.opts.pmGetOptionFinish()):
                    return

            self.report(self.ctstamp, values)
            self.context.pmFreeResult(result)
            if self.samples and self.samples > 0:
                self.samples -= 1
            if self.delay and self.interpol and self.samples != 0:
                self.context.pmtimevalSleep(self.interval)

        # Allow modules to flush buffered values / say goodbye
        self.report(None, None)

    def extract(self, result):
        """ Extract the metric values from pmResult structure """
        # Metrics incl. all instance values, must match self.format on return
        values = []

        for i, metric in enumerate(self.metrics):
            # Per-metric values incl. all instance values
            # We use dict to make it easier to deal with gone/unknown instances
            values.append({})

            # Populate instance fields to have values for unavailable instances
            # Values are (instance id, instance name, instance value)
            for inst in self.insts[i][0]:
                values[i][inst] = (-1, None, NO_VAL)

            # No values available for this metric
            if result.contents.get_numval(i) == 0:
                continue

            # Process all fetched instances
            for j in range(result.contents.get_numval(i)):
                inst = result.contents.get_inst(i, j)

                # Locate the correct instance and its position
                if inst >= 0:
                    if inst not in self.insts[i][0]:
                        # Ignore newly emerged instances
                        continue
                    k = 0
                    while inst != self.insts[i][0][k]:
                        k += 1

                # Extract and scale the value
                try:
                    # Use native type if no rescaling needed
                    if self.metrics[metric][2][2] == 1 and \
                       str(self.descs[i].contents.units) == \
                       str(self.metrics[metric][2][1]):
                        rescale = 0
                        vtype = self.descs[i].contents.type
                    else:
                        rescale = 1
                        vtype = PM_TYPE_DOUBLE

                    atom = self.context.pmExtractValue(
                        result.contents.get_valfmt(i),
                        result.contents.get_vlist(i, j),
                        self.descs[i].contents.type,
                        vtype)

                    if self.metrics[metric][3] != 1 and rescale and \
                       self.descs[i].contents.type != PM_TYPE_STRING and \
                       self.can_scale:
                        atom = self.context.pmConvScale(
                            vtype,
                            atom, self.descs, i,
                            self.metrics[metric][2][1])

                    val = atom.dref(vtype)
                    if rescale and self.can_scale and \
                       self.descs[i].contents.type != PM_TYPE_STRING:
                        val *= self.metrics[metric][2][2]
                        val = int(val) if val == int(val) else val

                    if inst >= 0:
                        values[i][inst] = (inst, self.insts[i][1][k], val)
                    else:
                        values[i][PM_IN_NULL] = (-1, None, val)

                except pmapi.pmErr as error:
                    sys.stderr.write("%s: %s, aborting.\n" % (metric, str(error)))
                    sys.exit(1)

        # Convert dicts to lists
        vals = []
        for v in values:
            vals.append(v.values())
        values = vals

        # Store current and previous values
        # Output modules need to handle non-existing self.prevvals
        self.prevvals = self.currvals
        self.currvals = values

        return values # XXX Redundant now

    def report(self, tstamp, values):
        """ Report the metric values """
        if tstamp != None:
            ts = self.context.pmLocaltime(tstamp.tv_sec)
            us = int(tstamp.tv_usec)
            dt = datetime(ts.tm_year+1900, ts.tm_mon+1, ts.tm_mday,
                          ts.tm_hour, ts.tm_min, ts.tm_sec, us, None)
            tstamp = dt.strftime(self.timefmt)

        if self.output == OUTPUT_ARCHIVE:
            self.write_archive(tstamp, values)
        if self.output == OUTPUT_CSV:
            self.write_csv(tstamp, values)
        if self.output == OUTPUT_STDOUT:
            self.write_stdout(tstamp, values)
        if self.output == OUTPUT_ZABBIX:
            self.write_zabbix(tstamp, values)

    def define_format(self):
        """ Define stdout format string """
        index = 0
        if self.timestamp == 0:
            #self.format = "{:}{}"
            self.format = "{0:}{1}"
            index += 2
        else:
            tstamp = datetime.fromtimestamp(time.time()).strftime(self.timefmt)
            #self.format = "{:" + str(len(tstamp)) + "}{}"
            self.format = "{" + str(index) + ":" + str(len(tstamp)) + "}"
            index += 1
            self.format += "{" + str(index) + "}"
            index += 1
        for i, metric in enumerate(self.metrics):
            ins = 1 if self.insts[i][0][0] == PM_IN_NULL else len(self.insts[i][0])
            for _ in range(ins):
                l = str(self.metrics[metric][4])
                #self.format += "{:>" + l + "." + l + "}{}"
                self.format += "{" + str(index) + ":>" + l + "." + l + "}"
                index += 1
                self.format += "{" + str(index) + "}"
                index += 1
        #self.format = self.format[:-2]
        l = len(str(index-1)) + 2
        self.format = self.format[:-l]

    def write_ext_header(self):
        """ Write extended header """
        comm = "#" if self.output == OUTPUT_CSV else ""

        if self.runtime != -1:
            duration = self.runtime
            samples = self.samples
        else:
            if self.samples:
                duration = (self.samples - 1) * int(self.interval)
                samples = self.samples
                if self.context.type == PM_CONTEXT_ARCHIVE:
                    if not self.interpol:
                        samples = str(samples) + " (requested)"
            else:
                duration = int(float(self.opts.pmGetOptionFinish()) - float(self.opts.pmGetOptionStart()))
                samples = (duration / int(self.interval)) + 1
                duration = (samples - 1) * int(self.interval)
                if self.context.type == PM_CONTEXT_ARCHIVE:
                    if not self.interpol:
                        samples = "N/A"
        endtime = float(self.opts.pmGetOptionStart()) + duration

        if self.context.type == PM_CONTEXT_ARCHIVE:
            host = self.context.pmGetArchiveLabel().hostname
            if not self.interpol and not self.opts.pmGetOptionFinish():
                endtime = self.context.pmGetArchiveEnd()
        if self.context.type == PM_CONTEXT_HOST:
            host = self.source
        if self.context.type == PM_CONTEXT_LOCAL:
            host = "localhost, using DSO PMDAs"

        # Figure out the current timezone using the PCP convention
        if self.opts.pmGetOptionTimezone():
            currtz = self.opts.pmGetOptionTimezone()
        else:
            dst = time.localtime().tm_isdst
            offset = time.altzone if dst else time.timezone
            currtz = time.tzname[dst]
            if offset:
                currtz += str(offset/3600)
        timezone = currtz

        if self.context.type == PM_CONTEXT_ARCHIVE:
            if self.context.pmGetArchiveLabel().tz != timezone:
                timezone = self.context.pmGetArchiveLabel().tz
                timezone += " (creation, current is " + currtz + ")"

        print(comm)
        if self.context.type == PM_CONTEXT_ARCHIVE:
            print(comm + "  archive: " + self.source)
        print(comm + "     host: " + host)
        print(comm + " timezone: " + timezone)
        print(comm + "    start: " + time.asctime(time.localtime(self.opts.pmGetOptionStart())))
        print(comm + "      end: " + time.asctime(time.localtime(endtime)))
        print(comm + "  samples: " + str(samples))
        if not (self.context.type == PM_CONTEXT_ARCHIVE and not self.interpol):
            print(comm + " interval: " + str(float(self.interval)) + " sec")
            print(comm + " duration: " + str(duration) + " sec")
        else:
            print(comm + " interval: N/A")
            print(comm + " duration: N/A")
        print(comm)

    def write_header(self):
        """ Write metrics header """
        if self.output == OUTPUT_ARCHIVE:
            sys.stdout.write("Recording archive %s" % self.archive)
            if self.runtime != -1:
                sys.stdout.write(":\n%s samples(s) with %.1f sec interval ~ %d sec duration.\n" % (self.samples, float(self.interval), self.runtime))
            elif self.samples:
                duration = (self.samples - 1) * int(self.interval)
                sys.stdout.write(":\n%s samples(s) with %.1f sec interval ~ %d sec duration.\n" % (self.samples, float(self.interval), duration))
            else:
                if self.context.type != PM_CONTEXT_ARCHIVE:
                    sys.stdout.write("... (Ctrl-C to stop)")
                sys.stdout.write("\n")
            return

        if self.output == OUTPUT_CSV:
            header = "metric" + self.delimiter
            if self.timestamp == 1:
                header += "timestamp" + self.delimiter
            for f in "name", "unit", "value":
                header += f + self.delimiter
            header = header[:-len(self.delimiter)]
            print(header)

        if self.output == OUTPUT_STDOUT:
            names = ["", self.delimiter] # no timestamp on header line
            insts = ["", self.delimiter] # no timestamp on instances line
            units = ["", self.delimiter] # no timestamp on units line
            prnti = 0
            for i, metric in enumerate(self.metrics):
                ins   = 1 if self.insts[i][0][0] == PM_IN_NULL else len(self.insts[i][0])
                prnti = 1 if self.insts[i][0][0] != PM_IN_NULL else 0
                for j in range(ins):
                    names.append(self.metrics[metric][0])
                    names.append(self.delimiter)
                    units.append(self.metrics[metric][2][0])
                    units.append(self.delimiter)
                    if prnti == 1:
                        insts.append(self.insts[i][1][j])
                    else:
                        insts.append(self.delimiter)
                    insts.append(self.delimiter)
            del names[-1]
            del units[-1]
            del insts[-1]
            print(self.format.format(*tuple(names)))
            if prnti == 1:
                print(self.format.format(*tuple(insts)))
            if self.unitinfo:
                print(self.format.format(*tuple(units)))

        if self.output == OUTPUT_ZABBIX:
            if self.context.type == PM_CONTEXT_ARCHIVE:
                self.delay = 0
                self.interpol = 0
                self.zabbix_interval = 250 # See zabbix_sender(8)
                sys.stdout.write("Sending archived metrics to Zabbix server %s...\n(Ctrl-C to stop)\n" % self.zabbix_server)
                return

            sys.stdout.write("Sending metrics to Zabbix server %s every %d sec" % (self.zabbix_server, self.zabbix_interval))
            if self.runtime != -1:
                sys.stdout.write(":\n%s samples(s) with %.1f sec interval ~ %d sec runtime.\n" % (self.samples, float(self.interval), self.runtime))
            elif self.samples:
                duration = (self.samples - 1) * int(self.interval)
                sys.stdout.write(":\n%s samples(s) with %.1f sec interval ~ %d sec runtime.\n" % (self.samples, float(self.interval), duration))
            else:
                sys.stdout.write("...\n(Ctrl-C to stop)\n")

    def write_archive(self, timestamp, values):
        """ Write an archive record """
        if timestamp == None and values == None:
            # Complete and close
            self.log.pmiEnd()
            self.log = None
            return

        if self.log == None:
            # Create a new archive
            self.log = pmi.pmiLogImport(self.archive)
            if self.context.type == PM_CONTEXT_ARCHIVE:
                self.log.pmiSetHostname(self.context.pmGetArchiveLabel().hostname)
                self.log.pmiSetTimezone(self.context.pmGetArchiveLabel().tz)
            for i, metric in enumerate(self.metrics):
                self.log.pmiAddMetric(metric,
                                      self.pmids[i],
                                      self.descs[i].contents.type,
                                      self.descs[i].contents.indom,
                                      self.descs[i].contents.sem,
                                      self.descs[i].contents.units)
                ins = 0 if self.insts[i][0][0] == PM_IN_NULL else len(self.insts[i][0])
                try:
                    for j in range(ins):
                        self.log.pmiAddInstance(self.descs[i].contents.indom, self.insts[i][1][j], self.insts[i][0][j])
                except pmi.pmiErr as error:
                    if error.args[0] == PMI_ERR_DUPINSTNAME:
                        continue

        # Add current values
        data = 0
        for i, metric in enumerate(self.metrics):
            ins = 1 if self.insts[i][0][0] == PM_IN_NULL else len(self.insts[i][0])
            for j in range(ins):
                if str(list(values[i])[j][2]) != NO_VAL:
                    data = 1
                    inst = self.insts[i][1][j]
                    if inst == None: # RHBZ#1285371
                        inst = ""
                    if self.descs[i].contents.type == PM_TYPE_STRING:
                        self.log.pmiPutValue(metric, inst, str(values[i][j][2]))
                    elif self.descs[i].contents.type == PM_TYPE_FLOAT or \
                         self.descs[i].contents.type == PM_TYPE_DOUBLE:
                        self.log.pmiPutValue(metric, inst, "%f" % values[i][j][2])
                    else:
                        self.log.pmiPutValue(metric, inst, "%d" % values[i][j][2])

        # Flush
        if data:
            # pylint: disable=maybe-no-member
            self.log.pmiWrite(self.ctstamp.tv_sec, self.ctstamp.tv_usec)

    def write_csv(self, timestamp, values):
        """ Write results in CSV format """
        if timestamp == None and values == None:
            # Silent goodbye
            return

        # Print the results
        for i, metric in enumerate(self.metrics):
            ins = 1 if self.insts[i][0][0] == PM_IN_NULL else len(self.insts[i][0])
            for j in range(ins):
                line = metric
                if self.insts[i][1][j]:
                    line += "." + str(self.insts[i][1][j])
                line += self.delimiter
                if self.timestamp == 1:
                    line += timestamp + self.delimiter
                line += str(self.metrics[metric][0]) + self.delimiter
                line += str(self.metrics[metric][2][0]) + self.delimiter
                if type(list(values[i])[j][2]) is float:
                    fmt = "." + str(self.precision) + "f"
                    line += format(list(values[i])[j][2], fmt)
                else:
                    line += str(list(values[i])[j][2])
                print(line)

    def write_stdout(self, timestamp, values):
        """ Write a line to stdout """
        if timestamp == None and values == None:
            # Silent goodbye
            return

        #fmt = self.format.split("{}")
        fmt = re.split("{\\d+}", self.format)
        line = []

        if self.timestamp == 0:
            line.append("")
        else:
            line.append(timestamp)
        line.append(self.delimiter)

        k = 0
        for i, metric in enumerate(self.metrics):
            l = self.metrics[metric][4]

            for j in range(len(values[i])):
                k += 1

                # Raw or rate
                if not self.metrics[metric][3] and \
                  (self.prevvals == None or list(self.prevvals[i])[j][2] == NO_VAL):
                    # Rate not yet possible
                    value = NO_VAL
                elif self.metrics[metric][3] or \
                  self.descs[i].sem != PM_SEM_COUNTER or \
                  list(values[i])[j][2] == NO_VAL:
                    # Raw
                    value = list(values[i])[j][2]
                else:
                    # Rate
                    scale = 1
                    if self.descs[i].contents.units.dimTime != 0:
                        if self.descs[i].contents.units.scaleTime > PM_TIME_SEC:
                            scale = pow(60, (PM_TIME_SEC - self.descs[i].contents.units.scaleTime))
                        else:
                            scale = pow(1000, (PM_TIME_SEC - self.descs[i].contents.units.scaleTime))
                    delta = scale * (float(self.ctstamp) - float(self.ptstamp))
                    value = (list(values[i])[j][2] - list(self.prevvals[i])[j][2]) / delta if delta else 0

                # Make sure the value fits
                if type(value) is int or type(value) is long:
                    if len(str(value)) > l:
                        value = TRUNC
                    else:
                        #fmt[k] = "{:" + str(l) + "d}"
                        fmt[k] = "{X:" + str(l) + "d}"

                if type(value) is float:
                    c = self.precision
                    s = len(str(int(value)))
                    if s > l:
                        c = -1
                        value = TRUNC
                    #for _ in reversed(range(c+1)):
                        #t = "{:" + str(l) + "." + str(c) + "f}"
                    for f in reversed(range(c+1)):
                        r = "{X:" + str(l) + "." + str(c) + "f}"
                        t = "{0:" + str(l) + "." + str(c) + "f}"
                        if len(t.format(value)) > l:
                            c -= 1
                        else:
                            #fmt[k] = t
                            fmt[k] = r
                            break

                line.append(value)
                line.append(self.delimiter)

        del line[-1]
        #print('{}'.join(fmt).format(*tuple(line)))
        index = 0
        nfmt = ""
        for f in fmt:
            nfmt += f.replace("{X:", "{" + str(index) + ":")
            index += 1
            nfmt += "{" + str(index) + "}"
            index += 1
        l = len(str(index-1)) + 2
        nfmt = nfmt[:-l]
        print(nfmt.format(*tuple(line)))

    def write_zabbix(self, timestamp, values):
        """ Write (send) metrics to a Zabbix server """
        if timestamp == None and values == None:
            # Send any remaining buffered values
            if self.zabbix_metrics:
                send_to_zabbix(self.zabbix_metrics, self.zabbix_server, self.zabbix_port)
                self.zabbix_metrics = []
            return

        # Collect the results
        ts = float(self.ctstamp)
        if self.zabbix_prevsend == None:
            self.zabbix_prevsend = ts
        for i, metric in enumerate(self.metrics):
            ins = 1 if self.insts[i][0][0] == PM_IN_NULL else len(self.insts[i][0])
            for j in range(ins):
                key = ZBXPRFX + metric
                if self.insts[i][1][j]:
                    key += "[" + str(self.insts[i][1][j]) + "]"
                val = str(list(values[i])[j][2])
                self.zabbix_metrics.append(ZabbixMetric(self.zabbix_host, key, val, ts))

        # Send when needed
        if self.context.type == PM_CONTEXT_ARCHIVE:
            if len(self.zabbix_metrics) >= self.zabbix_interval:
                send_to_zabbix(self.zabbix_metrics, self.zabbix_server, self.zabbix_port)
                self.zabbix_metrics = []
        elif ts - self.zabbix_prevsend > self.zabbix_interval:
            send_to_zabbix(self.zabbix_metrics, self.zabbix_server, self.zabbix_port)
            self.zabbix_metrics = []
            self.zabbix_prevsend = ts

    def connect(self):
        """ Establish a PMAPI context to archive, host or local, via args """
        self.context = pmapi.pmContext.fromOptions(self.opts, sys.argv)

    def finalize(self):
        """ Finalize and clean up """
        if self.log:
            self.log.pmiEnd()
            self.log = None

if __name__ == '__main__':
    try:
        P = PMReporter()
        P.read_config()
        P.prepare_metrics()
        P.connect()
        P.validate_config()
        P.validate_metrics()
        P.execute()
        P.finalize()

    except pmapi.pmErr as error:
        sys.stderr.write('%s: %s\n' % (error.progname(), error.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
    except IOError as error:
        sys.stderr.write("%s\n" % str(error))
    except KeyboardInterrupt:
        sys.stdout.write("\n")
        P.finalize()
