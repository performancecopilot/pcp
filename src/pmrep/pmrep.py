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
# pylint: disable=invalid-name, line-too-long, no-self-use, bad-whitespace
# pylint: disable=too-many-boolean-expressions, too-many-statements
# pylint: disable=too-many-instance-attributes, too-many-locals
# pylint: disable=too-many-branches, too-many-nested-blocks
# pylint: disable=bare-except, broad-except

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
import errno
import time
import math
import csv
import sys
import os
import re

from pcp import pmapi, pmi
from cpmapi import PM_CONTEXT_ARCHIVE, PM_CONTEXT_HOST, PM_CONTEXT_LOCAL, PM_MODE_FORW, PM_MODE_INTERP, PM_ERR_TYPE, PM_ERR_EOL, PM_ERR_NAME, PM_IN_NULL, PM_SEM_COUNTER, PM_TIME_MSEC, PM_TIME_SEC, PM_XTB_SET, PM_DEBUG_APPL1
from cpmapi import PM_TYPE_32, PM_TYPE_U32, PM_TYPE_64, PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE, PM_TYPE_STRING
from cpmi import PMI_ERR_DUPINSTNAME

if sys.version_info[0] >= 3:
    long = int # pylint: disable=redefined-builtin

# Default config
DEFAULT_CONFIG = ["./pmrep.conf", "$HOME/.pmrep.conf", "$HOME/.pcp/pmrep.conf", "$PCP_SYSCONF_DIR/pmrep/pmrep.conf"]

# Defaults
CONFVER = 1
CSVSEP  = ","
CSVTIME = "%Y-%m-%d %H:%M:%S"
OUTSEP  = "  "
OUTTIME = "%H:%M:%S"
ZBXPORT = 10051
ZBXPRFX = "pcp."
NO_VAL  = "N/A"
NO_INST = "~"
TRUNC   = "xxx"

# Output targets
OUTPUT_ARCHIVE = "archive"
OUTPUT_CSV     = "csv"
OUTPUT_STDOUT  = "stdout"
OUTPUT_ZABBIX  = "zabbix"

class ZabbixMetric(object): # pylint: disable=too-few-public-methods
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
            sys.stderr.flush()
            return False
        return True
    except socket.timeout as err:
        sys.stderr.write("Zabbix connection timed out: " + str(err))
        sys.stderr.flush()
        return False
    finally:
        zabbix.close()

