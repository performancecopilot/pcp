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

# pylint: disable=superfluous-parens, bad-whitespace
# pylint: disable=invalid-name, line-too-long, no-self-use
# pylint: disable=too-many-boolean-expressions, too-many-statements
# pylint: disable=too-many-instance-attributes, too-many-locals
# pylint: disable=too-many-branches, too-many-nested-blocks
# pylint: disable=bare-except, broad-except
# pylint: disable=too-many-lines

""" Performance Metrics Reporter """

# Common imports
from collections import OrderedDict
import errno
import sys

# Our imports
from datetime import datetime, timedelta
import socket
import time
import math
import re
import os

# PCP Python PMAPI
from pcp import pmapi, pmi, pmconfig
from cpmapi import PM_CONTEXT_ARCHIVE, PM_CONTEXT_LOCAL
from cpmapi import PM_ERR_EOL, PM_IN_NULL, PM_DEBUG_APPL1
from cpmapi import PM_TYPE_FLOAT, PM_TYPE_DOUBLE, PM_TYPE_STRING
from cpmapi import PM_TIME_SEC
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
NO_VAL  = "N/A"
NO_INST = "~"

# pmrep output targets
OUTPUT_ARCHIVE = "archive"
OUTPUT_CSV     = "csv"
OUTPUT_STDOUT  = "stdout"

