#!/usr/bin/env pmpython
#
# Copyright (C) 2023-2024 Nathan Scott <nathans@debian.org>
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

# pylint: disable=superfluous-parens
# pylint: disable=invalid-name, line-too-long, no-self-use
# pylint: disable=too-many-boolean-expressions, too-many-statements
# pylint: disable=too-many-instance-attributes, too-many-locals
# pylint: disable=too-many-branches, too-many-nested-blocks, too-many-arguments
# pylint: disable=broad-except

""" PCP to Apache Arrow (especially parquet format) Bridge """

# Common imports
from collections import OrderedDict
import sys
import os

# Arrow imports
import pyarrow as pa
import pyarrow.dataset as ds

# PCP Python API
from pcp import pmapi, pmconfig
from cpmapi import pmSetContextOptions
from cpmapi import PM_CONTEXT_ARCHIVE, PM_INDOM_NULL, PM_SEM_COUNTER
from cpmapi import PM_TYPE_32, PM_TYPE_64, PM_TYPE_U32, PM_TYPE_U64
from cpmapi import PM_TYPE_FLOAT, PM_TYPE_DOUBLE, PM_TYPE_STRING

# Default config
DEFAULT_CONFIG = ["./pcp2arrow.conf", "$HOME/.pcp2arrow.conf", "$HOME/.pcp/pcp2arrow.conf", "$PCP_SYSCONF_DIR/pcp2arrow.conf"]

# Defaults
CONFVER = 1

