# Copyright (C) 2015-2018 Marko Myllynen <myllynen@redhat.com>
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
# pylint: disable=too-many-branches, too-many-nested-blocks
# pylint: disable=broad-except, too-many-public-methods

""" PCP Python Utils Config Routines """

from collections import OrderedDict
try:
    import configparser as ConfigParser
except ImportError:
    import ConfigParser
import signal
import time
import csv
import sys
import os
import re

from pcp import pmapi

# Common defaults (for applicable utils)
TRUNC = "xxx"
VERSION = 1
CURR_INSTS = False
MAX_INSTS = 1024

class pmConfig(object):
    """ Config reader and validator """
    def __init__(self, util):
        # Common special command line switches
        self.arghelp = ('-?', '--help', '-V', '--version')

        # Supported metricset specifiers
        self.metricspec = ('label', 'instances', 'unit', 'type',
                           'width', 'precision', 'limit', 'formula')

        # Main utility reference
        self.util = util

        # Metric details
        self.pmids = []
        self.descs = []
        self.insts = []

        # Pause helpers
        self._round = 0
        self._init_ts = None

        # Predicate metric references
        self._pred_index = []
        self._pred_indom = []

        # Instance regex cache
        self._re_cache = {}

        # Pass data with pmTraversePMNS
        self._tmp = []

    def set_signal_handler(self):
        """ Set default signal handler """
        def handler(_signum, _frame):
            """ Default signal handler """
            self.util.finalize()
            sys.exit(0)
        for sig in "SIGHUP", "SIGTERM":
            try:
                signum = getattr(signal, sig)
                signal.signal(signum, handler)
            except Exception:
                pass

    def set_config_file(self, default_config):
        """ Set default config file """
        config = None
        usrdir = os.path.expanduser('~')
        sysdir = pmapi.pmContext.pmGetConfig("PCP_SYSCONF_DIR")
        for conf in default_config:
            conf = conf.replace("$HOME", usrdir)
            conf = conf.replace("$PCP_SYSCONF_DIR", sysdir)
            if os.path.isfile(conf) and os.access(conf, os.R_OK):
                config = conf
                break

        # Possibly override the default config file before
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

    def set_attr(self, name, value):
        """ Set options read from file """
        if name == 'colxrow':
            # As a special service for pmrep(1) utility we handle
            # its config colxrow parameter here with minimal impact.
            if value.startswith('"') and value.endswith('"'):
                value = value[1:-1]
            self.util.colxrow = value
            return
        if value in ('true', 'True', 'y', 'yes', 'Yes'):
            value = 1
        if value in ('false', 'False', 'n', 'no', 'No'):
            value = 0
        if name == 'speclocal':
            self.util.speclocal = value
        elif name == 'derived':
            if ';' in value:
                self.util.derived = value
            else:
                self.util.derived = str(value).replace(",", ";")
        elif name == 'samples':
            self.util.opts.pmSetOptionSamples(value)
            self.util.samples = self.util.opts.pmGetOptionSamples()
        elif name == 'interval':
            self.util.opts.pmSetOptionInterval(value)
            self.util.interval = self.util.opts.pmGetOptionInterval()
        elif name == 'type':
            if value == 'raw':
                self.util.type = 1
            else:
                self.util.type = 0
        elif name == 'type_prefer':
            if value == 'raw':
                self.util.type_prefer = 1
            else:
                self.util.type_prefer = 0
        elif name == 'instances':
            self.util.instances = value.split(",") # pylint: disable=no-member
        else:
            try:
                setattr(self.util, name, int(value))
            except ValueError:
                if value.startswith('"') and value.endswith('"'): # pylint: disable=no-member
                    value = value[1:-1]
                setattr(self.util, name, value)

    def read_section_options(self, config, section):
        """ Read options from a configuration file section """
        if not config.has_section(section):
            return
        for opt in config.options(section):
            if opt in self.util.keys:
                self.set_attr(opt, config.get(section, opt))
            elif section == 'options':
                sys.stderr.write("Unknown option %s in [%s].\n" % (opt, section))
                sys.exit(1)

    def read_options(self):
        """ Read options from configuration file """
        config = ConfigParser.SafeConfigParser()
        config.optionxform = str
        if self.util.config:
            try:
                config.read(self.util.config)
            except ConfigParser.Error as error:
                sys.stderr.write("Failed to read configuration file '%s', line %d:\n%s\n"
                                 % (self.util.config, error.lineno, str(error.message).split("\n")[0])) # pylint: disable=no-member
                sys.exit(1)
        self.read_section_options(config, 'options')
        for arg in iter(sys.argv[1:]):
            if arg.startswith(":") and arg[1:] in config.sections():
                self.read_section_options(config, arg[1:])

    def read_cmd_line(self):
        """ Read command line options """
        pmapi.c_api.pmSetOptionFlags(pmapi.c_api.PM_OPTFLAG_DONE)
        if pmapi.c_api.pmGetOptionsFromList(sys.argv):
            raise pmapi.pmUsageErr()
        return pmapi.c_api.pmGetOperands()

    def parse_instances(self, instances):
        """ Parse user-supplied instances string """
        insts = []
        reader = csv.reader([instances])
        for inst in list(reader)[0]:
            if inst.startswith('"') or inst.startswith("'"):
                inst = inst[1:]
            if inst.endswith('"') or inst.endswith("'"):
                inst = inst[:-1]
            insts.append(inst)
        return insts

    def parse_metric_spec_instances(self, spec):
        """ Parse instances from metric spec """
        insts = []
        if spec.count(",") < 2:
            return spec + ",,", insts
        # User may supply quoted or unquoted instance specification
        # Conf file preserves outer quotes, command line does not
        # We need to detect which is the case here. What a mess.
        quoted = 0
        s = spec.split(",")[2]
        if s and len(s) > 2 and (s[0] == "'" or s[0] == '"'):
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
            if s:
                insts = [s]
        if spec.count(",") < 2:
            spec += ",,"
        return spec, insts

    def parse_new_verbose_metric(self, metrics, key, value):
        """ Parse new verbose metric """
        metrics[key] = [value]
        for index in range(0, len(self.metricspec)):
            if len(metrics[key]) <= index:
                if index == 2:
                    metrics[key].append([])
                else:
                    metrics[key].append(None)

    def parse_verbose_metric_info(self, metrics, key, spec, value):
        """ Parse additional verbose metric info """
        if value.startswith('"') and value.endswith('"'):
            value = value[1:-1]
        if spec == "formula":
            if self.util.derived is None:
                self.util.derived = ";" + metrics[key][0] + "=" + value
            else:
                self.util.derived += ";" + metrics[key][0] + "=" + value
        else:
            if self.metricspec.index(spec) == 1:
                metrics[key][self.metricspec.index(spec)+1] = [value]
            else:
                metrics[key][self.metricspec.index(spec)+1] = value

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
                self.parse_new_verbose_metric(metrics, key, value)
            else:
                # Additional info
                key, spec = key.rsplit(".")
                if key not in metrics:
                    sys.stderr.write("Undeclared metric key %s.\n" % key)
                    sys.exit(1)
                self.parse_verbose_metric_info(metrics, key, spec, value)

    def prepare_metrics(self):
        """ Construct and prepare the initial metricset """
        metrics = self.util.opts.pmGetOperands()
        if not metrics:
            sys.stderr.write("No metrics specified.\n")
            raise pmapi.pmUsageErr()

        # Read config
        config = ConfigParser.SafeConfigParser()
        config.optionxform = str
        if self.util.config:
            try:
                config.read(self.util.config)
            except ConfigParser.Error as error:
                sys.stderr.write("Failed to read configuration file '%s', line %d:\n%s\n"
                                 % (self.util.config, error.lineno, str(error.message).split("\n")[0])) # pylint: disable=no-member
                sys.exit(1)

        # First read global metrics (if not disabled already)
        globmet = OrderedDict()
        if self.util.globals == 1:
            if config.has_section('global'):
                parsemet = OrderedDict()
                for key in config.options('global'):
                    if key in self.util.keys:
                        sys.stderr.write("No options allowed in [global] section.\n")
                        sys.exit(1)
                    self.parse_metric_info(parsemet, key, config.get('global', key))
                for metric in parsemet:
                    name = parsemet[metric][:1][0]
                    globmet[name] = parsemet[metric][1:]

        # Add command line and configuration file metricsets
        tempmet = OrderedDict()
        for metric in metrics:
            if metric.startswith(":"):
                tempmet[metric[1:]] = None
            else:
                spec, insts = self.parse_metric_spec_instances(metric)
                m = spec.split(",")
                m[2] = insts
                tempmet[m[0]] = m[1:]

        # Get config and set details for configuration file metricsets
        confmet = OrderedDict()
        for spec in tempmet:
            if tempmet[spec] is None:
                if config.has_section(spec):
                    parsemet = OrderedDict()
                    for key in config.options(spec):
                        if key not in self.util.keys:
                            self.parse_metric_info(parsemet, key, config.get(spec, key))
                    for metric in parsemet:
                        name = parsemet[metric][:1][0]
                        confmet[name] = parsemet[metric][1:]
                    tempmet[spec] = confmet
                else:
                    raise IOError("Metricset definition '%s' not found." % metric)

        # Create the combined metricset
        if self.util.globals == 1:
            for metric in globmet:
                self.util.metrics[metric] = globmet[metric]
        for metric in tempmet:
            if isinstance(tempmet[metric], list):
                self.util.metrics[metric] = tempmet[metric]
            else:
                if tempmet[metric]:
                    for m in tempmet[metric]:
                        self.util.metrics[m] = confmet[m]

        if not self.util.metrics:
            raise IOError("No metrics specified.")

    def do_live_filtering(self):
        """ Check if doing live filtering """
        if hasattr(self.util, 'live_filter') and self.util.live_filter:
            return True
        return False

    def do_invert_filtering(self):
        """ Check if doing invert filtering """
        if hasattr(self.util, 'invert_filter') and self.util.invert_filter:
            return True
        return False

    def check_metric(self, metric):
        """ Validate individual metric and get its details """
        try:
            pmid = self.util.context.pmLookupName(metric)[0]
            if pmid in self.pmids:
                # Always ignore duplicates
                return
            desc = self.util.context.pmLookupDescs(pmid)[0]
            if desc.contents.indom == pmapi.c_api.PM_IN_NULL:
                inst = ([pmapi.c_api.PM_IN_NULL], [None])     # mem.util.free
            else:
                if self.util.context.type == pmapi.c_api.PM_CONTEXT_ARCHIVE:
                    inst = self.util.context.pmGetInDomArchive(desc)
                else:
                    inst = self.util.context.pmGetInDom(desc) # disk.dev.read
                if not inst[0]:
                    inst = ([pmapi.c_api.PM_IN_NULL], [None]) # pmcd.pmie.logfile
            # Reject unsupported types
            if not (desc.contents.type == pmapi.c_api.PM_TYPE_32 or
                    desc.contents.type == pmapi.c_api.PM_TYPE_U32 or
                    desc.contents.type == pmapi.c_api.PM_TYPE_64 or
                    desc.contents.type == pmapi.c_api.PM_TYPE_U64 or
                    desc.contents.type == pmapi.c_api.PM_TYPE_FLOAT or
                    desc.contents.type == pmapi.c_api.PM_TYPE_DOUBLE or
                    desc.contents.type == pmapi.c_api.PM_TYPE_STRING):
                raise pmapi.pmErr(pmapi.c_api.PM_ERR_TYPE)
            instances = self.util.instances if not self._tmp else self._tmp
            if hasattr(self.util, 'omit_flat') and self.util.omit_flat and not inst[1][0]:
                return
            if instances and inst[1][0] and not self.do_live_filtering():
                found = [[], []]
                for r in instances:
                    try:
                        cr = re.compile(r'\A' + r + r'\Z')
                        for i, s in enumerate(inst[1]):
                            if re.match(cr, s):
                                found[0].append(inst[0][i])
                                found[1].append(inst[1][i])
                        del cr
                    except Exception as error:
                        sys.stderr.write("Invalid regex '%s': %s.\n" % (r, error))
                        sys.exit(1)
                if not found[0]:
                    return
                inst = tuple(found)
            self.pmids.append(pmid)
            self.descs.append(desc)
            self.insts.append(inst)
        except pmapi.pmErr as error:
            if hasattr(self.util, 'ignore_incompat') and self.util.ignore_incompat:
                return
            sys.stderr.write("Invalid metric %s (%s).\n" % (metric, str(error)))
            sys.exit(1)

    def format_metric_label(self, label):
        """ Format a metric text label """
        # See src/libpcp/src/units.c
        if ' / ' in label:
            label = label.replace("nanosec", "ns").replace("microsec", "us")
            label = label.replace("millisec", "ms").replace("sec", "s")
            label = label.replace("min", "min").replace("hour", "h")
            label = label.replace(" / ", "/")
        return label

    class pmfg_items_to_indom(object): # pylint: disable=too-few-public-methods
        """ Helper to provide consistent interface with pmfg items and indoms """
        def __init__(self, items):
            """ Initialize an instance with items """
            self._items = items

        def __call__(self):
            """ Retrieve the items """
            return self._items

    def validate_common_options(self):
        """ Validate common utility options """
        try:
            err = "Integer expected"
            if hasattr(self.util, 'rank') and self.util.rank:
                self.util.rank = int(self.util.rank)
            if hasattr(self.util, 'limit_filter') and self.util.limit_filter:
                self.util.limit_filter = int(self.util.limit_filter)
            if hasattr(self.util, 'limit_filter_force') and self.util.limit_filter_force:
                self.util.limit_filter_force = int(self.util.limit_filter_force)
            err = "Non-negative integer expected"
            if hasattr(self.util, 'width') and self.util.width:
                self.util.width = int(self.util.width)
                if self.util.width < 0:
                    raise ValueError(err)
            if hasattr(self.util, 'width_force') and self.util.width_force:
                self.util.width_force = int(self.util.width_force)
                if self.util.width_force < 0:
                    raise ValueError(err)
            if hasattr(self.util, 'precision') and self.util.precision:
                self.util.precision = int(self.util.precision)
                if self.util.precision < 0:
                    raise ValueError(err)
            if hasattr(self.util, 'precision_force') and self.util.precision_force:
                self.util.precision_force = int(self.util.precision_force)
                if self.util.precision_force < 0:
                    raise ValueError(err)
            if hasattr(self.util, 'repeat_header') and self.util.repeat_header:
                self.util.repeat_header = int(self.util.repeat_header)
                if self.util.repeat_header < 0:
                    raise ValueError(err)
        except ValueError:
            sys.stderr.write("Error while parsing options: %s.\n" % err)
            sys.exit(1)

    def validate_metrics(self, curr_insts=CURR_INSTS, max_insts=MAX_INSTS):
        """ Validate the metricset """
        if hasattr(self.util, 'predicate') and self.util.predicate:
            for predicate in self.util.predicate.split(","):
                if predicate not in self.util.metrics:
                    self.util.metrics[predicate] = ['', []]

        # Check the metrics against PMNS, resolve non-leaf metrics
        if self.util.derived:
            for derived in filter(None, self.util.derived.split(";")):
                if derived.startswith("/") or derived.startswith("."):
                    try:
                        self.util.context.pmLoadDerivedConfig(derived)
                    except pmapi.pmErr as error:
                        sys.stderr.write("Failed to load derived metric definitions from file '%s':\n%s.\n" % (derived, str(error)))
                        sys.exit(1)
                else:
                    err = ""
                    try:
                        name, expr = derived.split("=", 1)
                        self.util.context.pmLookupName(name.strip())
                    except pmapi.pmErr as error:
                        if error.args[0] != pmapi.c_api.PM_ERR_NAME:
                            err = error.message()
                        else:
                            try:
                                self.util.context.pmRegisterDerived(name.strip(), expr.strip())
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

        if not hasattr(self.util, 'leaf_only') or not self.util.leaf_only:
            # Prepare for non-leaf metrics while preserving metric order
            metrics = self.util.metrics
            self.util.metrics = OrderedDict()

            def metric_base_check(metric):
                """ Helper to support non-leaf metricspecs """
                from copy import deepcopy
                if metric != self._tmp:
                    if metric not in self.util.metrics:
                        self.util.metrics[metric] = deepcopy(metrics[self._tmp])
                else:
                    self.util.metrics[metric] = deepcopy(metrics[metric])

            # Resolve non-leaf metrics to allow metricspecs like disk.dm,,,MB
            for metric in list(metrics):
                self._tmp = metric
                try:
                    self.util.context.pmTraversePMNS(metric, metric_base_check)
                except pmapi.pmErr as error:
                    from copy import deepcopy
                    self.util.metrics[metric] = deepcopy(metrics[metric])

        metrics = self.util.metrics
        self.util.metrics = OrderedDict()

        for metric in metrics:
            try:
                l = len(self.pmids)
                self._tmp = metrics[metric][1]
                self.util.context.pmTraversePMNS(metric, self.check_metric)
                if len(self.pmids) == l:
                    # No compatible metrics found
                    continue
                else:
                    self.util.metrics[metric] = metrics[metric]
            except pmapi.pmErr as error:
                sys.stderr.write("Invalid metric %s (%s).\n" % (metric, str(error)))
                sys.exit(1)

        # Exit if no metrics with specified instances found
        if not self.insts:
            sys.stderr.write("No matching instances found.\n")
            # Try to help the user to get the instance specifications right
            if self.util.instances:
                print("\nRequested global instances:")
                print(self.util.instances)
            sys.exit(1)

        # Finalize the metricset
        incompat_metrics = OrderedDict()
        for i, metric in enumerate(self.util.metrics):
            # Fill in all fields for easier checking later
            for index in range(0, 8):
                if len(self.util.metrics[metric]) <= index:
                    if index == 1:
                        self.util.metrics[metric].append([])
                    else:
                        self.util.metrics[metric].append(None)

            # Text label
            if not self.util.metrics[metric][0]:
                # mem.util.free -> m.u.free
                name = ""
                for m in metric.split("."):
                    name += m[0] + "."
                self.util.metrics[metric][0] = name[:-2] + metric.split(".")[-1]

            # Instance(s)
            if not self.util.metrics[metric][1] and self.util.instances:
                if self.insts[i][0][0] != pmapi.c_api.PM_IN_NULL:
                    self.util.metrics[metric][1] = self.util.instances
            if self.insts[i][0][0] == pmapi.c_api.PM_IN_NULL:
                self.util.metrics[metric][1] = []

            # Rawness
            if hasattr(self.util, 'type_prefer') and not self.util.metrics[metric][3]:
                self.util.metrics[metric][3] = self.util.type_prefer
            elif self.util.metrics[metric][3] == 'raw':
                self.util.metrics[metric][3] = 1
            else:
                self.util.metrics[metric][3] = 0
            # As a special service for the pmrep(1) utility,
            # we force raw output with its archive mode.
            if (hasattr(self.util, 'type') and self.util.type == 1) or \
               self.util.metrics[metric][3] == 'raw' or \
               self.util.output == 'archive':
                self.util.metrics[metric][3] = 1

            # Dimension test helpers
            def is_count(unit):
                """ Test count dimension """
                if unit.dimCount == 1 and ( \
                   unit.dimSpace == 0 and \
                   unit.dimTime == 0):
                    return True
                return False

            def is_space(unit):
                """ Test space dimension """
                if unit.dimSpace == 1 and ( \
                   unit.dimCount == 0 and \
                   unit.dimTime == 0):
                    return True
                return False

            def is_time(unit):
                """ Test time dimension """
                if unit.dimTime == 1 and ( \
                   unit.dimCount == 0 and \
                   unit.dimSpace == 0):
                    return True
                return False

            # Set unit/scale
            unit = self.descs[i].contents.units
            if is_count(unit):
                if hasattr(self.util, 'count_scale_force') and self.util.count_scale_force:
                    self.util.metrics[metric][2] = self.util.count_scale_force
                elif hasattr(self.util, 'count_scale') and self.util.count_scale and \
                   not self.util.metrics[metric][2]:
                    self.util.metrics[metric][2] = self.util.count_scale
                elif not self.util.metrics[metric][2]:
                    self.util.metrics[metric][2] = str(unit)
            if is_space(unit):
                if hasattr(self.util, 'space_scale_force') and self.util.space_scale_force:
                    self.util.metrics[metric][2] = self.util.space_scale_force
                elif hasattr(self.util, 'space_scale') and self.util.space_scale and \
                   not self.util.metrics[metric][2]:
                    self.util.metrics[metric][2] = self.util.space_scale
                elif not self.util.metrics[metric][2]:
                    self.util.metrics[metric][2] = str(unit)
            if is_time(unit):
                if hasattr(self.util, 'time_scale_force') and self.util.time_scale_force:
                    self.util.metrics[metric][2] = self.util.time_scale_force
                elif hasattr(self.util, 'time_scale') and self.util.time_scale and \
                   not self.util.metrics[metric][2]:
                    self.util.metrics[metric][2] = self.util.time_scale
                elif not self.util.metrics[metric][2]:
                    self.util.metrics[metric][2] = str(unit)
            if not self.util.metrics[metric][2]:
                self.util.metrics[metric][2] = str(unit)

            # Finalize text label and unit/scale
            try:
                label = self.util.metrics[metric][2]
                (unitstr, mult) = self.util.context.pmParseUnitsStr(self.util.metrics[metric][2])
                if self.util.metrics[metric][3] == 0 and \
                   self.descs[i].contents.type != pmapi.c_api.PM_TYPE_STRING and \
                   self.descs[i].sem == pmapi.c_api.PM_SEM_COUNTER and \
                   '/' not in label:
                    label += " / s"
                label = self.format_metric_label(label)
                self.util.metrics[metric][2] = (label, unitstr, mult)
            except pmapi.pmErr as error:
                sys.stderr.write("%s: %s.\n" % (str(error), self.util.metrics[metric][2]))
                sys.exit(1)

            # Set metric type - default to double for precision
            mtype = pmapi.c_api.PM_TYPE_DOUBLE
            # But use native type if nothing else was requested
            if str(unitstr) == str(self.descs[i].contents.units):
                mtype = self.descs[i].contents.type
            # However always use double for non-raw counters
            if self.util.metrics[metric][3] == 0 and \
               self.descs[i].contents.sem == pmapi.c_api.PM_SEM_COUNTER:
                mtype = pmapi.c_api.PM_TYPE_DOUBLE
            # Strings will be strings right till the end
            if self.descs[i].contents.type == pmapi.c_api.PM_TYPE_STRING:
                mtype = self.descs[i].contents.type

            # Set width
            if self.util.metrics[metric][4]:
                try:
                    self.util.metrics[metric][4] = int(self.util.metrics[metric][4])
                    if self.util.metrics[metric][4] < 0:
                        raise ValueError
                except Exception:
                    sys.stderr.write("Non-negative integer expected: %s\n" % metric)
                    sys.exit(1)
            elif hasattr(self.util, 'width'):
                self.util.metrics[metric][4] = self.util.width
            else:
                self.util.metrics[metric][4] = 0 # Auto-adjust
            if hasattr(self.util, 'width_force') and self.util.width_force is not None:
                self.util.metrics[metric][4] = self.util.width_force
            if not self.util.metrics[metric][4]:
                self.util.metrics[metric][4] = len(self.util.metrics[metric][0])
            if self.util.metrics[metric][4] < len(TRUNC):
                self.util.metrics[metric][4] = len(TRUNC) # Forced minimum

            # Set precision
            # NB. We need to take into account that clients expect pmfg item in [5]
            if self.util.metrics[metric][5]:
                try:
                    self.util.metrics[metric][6] = int(self.util.metrics[metric][5])
                except Exception:
                    sys.stderr.write("Non-negative integer expected: %s\n" % metric)
                    sys.exit(1)
            elif hasattr(self.util, 'precision'):
                self.util.metrics[metric][6] = self.util.precision
            else:
                self.util.metrics[metric][6] = 3 # Built-in default
            if hasattr(self.util, 'precision_force') and self.util.precision_force is not None:
                self.util.metrics[metric][6] = self.util.precision_force
            self.util.metrics[metric][5] = None

            # Set value limit filter
            if self.util.metrics[metric][7]:
                try:
                    self.util.metrics[metric][7] = int(self.util.metrics[metric][7])
                except Exception:
                    sys.stderr.write("Integer expected: %s\n" % metric)
                    sys.exit(1)
            elif hasattr(self.util, 'limit_filter'):
                self.util.metrics[metric][7] = self.util.limit_filter
            if hasattr(self.util, 'limit_filter_force') and self.util.limit_filter_force:
                self.util.metrics[metric][7] = self.util.limit_filter_force
            if self.descs[i].contents.type == pmapi.c_api.PM_TYPE_STRING:
                self.util.metrics[metric][7] = None

            # Add fetchgroup items
            try:
                items = []
                max_insts = max(1, max_insts)
                scale = self.util.metrics[metric][2][0]
                if curr_insts and self.util.metrics[metric][1]:
                    mitems = 0
                    vanished = []
                    for j in range(0, len(self.insts[i][1])):
                        if mitems < max_insts:
                            try:
                                items.append((self.insts[i][0][j], self.insts[i][1][j], self.util.pmfg.extend_item(metric, mtype, scale, self.insts[i][1][j])))
                                mitems += 1
                            except pmapi.pmErr as error:
                                if error.args[0] == pmapi.c_api.PM_ERR_CONV:
                                    raise
                                vanished.append(j)
                        else:
                            del self.insts[i][0][-1]
                            del self.insts[i][1][-1]
                    if mitems > 0:
                        for v in reversed(vanished):
                            del self.insts[i][0][v]
                            del self.insts[i][1][v]
                    self.util.metrics[metric][5] = self.pmfg_items_to_indom(items)
                else:
                    self.util.metrics[metric][5] = self.util.pmfg.extend_indom(metric, mtype, scale, max_insts)

                # Populate per-metric regex cache for live filtering
                if self.do_live_filtering():
                    try:
                        self._re_cache[metric] = []
                        for r in self.util.metrics[metric][1]:
                            self._re_cache[metric].append(re.compile(r'\A' + r + r'\Z'))
                    except Exception as error:
                        sys.stderr.write("Invalid regex '%s': %s.\n" % (r, error))
                        sys.exit(1)
            except Exception:
                if hasattr(self.util, 'ignore_incompat') and self.util.ignore_incompat:
                    # Schedule the metric for removal
                    incompat_metrics[metric] = i
                else:
                    raise

        # Remove all traces of incompatible metrics
        for metric in reversed(incompat_metrics):
            del self.pmids[incompat_metrics[metric]]
            del self.descs[incompat_metrics[metric]]
            del self.insts[incompat_metrics[metric]]
            del self.util.metrics[metric]
        del incompat_metrics

        # Verify that we have valid metrics
        if not self.util.metrics:
            sys.stderr.write("No compatible metrics found.\n")
            sys.exit(1)

        if hasattr(self.util, 'predicate') and self.util.predicate:
            self.validate_predicate()

    def finalize_options(self):
        """ Finalize util options """
        # Runtime overrides samples/interval
        if self.util.opts.pmGetOptionFinishOptarg():
            origin = float(self.util.opts.pmGetOptionOrigin()) if self.util.opts.pmGetOptionOrigin() is not None else 0
            self.util.runtime = float(self.util.opts.pmGetOptionFinish()) - origin
            if self.util.opts.pmGetOptionSamples():
                self.util.samples = self.util.opts.pmGetOptionSamples()
                if self.util.samples < 2:
                    self.util.samples = 2
                self.util.interval = float(self.util.runtime) / (self.util.samples - 1)
                self.util.opts.pmSetOptionInterval(str(self.util.interval))
                self.util.interval = self.util.opts.pmGetOptionInterval()
            else:
                self.util.interval = self.util.opts.pmGetOptionInterval()
                if not self.util.interval:
                    self.util.interval = pmapi.timeval(0)
                try:
                    self.util.samples = int(self.util.runtime / float(self.util.interval) + 1)
                except Exception:
                    pass
        else:
            self.util.samples = self.util.opts.pmGetOptionSamples()
            self.util.interval = self.util.opts.pmGetOptionInterval()

        if float(self.util.interval) <= 0:
            sys.stderr.write("Interval must be greater than zero.\n")
            sys.exit(1)

    def pause(self):
        """ Pause before next sampling """
        self._round += 1

        if not self._init_ts:
            self._init_ts = float(self.util.pmfg_ts().strftime("%s.%f"))

        wakeup = self._init_ts + float(self.util.interval) * self._round

        sleep = wakeup - time.time()

        if sleep > 0:
            time.sleep(sleep)

    def filter_instance(self, metric, name):
        """ Filter instance name against metric instances """
        if not self._re_cache[metric]:
            return True

        for cr in self._re_cache[metric]:
            if re.match(cr, name):
                return True

        return False

    def rank(self, instances):
        """ Rank instances """
        if not self.util.rank:
            return instances
        rank = abs(self.util.rank)
        revs = True if self.util.rank > 0 else False
        return sorted(instances, key=lambda value: value[2], reverse=revs)[:rank]

    def validate_predicate(self):
        """ Validate predicate filter reference metrics """
        for predicate in self.util.predicate.split(","):
            index = -1
            for i, metric in enumerate(self.util.metrics):
                if metric == predicate:
                    index = i
                    self._pred_index.append(i)
                    self._pred_indom.append(self.descs[i].contents.indom)
                    break

            if index < 0:
                sys.stderr.write("Internal error, predicate metric not found!")
                sys.exit(2)

            if self.insts[index][0][0] == pmapi.c_api.PM_IN_NULL:
                sys.stderr.write("Predicate metric must have instances.\n")
                sys.exit(1)

            if self.descs[index].contents.type == pmapi.c_api.PM_TYPE_STRING:
                sys.stderr.write("Predicate metric values must be numeric.\n")
                sys.exit(1)

    def get_sorted_results(self):
        """ Get filtered and ranked results """
        results = OrderedDict()
        if hasattr(self.util, 'predicate') and self.util.predicate:
            predicates = self.util.predicate.split(",")
        else:
            predicates = ()
        early_live_filter = self.do_live_filtering() and not self.do_invert_filtering()
        for i, metric in enumerate(self.util.metrics):
            results[metric] = []
            try:
                for inst, name, val in self.util.metrics[metric][5]():
                    try:
                        # Ignore transient instances
                        if inst != pmapi.c_api.PM_IN_NULL and not name:
                            continue
                        if early_live_filter and inst != pmapi.c_api.PM_IN_NULL and \
                           not self.filter_instance(metric, name):
                            continue
                        value = val()
                        if self.util.metrics[metric][7]:
                            if metric not in predicates:
                                limit = self.util.metrics[metric][7]
                                if limit > 0 and value < limit:
                                    continue
                                elif limit < 0 and value > abs(limit):
                                    continue
                        results[metric].append((inst, name, value))
                    except Exception:
                        pass
            except Exception:
                pass

        if predicates:
            pred_insts = {}
            for _pred_index, predicate in enumerate(predicates):
                results[predicate] = self.rank(results[predicate])
                if self._pred_indom[_pred_index] not in pred_insts:
                    pred_insts[self._pred_indom[_pred_index]] = []
                pred_insts[self._pred_indom[_pred_index]].extend([i[0] for i in results[predicate]])
            for i, metric in enumerate(results):
                if metric in predicates:
                    # Predicate instance values may all get filtered,
                    # but other metrics' instance values may be above
                    # the filter so predicate is filtered after rank.
                    if self.util.metrics[metric][7]:
                        limit = self.util.metrics[metric][7]
                        if limit > 0:
                            results[metric] = [x for x in results[metric] if x[2] >= limit]
                        elif limit < 0:
                            results[metric] = [x for x in results[metric] if x[2] <= abs(limit)]
                    continue
                if self.descs[i].contents.indom not in self._pred_indom:
                    results[metric] = self.rank(results[metric])
                    continue
                inst_index = self.descs[i].contents.indom
                results[metric] = [i for i in results[metric] if i[0] in pred_insts[inst_index]]
        else:
            if hasattr(self.util, 'rank') and self.util.rank:
                for metric in results:
                    results[metric] = self.rank(results[metric])

        if self.do_live_filtering() and self.do_invert_filtering():
            for metric in results:
                results[metric] = [i for i in results[metric] if self.filter_instance(metric, i[1])]

        return results