class PMReporter(object):
    """ Report PCP metrics """
    def __init__(self):
        """ Construct object, prepare for command line handling """
        self.context = None
        self.daemonize = 0
        self.pmconfig = pmconfig.pmConfig(self)
        self.opts = self.options()

        # Configuration directives
        self.keys = ('source', 'output', 'derived', 'header', 'globals',
                     'samples', 'interval', 'type', 'precision', 'daemonize',
                     'timestamp', 'unitinfo', 'colxrow',
                     'delay', 'width', 'delimiter', 'extcsv',
                     'extheader', 'repeat_header', 'timefmt', 'interpol',
                     'count_scale', 'space_scale', 'time_scale', 'version',
                     'speclocal', 'instances', 'ignore_incompat', 'omit_flat')

        # The order of preference for options (as present):
        # 1 - command line options
        # 2 - options from configuration file(s)
        # 3 - built-in defaults defined below
        self.check = 0
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
        self.interval = pmapi.timeval(1)       # 1 sec
        self.opts.pmSetOptionInterval(str(1))  # 1 sec
        self.delay = 0
        self.type = 0
        self.ignore_incompat = 0
        self.instances = []
        self.omit_flat = 0
        self.colxrow = None
        self.width = 0
        self.precision = 3 # .3f
        self.delimiter = None
        self.extcsv = 0
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
        self.prev_ts = None
        self.runtime = -1

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
        opts.pmSetShortOptions("a:h:LK:c:Co:F:e:D:V?HUGpA:S:T:O:s:t:Z:zdrIi:vX:w:P:l:kxE:f:uq:b:y:")
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
        opts.pmSetLongOption("output", 1, "o", "OUTPUT", "output target: archive, csv, stdout (default)")
        opts.pmSetLongOption("output-file", 1, "F", "OUTFILE", "output file")
        opts.pmSetLongOption("derived", 1, "e", "FILE|DFNT", "derived metrics definitions")
        self.daemonize = opts.pmSetLongOption("daemonize", 0, "", "", "daemonize on startup") # > 1
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
        opts.pmSetLongOption("precision", 1, "P", "N", "N digits after the decimal separator (default: 3)")
        opts.pmSetLongOption("delimiter", 1, "l", "STR", "delimiter to separate csv/stdout columns")
        opts.pmSetLongOption("extended-csv", 0, "k", "", "write extended CSV")
        opts.pmSetLongOption("extended-header", 0, "x", "", "display extended header")
        opts.pmSetLongOption("repeat-header", 1, "E", "N", "repeat stdout headers every N lines")
        opts.pmSetLongOption("timestamp-format", 1, "f", "STR", "strftime string for timestamp format")
        opts.pmSetLongOption("no-interpol", 0, "u", "", "disable interpolation mode with archives")
        opts.pmSetLongOption("count-scale", 1, "q", "SCALE", "default count unit")
        opts.pmSetLongOption("space-scale", 1, "b", "SCALE", "default space unit")
        opts.pmSetLongOption("time-scale", 1, "y", "SCALE", "default time unit")

        return opts

    def option_override(self, opt):
        """ Override standard PCP options """
        if opt == 'H' or opt == 'K' or opt == 'p':
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
        elif opt == 'o':
            if optarg == OUTPUT_ARCHIVE:
                self.output = OUTPUT_ARCHIVE
            elif optarg == OUTPUT_CSV:
                self.output = OUTPUT_CSV
            elif optarg == OUTPUT_STDOUT:
                self.output = OUTPUT_STDOUT
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
            self.instances = self.instances + self.pmconfig.parse_instances(optarg)
        elif opt == 'v':
            self.omit_flat = 1
        elif opt == 'X':
            self.colxrow = optarg
        elif opt == 'w':
            try:
                self.width = int(optarg)
            except:
                sys.stderr.write("Error while parsing options: Integer expected.\n")
                sys.exit(1)
        elif opt == 'P':
            try:
                self.precision = int(optarg)
            except:
                sys.stderr.write("Error while parsing options: Integer expected.\n")
                sys.exit(1)
        elif opt == 'l':
            self.delimiter = optarg
        elif opt == 'k':
            self.extcsv = 1
        elif opt == 'x':
            self.extheader = 1
        elif opt == 'E':
            try:
                self.repeat_header = int(optarg)
            except:
                sys.stderr.write("Error while parsing options: Integer expected.\n")
                sys.exit(1)
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

        # Check how we were invoked and adjust output
        if sys.argv[0].endswith("pcp2csv"):
            self.output = OUTPUT_CSV

        if self.output == OUTPUT_ARCHIVE and not self.outfile:
            sys.stderr.write("Archive must be defined with archive output.\n")
            sys.exit(1)

        self.pmconfig.finalize_options()

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

        # Time
        self.localtz = self.context.get_current_tz()

        # Common preparations
        self.context.prepare_execute(self.opts, self.output == OUTPUT_ARCHIVE, self.interpol, self.interval)

        # Set output primitives
        if self.delimiter is None:
            if self.output == OUTPUT_CSV:
                self.delimiter = CSVSEP
            else:
                self.delimiter = OUTSEP

        if self.timefmt is None:
            if self.output == OUTPUT_CSV:
                self.timefmt = CSVTIME
            else:
                self.timefmt = OUTTIME
        if not self.timefmt:
            self.timestamp = 0

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

    def prepare_writer(self):
        """ Prepare generic stdout writer """
        if not self.writer:
            if self.output == OUTPUT_ARCHIVE or \
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
            for _ in range(len(self.pmconfig.insts[i][0])):
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

        if self.context.type == PM_CONTEXT_LOCAL:
            host = "localhost, using DSO PMDAs"
        else:
            host = self.context.pmGetContextHostName()

        timezone = self.context.posix_tz_to_utc_offset(self.context.get_current_tz(self.opts))
        if timezone != self.context.posix_tz_to_utc_offset(self.localtz):
            timezone += " (reporting, current is " + self.context.posix_tz_to_utc_offset(self.localtz) + ")"

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

        instances = sum([len(x[0]) for x in self.pmconfig.insts])
        insts_txt = "instances" if instances != 1 else "instance"

        if self.context.type == PM_CONTEXT_ARCHIVE and not self.interpol:
            duration = float(self.opts.pmGetOptionFinish()) - float(self.opts.pmGetOptionOrigin())

        def secs_to_readable(seconds):
            """ Convert seconds to easily readable format """
            seconds = float(math.floor((seconds) + math.copysign(0.5, seconds)))
            parts = str(timedelta(seconds=int(round(seconds)))).split(':')
            if len(parts[0]) == 1:
                parts[0] = "0" + parts[0]
            elif parts[0][-2] == " ":
                parts[0] = parts[0].rsplit(" ", 1)[0] + " 0" + parts[0].rsplit(" ", 1)[1]
            return ":".join(parts)

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
        self.writer.write(comm + "  metrics: " + str(len(self.metrics)) + " (" + str(instances) + " " + insts_txt + ")\n")
        self.writer.write(comm + "  samples: " + str(samples) + "\n")
        if not (self.context.type == PM_CONTEXT_ARCHIVE and not self.interpol):
            self.writer.write(comm + " interval: " + str(float(self.interval)) + " sec\n")
        else:
            self.writer.write(comm + " interval: N/A\n")
        self.writer.write(comm + " duration: " + secs_to_readable(duration) + "\n")
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
            if self.extcsv:
                self.writer.write("Host,Interval,")
            self.writer.write("Time")
            for i, metric in enumerate(self.metrics):
                for j in range(len(self.pmconfig.insts[i][0])):
                    name = metric
                    if self.pmconfig.insts[i][0][0] != PM_IN_NULL and self.pmconfig.insts[i][1][j]:
                        name += "-" + self.pmconfig.insts[i][1][j]
                    # Mark metrics with instance domain but without instances
                    if self.pmconfig.descs[i].contents.indom != PM_IN_NULL and self.pmconfig.insts[i][1][0] is None:
                        name += "-"
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
                prnti = 1 if self.pmconfig.insts[i][0][0] != PM_IN_NULL else prnti
                for j in range(len(self.pmconfig.insts[i][0])):
                    names.append(self.metrics[metric][0])
                    names.append(self.delimiter)
                    units.append(self.metrics[metric][2][0])
                    units.append(self.delimiter)
                    if prnti == 1 and self.pmconfig.insts[i][1][j]:
                        insts.append(self.pmconfig.insts[i][1][j])
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
            self.recorded = {} # pylint: disable=attribute-defined-outside-init
            if self.context.type == PM_CONTEXT_ARCHIVE:
                self.pmi.pmiSetHostname(self.context.pmGetArchiveLabel().hostname)
            self.pmi.pmiSetTimezone(self.context.get_current_tz(self.opts))
            for i, metric in enumerate(self.metrics):
                self.pmi.pmiAddMetric(metric,
                                      self.pmconfig.pmids[i],
                                      self.pmconfig.descs[i].contents.type,
                                      self.pmconfig.descs[i].contents.indom,
                                      self.pmconfig.descs[i].contents.sem,
                                      self.pmconfig.descs[i].contents.units)
                ins = 0 if self.pmconfig.insts[i][0][0] == PM_IN_NULL else len(self.pmconfig.insts[i][0])
                for j in range(ins):
                    if metric not in self.recorded:
                        self.recorded[metric] = []
                    self.recorded[metric].append(self.pmconfig.insts[i][0][j])
                    try:
                        self.pmi.pmiAddInstance(self.pmconfig.descs[i].contents.indom, self.pmconfig.insts[i][1][j], self.pmconfig.insts[i][0][j])
                    except pmi.pmiErr as error:
                        if error.args[0] == PMI_ERR_DUPINSTNAME:
                            pass

        # Add current values
        data = 0
        for i, metric in enumerate(self.metrics):
            try:
                for inst, name, val in self.metrics[metric][5](): # pylint: disable=unused-variable
                    try:
                        if inst != PM_IN_NULL and not name:
                            continue
                        value = val()
                        if inst != PM_IN_NULL and inst not in self.recorded[metric]:
                            self.recorded[metric].append(inst)
                            try:
                                self.pmi.pmiAddInstance(self.pmconfig.descs[i].contents.indom, name, inst)
                            except pmi.pmiErr as error:
                                if error.args[0] == PMI_ERR_DUPINSTNAME:
                                    pass
                        if self.pmconfig.descs[i].contents.type == PM_TYPE_STRING:
                            self.pmi.pmiPutValue(metric, name, value)
                        elif self.pmconfig.descs[i].contents.type == PM_TYPE_FLOAT or \
                             self.pmconfig.descs[i].contents.type == PM_TYPE_DOUBLE:
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

        ts = self.context.datetime_to_secs(self.pmfg_ts(), PM_TIME_SEC)

        if self.prev_ts is None:
            self.prev_ts = ts

        # Construct the results
        line = ""
        if self.extcsv:
            if self.context.type == PM_CONTEXT_LOCAL:
                host = "localhost"
            else:
                host = self.context.pmGetContextHostName()
            line += host + ","
            line += str(int(ts - self.prev_ts + 0.5)) + ","
            self.prev_ts = ts
        line += timestamp
        if self.extcsv:
            line += " " + self.context.posix_tz_to_utc_offset(self.context.get_current_tz(self.opts))

        # Avoid crossing the C/Python boundary more than once per metric
        res = {}
        for _, metric in enumerate(self.metrics):
            try:
                for inst, name, val in self.metrics[metric][5](): # pylint: disable=unused-variable
                    try:
                        value = val()
                        if isinstance(value, float):
                            value = round(value, self.precision)
                        res[metric + str(inst)] = value
                    except:
                        pass
            except:
                pass

        # Add corresponding values for each column in the static header
        for i, metric in enumerate(self.metrics):
            for j in range(len(self.pmconfig.insts[i][0])):
                line += self.delimiter
                if metric + str(self.pmconfig.insts[i][0][j]) in res:
                    value = res[metric + str(self.pmconfig.insts[i][0][j])]
                    if isinstance(value, str):
                        value = value.replace(self.delimiter, "_").replace("\n", " ").replace('"', " ")
                        line += str('"' + value + '"')
                    else:
                        line += str(value)
        self.writer.write(line + "\n")

    def write_stdout(self, timestamp):
        """ Write a line to stdout """
        if self.colxrow is None:
            self.write_stdout_std(timestamp)
        else:
            self.write_stdout_colxrow(timestamp)

    def check_non_number(self, value, width):
        """ Check and handle float inf, -inf, and NaN """
        if math.isinf(value):
            if value > 0:
                value = "inf" if width >= 3 else pmconfig.TRUNC
            else:
                value = "-inf" if width >= 4 else pmconfig.TRUNC
        elif math.isnan(value):
            value = "NaN" if width >= 3 else pmconfig.TRUNC
        return value

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

        # Avoid crossing the C/Python boundary more than once per metric
        res = {}
        for _, metric in enumerate(self.metrics):
            try:
                for inst, name, val in self.metrics[metric][5](): # pylint: disable=unused-variable
                    try:
                        value = val()
                        if isinstance(value, float):
                            value = round(value, self.precision)
                        elif isinstance(value, str):
                            value = value.replace("\n", "\\n")
                            if not self.delimiter.isspace():
                                value = value.replace(self.delimiter, "_")
                        res[metric + str(inst)] = value
                    except:
                        pass
            except:
                pass

        # Add corresponding values for each column in the static header
        k = 0
        for i, metric in enumerate(self.metrics):
            l = self.metrics[metric][4]
            for j in range(len(self.pmconfig.insts[i][0])):
                k += 1
                if metric + str(self.pmconfig.insts[i][0][j]) in res:
                    value = res[metric + str(self.pmconfig.insts[i][0][j])]
                    # Make sure the value fits
                    if isinstance(value, (int, long)):
                        if len(str(value)) > l:
                            value = pmconfig.TRUNC
                        else:
                            #fmt[k] = "{:" + str(l) + "d}"
                            fmt[k] = "{X:" + str(l) + "d}"
                    elif isinstance(value, str):
                        if len(value) > l:
                            value = pmconfig.TRUNC

                    if isinstance(value, float) and \
                       not math.isinf(value) and \
                       not math.isnan(value):
                        c = self.precision
                        s = len(str(int(value)))
                        if s > l:
                            c = -1
                            value = pmconfig.TRUNC
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
                else:
                    line.append(NO_VAL)
                    line.append(self.delimiter)

        del line[-1]
        #self.writer.write('{}'.join(fmt).format(*tuple(line)) + "\n")
        index = 0
        nfmt = ""
        for f in fmt:
            if isinstance(line[index], float):
                line[index] = self.check_non_number(line[index], self.metrics[metric][4])
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
            for instance in self.pmconfig.insts[i][1]:
                if instance not in insts:
                    insts.append(instance)

        # Avoid crossing the C/Python boundary more than once per metric
        res = OrderedDict()
        for _, metric in enumerate(self.metrics):
            res[metric] = []
            try:
                for inst, name, val in self.metrics[metric][5](): # pylint: disable=unused-variable
                    try:
                        if inst != PM_IN_NULL and not name:
                            res[metric].append(['', '', NO_VAL])
                        else:
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
                        if isinstance(value, str):
                            value = value.replace("\n", "\\n")
                            if not self.delimiter.isspace():
                                value = value.replace(self.delimiter, "_")
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
                        value = pmconfig.TRUNC
                    else:
                        fmt[k] = "{X:" + str(l) + "d}"

                if isinstance(value, float) and \
                   not math.isinf(value) and \
                   not math.isnan(value):
                    c = self.precision
                    s = len(str(int(value)))
                    if s > l:
                        c = -1
                        value = pmconfig.TRUNC
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
                if isinstance(line[index], float):
                    line[index] = self.check_non_number(line[index], self.metrics[metric][4])
                nfmt += f.replace("{X:", "{" + str(index) + ":")
                index += 1
                nfmt += "{" + str(index) + "}"
                index += 1
            l = len(str(index-1)) + 2
            nfmt = nfmt[:-l]
            output += nfmt.format(*tuple(line)) + "\n"

        self.writer.write(output)

    def finalize(self):
        """ Finalize and clean up """
        if self.writer:
            try:
                self.writer.flush()
            except socket.error as error:
                if error.errno != errno.EPIPE:
                    raise
            try:
                self.writer.close()
            except:
                pass
            self.writer = None
        if self.pmi:
            self.pmi.pmiEnd()
            self.pmi = None

if __name__ == '__main__':
    try:
        P = PMReporter()
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
