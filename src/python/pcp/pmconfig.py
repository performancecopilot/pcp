#
# Copyright (C) 2015-2021 Marko Myllynen <myllynen@redhat.com>
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

# pylint: disable=superfluous-parens, too-many-lines
# pylint: disable=invalid-name, line-too-long, no-self-use
# pylint: disable=too-many-boolean-expressions, too-many-statements
# pylint: disable=too-many-instance-attributes, too-many-locals
# pylint: disable=too-many-branches, too-many-nested-blocks
# pylint: disable=broad-except, too-many-public-methods

""" PCP Python Utils Config Routines """

from copy import deepcopy
from collections import OrderedDict
try:
    import configparser as ConfigParser
except ImportError:
    import ConfigParser
import signal
import time
import math
import csv
import sys
import os
import re

from pcp import pmapi

# Common defaults (for applicable utils)
TRUNC = "xxx"
VERSION = 1
CURR_INSTS = False

class pmConfig(object):
    """ Config reader and validator """
    def __init__(self, util):
        # Common special command line switches
        self.arghelp = ('-?', '--help', '-V', '--version')

        # Supported metricset specifiers - label is txt label of metricspec
        self.metricspec = ('label', 'instances', 'unit', 'type',
                           'width', 'precision', 'limit', 'formula')

        # Main utility reference
        self.util = util

        # Metric details
        self.pmids = []
        self.descs = []
        self.insts = []
        self.texts = []
        self.labels = []                 # PCP labels of initial instances
        self.res_labels = OrderedDict()  # PCP labels of current results

        # Pause helpers
        self._round = 0
        self._init_ts = None

        # Predicate metric references
        self._pred_indom = []

        # Instance regex cache
        self._re_cache = {}

        # Pass data with pmTraversePMNS
        self._tmp = []

        # Store configured metrics to avoid rereading on PMNS updates
        self._conf_metrics = OrderedDict()

        # Update PCP labels on instance changes
        self._prev_insts = []

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

    # Deprecated, use set_config_path() below instead
    def set_config_file(self, default_config):
        """ Set default config file """
        return self.set_config_path(default_config)

    def set_config_path(self, default_config):
        """ Set default config path """
        config = None
        usrdir = os.path.expanduser('~')
        sysdir = pmapi.pmContext.pmGetConfig("PCP_SYSCONF_DIR")
        for conf in default_config:
            conf = conf.replace("$HOME", usrdir)
            conf = conf.replace("$PCP_SYSCONF_DIR", sysdir)
            if os.access(conf, os.R_OK) and \
               (os.path.isfile(conf) or os.path.isdir(conf)):
                config = conf
                break

        # Possibly override the default config path before
        # parsing the rest of the command line options
        args = iter(sys.argv[1:])
        for arg in args:
            if arg in self.arghelp:
                return None
            if arg in ('-c', '--config') or arg.startswith("-c"):
                try:
                    if arg in ('-c', '--config'):
                        config = next(args)
                    else:
                        config = arg.replace("-c", "", 1)
                    if not os.access(config, os.R_OK) or \
                       not (os.path.isfile(config) or os.path.isdir(config)):
                        if not os.path.exists(config):
                            err = "No such file or directory"
                        elif not os.access(config, os.R_OK):
                            err = "Permission denied"
                        else:
                            err = "Not a regular file"
                        raise IOError("Failed to read configuration from '%s':\n%s." % (config, err))
                except StopIteration:
                    break

        return config

    def _get_conf_files(self):
        """ Helper to get individual config files """
        conf_files = []
        if self.util.config:
            if os.path.isfile(self.util.config):
                conf_files.append(self.util.config)
            else:
                for f in sorted(os.listdir(self.util.config)):
                    fn = os.path.join(self.util.config, f)
                    if fn.endswith(".conf") and os.access(fn, os.R_OK) and os.path.isfile(fn):
                        conf_files.append(fn)
        return conf_files

    def set_attr(self, name, value):
        """ Set options read from file """
        value = str(value)
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
            if value.find(';') != -1:
                self.util.derived = value
            else:
                self.util.derived = value.replace(",", ";")
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
            self.util.instances = value.split(",")
        else:
            try:
                setattr(self.util, name, int(value))
            except ValueError:
                if value.startswith('"') and value.endswith('"'):
                    value = value[1:-1]
                setattr(self.util, name, value)

    def read_section_options(self, config, section):
        """ Read options from a configuration file section """
        if not config.has_section(section):
            return
        for opt in config.options(section):
            if opt in self.util.keys and not config.get(section, opt):
                raise ValueError("No value set for option %s in [%s]" % (opt, section))
            if opt in self.util.keys:
                self.set_attr(opt, config.get(section, opt))
            elif section == 'options':
                raise ValueError("Unknown option %s in [%s]" % (opt, section))

    def read_options(self):
        """ Read options from configuration file """
        # Python < 3.2 compat
        if sys.version_info[0] >= 3 and sys.version_info[1] >= 2:
            config = ConfigParser.ConfigParser()
        else:
            config = ConfigParser.SafeConfigParser()
        config.optionxform = str
        for conf in self._get_conf_files():
            try:
                config.read(conf)
                section = 'options'
                self.read_section_options(config, section)
                for arg in iter(sys.argv[1:]):
                    if arg.startswith(":") and arg[1:] in config.sections():
                        section = arg[1:]
                        self.read_section_options(config, section)
            except ConfigParser.Error as error:
                lineno = str(error.lineno) if hasattr(error, 'lineno') else error.errors[0][0]
                sys.stderr.write("Failed to read configuration file '%s', line %s:\n%s\n"
                                 % (conf, lineno, str(error.message)))
                sys.exit(1)
            except ValueError as error:
                sys.stderr.write("Failed to read configuration file '%s':\n%s.\n" % (conf, error))
                sys.exit(1)

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
            if '.' not in key or key.rsplit(".")[1] not in self.metricspec:
                # New metric
                self.parse_new_verbose_metric(metrics, key, value)
            else:
                # Additional info
                key, spec = key.rsplit(".")
                if key not in metrics:
                    raise ValueError("Undeclared metric key %s" % key)
                self.parse_verbose_metric_info(metrics, key, spec, value)

    def prepare_metrics(self):
        """ Construct and prepare metricset """
        metrics = self.util.opts.pmGetOperands()
        if not metrics:
            sys.stderr.write("No metrics specified.\n")
            raise pmapi.pmUsageErr()

        def read_cmd_line_items():
            """ Helper to read command line items """
            tempmet = OrderedDict()
            for metric in metrics:
                if metric.startswith(":"):
                    tempmet[metric[1:]] = None
                else:
                    spec, insts = self.parse_metric_spec_instances(metric)
                    m = spec.split(",")
                    m[2] = insts
                    tempmet[m[0]] = m[1:]
            return tempmet

        # Metrics from different sources
        globmet = OrderedDict()
        confmet = OrderedDict()
        cmdlmet = read_cmd_line_items()
        sources = OrderedDict()

        # Read config
        # Python < 3.2 compat
        if sys.version_info[0] >= 3 and sys.version_info[1] >= 2:
            config = ConfigParser.ConfigParser()
            all_sets = ConfigParser.ConfigParser()
        else:
            config = ConfigParser.SafeConfigParser()
            all_sets = ConfigParser.SafeConfigParser()
        all_sets.optionxform = str
        config.optionxform = str
        for conf in self._get_conf_files():
            try:
                config.read(conf)
            except ConfigParser.Error as error:
                lineno = str(error.lineno) if hasattr(error, 'lineno') else error.errors[0][0]
                sys.stderr.write("Failed to read configuration file '%s', line %s:\n%s\n"
                                 % (conf, lineno, str(error.message)))
                sys.exit(1)

            # Read global metrics
            if self.util.globals == 1:
                if config.has_section('global'):
                    parsemet = OrderedDict()
                    for key in config.options('global'):
                        if key in self.util.keys:
                            sys.stderr.write("Failed to read configuration file ")
                            sys.stderr.write("'%s':\nSection [global] contains options.\n" % conf)
                            sys.exit(1)
                        if not config.get('global', key):
                            sys.stderr.write("Failed to read configuration file ")
                            sys.stderr.write("'%s':\nNo value set for %s in [global].\n" % (conf, key))
                            sys.exit(1)
                        try:
                            self.parse_metric_info(parsemet, key, config.get('global', key))
                        except ValueError as error:
                            sys.stderr.write("Failed to read configuration file ")
                            sys.stderr.write("'%s':\n" + str(error) % conf + ".\n")
                            sys.exit(1)
                    for metric in parsemet:
                        name = parsemet[metric][:1][0]
                        globmet[name] = parsemet[metric][1:]

            # Add latest metricsets to full configuration
            for section in config.sections():
                if all_sets.has_section(section):
                    all_sets.remove_section(section)
                all_sets.add_section(section)
                sources[section] = conf
                for key, value in config.items(section):
                    all_sets.set(section, key, value.replace('%', '%%'))
                if section not in ('options', 'global'):
                    config.remove_section(section)

        # Get details for configuration file metricsets
        for spec in cmdlmet:
            if cmdlmet[spec] is None:
                if all_sets.has_section(spec):
                    parsemet = OrderedDict()
                    for key in all_sets.options(spec):
                        if not all_sets.get(spec, key):
                            conf = sources[spec]
                            sys.stderr.write("Failed to read configuration file ")
                            sys.stderr.write("'%s':\nNo value set for %s in [%s].\n" % (conf, key, spec))
                            sys.exit(1)
                        if key not in self.util.keys:
                            try:
                                self.parse_metric_info(parsemet, key, all_sets.get(spec, key))
                            except ValueError as error:
                                conf = sources[spec]
                                sys.stderr.write("Failed to read configuration file ")
                                sys.stderr.write("'%s':\n" % conf + str(error) + ".\n")
                                sys.exit(1)
                    for metric in parsemet:
                        name = parsemet[metric][:1][0]
                        confmet[name] = parsemet[metric][1:]
                    cmdlmet[spec] = confmet

        # Check for metricsets not found
        for spec in cmdlmet:
            if cmdlmet[spec] is None:
                sys.stderr.write("Metricset definition ':%s' not found.\n" % spec)
                sys.exit(1)

        # Create combined metricset
        if self.util.globals == 1:
            for metric in globmet:
                self.util.metrics[metric] = globmet[metric]
        for metric in cmdlmet:
            if isinstance(cmdlmet[metric], list):
                self.util.metrics[metric] = cmdlmet[metric]
            else:
                if cmdlmet[metric]:
                    for m in cmdlmet[metric]:
                        self.util.metrics[m] = confmet[m]

        if not self.util.metrics:
            sys.stderr.write("No metrics specified.\n")
            sys.exit(1)

        self._conf_metrics = deepcopy(self.util.metrics)

    def provide_texts(self):
        """ Check if help texts requested """
        if hasattr(self.util, 'include_texts') and self.util.include_texts:
            return True
        return False

    def provide_labels(self):
        """ Check if labels needed """
        if hasattr(self.util, 'include_labels') and self.util.include_labels:
            return True
        return False

    def _dict_to_flat_list(self, d):
        """ Helper to flatten dict to list """
        items = []
        for k, v in d.items():
            if isinstance(v, dict):
                items.extend(self._dict_to_flat_list(v))
            else:
                items.append((k, v))
        return items

    def merge_labels(self, d1, d2):
        """ Helper to merge label dicts """
        d3 = d1.copy()
        d3.update(d2)
        return d3

    def get_labels_str(self, metric, inst=None, curr=True, combine=True):
        """ Return labels as string """
        if curr:
            ref = self.res_labels[metric]
        else:
            ref = self.labels[list(self.util.metrics.keys()).index(metric)]
        if inst in (None, pmapi.c_api.PM_IN_NULL):
            labels = ref[0]
        else:
            if curr:
                inst_labels = {} if inst not in ref[1] else ref[1][inst]
            else:
                inst_labels = {} if not ref[1] else ref[1][inst]
            if not combine:
                labels = inst_labels
                if not labels:
                    return "{}"
            else:
                metric_labels = ref[0]
                labels = self.merge_labels(metric_labels, inst_labels)
        return "{" + ','.join("'%s':'%s'" % (k, v) for (k, v) in self._dict_to_flat_list(labels)) + "}"

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

    def get_metric_indom(self, desc):
        """ Get instance domain for metric """
        if self.util.context.type == pmapi.c_api.PM_CONTEXT_ARCHIVE:
            return self.util.context.pmGetInDomArchive(desc)
        return self.util.context.pmGetInDom(desc)

    def get_inst_labels(self, indom, curr=True, insts=[]): # pylint: disable=dangerous-default-value
        """ Get instance labels """
        if indom == pmapi.c_api.PM_INDOM_NULL:
            return {} if curr else []
        if curr:
            return self.util.context.pmGetInstancesLabels(indom)
        inst_labels = []
        indom_labels = self.util.context.pmGetInstancesLabels(indom)
        for i in insts:
            inst_labels.append(indom_labels[i] if i in indom_labels else {})
        return inst_labels

    def get_proc_basename(self, proc):
        """ Get process basename """
        if proc.startswith('('):
            # Kernel thread
            proc = proc[1:-1]
        else:
            # User space process
            if proc.startswith('-'):
                proc = proc[1:]
            if proc.endswith(':'):
                proc = proc[:-1]
            proc = os.path.basename(proc)
        return proc

    def check_metric(self, metric):
        """ Validate individual metric and get its details """
        try:
            pmid = self.util.context.pmLookupName(metric)[0]
            if pmid in self.pmids:
                # Always ignore duplicates
                return
            desc = self.util.context.pmLookupDescs(pmid)[0]
            if desc.contents.indom == pmapi.c_api.PM_INDOM_NULL:
                inst = ([pmapi.c_api.PM_IN_NULL], [None])     # mem.util.free
            else:
                inst = self.get_metric_indom(desc)            # disk.dev.read
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
                    hit = False
                    try:
                        if r.isdigit():
                            msg = "Invalid instance"
                            if inst[1][0].split()[0].isdigit():
                                for i, s in enumerate(inst[1]):
                                    sp = s.split()[0]
                                    if sp.isdigit() and int(r) == int(sp):
                                        found[0].append(inst[0][i])
                                        found[1].append(inst[1][i])
                                        hit = True
                                        break
                        if r.replace('.', '').replace('_', '').replace('-', '').isalnum():
                            msg = "Invalid process"
                            if ' ' in inst[1][0] and inst[1][0].split()[0].isdigit():
                                for i, s in enumerate(inst[1]):
                                    if r == self.get_proc_basename(s.split()[1]):
                                        found[0].append(inst[0][i])
                                        found[1].append(inst[1][i])
                                        hit = True
                        if not hit:
                            msg = "Invalid regex"
                            cr = re.compile(r'\A' + r + r'\Z')
                            for i, s in enumerate(inst[1]):
                                if re.match(cr, s):
                                    found[0].append(inst[0][i])
                                    found[1].append(inst[1][i])
                            del cr
                    except Exception as error:
                        sys.stderr.write("%s '%s': %s.\n" % (msg, r, error))
                        sys.exit(1)
                if not found[0]:
                    return
                inst = tuple(found)
            self.pmids.append(pmid)
            self.descs.append(desc)
            self.insts.append(inst)
            if self.provide_texts():
                line, full, doml, domh = None, None, None, None
                try:
                    line = self.util.context.pmLookupText(pmid, pmapi.c_api.PM_TEXT_ONELINE)
                    full = self.util.context.pmLookupText(pmid, pmapi.c_api.PM_TEXT_HELP)
                    if desc.contents.indom != pmapi.c_api.PM_INDOM_NULL:
                        doml = self.util.context.pmLookupInDomText(desc, pmapi.c_api.PM_TEXT_ONELINE)
                        domh = self.util.context.pmLookupInDomText(desc, pmapi.c_api.PM_TEXT_HELP)
                except pmapi.pmErr as error:
                    if error.args[0] != pmapi.c_api.PM_ERR_TEXT:
                        raise
                self.texts.append([line, full, doml, domh])
            metric_labels = {}
            inst_labels = []
            ri_labels = {}
            if self.provide_labels():
                try:
                    metric_labels = self.util.context.pmLookupLabels(pmid)
                    inst_labels = self.get_inst_labels(desc.contents.indom, False, inst[0])
                    ri_labels = self.get_inst_labels(desc.contents.indom)
                except Exception:
                    pass
            self.labels.append([metric_labels, inst_labels])
            self.res_labels[metric] = [metric_labels, ri_labels]
        except pmapi.pmErr as error:
            if hasattr(self.util, 'ignore_incompat') and self.util.ignore_incompat:
                return
            sys.stderr.write("Invalid metric %s (%s).\n" % (metric, str(error)))
            sys.exit(1)

    def ignore_unknown_metrics(self):
        """ Check if unknown metrics are ignored """
        if hasattr(self.util, 'ignore_unknown') and self.util.ignore_unknown:
            return True
        return False

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

    def integer_roundup(self, value, upper):
        """ Round an integer value up to the nearest upper integer """
        return int(math.ceil(value / float(upper))) * upper

    def validate_common_options(self):
        """ Validate common utility options """
        try:
            err = "Integer expected"
            attr = "unknown"
            if hasattr(self.util, 'rank') and self.util.rank:
                attr = 'rank'
                self.util.rank = int(self.util.rank)
            if hasattr(self.util, 'limit_filter') and self.util.limit_filter:
                attr = 'limit_filter'
                self.util.limit_filter = int(self.util.limit_filter)
            if hasattr(self.util, 'limit_filter_force') and self.util.limit_filter_force:
                attr = 'limit_filter_force'
                self.util.limit_filter_force = int(self.util.limit_filter_force)
            err = "Non-negative integer expected"
            if hasattr(self.util, 'width') and self.util.width:
                attr = 'width'
                self.util.width = int(self.util.width)
                if self.util.width < 0:
                    raise ValueError(err)
            if hasattr(self.util, 'width_force') and self.util.width_force:
                attr = 'width_force'
                self.util.width_force = int(self.util.width_force)
                if self.util.width_force < 0:
                    raise ValueError(err)
            if hasattr(self.util, 'precision') and self.util.precision:
                attr = 'precision'
                self.util.precision = int(self.util.precision)
                if self.util.precision < 0:
                    raise ValueError(err)
            if hasattr(self.util, 'precision_force') and self.util.precision_force:
                attr = 'precision_force'
                self.util.precision_force = int(self.util.precision_force)
                if self.util.precision_force < 0:
                    raise ValueError(err)
            if hasattr(self.util, 'repeat_header') and self.util.repeat_header:
                attr = 'repeat_header'
                if self.util.repeat_header != "auto":
                    self.util.repeat_header = int(self.util.repeat_header)
                    if self.util.repeat_header < 0:
                        raise ValueError(err)
        except ValueError:
            sys.stderr.write("Error while reading option %s: %s.\n" % (attr, err))
            sys.exit(1)

    def validate_metrics(self, curr_insts=CURR_INSTS, max_insts=0):
        """ Validate the metricset """
        # Check the metrics against PMNS, resolve non-leaf metrics

        if not hasattr(self.util, 'leaf_only'):
            self.util.metrics = deepcopy(self._conf_metrics)

        if hasattr(self.util, 'predicate') and self.util.predicate:
            for predicate in self.util.predicate.split(","):
                if predicate not in self.util.metrics:
                    self.util.metrics[predicate] = ['', []]

        if self.util.derived:
            for derived in filter(None, self.util.derived.split(";")):
                if derived.startswith("/") or derived.startswith("."):
                    try:
                        self.util.context.pmLoadDerivedConfig(derived)
                    except pmapi.pmErr as error:
                        sys.stderr.write("Failed to load derived metric definitions ")
                        sys.stderr.write("from file '%s':\n%s.\n" % (derived, str(error)))
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
                            sys.stderr.write("Failed to register derived metric:\n%s.\n" % err)
                            sys.exit(1)

        if not hasattr(self.util, 'leaf_only') or not self.util.leaf_only:
            # Prepare for non-leaf metrics while preserving metric order
            metrics = self.util.metrics
            self.util.metrics = OrderedDict()

            def metric_base_check(metric):
                """ Helper to support non-leaf metricspecs """
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
                    if error.args[0] != pmapi.c_api.PM_ERR_NAME:
                        raise
                    # Ignore unknown metrics if so requested
                    ignore = False
                    try:
                        self.util.context.pmLookupName(metric)
                    except pmapi.pmErr as error:
                        if error.args[0] != pmapi.c_api.PM_ERR_NAME:
                            raise
                        if self.ignore_unknown_metrics() and metric in self._conf_metrics:
                            ignore = True
                    if not ignore:
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
        if not self.insts and not self.ignore_unknown_metrics():
            sys.stderr.write("No matching instances found.\n")
            # Try to help the user to get the instance specifications right
            if self.util.instances:
                print("\nRequested global instances:")
                print(self.util.instances)
            sys.exit(1)

        # Dynamically set fetchgroup max instances
        # if not specified explicitly by the caller
        dynamic_insts = not max_insts

        # Finalize metricset
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

            if dynamic_insts:
                max_insts = self.integer_roundup(len(self.insts[i][0]), 1000)

            # Rawness
            if hasattr(self.util, 'type_prefer') and not self.util.metrics[metric][3]:
                self.util.metrics[metric][3] = self.util.type_prefer
            elif self.util.metrics[metric][3] == 'raw':
                self.util.metrics[metric][3] = 1
            else:
                self.util.metrics[metric][3] = 0
            # Force raw output with archive mode of any tool in order to
            # create pmlogger(1) compatible archives that can be merged.
            if (hasattr(self.util, 'type') and self.util.type == 1) or \
               self.util.metrics[metric][3] == 'raw' or \
               (hasattr(self.util, 'output') and self.util.output == 'archive'):
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

            # Force native units with archive mode of any tool in order to
            # create pmlogger(1) compatible archives that can be merged.
            if hasattr(self.util, 'output') and self.util.output == 'archive':
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
                        try:
                            item = self.util.pmfg.extend_item(metric, mtype, scale, self.insts[i][1][j])
                            items.append((self.insts[i][0][j], self.insts[i][1][j], item))
                            mitems += 1
                        except pmapi.pmErr as error:
                            if error.args[0] == pmapi.c_api.PM_ERR_CONV:
                                raise
                            vanished.append(j)
                    if mitems > 0:
                        for v in reversed(vanished):
                            del self.insts[i][0][v]
                            del self.insts[i][1][v]
                            if self.provide_labels():
                                del self.labels[i][1][v]
                    self.util.metrics[metric][5] = self.pmfg_items_to_indom(items)
                else:
                    self.util.metrics[metric][5] = \
                        self.util.pmfg.extend_indom(metric, mtype, scale, max_insts)

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
            if self.provide_texts():
                del self.texts[incompat_metrics[metric]]
            if self.provide_labels():
                del self.labels[incompat_metrics[metric]]
            del self.util.metrics[metric]
        del incompat_metrics

        # Verify that we have valid metrics
        if not self.util.metrics:
            if not self.ignore_unknown_metrics():
                sys.stderr.write("No compatible metrics found.\n")
            else:
                sys.stderr.write("Not one known metric found.\n")
            sys.exit(1)

        if hasattr(self.util, 'predicate') and self.util.predicate:
            self.validate_predicate()

    def finalize_options(self):
        """ Finalize util options """
        # Runtime overrides samples/interval
        if self.util.opts.pmGetOptionFinishOptarg():
            if self.util.opts.pmGetOptionOrigin() is None:
                origin = 0
            else:
                origin = float(self.util.opts.pmGetOptionOrigin())
            self.util.runtime = float(self.util.opts.pmGetOptionFinish()) - origin
            if self.util.opts.pmGetOptionSamples():
                self.util.samples = self.util.opts.pmGetOptionSamples()
                self.util.samples = max(2, self.util.samples)
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

    def clear_metrics(self):
        """ Clear metricset """
        self.util.metrics = OrderedDict()
        self.pmids = []
        self.descs = []
        self.insts = []
        self.texts = []
        self.labels = []
        self.res_labels = OrderedDict()
        self.util.pmfg.clear()
        self.util.pmfg_ts = None

    def update_metrics(self, curr_insts=CURR_INSTS, max_insts=0):
        """ Update metricset """
        self.clear_metrics()
        self.util.pmfg_ts = self.util.pmfg.extend_timestamp()
        self.validate_metrics(curr_insts, max_insts)

    def names_change_action(self):
        """ Action to take when namespace change occurs:
            ignore=0, abort=1, update=2 """
        if hasattr(self.util, 'names_change'):
            return self.util.names_change
        return 0 # By default ignore name change notification from pmcd(1)

    def fetch(self):
        """ Sample using fetchgroup and handle special cases """
        try:
            state = self.util.pmfg.fetch()
        except pmapi.pmErr as error:
            if error.args[0] == pmapi.c_api.PM_ERR_EOL:
                return -1
            if error.args[0] == pmapi.c_api.PM_ERR_TOOSMALL:
                raise pmapi.pmErr(pmapi.c_api.PM_ERR_TOOSMALL,
                                  "\nNo metrics or instances to report present.")
            raise error

        # Watch for end time in uninterpolated mode
        if not self.util.interpol:
            sample = self.util.pmfg_ts().strftime('%s')
            finish = self.util.opts.pmGetOptionFinish()
            if float(sample) > float(finish):
                return -2

        # Handle any PMCD state change notification
        if state & pmapi.c_api.PMCD_NAMES_CHANGE:
            action = self.names_change_action()
            if action == 1:
                return -3
            elif action == 2:
                return 1

        # Successfully completed sampling
        return 0

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

        for r in self.util.metrics[metric][1]:
            if r.replace('.', '').replace('_', '').replace('-', '').isalnum():
                if ' ' in name and name.split()[0].isdigit():
                    if r == self.get_proc_basename(name.split()[1]):
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
        revs = bool(self.util.rank > 0)
        return sorted(instances, key=lambda value: value[2], reverse=revs)[:rank]

    def validate_predicate(self):
        """ Validate predicate filter reference metrics """
        for predicate in self.util.predicate.split(","):
            if predicate not in self.util.metrics:
                sys.stderr.write("Predicate metric %s filtered out.\n" % predicate)
                sys.exit(1)

            i = list(self.util.metrics.keys()).index(predicate)
            self._pred_indom.append(self.descs[i].contents.indom)

            if self.insts[i][0][0] == pmapi.c_api.PM_IN_NULL:
                sys.stderr.write("Predicate metric must have instances.\n")
                sys.exit(1)

            if self.descs[i].contents.type == pmapi.c_api.PM_TYPE_STRING:
                sys.stderr.write("Predicate metric values must be numeric.\n")
                sys.exit(1)

    # Deprecated, use get_ranked_results() below instead
    def get_sorted_results(self, valid_only=False):
        """ Deprecated, use get_ranked_results() instead """
        return self.get_ranked_results(valid_only)

    def get_ranked_results(self, valid_only=False):
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

            if valid_only and not results[metric]:
                del results[metric]

        if self.provide_labels():
            insts = [(metric, list(zip(*results[metric]))[0]) for metric in results if results[metric]]
            if self._prev_insts != insts:
                prev_labels = self.res_labels
                self.res_labels = OrderedDict()
                self._prev_insts = insts
                for metric in results:
                    ri_labels = None
                    if metric in prev_labels:
                        metric_labels = prev_labels[metric][0]
                        prev_insts = prev_labels[metric][1].keys()
                        curr_insts = list(zip(*results[metric]))[0] if results[metric] else {}
                        if all(inst in prev_insts for inst in curr_insts):
                            ri_labels = prev_labels[metric][1]
                    else:
                        i = list(self.util.metrics.keys()).index(metric)
                        metric_labels = self.util.context.pmLookupLabels(self.pmids[i])
                    if ri_labels is None:
                        ri_labels = self.get_inst_labels(self.descs[i].contents.indom)
                    self.res_labels[metric] = [metric_labels, ri_labels]

        if not results:
            return results

        if predicates:
            pred_insts = {}
            for i, predicate in enumerate(predicates):
                results[predicate] = self.rank(results[predicate])
                p = self._pred_indom[i]
                if p not in pred_insts:
                    pred_insts[p] = []
                pred_insts[p].extend(x[0] for x in results[predicate] if x[0] not in pred_insts[p])
            for metric in results:
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
                i = list(self.util.metrics.keys()).index(metric)
                if self.descs[i].contents.indom not in self._pred_indom:
                    results[metric] = self.rank(results[metric])
                    continue
                inst_index = self.descs[i].contents.indom
                results[metric] = [x for x in results[metric] if x[0] in pred_insts[inst_index]]
        else:
            if hasattr(self.util, 'rank') and self.util.rank:
                for metric in results:
                    results[metric] = self.rank(results[metric])

        if self.do_live_filtering() and self.do_invert_filtering():
            for metric in results:
                results[metric] = [x for x in results[metric] if self.filter_instance(metric, x[1])]

        return results
