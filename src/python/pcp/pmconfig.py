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

# pylint: disable=superfluous-parens
# pylint: disable=invalid-name, line-too-long, no-self-use
# pylint: disable=too-many-boolean-expressions, too-many-statements
# pylint: disable=too-many-instance-attributes, too-many-locals
# pylint: disable=too-many-branches, too-many-nested-blocks
# pylint: disable=bare-except, broad-except

""" PCP Python Utils Config Routines """

from collections import OrderedDict
try:
    import ConfigParser
except ImportError:
    import configparser as ConfigParser
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
        self.metricspec = ('label', 'instances', 'unit', 'type', 'width', 'formula')

        # Main utility reference
        self.util = util

        # Metric details
        self.pmids = []
        self.descs = []
        self.insts = []

        # Pause helpers
        self._round = 0
        self._init_ts = None

        # Pass data with pmTraversePMNS
        self._tmp = []

    def set_signal_handler(self):
        """ Set default signal handler """
        def handler(signum, frame):
            """ Default signal handler """
            self.util.finalize()
            sys.exit(0)
        for sig in "SIGHUP", "SIGTERM":
            try:
                signum = getattr(signal, sig)
                signal.signal(signum, handler)
            except:
                pass

    def set_config_file(self, default_config):
        """ Set default config file """
        config = None
        for conf in default_config:
            conf = conf.replace("$HOME", os.getenv("HOME"))
            conf = conf.replace("$PCP_SYSCONF_DIR", pmapi.pmContext.pmGetConfig("PCP_SYSCONF_DIR"))
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
            self.util.colxrow = value
            return
        if value in ('true', 'True', 'y', 'yes', 'Yes'):
            value = 1
        if value in ('false', 'False', 'n', 'no', 'No'):
            value = 0
        if name == 'speclocal':
            if not self.util.speclocal or not self.util.speclocal.startswith("K:"):
                self.util.speclocal = value
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
        if self.util.config:
            config.read(self.util.config)
        self.read_section_options(config, 'options')
        for arg in iter(sys.argv[1:]):
            if arg.startswith(":") and arg[1:] in config.sections():
                self.read_section_options(config, arg[1:])

    def read_cmd_line(self):
        """ Read command line options """
        pmapi.c_api.pmSetOptionFlags(pmapi.c_api.PM_OPTFLAG_DONE)
        if pmapi.c_api.pmGetOptionsFromList(sys.argv):
            raise pmapi.pmUsageErr()

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

    def parse_metric_spec_instances(self, spec):
        """ Parse instances from metric spec """
        insts = []
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
            if s:
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
                        if index == 2:
                            metrics[key].append([])
                        else:
                            metrics[key].append(None)
            else:
                # Additional info
                key, spec = key.rsplit(".")
                if key not in metrics:
                    sys.stderr.write("Undeclared metric key %s.\n" % key)
                    sys.exit(1)
                if value.startswith('"') and value.endswith('"'):
                    value = value[1:-1]
                if spec == "formula":
                    if self.util.derived is None:
                        self.util.derived = metrics[key][0] + "=" + value
                    else:
                        self.util.derived += "@" + metrics[key][0] + "=" + value
                else:
                    if self.metricspec.index(spec) == 1:
                        metrics[key][self.metricspec.index(spec)+1] = [value]
                    else:
                        metrics[key][self.metricspec.index(spec)+1] = value

    def prepare_metrics(self):
        """ Construct and prepare the initial metrics set """
        metrics = self.util.opts.pmGetOperands()
        if not metrics:
            sys.stderr.write("No metrics specified.\n")
            raise pmapi.pmUsageErr()

        # Read config
        config = ConfigParser.SafeConfigParser()
        if self.util.config:
            config.read(self.util.config)

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

        # Create the combined metrics set
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

    def check_metric(self, metric):
        """ Validate individual metric and get its details """
        try:
            pmid = self.util.context.pmLookupName(metric)[0]
            if pmid in self.pmids:
                # Always ignore duplicates
                return
            desc = self.util.context.pmLookupDescs(pmid)[0]
            try:
                if self.util.context.type == pmapi.c_api.PM_CONTEXT_ARCHIVE:
                    inst = self.util.context.pmGetInDomArchive(desc)
                else:
                    inst = self.util.context.pmGetInDom(desc) # disk.dev.read
                if not inst[0]:
                    inst = ([pmapi.c_api.PM_IN_NULL], [None]) # pmcd.pmie.logfile
            except pmapi.pmErr:
                inst = ([pmapi.c_api.PM_IN_NULL], [None])     # mem.util.free
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
            if self.util.omit_flat and instances and not inst[1][0]:
                return
            if instances and inst[1][0]:
                found = [[], []]
                for r in instances:
                    try:
                        cr = re.compile(r'\A' + r + r'\Z')
                        for i, s in enumerate(inst[1]):
                            if re.match(cr, s):
                                found[0].append(inst[0][i])
                                found[1].append(inst[1][i])
                    except Exception as e:
                        sys.stderr.write("Invalid regex '%s': %s.\n" % (r, e))
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
        """ Format a metric label """
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

    def validate_metrics(self, curr_insts=CURR_INSTS, max_insts=MAX_INSTS):
        """ Validate the metrics set """
        # Check the metrics against PMNS, resolve non-leaf metrics
        if self.util.derived:
            if self.util.derived.startswith("/") or self.util.derived.startswith("."):
                try:
                    self.util.context.pmLoadDerivedConfig(self.util.derived)
                except pmapi.pmErr as error:
                    sys.stderr.write("Failed to register derived metric: %s.\n" % str(error))
                    sys.exit(1)
            else:
                for definition in self.util.derived.split("@"):
                    err = ""
                    try:
                        name, expr = definition.split("=", 1)
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

        # Prepare for non-leaf metrics
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
                elif len(self.pmids) == l + 1:
                    # Leaf
                    if metric in self.util.context.pmNameAll(self.pmids[l]):
                        self.util.metrics[metric] = metrics[metric]
                    else:
                        # Handle single non-leaf case in an archive
                        self.util.metrics[self.util.context.pmNameID(self.pmids[l])] = []
                else:
                    # Non-leaf
                    for i in range(l, len(self.pmids)):
                        name = self.util.context.pmNameID(self.pmids[i])
                        # We ignore specs like disk.dm,,,MB on purpose, for now
                        self.util.metrics[name] = []
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

        # Finalize the metrics set
        incompat_metrics = OrderedDict()
        for i, metric in enumerate(self.util.metrics):
            # Fill in all fields for easier checking later
            for index in range(0, 6):
                if len(self.util.metrics[metric]) <= index:
                    if index == 1:
                        self.util.metrics[metric].append([])
                    else:
                        self.util.metrics[metric].append(None)

            # Label
            if not self.util.metrics[metric][0]:
                # mem.util.free -> m.u.free
                name = ""
                for m in metric.split("."):
                    name += m[0] + "."
                self.util.metrics[metric][0] = name[:-2] + m # pylint: disable=undefined-loop-variable

            # Instance(s)
            if not self.util.metrics[metric][1] and self.util.instances:
                if self.insts[i][0][0] != pmapi.c_api.PM_IN_NULL:
                    self.util.metrics[metric][1] = self.util.instances
            if self.insts[i][0][0] == pmapi.c_api.PM_IN_NULL:
                self.util.metrics[metric][1] = []

            # Rawness
            # As a special service for pmrep(1) utility we hardcode
            # support for its two output modes, archive and csv.
            if (hasattr(self.util, 'type') and self.util.type == 1) or \
               self.util.metrics[metric][3] == 'raw' or \
               self.util.output == 'archive' or \
               self.util.output == 'csv':
                self.util.metrics[metric][3] = 1
            else:
                self.util.metrics[metric][3] = 0

            # Set unit/scale if not specified on per-metric basis
            if not self.util.metrics[metric][2]:
                unit = self.descs[i].contents.units
                self.util.metrics[metric][2] = str(unit)
                if self.util.count_scale and \
                   unit.dimCount == 1 and ( \
                   unit.dimSpace == 0 and \
                   unit.dimTime == 0):
                    self.util.metrics[metric][2] = self.util.count_scale
                if self.util.space_scale and \
                   unit.dimSpace == 1 and ( \
                   unit.dimCount == 0 and \
                   unit.dimTime == 0):
                    self.util.metrics[metric][2] = self.util.space_scale
                if self.util.time_scale and \
                   unit.dimTime == 1 and ( \
                   unit.dimCount == 0 and \
                   unit.dimSpace == 0):
                    self.util.metrics[metric][2] = self.util.time_scale

            # Finalize label and unit/scale
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
            # But use native type when else was not requested
            if str(unitstr) == str(self.descs[i].contents.units):
                mtype = self.descs[i].contents.type
            # However always use double for non-raw counters
            if self.util.metrics[metric][3] == 0 and \
               self.descs[i].contents.sem == pmapi.c_api.PM_SEM_COUNTER:
                mtype = pmapi.c_api.PM_TYPE_DOUBLE
            # Strings will be strings right till the end
            if self.descs[i].contents.type == pmapi.c_api.PM_TYPE_STRING:
                mtype = self.descs[i].contents.type

            # Set default width if not specified on per-metric basis
            if self.util.metrics[metric][4]:
                self.util.metrics[metric][4] = int(self.util.metrics[metric][4])
            elif hasattr(self.util, 'width') and self.util.width != 0:
                self.util.metrics[metric][4] = self.util.width
            else:
                self.util.metrics[metric][4] = len(self.util.metrics[metric][0])
            if self.util.metrics[metric][4] < len(TRUNC):
                self.util.metrics[metric][4] = len(TRUNC) # Forced minimum

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
                            except:
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
            except:
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

    def finalize_options(self):
        """ Finalize util options """
        # Runtime overrides samples/interval
        if self.util.opts.pmGetOptionFinishOptarg():
            self.util.runtime = float(self.util.opts.pmGetOptionFinish()) - float(self.util.opts.pmGetOptionOrigin())
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
                except:
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