class PCP2ARROW(object):
    """ PCP to ARROW """
    def __init__(self):
        """ Construct object, prepare for command line handling """
        self.context = None
        self.daemonize = False
        self.pmconfig = pmconfig.pmConfig(self)
        self.opts = self.options()

        # Configuration directives
        self.keys = ('source', 'output',
                     'samples', 'interval', 'type', 'daemonize', 'version',
                     'type_prefer', 'limit_filter', 'limit_filter_force',
                     'live_filter', 'rank', 'invert_filter', 'names_change',
                     'speclocal', 'instances',
                     'include_labels')

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
        self.globals = 1
        self.samples = None # forever
        self.interval = pmapi.timeval(60)      # 60 sec
        self.opts.pmSetOptionInterval(str(60)) # 60 sec
        self.type = 0
        self.type_prefer = self.type
        self.ignore_incompat = 1
        self.ignore_unknown = 1
        self.names_change = 0 # ignore
        self.instances = []
        self.live_filter = 0
        self.rank = 0
        self.limit_filter = 0
        self.limit_filter_force = 0
        self.invert_filter = 0
        self.include_labels = 0
        self.interpol = 0

        # Internal
        self.outfile = None
        self.runtime = -1
        self.schema = None
        self.matrix = {}  # dict of value vectors, keyed by column name
        self.indoms = {}  # dict of dict, keyed by indom ID first, inst ID next

        # Performance metrics store
        # key - metric name
        # values - 0:txt label, 1:instance(s), 2:unit/scale, 3:type,
        #          4:width, 5:pmfg item, 6:precision, 7:limit
        self.metrics = OrderedDict()
        self.pmfg = None
        self.pmfg_ts = None

    def options(self):
        """ Setup default command line argument option handling """
        opts = pmapi.pmOptions()
        opts.pmSetOptionCallback(self.option)
        opts.pmSetOverrideCallback(self.option_override)
        opts.pmSetShortOptions("a:h:LK:c:C:D:V?A:S:T:O:s:t:rRi:jJ:8:9:nZ:zo:")
        opts.pmSetShortUsage("[option...] [metricspec...]")

        opts.pmSetLongOptionHeader("General options")
        opts.pmSetLongOptionArchive()      # -a/--archive
        opts.pmSetLongOptionArchiveFolio() # --archive-folio
        opts.pmSetLongOptionContainer()    # --container
        opts.pmSetLongOptionHost()         # -h/--host
        opts.pmSetLongOptionLocalPMDA()    # -L/--local-PMDA
        opts.pmSetLongOptionSpecLocal()    # -K/--spec-local
        opts.pmSetLongOption("config", 1, "c", "FILE", "config file path")
        opts.pmSetLongOption("check", 0, "C", "", "check config and metrics and exit")
        opts.pmSetLongOption("output", 1, "o", "OUTFILE", "output file")
        opts.pmSetLongOptionDebug()        # -D/--debug
        opts.pmSetLongOptionVersion()      # -V/--version
        opts.pmSetLongOptionHelp()         # -?/--help

        opts.pmSetLongOptionHeader("Reporting options")
        opts.pmSetLongOptionAlign()        # -A/--align
        opts.pmSetLongOptionStart()        # -S/--start
        opts.pmSetLongOptionFinish()       # -T/--finish
        opts.pmSetLongOptionOrigin()       # -O/--origin
        opts.pmSetLongOptionSamples()      # -s/--samples
        opts.pmSetLongOptionInterval()     # -t/--interval
        opts.pmSetLongOptionTimeZone()     # -Z/--timezone
        opts.pmSetLongOptionHostZone()     # -z/--hostzone
        opts.pmSetLongOption("raw", 0, "r", "", "output raw counter values (no rate conversion)")
        opts.pmSetLongOption("raw-prefer", 0, "R", "", "prefer output raw counter values (no rate conversion)")
        #opts.pmSetLongOption("names-change", 1, "4", "ACTION", "update/ignore/abort on PMNS change (default: ignore)")
        opts.pmSetLongOption("instances", 1, "i", "STR", "instances to report (default: all current)")
        opts.pmSetLongOption("live-filter", 0, "j", "", "perform instance live filtering")
        opts.pmSetLongOption("rank", 1, "J", "COUNT", "limit results to COUNT highest/lowest valued instances")
        opts.pmSetLongOption("limit-filter", 1, "8", "LIMIT", "default limit for value filtering")
        opts.pmSetLongOption("limit-filter-force", 1, "9", "LIMIT", "forced limit for value filtering")
        opts.pmSetLongOption("invert-filter", 0, "n", "", "perform ranking before live filtering")
        #opts.pmSetLongOption("include-labels", 0, "m", "", "include metric label info")

        return opts

    def option_override(self, opt):
        """ Override standard PCP options """
        if opt in ('g', 'H', 'K', 'n', 'N'):
            return 1
        return 0

    def option(self, opt, optarg, _index):
        """ Perform setup for individual command line option """
        if opt == 'K':
            if not self.speclocal or not self.speclocal.startswith(";"):
                self.speclocal = ";" + optarg
            else:
                self.speclocal = self.speclocal + ";" + optarg
        elif opt == 'c':
            self.config = optarg
        elif opt == 'C':
            self.check = 1
        elif opt == 'e':
            if not self.derived or not self.derived.startswith(";"):
                self.derived = ";" + optarg
            else:
                self.derived = self.derived + ";" + optarg
        elif opt == 'o':
            if os.path.exists(optarg):
                sys.stderr.write("File %s already exists.\n" % optarg)
                sys.exit(1)
            self.outfile = optarg
        elif opt == 'r':
            self.type = 1
        elif opt == 'R':
            self.type_prefer = 1
        elif opt == '4':
            if optarg == 'ignore':
                self.names_change = 0
            elif optarg == 'abort':
                self.names_change = 1
            elif optarg == 'update':
                self.names_change = 2
            else:
                sys.stderr.write("Unknown names-change action '%s' specified.\n" % optarg)
                sys.exit(1)
        elif opt == 'i':
            self.instances = self.instances + self.pmconfig.parse_instances(optarg)
        elif opt == 'j':
            self.live_filter = 1
        elif opt == 'J':
            self.rank = optarg
        elif opt == '8':
            self.limit_filter = optarg
        elif opt == '9':
            self.limit_filter_force = optarg
        elif opt == 'n':
            self.invert_filter = 1
        elif opt == 'm':
            self.include_labels = 1
        else:
            raise pmapi.pmUsageErr()

    def configure(self):
        """ Process command line, read configuration and verify everything """
        os.putenv('PCP_DERIVED_CONFIG', '')

        self.pmconfig.read_cmd_line()

        ctx_type, self.source = pmapi.pmContext.set_connect_options(self.opts, self.source, self.speclocal)
        self.pmfg = pmapi.fetchgroup(ctx_type, self.source)
        self.pmfg_ts = self.pmfg.extend_timestamp()
        self.context = self.pmfg.get_context()
        ctx = self.context.ctx
        if pmSetContextOptions(ctx, self.opts.mode, self.opts.delta):
            raise pmapi.pmUsageErr()

        self.config = self.pmconfig.set_config_path(DEFAULT_CONFIG)
        self.pmconfig.read_options()
        self.pmconfig.prepare_metrics(pmns=True)
        self.pmconfig.set_signal_handler()

        if self.version != CONFVER:
            sys.stderr.write("Incompatible configuration file version (read v%s, need v%d).\n" % (self.version, CONFVER))
            sys.exit(1)

        self.pmconfig.validate_common_options()

        if not self.outfile:
            sys.stderr.write("No output file name given, cannot proceed.\n")
            sys.exit(1)

        self.pmconfig.validate_metrics(curr_insts=not self.live_filter)
        self.pmconfig.finalize_options()

    def execute(self):
        """ Fetch and append values """
        # Common preparations
        self.context.prepare_execute(self.opts, False, 1, self.interval)

        # Just checking
        if self.check == 1:
            return

        # Main loop
        refresh_metrics = 0
        while self.samples != 0:
            # Refresh metrics as needed
            if refresh_metrics:
                refresh_metrics = 0
                self.pmconfig.update_metrics(curr_insts=not self.live_filter)

            # Fetch values
            refresh_metrics = self.pmconfig.fetch()
            if refresh_metrics < 0:
                break

            # Append timestamp and values
            self.append(self.pmfg_ts())

            # Finally, prepare for the next round
            if self.samples and self.samples > 0:
                self.samples -= 1
            if self.context.type != PM_CONTEXT_ARCHIVE and self.samples != 0:
                self.pmconfig.pause()

    def lookup_indom(self, desc):
        if desc.indom in self.indoms:
            return self.indoms[desc.indom]
        indom = {}
        (insts, names) = self.pmconfig.get_metric_indom(desc)
        if insts is not None:
            for (inst, name) in zip(insts, names):
                indom[inst] = name
        self.indoms[desc.indom] = indom
        return indom

    def lookup_patype(self, desc):
        """ Find the appropriate arrow type for a metric descriptor """
        if desc.sem == PM_SEM_COUNTER and not self.type:
            return pa.float64
        if desc.type == PM_TYPE_32:
            return pa.int32
        if desc.type == PM_TYPE_64:
            return pa.int64
        if desc.type == PM_TYPE_U32:
            return pa.uint32
        if desc.type == PM_TYPE_U64:
            return pa.uint64
        if desc.type == PM_TYPE_FLOAT:
            return pa.float32
        if desc.type == PM_TYPE_DOUBLE:
            return pa.float64
        if desc.type == PM_TYPE_STRING:
            return pa.string
        return None

    def create_schema(self):
        """ Define the column names and associated types (table schema)
            starting with the timestamp column then all metrics[+insts]
        """
        self.schema = [pa.field('timestamp', pa.timestamp('ns'))]
        self.matrix['timestamp'] = []

        for i, metric in enumerate(self.metrics):
            desc = self.pmconfig.descs[i]
            patype = self.lookup_patype(desc)
            if patype is None:
                continue
            if desc.indom != PM_INDOM_NULL:
                indom = self.lookup_indom(desc)
                for inst in indom.keys():
                    metricspec = metric + '[' + indom[inst] + ']'
                    self.matrix[metricspec] = []
                    field = pa.field(metricspec, patype())
                    self.schema.append(field)
            else:
                self.matrix[metric] = []
                field = pa.field(metric, patype())
                self.schema.append(field)

    def append(self, timestamp):
        """ Append latest results (row) onto each arrow array (columns)
        """
        # Delayed until here as only now are values guaranteed
        if not self.schema:
            self.create_schema()

        # Append to timestamp column first
        self.matrix['timestamp'].append(timestamp)
        #print('Step:', timestamp)

        # Append either value or an Arrow nul (None) to each column
        results = self.pmconfig.get_ranked_results(valid_only=True)
        for i, metric in enumerate(self.metrics):
            desc = self.pmconfig.descs[i]
            if desc.indom == PM_INDOM_NULL:
                if metric not in results:
                    value = None
                else:
                    value = results[metric][0][2]
                self.matrix[metric].append(value)
                continue

            # Create a dictionary of values indexed by inst ID
            values = {}
            if metric in results:
                for instid, _, value in results[metric]:
                    values[instid] = value

            # Iterate all instances for the metric, use values
            indom = self.indoms[desc.indom]
            for instid in indom.keys():
                name = indom[instid]
                metricspec = metric + '[' + name + ']'
                if instid not in values:
                    value = None
                else:
                    value = values[instid]
                self.matrix[metricspec].append(value)

    def flush(self):
        """ Create the table object and flush the dataset """
        fulltable = pa.table(self.matrix, schema=pa.schema(self.schema))
        ds.write_dataset(fulltable, self.outfile, format="parquet")

if __name__ == '__main__':
    try:
        P = PCP2ARROW()
        P.configure()

        try:
            P.execute()
        except KeyboardInterrupt:  # allow interrupt to end live sampling
            if P.context.type != PM_CONTEXT_ARCHIVE:
                sys.stdout.write("Interrupted, flushing...\n")
                P.flush()
                sys.exit(0)
            sys.stdout.write("Interrupted, skipping flush.\n")
            sys.exit(1)

        P.flush()

    except pmapi.pmErr as error:
        sys.stderr.write("%s: %s" % (error.progname(), error.message()))
        sys.stderr.write("\n")
        sys.exit(1)
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)
    except (IOError, TypeError) as error:
        sys.stderr.write("%s\n" % str(error))
        sys.exit(1)