class PMReporter(object):
    """ Report PCP metrics """
    def __init__(self):
        """ Construct object, prepare for command line handling """
        self.context = None
        self.opts = self.options()

        # Configuration directives
        self.keys = ('source', 'output', 'derived', 'header', 'globals',
                     'samples', 'interval',
                     'timestamp', 'unitinfo', 'colxrow',
                     'delay', 'type', 'width', 'precision', 'delimiter',
                     'extheader', 'repeat_header', 'timefmt', 'interpol',
                     'count_scale', 'space_scale', 'time_scale', 'version',
                     'zabbix_server', 'zabbix_port', 'zabbix_host', 'zabbix_interval',
                     'speclocal', 'instances', 'ignore_incompat', 'omit_flat')

        # Special command line switches
        self.arghelp = ('-?', '--help', '-V', '--version')

        # The order of preference for options (as present):
        # 1 - command line options
        # 2 - options from configuration file(s)
        # 3 - built-in defaults defined below
        self.check = 0
        self.config = self.set_config_file()
        self.version = CONFVER
        self.source = "local:"
        self.output = OUTPUT_STDOUT
        self.speclocal = None
        self.derived = None
        self.header = 1
        self.unitinfo = 1
        self.globals = 1
        self.timestamp = 0
        self.samples = None # forever
        self.interval = pmapi.timeval(1)      # 1 sec
        self.opts.pmSetOptionInterval(str(1)) # 1 sec
        self.delay = 0
        self.type = 0
        self.ignore_incompat = 0
        self.instances = []
        self.omit_flat = 0
        self.colxrow = None
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

        # Not in pmrep.conf, won't overwrite
        self.outfile = None

        # Internal
        self.format = None # stdout format
        self.writer = None
        self.pmi = None
        self.localtz = None
        self.runtime = -1

        # Performance metrics store
        # key - metric name
        # values - 0:label, 1:instance(s), 2:unit/scale, 3:type, 4:width, 5:pmfg item
        self.metrics = OrderedDict()
        self.pmfg = None
        self.pmfg_ts = None

        # Corresponding config file metric specifiers
        self.metricspec = ('label', 'instances', 'unit', 'type', 'width', 'formula')

        self.pmids = []
        self.descs = []
        self.insts = []

        self.tmp = []

        # Zabbix integration
        self.zabbix_server = None
        self.zabbix_port = ZBXPORT
        self.zabbix_host = None
        self.zabbix_interval = None
        self.zabbix_prevsend = None
        self.zabbix_metrics = []

    def options(self):
        """ Setup default command line argument option handling """
        opts = pmapi.pmOptions()
        opts.pmSetOptionCallback(self.option)
        opts.pmSetOverrideCallback(self.option_override)
        opts.pmSetShortOptions("a:h:LK:c:Co:F:e:D:V?HUGpA:S:T:O:s:t:Z:zdrIi:vX:w:P:l:xE:f:uq:b:y:")
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
        opts.pmSetLongOption("output", 1, "o", "OUTPUT", "output target: archive, csv, stdout (default), or zabbix")
        opts.pmSetLongOption("output-file", 1, "F", "OUTFILE", "output file")
        opts.pmSetLongOption("derived", 1, "e", "FILE|DFNT", "derived metrics definitions")
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
        opts.pmSetLongOption("ignore-incompat", 0, "I", "", "ignore incompatible instances (default: abort)")
        opts.pmSetLongOption("instances", 1, "i", "STR", "instances to report (default: all current)")
        opts.pmSetLongOption("omit-flat", 0, "v", "", "omit single-valued metrics with -i (default: include)")
        opts.pmSetLongOption("colxrow", 1, "X", "STR", "swap stdout columns and rows using header label")
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
        if opt == 'H' or opt == 'p' or opt == 'K':
            return 1
        return 0

    def option(self, opt, optarg, index): # pylint: disable=unused-argument
        """ Perform setup for an individual command line option """
        if opt == 'K':
            if not self.speclocal or not self.speclocal.startswith("K:"):
                self.speclocal = "K:" + optarg
            else:
                self.speclocal = self.speclocal + "|" + optarg
        elif opt == 'c':
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
            if os.path.exists(optarg):
                sys.stderr.write("File %s already exists.\n" % optarg)
                sys.exit(1)
            self.outfile = optarg
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
        elif opt == 'I':
            self.ignore_incompat = 1
        elif opt == 'i':
            self.instances = self.instances + self.parse_instances(optarg)
        elif opt == 'v':
            self.omit_flat = 1
        elif opt == 'X':
            self.colxrow = optarg
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

    def set_config_file(self):
        """ Set configuration file """
        config = None
        for conf in DEFAULT_CONFIG:
            conf = conf.replace("$HOME", os.getenv("HOME"))
            conf = conf.replace("$PCP_SYSCONF_DIR", pmapi.pmContext.pmGetConfig("PCP_SYSCONF_DIR"))
            if os.path.isfile(conf) and os.access(conf, os.R_OK):
                config = conf
                break

        # Possibly override the built-in default config file before
        # parsing the rest of the command line options
        args = iter(sys.argv[1:])
        for arg in args:
            if arg in self.arghelp:
                return None
            if arg == '-c' or arg == '--config' or arg.startswith("-c"):
                try:
                    if arg == '-c' or arg == '--config':
                        config = next(args)
                    else:
                        config = arg.replace("-c", "", 1)
                    if not os.path.isfile(config) or not os.access(config, os.R_OK):
                        raise IOError("Failed to read configuration file '%s'." % config)
                except StopIteration:
                    break
        return config

    def parse_instances(self, instances):
        """ Parse user-supplied instances string """
        insts = []
        reader = csv.reader([instances])
        for _, inst in enumerate(list(reader)[0]):
            if inst.startswith('"') or inst.startswith("'"):
                inst = inst[1:]
            if inst.endswith('"') or inst.endswith("'"):
                inst = inst[:-1]
            insts.append(inst)
        return insts

    def set_attr(self, name, value):
        """ Helper to apply config file settings properly """
        if value in ('true', 'True', 'y', 'yes', 'Yes'):
            value = 1
        if value in ('false', 'False', 'n', 'no', 'No'):
            value = 0
        if name == 'source':
            self.source = value
        if name == 'speclocal':
            if not self.speclocal or not self.speclocal.startswith("K:"):
                self.speclocal = value
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
        elif name == 'instances':
            self.instances = value.split(",") # pylint: disable=no-member
        else:
            try:
                setattr(self, name, int(value))
            except ValueError:
                setattr(self, name, value)

    def read_config(self):
        """ Read options from configuration file """
        config = ConfigParser.SafeConfigParser()
        if self.config:
            config.read(self.config)
        if not config.has_section('options'):
            return
        for opt in config.options('options'):
            if opt in self.keys:
                self.set_attr(opt, config.get('options', opt))
            else:
                sys.stderr.write("Invalid directive '%s' in %s.\n" % (opt, self.config))
                sys.exit(1)

    def read_cmd_line(self):
        """ Read command line options """
        pmapi.c_api.pmSetOptionFlags(pmapi.c_api.PM_OPTFLAG_DONE)  # Do later
        if pmapi.c_api.pmGetOptionsFromList(sys.argv):
            raise pmapi.pmUsageErr()

    def parse_metric_spec_instances(self, spec):
        """ Parse instances from metric spec """
        insts = [None]
        if spec.count(",") < 2:
            return spec + ",,", insts
        # User may supply quoted or unquoted instance specification
        # Conf file preservers outer quotes, command line does not
        # We need to detect which is the case here. What a mess.
        quoted = 0
        s = spec.split(",")[2]
        if s and (s[1] == "'" or s[1] == '"'):
            quoted = 1
        if spec.count('"') or spec.count("'"):
            inststr = spec.partition(",")[2].partition(",")[2]
            q = inststr[0]
            inststr = inststr[:inststr.rfind(q)+1]
            if quoted:
                insts = self.parse_instances(inststr[1:-1])
            else:
                insts = self.parse_instances(inststr)
            spec = spec.replace(inststr, "")
        else:
            insts = [s]
        if spec.count(",") < 2:
            spec += ",,"
        return spec, insts

    def parse_metric_info(self, metrics, key, value):
        """ Parse metric information """
        # NB. Uses the config key, not the metric, as the dict key
        compact = False
        if ',' in value or ('.' in key and key.rsplit(".")[1] not in self.metricspec):
            compact = True
        # NB. Formulas may now contain commas, see pmRegisterDerived(3)
        if ',' in value and ('.' in key and key.rsplit(".")[1] == "formula"):
            compact = False
        if compact:
            # Compact / one-line definition
            spec, insts = self.parse_metric_spec_instances(key + "," + value)
            metrics[key] = spec.split(",")
            metrics[key][2] = insts
        else:
            # Verbose / multi-line definition
            if not '.' in key or key.rsplit(".")[1] not in self.metricspec:
                # New metric
                metrics[key] = [value]
                for index in range(0, 6):
                    if len(metrics[key]) <= index:
                        metrics[key].append(None)
            else:
                # Additional info
                key, spec = key.rsplit(".")
                if key not in metrics:
                    sys.stderr.write("Undeclared metric key %s.\n" % key)
                    sys.exit(1)
                if spec == "formula":
                    if self.derived is None:
                        self.derived = metrics[key][0] + "=" + value
                    else:
                        self.derived += "@" + metrics[key][0] + "=" + value
                else:
                    metrics[key][self.metricspec.index(spec)+1] = value

    def prepare_metrics(self):
        """ Construct and prepare the initial metrics set """
        metrics = self.opts.pmGetOperands()
        if not metrics:
            sys.stderr.write("No metrics specified.\n")
            raise pmapi.pmUsageErr()

        # Read config
        config = ConfigParser.SafeConfigParser()
        if self.config:
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
                spec, insts = self.parse_metric_spec_instances(metric)
                m = spec.split(",")
                m[2] = insts
                tempmet[m[0]] = m[1:]

        # Get config and set details for configuration file metric sets
        confmet = OrderedDict()
        for spec in tempmet:
            if tempmet[spec] is None:
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
            if isinstance(tempmet[metric], list):
                self.metrics[metric] = tempmet[metric]
            else:
                for m in tempmet[metric]:
                    self.metrics[m] = confmet[m]

    def connect(self):
        """ Establish a PMAPI context """
        context = None

        if pmapi.c_api.pmGetOptionArchives():
            context = pmapi.c_api.PM_CONTEXT_ARCHIVE
            self.opts.pmSetOptionContext(pmapi.c_api.PM_CONTEXT_ARCHIVE)
            self.source = self.opts.pmGetOptionArchives()[0]
        elif pmapi.c_api.pmGetOptionHosts():
            context = pmapi.c_api.PM_CONTEXT_HOST
            self.opts.pmSetOptionContext(pmapi.c_api.PM_CONTEXT_HOST)
            self.source = self.opts.pmGetOptionHosts()[0]
        elif pmapi.c_api.pmGetOptionLocalPMDA():
            context = pmapi.c_api.PM_CONTEXT_LOCAL
            self.opts.pmSetOptionContext(pmapi.c_api.PM_CONTEXT_LOCAL)
            self.source = None

        if not context:
            if '/' in self.source:
                context = pmapi.c_api.PM_CONTEXT_ARCHIVE
                self.opts.pmSetOptionArchive(self.source)
                self.opts.pmSetOptionContext(pmapi.c_api.PM_CONTEXT_ARCHIVE)
            elif self.source != '@':
                context = pmapi.c_api.PM_CONTEXT_HOST
                self.opts.pmSetOptionHost(self.source)
                self.opts.pmSetOptionContext(pmapi.c_api.PM_CONTEXT_HOST)
            else:
                context = pmapi.c_api.PM_CONTEXT_LOCAL
                self.opts.pmSetOptionLocalPMDA()
                self.opts.pmSetOptionContext(pmapi.c_api.PM_CONTEXT_LOCAL)
                self.source = None

        if context == pmapi.c_api.PM_CONTEXT_LOCAL and self.speclocal:
            self.speclocal = self.speclocal.replace("K:", "")
            for spec in self.speclocal.split("|"):
                self.opts.pmSetOptionSpecLocal(spec)

        # All options set, finalize configuration
        flags = self.opts.pmGetOptionFlags()
        self.opts.pmSetOptionFlags(flags | pmapi.c_api.PM_OPTFLAG_DONE)
        pmapi.c_api.pmEndOptions()

        self.pmfg = pmapi.fetchgroup(context, self.source)
        self.pmfg_ts = self.pmfg.extend_timestamp()
        self.context = self.pmfg.get_context()

        if pmapi.c_api.pmSetContextOptions(self.context.ctx, self.opts.mode, self.opts.delta):
            raise pmapi.pmUsageErr()

    def validate_config(self):
        """ Validate configuration options """
        if self.version != CONFVER:
            sys.stderr.write("Incompatible configuration file version (read v%s, need v%d).\n" % (self.version, CONFVER))
            sys.exit(1)

        if self.output == OUTPUT_ARCHIVE and not self.outfile:
            sys.stderr.write("Archive must be defined with archive output.\n")
            sys.exit(1)

        if self.output == OUTPUT_ZABBIX and (not self.zabbix_server or \
           not self.zabbix_port or not self.zabbix_host):
            sys.stderr.write("zabbix_server, zabbix_port, and zabbix_host must be defined with Zabbix.\n")
            sys.exit(1)

        # Runtime overrides samples/interval
        if self.opts.pmGetOptionFinishOptarg():
            self.runtime = float(self.opts.pmGetOptionFinish()) - float(self.opts.pmGetOptionOrigin())
            if self.opts.pmGetOptionSamples():
                self.samples = self.opts.pmGetOptionSamples()
                if self.samples < 2:
                    self.samples = 2
                self.interval = float(self.runtime) / (self.samples - 1)
                self.opts.pmSetOptionInterval(str(self.interval))
                self.interval = self.opts.pmGetOptionInterval()
            else:
                self.interval = self.opts.pmGetOptionInterval()
                if not self.interval:
                    self.interval = pmapi.timeval(0)
                try:
                    self.samples = int(self.runtime / float(self.interval) + 1)
                except:
                    pass
        else:
            self.samples = self.opts.pmGetOptionSamples()
            self.interval = self.opts.pmGetOptionInterval()

        if float(self.interval) <= 0:
            sys.stderr.write("Interval must be greater than zero.\n")
            sys.exit(1)

        if self.output == OUTPUT_ZABBIX:
            if self.zabbix_interval:
                self.zabbix_interval = float(pmapi.timeval.fromInterval(self.zabbix_interval))
                if self.zabbix_interval < float(self.interval):
                    self.zabbix_interval = float(self.interval)
            else:
                self.zabbix_interval = float(self.interval)

    def check_metric(self, metric):
        """ Validate individual metric and get its details """
        try:
            pmid = self.context.pmLookupName(metric)[0]
            if pmid in self.pmids:
                # Always ignore duplicates
                return
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
            instances = self.instances if not self.tmp[0] else self.tmp
            if self.omit_flat and instances and not inst[1][0]:
                return
            if instances and inst[1][0]:
                found = tuple([[], []])
                for r in instances:
                    cr = re.compile(r'\A' + r + r'\Z')
                    for i, s in enumerate(inst[1]):
                        if re.match(cr, s):
                            found[0].append(inst[0][i])
                            found[1].append(inst[1][i])
                if not found[0]:
                    return
                inst = found
            self.pmids.append(pmid)
            self.descs.append(desc)
            self.insts.append(inst)
        except pmapi.pmErr as error:
            if self.ignore_incompat:
                return
            sys.stderr.write("Invalid metric %s (%s).\n" % (metric, str(error)))
            sys.exit(1)

    def format_metric_label(self, label):
        """ Format a metric label """
        # See src/libpcp/src/units.c
        if ' / ' in label:
            label = label.replace("nanosec", "ns").replace("microsec", "us")
            label = label.replace("millisec", "ms").replace("sec", "s")
            label = label.replace("min", "min").replace("hour", "h")
            label = label.replace(" / ", "/")
        return label

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
                for definition in self.derived.split("@"):
                    err = ""
                    try:
                        name, expr = definition.split("=", 1)
                        self.context.pmLookupName(name.strip())
                    except pmapi.pmErr as error:
                        if error.args[0] != PM_ERR_NAME:
                            err = error.message()
                        else:
                            try:
                                self.context.pmRegisterDerived(name.strip(), expr.strip())
                                continue
                            except pmapi.pmErr as error:
                                err = error.message()
                    except ValueError as error:
                        err = "Invalid syntax (expected metric=expression)"
                    except Exception as error:
                        err = "Unidentified error"
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
                self.tmp = metrics[metric][1]
                if not 'append' in dir(self.tmp):
                    self.tmp = [self.tmp]
                self.context.pmTraversePMNS(metric, self.check_metric)
                if len(self.pmids) == l:
                    # No compatible metrics found
                    next # pylint: disable=pointless-statement
                elif len(self.pmids) == l + 1:
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

        # Exit if no metrics with specified instances found
        if not self.insts:
            sys.stderr.write("No matching instances found.\n")
            # Try to help the user to get the instance specifications right
            if self.instances:
                print("\nRequested global instances:")
                print(self.instances)
            sys.exit(1)

        # Finalize the metrics set
        incompat_metrics = {}
        for i, metric in enumerate(self.metrics):
            # Fill in all fields for easier checking later
            for index in range(0, 6):
                if len(self.metrics[metric]) <= index:
                    self.metrics[metric].append(None)

            # Label
            if not self.metrics[metric][0]:
                # mem.util.free -> m.u.free
                name = ""
                for m in metric.split("."):
                    name += m[0] + "."
                self.metrics[metric][0] = name[:-2] + m # pylint: disable=undefined-loop-variable

            # Rawness
            if self.metrics[metric][3] == 'raw' or self.type == 1 or \
               self.output == OUTPUT_ARCHIVE or \
               self.output == OUTPUT_CSV or \
               self.output == OUTPUT_ZABBIX:
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
            mtype = None
            try:
                if self.metrics[metric][3] == 0 and \
                   self.descs[i].contents.type != PM_TYPE_STRING:
                    (unitstr, mult) = self.context.pmParseUnitsStr(self.metrics[metric][2])
                    label = self.metrics[metric][2]
                    if self.descs[i].sem == PM_SEM_COUNTER:
                        mtype = PM_TYPE_DOUBLE
                        if '/' not in label:
                            label += " / s"
                    label = self.format_metric_label(label)
                    self.metrics[metric][2] = (label, unitstr, mult)
                else:
                    label = self.format_metric_label(unitstr)
                    self.metrics[metric][2] = (label, unitstr, 1)
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

            # Add fetchgroup item
            try:
                scale = self.metrics[metric][2][0]
                self.metrics[metric][5] = self.pmfg.extend_indom(metric, mtype, scale, 1024)
            except:
                if self.ignore_incompat:
                    # Schedule the metric for removal
                    incompat_metrics[metric] = i
                else:
                    raise

        # Remove all traces of incompatible metrics
        for metric in incompat_metrics:
            del self.pmids[incompat_metrics[metric]]
            del self.descs[incompat_metrics[metric]]
            del self.insts[incompat_metrics[metric]]
            del self.metrics[metric]
        incompat_metrics = {}

        # Verify that we have valid metrics
        if not self.metrics:
            sys.stderr.write("No compatible metrics found.\n")
            sys.exit(1)

    def get_local_tz(self, set_dst=-1):
        """ Figure out the local timezone using the PCP convention """
        dst = time.localtime().tm_isdst
        if set_dst >= 0:
            dst = 1 if set_dst else 0
        offset = time.altzone if dst else time.timezone
        currtz = time.tzname[dst]
        if offset:
            offset = offset/3600
            offset = int(offset) if offset == int(offset) else offset
            if offset >= 0:
                offset = "+" + str(offset)
            currtz += str(offset)
        return currtz

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
                step = self.interval.tv_sec * 1000 + self.interval.tv_usec / 1000
                mode |= PM_XTB_SET(PM_TIME_MSEC)
        return (mode, int(step))

    def execute(self):
        """ Fetch and report """
        # Debug
        if self.context.pmDebug(PM_DEBUG_APPL1):
            sys.stdout.write("Known config file keywords: " + str(self.keys) + "\n")
            sys.stdout.write("Known metric spec keywords: " + str(self.metricspec) + "\n")

        # Set output primitives
        if self.delimiter is None:
            if self.output == OUTPUT_CSV:
                self.delimiter = CSVSEP
            else:
                self.delimiter = OUTSEP

        # Time
        self.localtz = self.get_local_tz()
        if self.opts.pmGetOptionHostZone():
            os.environ['TZ'] = self.context.pmWhichZone()
            time.tzset()
        else:
            tz = self.localtz
            if self.context.type == PM_CONTEXT_ARCHIVE:
                # Determine correct local TZ based on DST of the archive
                tz = self.get_local_tz(time.localtime(self.opts.pmGetOptionOrigin()).tm_isdst)
            os.environ['TZ'] = tz
            time.tzset()
            self.context.pmNewZone(tz)
        if self.opts.pmGetOptionTimezone():
            os.environ['TZ'] = self.opts.pmGetOptionTimezone()
            time.tzset()
            self.context.pmNewZone(self.opts.pmGetOptionTimezone())

        if self.timefmt is None:
            if self.output == OUTPUT_CSV:
                self.timefmt = CSVTIME
            else:
                self.timefmt = OUTTIME
        if not self.timefmt:
            self.timestamp = 0

        # Set delay mode, interpolation
        if self.context.type != PM_CONTEXT_ARCHIVE:
            self.delay = 1
            self.interpol = 1

        # Print preparation
        self.prepare_writer()
        if self.output == OUTPUT_STDOUT:
            self.prepare_stdout()

        # Headers
        if self.extheader == 1:
            self.extheader = 0
            self.write_ext_header()

        if self.header == 1:
            self.header = 0
            self.write_header()
        else:
            self.repeat_header = 0

        # Archive fetching mode
        if self.context.type == PM_CONTEXT_ARCHIVE:
            (mode, step) = self.get_mode_step()
            self.context.pmSetMode(mode, self.opts.pmGetOptionOrigin(), step)

        # Just checking
        if self.check == 1:
            return

        # Main loop
        lines = 0
        while self.samples != 0:
            # Repeat the header if needed
            if self.output == OUTPUT_STDOUT:
                if lines > 1 and self.repeat_header == lines:
                    self.write_header()
                    lines = 0
                lines += 1

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

        if self.output == OUTPUT_ARCHIVE:
            self.write_archive(tstamp)
        if self.output == OUTPUT_CSV:
            self.write_csv(tstamp)
        if self.output == OUTPUT_STDOUT:
            self.write_stdout(tstamp)
        if self.output == OUTPUT_ZABBIX:
            self.write_zabbix(tstamp)

    def prepare_writer(self):
        """ Prepare generic stdout writer """
        if not self.writer:
            if self.output == OUTPUT_ARCHIVE or \
               self.output == OUTPUT_ZABBIX or \
               self.outfile is None:
                self.writer = sys.stdout
            else:
                self.writer = open(self.outfile, 'wt')
        return self.writer

    def prepare_stdout(self):
        """ Prepare stdout output """
        if self.colxrow is None:
            self.prepare_stdout_std()
        else:
            self.prepare_stdout_colxrow()

    def prepare_stdout_std(self):
        """ Prepare standard formatted stdout output """
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
            for _ in range(len(self.insts[i][0])):
                l = str(self.metrics[metric][4])
                #self.format += "{:>" + l + "." + l + "}{}"
                self.format += "{" + str(index) + ":>" + l + "." + l + "}"
                index += 1
                self.format += "{" + str(index) + "}"
                index += 1
        #self.format = self.format[:-2]
        l = len(str(index-1)) + 2
        self.format = self.format[:-l]

    def prepare_stdout_colxrow(self):
        """ Prepare columns and rows swapped stdout output """
        index = 0

        # Timestamp
        if self.timestamp == 0:
            self.format = "{0:}{1}"
            index += 2
        else:
            tstamp = datetime.fromtimestamp(time.time()).strftime(self.timefmt)
            self.format = "{0:<" + str(len(tstamp)) + "." + str(len(tstamp)) + "}{1}"
            index += 2

        # Instance name
        if self.colxrow:
            self.format += "{2:<" + str(len(self.colxrow)) + "." + str(len(self.colxrow)) + "}{3}"
        else:
            self.format += "{2:<" + str(8) + "." + str(8) + "}{3}"
        index += 2

        # Metrics
        for _, metric in enumerate(self.metrics):
            l = str(self.metrics[metric][4])
            # Value truncated and aligned
            self.format += "{" + str(index) + ":>" + l + "." + l + "}"
            index += 1
            # Dummy
            self.format += "{" + str(index) + "}"
            index += 1

        # Drop the last dummy
        l = len(str(index-1)) + 2
        self.format = self.format[:-l]

    def write_ext_header(self):
        """ Write extended header """
        comm = "#" if self.output == OUTPUT_CSV else ""

        if self.context.type == PM_CONTEXT_ARCHIVE:
            host = self.context.pmGetArchiveLabel().get_hostname()
        if self.context.type == PM_CONTEXT_HOST:
            host = self.context.pmGetContextHostName()
        if self.context.type == PM_CONTEXT_LOCAL:
            host = "localhost, using DSO PMDAs"

        timezone = self.get_local_tz()
        if timezone != self.localtz:
            timezone += " (reporting, current is " + self.localtz + ")"

        if self.runtime != -1:
            duration = self.runtime
            samples = self.samples
        else:
            if self.samples:
                duration = (self.samples - 1) * float(self.interval)
                samples = self.samples
            else:
                duration = float(self.opts.pmGetOptionFinish()) - float(self.opts.pmGetOptionOrigin())
                samples = int(duration / float(self.interval) + 1)
                duration = (samples - 1) * float(self.interval)
        endtime = float(self.opts.pmGetOptionOrigin()) + duration
        duration = int(duration) if duration == int(duration) else "{0:.3f}".format(duration)

        if self.context.type == PM_CONTEXT_ARCHIVE:
            endtime = float(self.context.pmGetArchiveEnd())
            if not self.interpol and self.opts.pmGetOptionSamples():
                samples = str(samples) + " (requested)"
            elif not self.interpol:
                samples = "N/A" # pylint: disable=redefined-variable-type

        self.writer.write(comm + "\n")
        if self.context.type == PM_CONTEXT_ARCHIVE:
            self.writer.write(comm + "  archive: " + self.source + "\n")
        self.writer.write(comm + "     host: " + host + "\n")
        self.writer.write(comm + " timezone: " + timezone + "\n")
        self.writer.write(comm + "    start: " + time.asctime(time.localtime(self.opts.pmGetOptionOrigin())) + "\n")
        self.writer.write(comm + "      end: " + time.asctime(time.localtime(endtime)) + "\n")
        self.writer.write(comm + "  metrics: " + str(len(self.metrics)) + "\n")
        self.writer.write(comm + "  samples: " + str(samples) + "\n")
        if not (self.context.type == PM_CONTEXT_ARCHIVE and not self.interpol):
            self.writer.write(comm + " interval: " + str(float(self.interval)) + " sec\n")
            self.writer.write(comm + " duration: " + str(duration) + " sec\n")
        else:
            self.writer.write(comm + " interval: N/A\n")
            self.writer.write(comm + " duration: N/A\n")
        self.writer.write(comm + "\n")

    def write_header(self):
        """ Write info header """
        if self.output == OUTPUT_ARCHIVE:
            self.writer.write("Recording %d metrics to %s" % (len(self.metrics), self.outfile))
            if self.runtime != -1:
                self.writer.write(":\n%s samples(s) with %.1f sec interval ~ %d sec duration.\n" % (self.samples, float(self.interval), self.runtime))
            elif self.samples:
                duration = (self.samples - 1) * float(self.interval)
                self.writer.write(":\n%s samples(s) with %.1f sec interval ~ %d sec duration.\n" % (self.samples, float(self.interval), duration))
            else:
                self.writer.write("...")
                if self.context.type != PM_CONTEXT_ARCHIVE:
                    self.writer.write(" (Ctrl-C to stop)")
                self.writer.write("\n")
            return

        if self.output == OUTPUT_CSV:
            self.writer.write("Time")
            for i, metric in enumerate(self.metrics):
                for j in range(len(self.insts[i][0])):
                    if self.insts[i][0][0] != PM_IN_NULL and self.insts[i][1][j]:
                        name = metric + "-" + self.insts[i][1][j]
                    else:
                        name = metric
                    name = name.replace(self.delimiter, " ").replace("\n", " ").replace("\"", " ")
                    self.writer.write(self.delimiter + "\"" + name + "\"")
            self.writer.write("\n")

        if self.output == OUTPUT_STDOUT:
            names = ["", self.delimiter] # no timestamp on header line
            insts = ["", self.delimiter] # no timestamp on instances line
            units = ["", self.delimiter] # no timestamp on units line
            if self.colxrow is not None:
                names += [self.colxrow, self.delimiter]
                units += ["", self.delimiter]
            prnti = 0
            for i, metric in enumerate(self.metrics):
                if self.colxrow is not None:
                    names.append(self.metrics[metric][0])
                    names.append(self.delimiter)
                    units.append(self.metrics[metric][2][0])
                    units.append(self.delimiter)
                    continue
                prnti = 1 if self.insts[i][0][0] != PM_IN_NULL else prnti
                for j in range(len(self.insts[i][0])):
                    names.append(self.metrics[metric][0])
                    names.append(self.delimiter)
                    units.append(self.metrics[metric][2][0])
                    units.append(self.delimiter)
                    if prnti == 1 and self.insts[i][1][j]:
                        insts.append(self.insts[i][1][j])
                    else:
                        insts.append(self.delimiter)
                    insts.append(self.delimiter)
            del names[-1]
            del units[-1]
            del insts[-1]
            self.writer.write(self.format.format(*tuple(names)) + "\n")
            if prnti == 1:
                self.writer.write(self.format.format(*tuple(insts)) + "\n")
            if self.unitinfo:
                self.writer.write(self.format.format(*tuple(units)) + "\n")

        if self.output == OUTPUT_ZABBIX:
            if self.context.type == PM_CONTEXT_ARCHIVE:
                self.delay = 0
                self.interpol = 0
                self.zabbix_interval = 250 # See zabbix_sender(8)
                self.writer.write("Sending %d archived metrics to Zabbix server %s...\n(Ctrl-C to stop)\n" % (len(self.metrics), self.zabbix_server))
                return

            self.writer.write("Sending %d metrics to Zabbix server %s every %d sec" % (len(self.metrics), self.zabbix_server, self.zabbix_interval))
            if self.runtime != -1:
                self.writer.write(":\n%s samples(s) with %.1f sec interval ~ %d sec runtime.\n" % (self.samples, float(self.interval), self.runtime))
            elif self.samples:
                duration = (self.samples - 1) * float(self.interval)
                self.writer.write(":\n%s samples(s) with %.1f sec interval ~ %d sec runtime.\n" % (self.samples, float(self.interval), duration))
            else:
                self.writer.write("...\n(Ctrl-C to stop)\n")

    def write_archive(self, timestamp):
        """ Write an archive record """
        if timestamp is None:
            # Complete and close
            self.pmi.pmiEnd()
            self.pmi = None
            return

        if self.pmi is None:
            # Create a new archive
            self.pmi = pmi.pmiLogImport(self.outfile)
            self.recorded = OrderedDict() # pylint: disable=attribute-defined-outside-init
            if self.context.type == PM_CONTEXT_ARCHIVE:
                self.pmi.pmiSetHostname(self.context.pmGetArchiveLabel().hostname)
                self.pmi.pmiSetTimezone(self.context.pmGetArchiveLabel().tz)
            for i, metric in enumerate(self.metrics):
                self.pmi.pmiAddMetric(metric,
                                      self.pmids[i],
                                      self.descs[i].contents.type,
                                      self.descs[i].contents.indom,
                                      self.descs[i].contents.sem,
                                      self.descs[i].contents.units)
                ins = 0 if self.insts[i][0][0] == PM_IN_NULL else len(self.insts[i][0])
                for j in range(ins):
                    if metric not in self.recorded:
                        self.recorded[metric] = []
                    self.recorded[metric].append(self.insts[i][0][j])
                    try:
                        self.pmi.pmiAddInstance(self.descs[i].contents.indom, self.insts[i][1][j], self.insts[i][0][j])
                    except pmi.pmiErr as error:
                        if error.args[0] == PMI_ERR_DUPINSTNAME:
                            continue

        # Add current values
        data = 0
        for i, metric in enumerate(self.metrics):
            try:
                for inst, name, val in self.metrics[metric][5]():
                    try:
                        value = val()
                        if inst != PM_IN_NULL and inst not in self.recorded[metric]:
                            self.recorded[metric].append(inst)
                            try:
                                self.pmi.pmiAddInstance(self.descs[i].contents.indom, name, inst)
                            except pmi.pmiErr as error:
                                if error.args[0] == PMI_ERR_DUPINSTNAME:
                                    pass
                        if self.descs[i].contents.type == PM_TYPE_STRING:
                            self.pmi.pmiPutValue(metric, name, value)
                        elif self.descs[i].contents.type == PM_TYPE_FLOAT or \
                             self.descs[i].contents.type == PM_TYPE_DOUBLE:
                            self.pmi.pmiPutValue(metric, name, "%f" % value)
                        else:
                            self.pmi.pmiPutValue(metric, name, "%d" % value)
                        data = 1
                    except:
                        pass
            except:
                pass

        # Flush
        if data:
            self.pmi.pmiWrite(int(self.pmfg_ts().strftime('%s')), self.pmfg_ts().microsecond)

    def write_csv(self, timestamp):
        """ Write results in CSV format """
        if timestamp is None:
            # Silent goodbye
            return

        # Print the results
        line = timestamp
        for i, metric in enumerate(self.metrics):
            for j in range(len(self.insts[i][0])):
                line += self.delimiter
                found = 0
                try:
                    for inst, name, val in self.metrics[metric][5](): # pylint: disable=unused-variable
                        if inst == PM_IN_NULL or inst == self.insts[i][0][j]:
                            found = 1
                            break
                    if not found:
                        continue
                except:
                    continue

                try:
                    value = val() # pylint: disable=undefined-loop-variable
                except:
                    value = NO_VAL
                if isinstance(value, float):
                    fmt = "." + str(self.precision) + "f"
                    line += format(value, fmt)
                elif isinstance(value, (int, long)):
                    line += str(value)
                else:
                    if value == NO_VAL:
                        line += '""'
                    else:
                        if value:
                            value = value.replace(self.delimiter, " ").replace("\n", " ").replace("\"", " ")
                            line += str("\"" + value + "\"")
        self.writer.write(line + "\n")

    def write_stdout(self, timestamp):
        """ Write a line to stdout """
        if self.colxrow is None:
            self.write_stdout_std(timestamp)
        else:
            self.write_stdout_colxrow(timestamp)

    def write_stdout_std(self, timestamp):
        """ Write a line to standard formatted stdout """
        if timestamp is None:
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

            for j in range(len(self.insts[i][0])):
                k += 1

                found = 0
                try:
                    for inst, name, val in self.metrics[metric][5](): # pylint: disable=unused-variable
                        if inst == PM_IN_NULL or inst == self.insts[i][0][j]:
                            found = 1
                            break
                except:
                    pass
                if not found:
                    value = NO_VAL
                else:
                    try:
                        value = val() # pylint: disable=undefined-loop-variable
                        if isinstance(value, list):
                            value = value[0]
                    except:
                        value = NO_VAL

                # Make sure the value fits
                if isinstance(value, (int, long)):
                    if len(str(value)) > l:
                        value = TRUNC
                    else:
                        #fmt[k] = "{:" + str(l) + "d}"
                        fmt[k] = "{X:" + str(l) + "d}"

                if isinstance(value, float) and not math.isinf(value):
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
        #self.writer.write('{}'.join(fmt).format(*tuple(line)) + "\n")
        index = 0
        nfmt = ""
        for f in fmt:
            if isinstance(line[index], float) and math.isinf(line[index]):
                line[index] = "inf"
            nfmt += f.replace("{X:", "{" + str(index) + ":")
            index += 1
            nfmt += "{" + str(index) + "}"
            index += 1
        l = len(str(index-1)) + 2
        nfmt = nfmt[:-l]
        self.writer.write(nfmt.format(*tuple(line)) + "\n")

    def write_stdout_colxrow(self, timestamp):
        """ Write a line to columns and rows swapped stdout """
        if timestamp is None:
            # Silent goodbye
            return

        # Collect the instances in play
        insts = []
        for i in range(len(self.metrics)):
            for instance in self.insts[i][1]:
                if instance not in insts:
                    insts.append(instance)

        # Avoid crossing the C/Python boundary more than once per metric
        res = OrderedDict()
        for _, metric in enumerate(self.metrics):
            res[metric] = []
            try:
                for inst, name, val in self.metrics[metric][5]():
                    try:
                        res[metric].append([inst, name, val()])
                    except:
                        res[metric].append([inst, name, NO_VAL])
                if not res[metric]:
                    res[metric].append(['', '', NO_VAL])
            except:
                res[metric].append(['', '', NO_VAL])

        # Avoid per-line I/O
        output = ""

        # Painfully iterate over what we have, the logic below
        # being that we need to construct each line independently
        for instance in insts:
            # Split on dummies
            fmt = re.split("{\\d+}", self.format)

            # Start a new line
            k = 0
            line = []

            # Add timestamp as wanted
            if self.timestamp == 0:
                line.append("")
            else:
                line.append(timestamp)
            line.append(self.delimiter)
            k += 1

            # Add instance (may be None for singular indoms)
            line.append(str(instance))
            line.append(self.delimiter)
            k += 1

            # Look for this instance from each metric
            for metric in self.metrics:
                l = self.metrics[metric][4]

                found = 0
                value = NO_VAL
                for inst in res[metric]:
                    if inst[1] == instance:
                        # This metric has the instance we're
                        # processing, grab it and format below
                        value = inst[2]
                        found = 1
                        break

                if not found:
                    # Not an instance this metric has,
                    # add a placeholder and move on
                    line.append(NO_INST)
                    line.append(self.delimiter)
                    k += 1
                    continue

                # Make sure the value fits
                if isinstance(value, (int, long)):
                    if len(str(value)) > l:
                        value = TRUNC
                    else:
                        fmt[k] = "{X:" + str(l) + "d}"

                if isinstance(value, float) and not math.isinf(value):
                    c = self.precision
                    s = len(str(int(value)))
                    if s > l:
                        c = -1
                        value = TRUNC
                    for f in reversed(range(c+1)):
                        r = "{X:" + str(l) + "." + str(c) + "f}"
                        t = "{0:" + str(l) + "." + str(c) + "f}"
                        if len(t.format(value)) > l:
                            c -= 1
                        else:
                            fmt[k] = r
                            break

                # Finally add the value
                line.append(value)
                line.append(self.delimiter)
                k += 1

            # Print the line in a Python 2.6 compatible manner
            del line[-1]
            index = 0
            nfmt = ""
            for f in fmt:
                if isinstance(line[index], float) and math.isinf(line[index]):
                    line[index] = "inf"
                nfmt += f.replace("{X:", "{" + str(index) + ":")
                index += 1
                nfmt += "{" + str(index) + "}"
                index += 1
            l = len(str(index-1)) + 2
            nfmt = nfmt[:-l]
            output += nfmt.format(*tuple(line)) + "\n"

        self.writer.write(output)

    def write_zabbix(self, timestamp):
        """ Write (send) metrics to a Zabbix server """
        if timestamp is None:
            # Send any remaining buffered values
            if self.zabbix_metrics:
                send_to_zabbix(self.zabbix_metrics, self.zabbix_server, self.zabbix_port)
                self.zabbix_metrics = []
            return

        # Collect the results
        td = self.pmfg_ts() - datetime.fromtimestamp(0)
        ts = (td.microseconds + (td.seconds + td.days * 24.0 * 3600.0) * 10.0**6) / 10.0**6
        if self.zabbix_prevsend is None:
            self.zabbix_prevsend = ts
        for _, metric in enumerate(self.metrics):
            try:
                for inst, name, val in self.metrics[metric][5](): # pylint: disable=unused-variable
                    key = ZBXPRFX + metric
                    if name:
                        key += "[" + name + "]"
                    try:
                        value = str(val())
                        self.zabbix_metrics.append(ZabbixMetric(self.zabbix_host, key, value, ts))
                    except:
                        pass
            except:
                pass

        # Send when needed
        if self.context.type == PM_CONTEXT_ARCHIVE:
            if len(self.zabbix_metrics) >= self.zabbix_interval:
                send_to_zabbix(self.zabbix_metrics, self.zabbix_server, self.zabbix_port)
                self.zabbix_metrics = []
        elif ts - self.zabbix_prevsend > self.zabbix_interval:
            send_to_zabbix(self.zabbix_metrics, self.zabbix_server, self.zabbix_port)
            self.zabbix_metrics = []
            self.zabbix_prevsend = ts

    def finalize(self):
        """ Finalize and clean up """
        if self.writer:
            try:
                self.writer.flush()
            except socket.error as error:
                if error.errno != errno.EPIPE:
                    raise
            self.writer.close()
            self.writer = None
        if self.pmi:
            self.pmi.pmiEnd()
            self.pmi = None
        if self.zabbix_metrics:
            send_to_zabbix(self.zabbix_metrics, self.zabbix_server, self.zabbix_port)
            self.zabbix_metrics = []

if __name__ == '__main__':
    try:
        P = PMReporter()
        P.read_config()
        P.read_cmd_line()
        P.prepare_metrics()
        P.connect()
        P.validate_config()
        P.validate_metrics()
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
