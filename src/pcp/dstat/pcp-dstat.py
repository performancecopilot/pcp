#!/usr/bin/env pmpython
#
# Copyright (C) 2018-2022 Red Hat.
# Copyright (C) 2004-2016 Dag Wieers <dag@wieers.com>
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
# pylint: disable=missing-docstring,multiple-imports,invalid-name
# pylint: disable=too-many-arguments,too-many-positional-arguments
# pylint: disable=too-many-lines,too-many-nested-blocks,line-too-long
# pylint: disable=global-variable-undefined,global-at-module-level
# pylint: disable=bad-continuation,broad-except,bare-except

# Common imports
from collections import OrderedDict
try:
    import configparser as ConfigParser
except ImportError:
    import ConfigParser
import termios, struct, atexit, fcntl, sched, errno, time, re, sys, os

# PCP Python PMAPI
from pcp import pmapi, pmconfig
from cpmapi import PM_CONTEXT_ARCHIVE, PM_CONTEXT_HOST, PM_CONTEXT_LOCAL
from cpmapi import PM_TYPE_32, PM_TYPE_U32, PM_TYPE_64, PM_TYPE_U64
from cpmapi import PM_TYPE_DOUBLE, PM_TYPE_FLOAT, PM_TIME_MIN, PM_TIME_HOUR
from cpmapi import PM_TIME_NSEC, PM_TIME_USEC, PM_TIME_MSEC
from cpmapi import PM_ERR_EOL, PM_IN_NULL, pmUsageMessage

if sys.version >= '3':
    long = int

def py3round(number, ndigits=None):
    if sys.version >= '3':
        if ndigits is not None:
            return round(number, ndigits)
        return round(number)
    ndigits = ndigits if ndigits is not None else 0
    if abs(round(number) - number) == 0.5:
        return 2.0 * round(number / 2.0, ndigits)
    return round(number, ndigits)

TIMEFMT = os.getenv('DSTAT_TIMEFMT') or '%d-%m %H:%M:%S'

NOUNITS = pmapi.pmUnits()

THEME = {'default': ''}

COLOR = {
    'black': '\033[0;30m',
    'darkred': '\033[0;31m',
    'darkgreen': '\033[0;32m',
    'darkyellow': '\033[0;33m',
    'darkblue': '\033[0;34m',
    'darkmagenta': '\033[0;35m',
    'darkcyan': '\033[0;36m',
    'gray': '\033[0;37m',

    'darkgray': '\033[90m',
    'red': '\033[1;31m',
    'green': '\033[1;32m',
    'yellow': '\033[1;33m',
    'blue': '\033[1;34m',
    'magenta': '\033[1;35m',
    'cyan': '\033[1;36m',
    'white': '\033[1;37m',

    'blackbg': '\033[40m',
    'redbg': '\033[41m',
    'greenbg': '\033[42m',
    'yellowbg': '\033[43m',
    'bluebg': '\033[44m',
    'magentabg': '\033[45m',
    'cyanbg': '\033[46m',
    'whitebg': '\033[47m',
}

ANSI = {
    'reset': '\033[0;0m',
    'bold': '\033[1m',
    'reverse': '\033[2m',
    'underline': '\033[4m',

    'clear': '\033[2J',
    'clearline': '\033[2K',
    'save': '\033[s',
    'restore': '\033[u',
    'nolinewrap': '\033[7l',

    'default': '\033[0;0m',
}

CHAR = {
    'pipe': '|',
    'colon': ':',
    'gt': '>',
    'space': ' ',
    'dash': '-',
    'plus': '+',
    'underscore': '_',
    'sep': ',',
}

class DstatTerminal:
    """Manage aspects of querying and manipulating the output terminal"""

    def __init__(self):
        self.termsize = None, 0
        try:
            termios.TIOCGWINSZ
        except:
            try:
                import curses # pylint: disable=import-outside-toplevel
                curses.setupterm()
                curses.tigetnum('lines')
                curses.tigetnum('cols')
            except:
                pass
            else:
                self.termsize = None, 2
        else:
            self.termsize = None, 1

    def get_size(self):
        """Return the dynamic terminal geometry"""
        if not self.termsize[0]:
            try:
                import curses # pylint: disable=import-outside-toplevel
                if self.termsize[1] == 1:
                    s = struct.pack('HHHH', 0, 0, 0, 0)
                    x = fcntl.ioctl(sys.stdout.fileno(), termios.TIOCGWINSZ, s)
                    return struct.unpack('HHHH', x)[:2]
                elif self.termsize[1] == 2:
                    curses.setupterm()
                    return curses.tigetnum('lines'), curses.tigetnum('cols')
                else:
                    self.termsize = (int(os.environ['LINES']), int(os.environ['COLUMNS']))
            except:
                self.termsize = 25, 80
        return self.termsize

    def get_color(self):
        """Return whether the system can use colors or not"""
        if sys.stdout.isatty():
            try:
                import curses # pylint: disable=import-outside-toplevel
                curses.setupterm()
                if curses.tigetnum('colors') < 0:
                    return False
            except ImportError:
                sys.stderr.write('Color support is disabled as python-curses is not installed.')
                return False
            except:
                sys.stderr.write('Color support is disabled as curses does not find terminal "%s".' % os.getenv('TERM'))
                return False
            return True
        return False

    @staticmethod
    def set_title(arguments, context):
        """ Write terminal title, if terminal (and shell?) is capable """
        if not sys.stdout.isatty():
            return
        term = os.getenv('TERM')
        if not term:
            return
        shell = os.getenv('SHELL')
        xshell = os.getenv('XTERM_SHELL')
        if not shell:
            shell = xshell
        if not shell or shell != '/bin/bash':
            return
        if term[:5] != 'xterm' and term[:6] != 'screen':
            return
        import getpass # pylint: disable=import-outside-toplevel
        user = getpass.getuser()
        host = context.pmGetContextHostName()
        host = host.split('.')[0]
        path = context.pmProgname()
        args = path + ' ' + ' '.join(arguments)
        sys.stdout.write('\033]0;(%s@%s) %s\007' % (user, host, args))

    @staticmethod
    def set_theme(blackonwhite):
        THEME['title'] = COLOR['darkblue']
        THEME['frame'] = COLOR['darkblue']
        THEME['default'] = ANSI['default']
        THEME['error'] = COLOR['white'] + COLOR['redbg']
        THEME['debug'] = COLOR['darkred']
        THEME['input'] = COLOR['darkgray']
        THEME['roundtrip'] = COLOR['darkblue']
        THEME['text_hi'] = COLOR['darkgray']
        THEME['unit_hi'] = COLOR['darkgray']
        if blackonwhite:
            THEME['subtitle'] = COLOR['darkcyan'] + ANSI['underline']
            THEME['done_lo'] = COLOR['black']
            THEME['done_hi'] = COLOR['darkgray']
            THEME['text_lo'] = COLOR['black']
            THEME['unit_lo'] = COLOR['black']
            THEME['unit_hi'] = COLOR['darkgray']
            THEME['colors_lo'] = (COLOR['darkred'], COLOR['darkmagenta'],
                    COLOR['darkgreen'], COLOR['darkblue'], COLOR['darkcyan'],
                    COLOR['black'], COLOR['red'], COLOR['green'])
            THEME['colors_hi'] = (COLOR['red'], COLOR['magenta'],
                    COLOR['green'], COLOR['blue'], COLOR['cyan'],
                    COLOR['darkgray'], COLOR['darkred'], COLOR['darkgreen'])
        else:
            THEME['subtitle'] = COLOR['blue'] + ANSI['underline']
            THEME['done_lo'] = COLOR['white']
            THEME['done_hi'] = COLOR['gray']
            THEME['text_lo'] = COLOR['gray']
            THEME['unit_lo'] = COLOR['darkgray']
            THEME['colors_lo'] = (COLOR['red'], COLOR['yellow'],
                    COLOR['green'], COLOR['blue'], COLOR['cyan'],
                    COLOR['white'], COLOR['darkred'], COLOR['darkgreen'])
            THEME['colors_hi'] = (COLOR['darkred'], COLOR['darkyellow'],
                    COLOR['darkgreen'], COLOR['darkblue'], COLOR['darkcyan'],
                    COLOR['gray'], COLOR['red'], COLOR['green'])


class DstatPlugin(object):
    """ Performance metrics group, for generating reports on one or
        more performance metrics (term/CSV) using pmConfig services.
    """
    def __init__(self, label):
        #sys.stderr.write("New plugin: %s\n" % label)
        self.name = label   # name of this plugin (config section)
        self.label = label
        self.instances = None
        self.unit = None
        self.type = None
        self.width = 5
        self.precision = None
        self.limit = None
        self.printtype = None
        self.colorstep = None
        self.grouptype = None   # flag for metric/inst group displays
        self.cullinsts = None   # regex pattern for dropped instances
        self.valuesets = {} # dict of sample values within one 'delay'
        self.metrics = []   # list of all metrics (states) for plugin
        self.names = []     # list of all column names for the plugin
        self.mgroup = []    # list of names of metrics in this plugin
        self.igroup = []    # list of names of this plugins instances

    def apply(self, metric):
        """ Apply default pmConfig list values where none exist as yet
            The indices are based on the extended pmConfig.metricspec.
            Note: we keep the plugin name for doing reverse DstatPlugin
            lookups from pmConfig metricspecs at fetch (sampling) time.
        """
        # slot zero is magic - holds metric name (during setup only)
        if metric[1] is None:
            metric[1] = self.label
        if metric[2] == []:
            metric[2] = self.instances
        if metric[3] is None:
            metric[3] = self.unit
        if metric[4] is None:
            metric[4] = self.type
        if metric[5] is None:
            metric[5] = self.width
        # slot six also magic - fetchgroup state
        if metric[7] is None:
            metric[7] = self.precision
        if metric[8] is None:
            metric[8] = self.limit
        if metric[9] is None:
            metric[9] = self.printtype
        if metric[10] is None:
            metric[10] = self.colorstep
        if metric[11] is None:
            metric[11] = self.grouptype
        if metric[12] is None:
            metric[12] = self.cullinsts
        metric[13] = self   # back-pointer to this from metric dict
        metric[14] = self.valuesets  # recent samples for averaging

    def prepare_grouptype(self, instlist, fullinst):
        """Setup a list of instances from the command line"""
        if fullinst:
            self.grouptype = 1
            instlist = []
        elif instlist is None:
            instlist = ['total']
        if 'total' in instlist:
            self.grouptype = 2 if (len(instlist) == 1) else 3
            instlist.remove('total') # remove command line arg
        else:
            self.grouptype = 1
        self.instances = instlist

    def statwidth(self):
        """Return complete width for this plugin"""
        if self.grouptype == 4:
            return self.width
        return len(self.names) * self.width + len(self.names) - 1

    def instlist(self):
        if self.grouptype == 3:
            return self.igroup + ['total']
        elif self.grouptype == 2:
            return ['total']
        return self.igroup

    def title(self):
        if self.grouptype is None or self.grouptype == 4:
            width = self.statwidth()
            label = self.label[0:width].center(width).replace(' ', '-')
            return THEME['title'] + label + THEME['default']
        ret = ''
        ilist = self.instlist()
        for i, name in enumerate(ilist):
            if name is None:
                continue
            name = self.label.replace('%I', name)
            width = self.statwidth()
            label = name[0:width].center(width).replace(' ', '-')
            ret = ret + THEME['title'] + label
            if i + 1 != len(ilist):
                if op.color:
                    ret = ret + THEME['frame'] + CHAR['dash'] + THEME['title']
                else:
                    ret = ret + CHAR['space']
        return ret + THEME['default']

    def subtitle(self):
        ret = ''
        if self.grouptype is None or self.grouptype == 4:
            for i, nick in enumerate(self.names):
                label = nick[0:self.width].center(self.width)
                ret = ret + THEME['subtitle'] + label + THEME['default']
                if self.grouptype == 4:
                    break
                if i + 1 != len(self.names):
                    ret = ret + CHAR['space']
            return ret
        ilist = self.instlist()
        for i, _ in enumerate(ilist):
            for j, nick in enumerate(self.names):
                label = nick[0:self.width].center(self.width)
                ret = ret + THEME['subtitle'] + label + THEME['default']
                if self.grouptype == 4:  # top
                    return ret
                if j + 1 != len(self.names):
                    ret = ret + CHAR['space']
            if i + 1 != len(ilist):
                ret = ret + THEME['frame'] + CHAR['colon']
        return ret

    def csvtitle(self):
        if self.grouptype is None or self.grouptype == 4:
            ret = '"' + self.label
            if self.grouptype is None:
                return ret + '"' + CHAR['sep'] * (len(self.names) - 1)
            return ret + ' ' + self.names[0] + '"' + CHAR['sep'] * (len(self.names) - 2)
        ret = ''
        ilist = self.instlist()
        for i, name in enumerate(ilist):
            if name is None:
                continue
            name = self.label.replace('%I', name)
            if i > 0:
                ret = ret + CHAR['sep']
            ret = ret + '"' + name  + '"'
            for j, _ in enumerate(self.names):
                if j > 0:
                    ret = ret + CHAR['sep']
        return ret

    def csvsubtitle(self):
        ret = ''
        if self.grouptype is None:
            for i, nick in enumerate(self.names):
                if i > 0:
                    ret = ret + CHAR['sep']
                ret = ret + '"' + nick + '"'
            return ret
        if self.grouptype == 4:
            for i, nick in enumerate(self.names):
                if i == 0:  # sort key
                    continue
                if i > 1:
                    ret = ret + CHAR['sep']
                ret = ret + '"' + nick + '"'
            return ret
        ilist = self.instlist()
        for i, name in enumerate(ilist):
            if name is None:
                continue
            name = self.label.replace('%I', name)
            for j, nick in enumerate(self.names):
                if j > 0 or i > 0:
                    ret = ret + CHAR['sep']
                ret = ret + '"' + name + CHAR['colon'] + nick + '"'
                if self.grouptype == 4:  # top
                    return ret
        return ret


class DstatTimePlugin(DstatPlugin):
    def __init__(self, name, label, width):
        DstatPlugin.__init__(self, name)
        self.label = label
        self.width = width


class DstatTool(object):
    """ PCP implementation of the classic Dstat utility """

    # Default configuration file directories
    DEFAULT_CONFIGS = ["$PCP_SYSCONF_DIR/dstat", "$HOME/.pcp/dstat"]
    CONFIG_VERSION = 1

    # Defaults
    VERSION = '1.0.0'   # Dstat version
    TIMEFMT = "yyyy-mm-dd hh:mm:ss"

    def __init__(self, arguments):
        """ Construct object, prepare for command line handling """
        global op
        op = self

        # Avoid tracedump with --version and non-existing --archive
        if '--version' in arguments and 'PCP_ARCHIVE' in os.environ:
            if not os.path.exists(os.environ['PCP_ARCHIVE']) and \
               not os.path.exists(os.environ['PCP_ARCHIVE'] + '.index'):
                sys.stderr.write(os.path.basename(sys.argv[0]))
                sys.stderr.write(": No such file or directory\n")
                sys.exit(1)

        self.inittime = time.time()
        self.context = None
        self.opts = self.options()
        self.arguments = arguments
        self.pmconfig = pmconfig.pmConfig(self)

        ### Add additional dstat metric specifiers
        dspec = (None, 'printtype', 'colorstep', 'grouptype', 'cullinsts',
                'plugin', 'valuesets', 'filter')
        mspec = self.pmconfig.metricspec + dspec
        self.pmconfig.metricspec = mspec

        ### Add global dstat configuration directives
        self.keys = ('header', 'interval', 'timefmt')

        ### The order of preference for options (as present):
        # 1 - command line options
        # 2 - options from configuration file(s)
        # 3 - built-in defaults defined below
        self.check = 0
        self.version = self.CONFIG_VERSION
        self.source = "local:"
        self.instances = None
        self.speclocal = None
        self.derived = None
        self.globals = 1
        self.samples = -1 # forever
        self.interval = pmapi.timeval(1)      # 1 sec
        self.opts.pmSetOptionInterval(str(1)) # 1 sec
        self.delay = 1.0
        self.type = 0
        self.type_prefer = self.type
        self.ignore_incompat = 0
        self.precision = 5 # .5f
        self.timefmt = self.TIMEFMT
        self.interpol = 1
        self.leaf_only = True

        # Internal
        self.missed = 0
        self.nomissed = False # report missed ticks by default
        self.runtime = -1
        self.plugins = []     # list of requested plugin names
        self.allplugins = []  # list of all known plugin names
        self.timeplugins = [] # list of the time plugin names
        self.timelist = []    # DstatPlugin time objects list
        self.totlist = []     # active DstatPlugin object list
        self.vislist = []     # visible DstatPlugin object list
        self.mapping = {}     # maps 'section/label' to plugin
        self.novalues = True  # values observed for this line

        self.full = False
        self.bits = False
        self.blackonwhite = False
        self.color = None
        self.debug = False
        self.verify = False
        self.show_conf = False
        self.header = 1
        self.output = False
        self.update = True
        self.pidfile = False
        self.float = False
        self.integer = False

        # Options for specific plugins
        self.cpulist = None
        self.disklist = None
        self.dmlist = None
        self.mdlist = None
        self.partlist = None
        self.intlist = None
        self.netlist = None
        self.netpacketlist = None
        self.swaplist = None

        ### Implicit if no terminal is used
        if not sys.stdout.isatty():
            self.color = False
            self.header = False
            self.update = False

        atexit.register(self.finalize)

        # Performance metrics store
        # key    - plugin/metric name
        # values - 0:text label, 1:instance(s), 2:unit/scale, 3:type,
        #          4:width, 5:pmfg item, 6:precision, 7:limit,
        #          [ 8:printtype, 9:colorstep, 10:grouptype, 11:cullinsts,
        #           12:plugin, 13:valuesets <- Dstat extras ]
        self.metrics = OrderedDict()
        self.pmfg = None
        self.pmfg_ts = None

        ### Initialise output device
        self.term = DstatTerminal()

        ### Read configuration and initialise plugins
        configs = self.prepare_plugins()
        self.create_time_plugins()

        ### Complete command line processing
        self.pmconfig.read_cmd_line()
        self.prepare_metrics(configs)
        if self.verify:
            sys.exit(0)

        ### Setup PMAPI context, console and optionally file
        self.connect()
        self.validate()
        self.prepare_output()

    def prepare_output(self):
        """ Complete all initialisation and get ready to begin sampling """
        self.pmconfig.set_signal_handler()
        self.term.set_title(self.arguments, self.context)
        self.term.set_theme(self.blackonwhite)
        if self.color is None:
            self.color = self.term.get_color()

        ### Empty ansi and theme databases when colors not in use
        if not self.color:
            for key in list(COLOR.keys()):
                COLOR[key] = ''
            for key in list(THEME.keys()):
                THEME[key] = ''
            for key in list(ANSI.keys()):
                ANSI[key] = ''
            THEME['colors_hi'] = (ANSI['default'],)
            THEME['colors_lo'] = (ANSI['default'],)

        ### Disable line-wrapping
        sys.stdout.write(ANSI['nolinewrap'])

        ### Create pidfile
        if self.pidfile:
            self.create_pidfile()

    def create_pidfile(self):
        try:
            with open(self.pidfile, 'w', 0) as pidfile:
                pidfile.write(str(os.getpid()))
                pidfile.close()
        except Exception as e:
            sys.stderr.write('Failed to create pidfile %s\n%s\n' % (self.pidfile, e))
            self.pidfile = False

    def create_time_plugins(self):
        timefmtlen = len(time.strftime(TIMEFMT, time.localtime()))
        timer = DstatTimePlugin('time', 'system', timefmtlen)
        timeradv = DstatTimePlugin('time-adv', 'system', timefmtlen + 4)
        epoch = DstatTimePlugin('epoch', 'epoch', 10)
        epochadv = DstatTimePlugin('epoch-adv', 'epoch', 14)
        names = [timer.name, timeradv.name, epoch.name, epochadv.name]
        self.timelist = [timer, timeradv, epoch, epochadv]
        self.timeplugins = names
        self.allplugins.append(names)

    def prepare_regex(self, value):
        try:
            value = re.compile(r'\A' + value + r'\Z')
        except Exception as reerr:
            sys.stderr.write("Invalid regex '%s': %s.\n" % (value, reerr))
            sys.exit(1)
        return value

    def prepare_plugins(self):
        paths = self.config_files(self.DEFAULT_CONFIGS)
        if not paths:
            sys.stderr.write("No configs found in: %s\n" % self.DEFAULT_CONFIGS)
            sys.exit(1)

        config = ConfigParser.RawConfigParser()
        config.optionxform = str
        try:
            found = config.read(paths)
        except ConfigParser.Error as cfgerr:
            sys.stderr.write("Config parse failure: %s\n" % cfgerr.message())
            sys.exit(1)
        except Exception:
            sys.stderr.write("Cannot parse configs in %s\n" % paths)
            sys.exit(1)

        if self.debug:
            print("Found configs: %s" % found)
            print("with sections: %s" % config.sections())

        for plugin in config.sections():
            self.allplugins.append(plugin)
            self.opts.pmSetLongOption(plugin, 0, '', '', '')
        return config

    def prepare_metrics(self, config):
        """ Using the list of requested plugins, prepare for sampling """

        # If no plugins were requested, or if all requested plugins
        # are displaying only time, add the default reporting stats.
        timelen = 0
        for section in self.plugins:
            if section in self.timeplugins:
                timelen += 1
        if not self.plugins or (timelen > 0 and timelen == len(self.plugins)):
            print('You did not select any stats, using -cdngy by default.')
            self.plugins += ['cpu', 'disk', 'net', 'page', 'sys']

        lib = self.pmconfig
        for section in self.plugins:
            metrics = OrderedDict()
            if section in self.timeplugins:
                index = self.timeplugins.index(section)
                plugin = self.timelist[index]
                name = 'dstat.' + section + '.' + plugin.name # metric name
                value = '0' # constant expression, always valid as a metric
                lib.parse_new_verbose_metric(metrics, name, name)
                lib.parse_verbose_metric_info(metrics, name, 'formula', value)
                lib.parse_verbose_metric_info(metrics, name, 'label', section)
            elif not config.has_section(section):
                sys.stderr.write("Ignoring unknown plugin '%s'\n" % section)
                continue
            else:
                plugin = DstatPlugin(section)

                for key in config.options(section):
                    value = config.get(section, key)
                    if key in lib.metricspec:
                        if self.debug:
                            print("Default %s %s -> %s" % (section, key, value))
                        if key in ['width', 'precision', 'limit', 'grouptype']:
                            value = int(value)
                        elif key in ['printtype']:
                            value = value[0]    # first character suffices
                        elif key in ['colorstep']:
                            value = float(value)
                        elif key in ['instances']:
                            value = lib.parse_instances(value)
                        elif key in ['cullinsts']:
                            value = self.prepare_regex(value)
                        setattr(plugin, key, value)
                    else:
                        if '.' in key:
                            mkey, spec = key.split(".")
                        else:
                            mkey, spec = key, 'formula'
                        name = 'dstat.' + section + '.' + mkey  # metric name
                        if name not in metrics:
                            lib.parse_new_verbose_metric(metrics, name, name)
                            if spec != 'label':
                                lib.parse_verbose_metric_info(metrics, name, 'label', mkey)
                        lib.parse_verbose_metric_info(metrics, name, spec, value)

                    if key in ['filter']:
                        if value == 'cpu':
                            plugin.prepare_grouptype(self.cpulist, self.full)
                        elif value == 'disk':
                            plugin.prepare_grouptype(self.disklist, self.full)
                        elif value == 'dm':
                            plugin.prepare_grouptype(self.dmlist, self.full)
                        elif value == 'md':
                            plugin.prepare_grouptype(self.mdlist, self.full)
                        elif value == 'part':
                            plugin.prepare_grouptype(self.partlist, self.full)
                        elif value == 'int':
                            plugin.prepare_grouptype(self.intlist, self.full)
                        elif value == 'net':
                            plugin.prepare_grouptype(self.netlist, self.full)
                        elif value == 'net-packets':
                            plugin.prepare_grouptype(self.netpacketlist, self.full)
                        elif value == 'swap':
                            plugin.prepare_grouptype(self.swaplist, self.full)

            for metric in metrics:
                name = metrics[metric][0]
                #print("Plugin[%s]: %s" % (name, metrics[metric]))
                plugin.apply(metrics[metric])
                state = metrics[metric][1:]
                plugin.metrics.append(state)
                self.metrics[name] = state
                #print("Appended[%s]: %s" % (name, state))

            self.totlist.append(plugin)

    def finalize_options(self):
        operands = self.opts.pmGetOperands()
        if not operands:
            operands = []
        else:
            try:
                self.interval = pmapi.timeval.fromInterval(operands[0])
                self.delay = float(self.interval)
            except:
                sys.stderr.write("Invalid sample delay '%s'\n" % operands[0])
                sys.exit(1)
        if len(operands) > 1:
            try:
                self.samples = int(operands[1]) + 1
            except:
                sys.stderr.write("Invalid sample count '%s'\n" % operands[1])
                sys.exit(1)
        if len(operands) > 2:
            sys.stderr.write("Incorrect argument list, try --help\n")
            sys.exit(1)

        if not self.samples:
            self.samples = -1

    def options(self):
        """ Setup default command line argument option handling """
        opts = pmapi.pmOptions()
        opts.pmSetOptionCallback(self.option)
        opts.pmSetOverrideCallback(self.option_override)
        opts.pmSetShortOptions("acC:dD:fghiI:lL:mM:nN:o:pP:qrsS:tTvVy?")
        opts.pmSetShortUsage("[-afv] [options...] [delay [count]]")
        opts.pmSetLongOptionText('Versatile tool for generating system resource statistics')

        opts.pmSetLongOptionHeader("Dstat options")
        opts.pmSetLongOption('cpu', 0, 'c', '', 'enable cpu stats')
        opts.pmSetLongOptionText(' '*5 + '-C 0,3,total' + ' '*10 + 'include cpu0, cpu3 and total')
        opts.pmSetLongOption('disk', 0, 'd', '', 'enable disk stats')
        opts.pmSetLongOptionText(' '*5 + '-D total,sda' + ' '*10 + 'include sda and total')
        opts.pmSetLongOption('device-mapper', 0, None, '', '')
        opts.pmSetLongOptionText('  --dm, --device-mapper' + ' '*1 + 'enable device mapper stats')
        opts.pmSetLongOptionText(' '*5 + '-L root,home,total' + ' '*4 + 'include root, home and total')
        opts.pmSetLongOption('multi-device', 0, None, '', '')
        opts.pmSetLongOptionText('  --md, --multi-device' + ' '*2 + 'enable multi-device driver stats')
        opts.pmSetLongOptionText(' '*5 + '-M total,md-0' + ' '*9 + 'include md-0 and total')
        opts.pmSetLongOption('partition', 0, None, '', '')
        opts.pmSetLongOptionText('  --part, --partition' + ' '*3 + 'enable disk partition stats')
        opts.pmSetLongOptionText(' '*5 + '-P total,sdb2' + ' '*9 + 'include sdb2 and total')
        opts.pmSetLongOption('page', 0, 'g', '', 'enable page stats')
        opts.pmSetLongOption('int', 0, 'i', '', 'enable interrupt stats')
        opts.pmSetLongOptionText(' '*5 + '-I 9,CAL' + ' '*14 + 'include int9 and function call interrupts')
        opts.pmSetLongOption('load', 0, 'l', '', 'enable load stats')
        opts.pmSetLongOption('mem', 0, 'm', '', 'enable memory stats')
        opts.pmSetLongOption('net', 0, 'n', '', 'enable network stats')
        opts.pmSetLongOptionText(' '*5 + '-N eth1,total' + ' '*9 + 'include eth1 and total')
        opts.pmSetLongOption('proc', 0, 'p', '', 'enable process stats')
        opts.pmSetLongOption('io', 0, 'r', '', 'enable io stats (I/O requests completed)')
        opts.pmSetLongOption('swap', 0, 's', '', 'enable swap stats')
        opts.pmSetLongOptionText(' '*5 + '-S swap1,total' + ' '*8 + 'include swap1 and total')
        opts.pmSetLongOption('time', 0, 't', '', 'enable time/date output')
        opts.pmSetLongOption('time-adv', 0, None, '', 'enable time/date output (with milliseconds)')
        opts.pmSetLongOption('epoch', 0, 'T', '', 'enable time counter (seconds since epoch)')
        opts.pmSetLongOption('epoch-adv', 0, None, '', 'enable time counter (milliseconds since epoch)')
        opts.pmSetLongOption('sys', 0, 'y', '', 'enable system stats')
        opts.pmSetLongOptionText('')
        opts.pmSetLongOption('aio', 0, None, '', 'enable aio stats')
        opts.pmSetLongOption('fs', 0, None, '', '')
        opts.pmSetLongOption('filesystem', 0, None, '', '')
        opts.pmSetLongOptionText('  --fs, --filesystem' + ' '*4 + 'enable fs stats')
        for group in 'ipc', 'lock', 'raw', 'socket', 'tcp', 'udp', 'unix', 'vm':
            opts.pmSetLongOption(group, 0, None, '', 'enable '+ group + 'stats')
        opts.pmSetLongOption('vm-adv', 0, None, '', 'enable advanced vm stats')
#       opts.pmSetLongOption('zones', 0, None, '', 'enable zoneinfo stats')
        opts.pmSetLongOptionText('')
        opts.pmSetLongOption('list', 0, None, '', 'list all available plugins')
        opts.pmSetLongOption('plugin', 0, None, '', 'enable external plugin by name, see --list')
        opts.pmSetLongOptionText('')
        opts.pmSetLongOption('all', 0, 'a', '', 'equals -cdngy (default)')
        opts.pmSetLongOption('full', 0, 'f', '', 'automatically expand -C, -D, -I, -N and -S lists')
        opts.pmSetLongOption('vmstat', 0, 'v', '', 'equals -pmgdsc -D total')
        opts.pmSetLongOptionText('')
        opts.pmSetLongOption('bits', 0, '', '', 'force bits for values expressed in bytes')
        opts.pmSetLongOption('float', 0, '', '', 'force float values on screen')
        opts.pmSetLongOption('integer', 0, '', '', 'force integer values on screen')
        opts.pmSetLongOptionText('')
        opts.pmSetLongOption('bw', 0, '', '', '')
        opts.pmSetLongOption('blackonwhite', 0, '', '', '')
        opts.pmSetLongOption('black-on-white', 0, '', '', '')
        opts.pmSetLongOptionText('  --bw, --blackonwhite' + ' '*2 + 'change colors for white background terminal')
        opts.pmSetLongOption('color', 0, '', '', 'force colors')
        opts.pmSetLongOption('nocolor', 0, '', '', 'disable colors')
        opts.pmSetLongOption('noheaders', 0, '', '', 'disable repetitive headers')
        opts.pmSetLongOption('noupdate', 0, '', '', 'disable intermediate updates')
        opts.pmSetLongOption('nomissed', 0, '', '', 'disable missed ticks warnings')
        opts.pmSetLongOption('output', 1, 'o', 'file', 'write CSV output to file')
        opts.pmSetLongOption('version', 0, 'V', '', '')
        opts.pmSetLongOption('debug', 1, None, '', '')
        opts.pmSetLongOption('dbg', 0, None, '', '')
        opts.pmSetLongOption('help', 0, 'h', '', '')
        opts.pmSetLongOptionText('')
        opts.pmSetLongOptionText('delay is the delay in seconds between each update (default: 1)')
        opts.pmSetLongOptionText('count is the number of updates to display before exiting (default: unlimited)')
        return opts

    def usage(self):
        """ Special case the -h option, usually -h/--host in PCP """
        pmUsageMessage()
        sys.exit(0)

    def option_override(self, opt):
        """ Override standard PCP options for Dstat utility """
        if opt in ('a', 'D', 'g', 'h', 'L', 'n', 'N', 'p', 's', 'S', 't', 'T', 'V'):
            return 1
        return 0

    def append_plugins(self, plugins):
        """ Activate a list of plugins, checking if already active first """
        for plugin in plugins:
            self.append_plugin(plugin)

    def append_plugin(self, plugin):
        """ Activate a single plugin, checking if already active first """
        if plugin not in self.plugins:
            self.plugins.append(plugin)

    def option(self, opt, arg, index):
        """ Perform setup for an individual command line option """
        if opt in ['dbg']:
            self.debug = True
        elif opt in ['c']:
            self.append_plugin('cpu')
        elif opt in ['C']:
            insts = arg.split(',')
            self.cpulist = sorted(['cpu' + str(x) for x in insts if x != 'total'])
            if 'total' in insts:
                self.cpulist.append('total')
        elif opt in ['d']:
            self.append_plugin('disk')
        elif opt in ['D']:
            insts = arg.split(',')
            self.disklist = sorted([x for x in insts if x != 'total'])
            if 'total' in insts:
                self.disklist.append('total')
        elif opt in ['device-mapper']:
            self.append_plugin('dm')
        elif opt in ['L']:
            insts = arg.split(',')
            self.dmlist = sorted([x for x in insts if x != 'total'])
            if 'total' in insts:
                self.dmlist.append('total')
        elif opt in ['multi-device']:
            self.append_plugin('md')
        elif opt in ['M']:
            insts = arg.split(',')
            self.mdlist = sorted([x for x in insts if x != 'total'])
            if 'total' in insts:
                self.mdlist.append('total')
        elif opt in ['partition']:
            self.append_plugin('part')
        elif opt in ['P']:
            insts = arg.split(',')
            self.partlist = sorted([x for x in insts if x != 'total'])
            if 'total' in insts:
                self.partlist.append('total')
        elif opt in ['filesystem']:
            self.append_plugin('fs')
        elif opt in ['g']:
            self.append_plugin('page')
        elif opt in ['i']:
            self.append_plugin('int')
        elif opt in ['I']:
            insts = arg.split(',')
            self.intlist = sorted(['line' + str(x) for x in insts if x != 'total'])
            if 'total' in insts:
                self.intlist.append('total')
        elif opt in ['l']:
            self.append_plugin('load')
        elif opt in ['m']:
            self.append_plugin('mem')
        elif opt in ['n']:
            self.append_plugin('net')
        elif opt in ['N']:
            insts = arg.split(',')
            self.netlist = sorted([x for x in insts if x != 'total'])
            self.netpacketlist = sorted([x for x in insts if x != 'total'])
            if 'total' in insts:
                self.netlist.append('total')
                self.netpacketlist.append('total')
        elif opt in ['p']:
            self.append_plugin('proc')
        elif opt in ['r']:
            self.append_plugin('io')
        elif opt in ['s']:
            self.append_plugin('swap')
        elif opt in ['S']:
            # pylint: disable=consider-using-generator
            self.swaplist = list(['/dev/' + str(x) for x in arg.split(',')])
        elif opt in ['t']:
            self.append_plugin('time')
        elif opt in ['T']:
            self.append_plugin('epoch')
        elif opt in ['y']:
            self.append_plugin('sys')
        elif opt in ['a', 'all']:
            self.append_plugins(['cpu', 'disk', 'net', 'page', 'sys'])
        elif opt in ['v', 'vmstat']:
            self.append_plugins(['proc', 'mem', 'page', 'disk', 'sys', 'cpu'])
        elif opt in ['f', 'full']:
            self.full = True
        elif opt in ['all-plugins']:
            self.append_plugins(self.allplugins)
        elif opt in ['bits']:
            self.bits = True
        elif opt in ['bw', 'black-on-white', 'blackonwhite']:
            self.blackonwhite = True
        elif opt in ['color']:
            self.color = True
            self.update = True
        elif opt in ['float']:
            self.float = True
        elif opt in ['integer']:
            self.integer = True
        elif opt in ['list']:
            self.show_conf = True
            self.show_plugins()
            sys.exit(0)
        elif opt in ['nocolor']:
            self.color = False
        elif opt in ['noheaders']:
            self.header = False
        elif opt in ['noupdate']:
            self.update = False
        elif opt in ['nomissed']:
            self.nomissed = True
        elif opt in ['o', 'output']:
            self.output = arg
        elif opt in ['pidfile']:
            self.pidfile = arg
        elif opt in ['q']:
            self.verify = True
        elif opt in ['h', '?']:
            self.usage()
        elif opt in ['V', 'version']:
            self.show_conf = True
            self.show_version()
            self.show_plugins()
            sys.exit(0)
        elif opt != '':
            self.append_plugin(opt)
        else:
            raise pmapi.pmUsageErr()

    def config_paths(self, conf):
        usrdir = os.path.expanduser('~')
        sysdir = pmapi.pmContext.pmGetConfig("PCP_SYSCONF_DIR")
        conf = conf.replace("$PCP_SYSCONF_DIR", sysdir)
        conf = conf.replace("$HOME", usrdir)
        return conf

    def config_files(self, configs):
        paths = []
        for conf in configs:
            conf = self.config_paths(conf)
            try:
                if not os.path.isdir(conf):
                    continue
                for filename in sorted(os.listdir(conf)):
                    length = len(filename)
                    # skip rpm packaging files or '.'-prefixed
                    if length > 1 and filename[0] == '.':
                        continue
                    if length > 7 and filename[(length-7):] == '.rpmnew':
                        continue
                    if length > 8 and filename[(length-8):] == '.rpmsave':
                        continue
                    paths.append(conf + '/' + filename)
            except:
                pass
        return paths

    def show_plugins(self):
        sys.stdout.write("timestamp plugins:\n\t")
        sep = ''
        for i, name in enumerate(sorted(self.timeplugins)):
            if i != 0:
                sep = ', '
            sys.stdout.write("%s%s" % (sep, name))
        sys.stdout.write("\n")
        _, columns = self.term.get_size()
        for i, path in enumerate(self.DEFAULT_CONFIGS):
            self.show_config_files(path, columns)

    def show_config_files(self, path, columns):
        files = self.config_files([path])
        if files == []:
            return
        path = self.config_paths(path)
        config = ConfigParser.RawConfigParser()
        config.optionxform = str
        try:
            config.read(files)
            sys.stdout.write("%s plugins:\n\t" % path)
            self.show_config_plugins(config, columns)
        except:
            sys.stderr.write("%s: failed to read configuration file(s)" % path)

    def show_config_plugins(self, config, columns):
        plugins = sorted(config.sections())
        cols2 = columns - 8
        mod = None
        for mod in plugins:
            cols2 = cols2 - len(mod) - 2
            if cols2 <= 0:
                sys.stdout.write('\n\t')
                cols2 = columns - len(mod) - 10
            if mod != plugins[-1]:
                sys.stdout.write("%s, " % mod)
        if mod is not None:
            sys.stdout.write("%s\n" % mod)

    def show_version(self):
        self.connect()
        platform = self.pmfg.extend_item('kernel.uname.sysname')
        kernel = self.pmfg.extend_item('kernel.uname.release')
        hertz = self.pmfg.extend_item('kernel.all.hz')
        cpucount = self.pmfg.extend_item('hinv.ncpu')
        pagesize = self.pmfg.extend_item('hinv.pagesize')
        self.pmfg.fetch()
        print('pcp-dstat %s' % self.context.pmGetConfig('PCP_VERSION'))
        print('Written by the PCP team <pcp@groups.io> and Dag Wieers <dag@wieers.com>')
        print('Homepages at https://pcp.io/ and http://dag.wieers.com/home-made/dstat/')
        print('')
        print('Platform %s' % platform())
        print('Kernel %s' % kernel())
        print('Python %s' % sys.version)
        print('')
        color = ""
        if not self.term.get_color():
            color = "no "
        print('Terminal type: %s (%scolor support)' % (os.getenv('TERM'), color))
        row, col = self.term.get_size()
        print('Terminal size: %d lines, %d columns' % (row, col))
        print('')
        print('Processors: %d' % cpucount())
        print('Pagesize: %d' % pagesize())
        print('Clock ticks per second: %d' % hertz())
        print('')
        self.pmfg.clear()

    def connect(self):
        """ Establish a PMAPI context, default is 'local:' with a fallback
            to using local context mode if pmcd(1) is not running locally.
        """
        context, self.source = pmapi.pmContext.set_connect_options(self.opts, self.source, self.speclocal)

        if context == PM_CONTEXT_ARCHIVE:
            self.update = False
        if context == PM_CONTEXT_HOST:
            try:
                self.pmfg = pmapi.fetchgroup(context, self.source)
            except pmapi.pmErr:
                if self.source != 'local:':
                    raise
                context = PM_CONTEXT_LOCAL
        if self.pmfg is None:
            self.pmfg = pmapi.fetchgroup(context, self.source)
        self.pmfg_ts = self.pmfg.extend_timestamp()
        self.context = self.pmfg.get_context()

        if pmapi.c_api.pmSetContextOptions(self.context.ctx, self.opts.mode, self.opts.delta):
            raise pmapi.pmUsageErr()

    def validate(self):
        """ Validate configuration options """
        if self.version != self.CONFIG_VERSION:
            sys.stderr.write("Incompatible configuration file version (read v%s, need v%d).\n" % (self.version, self.CONFIG_VERSION))
            sys.exit(1)

        self.pmconfig.validate_common_options()
        self.pmconfig.validate_metrics()

        for i, plugin in enumerate(self.totlist):
            for name in self.metrics:
                metric = self.metrics[name]
                if plugin != metric[12]:
                    continue
                plugin.mgroup.append(name)   # metric names

        for i, metric in enumerate(self.metrics):
            plugin = self.metrics[metric][12]
            insts = self.pmconfig.insts[i]
            for j in range(0, len(insts[0])):
                inum, inst = insts[0][j], insts[1][j]
                if inst not in plugin.igroup:
                    plugin.igroup.append(inst)
                name = self.metrics[metric][0]
                if inum != PM_IN_NULL:
                    name = name.replace('%d', str(inum)).replace('%s', inst)
                    name = name.replace('%i', str(inum)).replace('%I', inst)
                if name not in plugin.names:
                    plugin.names.append(name)   # instance names

        self.pmconfig.finalize_options()
        self.finalize_options()

    @staticmethod
    def dchg(var, width, base):
        "Convert decimal to string given base and length"
        c = 0
        var = float(var) # avoid loss of precision below
        while True:
            ret = str(long(py3round(var)))
            if len(ret) <= width:
                break
            var = var / base
            c = c + 1
        else:
            c = -1
        return ret, c

    @staticmethod
    def fchg(var, width, base):
        "Convert float to string given scale and length"
        c = 0
        while True:
            if var == 0:
                ret = str('0')
                break
            ret = str(long(py3round(var, width)))
            if len(ret) <= width:
                i = width - len(ret) - 1
                while i > 0:
                    ret = ('%%.%df' % i) % var
                    if len(ret) <= width and ret != str(long(py3round(var, width))):
                        break
                    i = i - 1
                else:
                    ret = str(long(py3round(var)))
                break
            var = var / base
            c = c + 1
        else:
            c = -1
        return ret, c

    @staticmethod
    def schg(string, width):
        "Ensure given string fits into and fills the given width"
        if len(string) < width:
            return string + '%-*s' % (width - len(string), ' ')
        if len(string) > width:
            return string[0:width]
        return string

    @staticmethod
    def showtime(plugin, stamp):
        "Format a sample time stamp"
        value = ''
        if plugin.name in ['epoch', 'epoch-adv']:    # time in seconds
            value = str(int(stamp.value))
        elif plugin.name in ['time', 'time-adv']:    # formatted time
            value = stamp().strftime(TIMEFMT)
        if plugin.name in ['epoch-adv', 'time-adv']: # with milliseconds
            value = value + '.' + str(stamp.value.tv_usec * 1000)[:3]
        return value

    @staticmethod
    def tchg(var, width):
        "Convert time string to given length"
        ret = '%2dh%02d' % (var / 60, var % 60)
        if len(ret) > width:
            ret = '%2dh' % (var / 60)
            if len(ret) > width:
                ret = '%2dd' % (var / 60 / 24)
                if len(ret) > width:
                    ret = '%2dw' % (var / 60 / 24 / 7)
        return ret

    def tshow(self, plugin, stamp):
        "Display sample time stamp"
        value = self.showtime(plugin, stamp)
        line = self.cprint(value, NOUNITS, 's', None, plugin.width, None)
        #sys.stderr.write("tshow result:\n%s%s\n" % (line, THEME['default']))
        return line

    def mgetkey(self, label, instid):
        "Get valueset lookup key for a given metric instance"
        return label + '[' + str(instid) + ']'

    def mlookup(self, valuesets, key):
        "Perform valueset lookup for a given metric instance"
        try:
            valueset = valuesets[key]
        except KeyError:
            valueset = []
        return valueset

    def mappend(self, valuesets, key, pmtype, value):
        "Add value into the given valueset for a later averaging calculation"
        valueset = self.mlookup(valuesets, key)
        if value is None or pmtype not in [PM_TYPE_32, PM_TYPE_U32, PM_TYPE_64,
                                    PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE]:
            valueset = []
        valueset.append(value)
        return valueset

    def maverage(self, valueset, key, pmtype):
        "Perform valueset averaging calculation, return current average"
        if valueset is None or len(valueset) == 0:
            return None
        if pmtype not in [PM_TYPE_32, PM_TYPE_U32, PM_TYPE_64,
                          PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE]:
            return valueset[0]
        value = sum(valueset) / len(valueset)
        #sys.stderr.write("average[%s] %s = %s / %s%s\n" %
        #    (key, value, sum(valueset), len(valueset), THEME['default']))
        return value

    def mupdate(self, valuesets, valueset, key):
        "Store latest values in a set in the valueset array"
        if valueset:
            valuesets[key] = valueset

    def mcleanup(self, valuesets, key):
        "Reset a valueset for storing values for the next sample"
        if step == op.delay:
            valueset = self.mlookup(valuesets, key)
            del valueset[:]
            return valueset
        return None

    def mshow(self, plugin, index, result):
        "Display stat results"
        metric = op.metrics[plugin.mgroup[index]]
        #sys.stderr.write("Result metric: %s\n" % metric)
        label = metric[0]
        units = metric[2][1]
        width = metric[4]
        pmtype = metric[5].pmtype
        printtype = metric[8]
        colorstep = metric[9]
        valuesets = metric[13]

        line = ''
        count = 0
        sep = CHAR['space']
        for instid, _, value in result:
            if count > 0:
                line = line + sep
            key = self.mgetkey(label, instid)
            valueset = self.mappend(valuesets, key, pmtype, value)
            value = self.maverage(valueset, key, pmtype)
            self.mcleanup(valuesets, key)
            self.mupdate(valuesets, valueset, key)
            #sys.stderr.write("mshow result value:\n%s%s\n" % (value, THEME['default']))
            line = line + self.cprint(value, units, printtype, pmtype, width, colorstep)
            count += 1
        if count == 0:
            line = line + self.cprint(None, units, printtype, pmtype, width, colorstep)
        #sys.stderr.write("mshow result line:\n%s%s\n" % (line, THEME['default']))
        return line

    @staticmethod
    def roundcsv(var):
        "Value rounding for comma-separated-value output"
        if var is None:
            return ''
        if isinstance(var, str):
            return '"' + var + '"'
        if var != round(var):
            return '%.3f' % var
        return '%d' % round(var)

    def mshowcsv(self, plugin, index, result):
        "Return stat results for CSV file"
        line = ''
        count = 0
        sep = CHAR['sep']
        for _, _, value in result:
            if count > 0:
                line = line + sep
            line = line + self.roundcsv(value)
            count += 1
        #sys.stderr.write("mshowcsv result:\n%s%s\n" % (line, THEME['default']))
        return line

    def tshowcsv(self, plugin, stamp):
        value = self.showtime(plugin, stamp)
        line = value.ljust(plugin.width)
        #sys.stderr.write("tshowcsv result:\n%s%s\n" % (line, THEME['default']))
        return line

    @staticmethod
    def instance_match(inst, plugin):
        if plugin.instances and inst in plugin.instances:
            return True
        return plugin.grouptype in [1, 4]

    @staticmethod
    def top_sort_key(name, plugin):
        if plugin.grouptype == 4:
            return name[len(name)-4:len(name)] == '.top'
        return False

    def gshow(self, plugin, results):
        "Display stat group results"
        line = ''
        count = 0
        col = THEME['frame'] + CHAR['colon']

        # first iterate over the result and update all metric instance valuesets
        for i, name in enumerate(plugin.mgroup):        # e.g. [usr, sys, idl]
            metric = op.metrics[plugin.mgroup[i]]
            result = results[name]
            valuesets = metric[13]
            pmtype = metric[5].pmtype
            label = metric[0]
            top_instance = None
            top_value = 0
            top_key = self.top_sort_key(name, plugin)  # boolean: top sort key?

            if top_key:
                plugin.igroup = []  # empty out for subsequent re-evaluation

            for instid, _, value in result:
                key = self.mgetkey(label, instid)
                valueset = self.mappend(valuesets, key, pmtype, value)
                self.mupdate(valuesets, valueset, key)

            # assess top-most instance and update instances list
            if top_key and pmtype in [PM_TYPE_32, PM_TYPE_U32, PM_TYPE_64,
                                    PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE]:
                for instid, instname, value in result:
                    key = self.mgetkey(label, instid)
                    valueset = self.mlookup(valuesets, key)
                    value = self.maverage(valueset, key, pmtype)
                    if value > top_value:
                        top_instance = instname
                        top_value = value
                if top_value == 0:  # short-circuit if no top-most instance found
                    return line + '%-*s' % (plugin.width, ' ')
                plugin.igroup = [top_instance]  # otherwise we restrict instances

        # next, iterate over specific instances requested and report values
        for inst in plugin.igroup:      # e.g. [cpu0, cpu1, total]
            for i, name in enumerate(plugin.mgroup):        # e.g. [usr, sys, idl]
                metric = op.metrics[plugin.mgroup[i]]
                result = results[name]
                valuesets = metric[13]
                label = metric[0]
                units = metric[2][1]
                width = metric[4]
                pmtype = metric[5].pmtype
                printtype = metric[8]
                colorstep = metric[9]
                value = None

                if plugin.grouptype == 4:
                    if self.top_sort_key(name, plugin):  # skip it if so
                        continue
                    if metric[10] is not None:
                        colorstep = int(metric[10])

                for instid, instname, _ in result:
                    if instname == inst:
                        key = self.mgetkey(label, instid)
                        valueset = self.mlookup(valuesets, key)
                        value = self.maverage(valueset, key, pmtype)
                #sys.stderr.write("[%s] inst=%s value=%s\n" % (name, inst, str(value)))
                if not self.instance_match(inst, plugin):
                    continue
                if plugin.grouptype == 2:   # total only
                    continue
                if count > 0 and (count % len(plugin.mgroup)) == 0:
                    line = line + col
                elif count > 0:
                    line = line + CHAR['space']
                line = line + self.cprint(value, units, printtype, pmtype, width, colorstep)
                count += 1

        if plugin.grouptype in [2, 3]:         # report 'total' (sum) calculation
            totals = [0] * len(plugin.mgroup)
            for i, name in enumerate(plugin.mgroup):        # e.g. [usr, sys, idl]
                metric = op.metrics[name]
                values = 0
                result = results[name]
                valuesets = metric[13]
                label = metric[0]
                units = metric[2][1]
                width = metric[4]
                pmtype = metric[5].pmtype
                printtype = metric[8]
                colorstep = metric[9]

                for instid, instname, _ in result:
                    if plugin.cullinsts is not None and re.match(plugin.cullinsts, instname):
                        continue
                    key = self.mgetkey(label, instid)
                    valueset = self.mlookup(valuesets, key)
                    totals[i] += self.maverage(valueset, key, pmtype)
                    values += 1

            if values == 0:
                totals = [None] * len(plugin.mgroup)
            if values and plugin.printtype == 'p':
                for i in range(0, len(plugin.mgroup)):
                    totals[i] /= values
            if line != '':
                line = line + col
            line = line + self.cprintlist(totals, units, printtype, pmtype, width, colorstep)

        # finally, throw away any values that are no longer needed
        for i, name in enumerate(plugin.mgroup):        # e.g. [usr, sys, idl]
            metric = op.metrics[name]
            valuesets = metric[13]
            label = metric[0]
            for instid, _, _ in result:
                key = self.mgetkey(label, instid)
                valueset = self.mcleanup(valuesets, key)
                self.mupdate(valuesets, valueset, key)

        #sys.stderr.write("gshow result line:\n%s%s\n" % (line, THEME['default']))
        return line

    def gshowcsv(self, plugin, results):
        "Return stat group results for CSV file"
        line = ''
        count = 0
        totals = [0] * len(plugin.mgroup)

        # first iterate over the result and update all metric instance valuesets
        for i, name in enumerate(plugin.mgroup):        # e.g. [usr, sys, idl]
            metric = op.metrics[plugin.mgroup[i]]
            result = results[name]
            pmtype = metric[5].pmtype
            top_instance = None
            top_value = 0
            top_key = self.top_sort_key(name, plugin)  # boolean: top sort key?

            if top_key:
                plugin.igroup = []  # empty out for subsequent re-evaluation

            # assess top-most instance and update instances list
            if top_key and pmtype in [PM_TYPE_32, PM_TYPE_U32, PM_TYPE_64,
                                    PM_TYPE_U64, PM_TYPE_FLOAT, PM_TYPE_DOUBLE]:
                for _, instname, value in result:
                    if value > top_value:
                        top_instance = instname
                        top_value = value
                if top_value == 0:  # short-circuit if no top-most instance found
                    return ''
                plugin.igroup = [top_instance]  # otherwise we restrict instances

        for inst in plugin.igroup:      # e.g. [cpu0, cpu1, total]
            for i, name in enumerate(plugin.mgroup):        # e.g. [usr, sys, idl]
                result = results[name]
                value = None
                if self.top_sort_key(name, plugin):
                    continue

                for _, instname, val in result:
                    if instname == inst:
                        value = val
                #sys.stderr.write("[%s] inst=%s name=%s value=%s\n" %
                #                (name, instid, instname, str(value)))
                if not self.instance_match(inst, plugin):
                    continue
                if plugin.grouptype == 2:   # total only
                    continue
                if count > 0:
                    line = line + CHAR['sep']
                line = line + self.roundcsv(value)
                count += 1

        if plugin.grouptype in [2, 3]:   # report 'total' (sum) calculation
            for i, name in enumerate(plugin.mgroup):        # e.g. [usr, sys, idl]
                values = 0
                result = results[name]
                for _, instname, val in result:
                    if plugin.cullinsts is not None and re.match(plugin.cullinsts, instname):
                        continue
                    totals[i] += val
                    values += 1
            if values == 0:
                totals = [None] * len(plugin.mgroup)
            if values and plugin.printtype == 'p':
                for i in range(0, len(plugin.mgroup)):
                    totals[i] /= values
            for value in totals:
                if line != '':
                    line = line + CHAR['sep']
                line = line + self.roundcsv(value)
        #sys.stderr.write("gshowcsv result line:\n%s%s\n" % (line, THEME['default']))
        return line

    @staticmethod
    def scale_time(value, scale):
        """ convert to canonical time units of seconds """
        div = 1.0
        mul = 1.0
        if scale == PM_TIME_NSEC:
            div = 1000000000.0
        elif scale == PM_TIME_USEC:
            div = 1000000.0
        elif scale == PM_TIME_MSEC:
            div = 1000.0
        elif scale == PM_TIME_MIN:
            mul = 60.0
        elif scale == PM_TIME_HOUR:
            mul = 3600.0
        return (float(value) / div) * mul

    @staticmethod
    def scale_space(value, scale):
        """ convert to canonical space units of bytes """
        if scale == 0:
            return value
        return value * pow(1024, scale)

    def cprintlist(self, values, units, prtype, pmtype, width, colorstep):
        """Return all columns color printed"""
        ret = sep = ''
        for value in values:
            ret = ret + sep + self.cprint(value, units, prtype, pmtype, width, colorstep)
            sep = CHAR['space']
        return ret

    def cprint(self, value, units, printtype, pmtype, width, colorstep):
        """Color print one column.  Note that @value may be None indicating
           there were no values available at sampling time in which case we
           print a blank section in the report.  If the entire line ends up
           blank, we filter it out later.
        """
        if value is not None:
            self.novalues = False
        else:
            value = ''
            printtype = 's'
            colorstep = None

        if printtype is None:
            if pmtype in [PM_TYPE_32, PM_TYPE_U32, PM_TYPE_64, PM_TYPE_U64]:
                printtype = 'd'
            elif pmtype in [PM_TYPE_DOUBLE, PM_TYPE_FLOAT]:
                printtype = 'f'
            else:
                printtype = 's'
        base = 1000
        if units.dimTime and printtype != 's':
            value = self.scale_time(value, units.scaleTime)
        if units.dimSpace and printtype != 's':
            base = 1024
            value = self.scale_space(value, units.scaleSpace)
        if units.dimCount and units.scaleCount and printtype != 's':
            value *= units.scaleCount

        ### Display units when base is exact 1000 or 1024
        showunit = False
        if colorstep is None:
            if width >= len(str(base)) and not isinstance(value, str):
                showunit = True
                width = width - 1

        ### If this is a negative value, return a dash
        if printtype in ('b', 'd', 'f') and value < 0:
            if showunit:
                return THEME['error'] + '-'.rjust(width) + CHAR['space'] + THEME['default']
            else:
                return THEME['error'] + '-'.rjust(width) + THEME['default']

        if op.bits and printtype in ('b', ):
            units = ('b', 'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y')
            base = 1000
            value = value * 8.0
        elif base != 1024:
            units = (CHAR['space'], 'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y')
        else:
            units = ('B', 'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y')

        if step == op.delay:
            colors = THEME['colors_lo']
            ctext = THEME['text_lo']
            cunit = THEME['unit_lo']
            cdone = THEME['done_lo']
        else:
            colors = THEME['colors_hi']
            ctext = THEME['text_hi']
            cunit = THEME['unit_hi']
            cdone = THEME['done_hi']

        #sys.stderr.write("printtype: %s width=%d\n" % (str(printtype), width))
        #sys.stderr.write("colorstep: %s value=%s\n" % (colorstep, str(value)))

        ### Convert value to string given base and field-length
        if op.integer and printtype in ('b', 'd', 'p', 'f'):
            ret, c = self.dchg(value, width, base)
        elif op.float and printtype in ('b', 'd', 'p', 'f'):
            ret, c = self.fchg(value, width, base)
        elif printtype in ('b', 'd', 'p'):
            ret, c = self.dchg(value, width, base)
        elif printtype in ('f',):
            ret, c = self.fchg(value, width, base)
        elif printtype in ('s',):
            ret, c = self.schg(value, width), ctext
        elif printtype in ('t',):
            ret, c = self.tchg(value, width), ctext
        else:
            raise TypeError('printtype %s not known to pcp-dstat.' % printtype)

        ### Set the metrics color
        if ret == '0':
            color = cunit
        elif printtype in ('p') and py3round(value) >= 100.0:
            color = cdone
        elif printtype not in ('s') and colorstep is not None and colorstep != 0:
            color = colors[int(value/colorstep) % len(colors)]
        elif printtype in ('b', 'd', 'f'):
            color = colors[c % len(colors)]
        else:
            color = ctext

        ### Justify value to left if string
        if printtype in ('s',):
            ret = color + ret.ljust(width)
        else:
            ret = color + ret.rjust(width)

        ### Add unit to output
        if showunit:
            if c != -1 and py3round(value) != 0:
                ret += cunit + units[c]
            else:
                ret += CHAR['space']

        return ret

    def show_header(self, visible):
        "Return the header for a set of module counters"
        line = ''
        ### Process title
        for o in visible:
            line += o.title()
            if o is not visible[-1]:
                line += THEME['frame'] + CHAR['space']
            elif self.totlist != visible:
                line += THEME['title'] + CHAR['gt']
        line += '\n'
        ### Process subtitle
        for o in visible:
            line += o.subtitle()
            if o is not visible[-1]:
                line += THEME['frame'] + CHAR['pipe']
            elif self.totlist != visible:
                line += THEME['title'] + CHAR['gt']
        return line + '\n'

    def show_csvheader(self, visible):
        "Return the header for CSV file"
        line = ''
        ### CSV Header
        if not os.path.exists(self.output):
            line += '"pcp-dstat ' + self.context.pmGetConfig('PCP_VERSION') + ' CSV Output"\n'
            line += '"Author:","PCP team <pcp@groups.io> and Dag Wieers <dag@wieers.com>",,,,"URL:","https://pcp.io/ and http://dag.wieers.com/home-made/dstat/"\n'
        import getpass # pylint: disable=import-outside-toplevel
        line += '"Host:","' + self.context.pmGetContextHostName() + '",,,,"User:","' + getpass.getuser() + '"\n'
        line += '"Cmdline:","' + self.context.pmProgname() + ' ' + ' '.join(self.arguments) + '",,,,"Date:","' + time.strftime('%d %b %Y %H:%M:%S %Z') + '"\n'
        ### Process title
        for o in visible:
            line += o.csvtitle()
            if o is not visible[-1]:
                line += CHAR['sep']
            elif self.totlist != visible:
                pass #line += THEME['title'] + CHAR['gt']
        line += '\n'
        ### Process subtitle
        for o in visible:
            line += o.csvsubtitle()
            if o is not visible[-1]:
                line += CHAR['sep']
            elif self.totlist != visible:
                pass #line += THEME['title'] + CHAR['gt']
        return line + '\n'

    @staticmethod
    def finalize():
        """ Finalize and clean up (atexit) """
        try:
            sys.stderr.close() # avoid python-generated warnings
            if not op.verify and not op.show_conf:
                if op.update:
                    sys.stdout.write('\n')
                if sys.stdout.isatty():
                    sys.stdout.write(ANSI['reset'])
            sys.stdout.flush()
            if op.pidfile:
                os.remove(op.pidfile)
        except:
            pass

    def perform(self, update):
        "Inner loop that calculates counters and constructs output"
        global oldvislist, vislist, showheader, showcsvheader, rows, cols
        global totaltime, starttime
        global loop, step

        starttime = time.time()
        loop = int((update - 1 + op.delay) / op.delay)
        step = int(((update - 1) % op.delay) + 1)

        # Get current time (may be different from schedule) for debugging
        if not op.debug:
            curwidth = 0
        else:
            if step == 1 or loop == 0:
                totaltime = 0
            curwidth = 8

        # If it takes longer than 500ms, then warn!
        if loop != 0 and starttime - self.inittime - update > 1:
            self.missed = self.missed + 1
            return

        # Initialise certain variables
        if loop == 0:
            rows, cols = 0, 0
            vislist = []
            oldvislist = []
            showheader = True
            showcsvheader = True

        if sys.stdout.isatty():
            oldcols = cols  # pylint: disable=used-before-assignment
            rows, cols = self.term.get_size()

            # Trim object list to what is visible on screen
            if oldcols != cols:
                vislist = []
                for o in self.totlist:
                    newwidth = curwidth + o.statwidth() + 1
                    if newwidth <= cols or (vislist == self.totlist[:-1] and newwidth < cols):
                        vislist.append(o)
                        curwidth = newwidth

            # Check when to display the header
            if self.header and rows >= 6:
                if oldvislist != vislist:
                    showheader = True
                elif not op.update and loop % (rows - 2) == 0:
                    showheader = True
                elif op.update and step == 1 and loop % (rows - 1) == 0:
                    showheader = True

            oldvislist = vislist
        else:
            vislist = self.totlist

        # Fetch values
        try:
            self.pmfg.fetch()
        except pmapi.pmErr as fetcherr:
            raise fetcherr

        # Calculate all objects (visible, invisible)
        onovalues = self.novalues
        self.novalues = True
        line = oline = ''
        i = 0

        # Walk the result dict reporting on visible plugins.
        # In conjuntion, we walk through the ordered results
        # dictionary - matching up to each plugin as we go.
        # Note that some plugins (time-based) will not have
        # any corresponding entry in the results.

        results = self.pmconfig.get_ranked_results()
        for i, plugin in enumerate(self.totlist):
            if i == 0:
                sep = ''
            else:
                sep = THEME['frame'] + CHAR['pipe']
            if plugin not in vislist:
                pass
            elif plugin in self.timelist:
                line = line + sep + self.tshow(plugin, self.pmfg_ts)
            elif plugin.grouptype is None:
                for m, name in enumerate(plugin.mgroup):
                    line = line + sep + self.mshow(plugin, m, results[name])
                    sep = CHAR['space']
            elif plugin.mgroup:
                line = line + sep + self.gshow(plugin, results)
            if self.totlist == vislist:
                continue
            if plugin in self.totlist and plugin not in vislist:
                line = line + THEME['frame'] + CHAR['gt']
                break

        if self.output:
            for i, plugin in enumerate(self.totlist):
                if i == 0:
                    sep = ''
                else:
                    sep = CHAR['sep']
                if plugin in self.timelist:
                    oline = oline + sep + self.tshowcsv(plugin, self.pmfg_ts)
                elif plugin.grouptype is None:
                    for m, name in enumerate(plugin.mgroup):
                        oline = oline + sep + self.mshowcsv(plugin, m, results[name])
                        sep = CHAR['sep']
                elif plugin.mgroup:
                    oline = oline + sep + self.gshowcsv(plugin, results)

        # Prepare the colors for intermediate updates
        # (last step in a loop is definitive)
        if step == op.delay:
            THEME['default'] = ANSI['reset']
        else:
            THEME['default'] = THEME['text_lo']

        # The first step is to show the definitive line if necessary
        newline = ''
        if op.update and not self.novalues:
            if step == 1 and update != 0 and not onovalues:
                newline = '\n'
                newline += ANSI['reset'] + ANSI['clearline'] + ANSI['save']
            elif loop != 0:
                newline = ANSI['restore']

        # Display header
        if showheader:
            if loop == 0 and self.totlist != vislist:
                sys.stderr.write('Terminal width too small, trimming output.\n')
            showheader = False
            sys.stdout.write(newline)
            newline = self.show_header(vislist)
            newline += ANSI['reset'] + ANSI['clearline'] + ANSI['save']

        # Display CSV header
        newoline = ''
        if op.output:
            if showcsvheader:
                showcsvheader = False
                if os.path.exists(self.output):
                    newoline += '\n\n'
                newoline += self.show_csvheader(self.totlist)

        if self.novalues:
            line = newline
        else:
            line = newline + line
        if self.novalues:
            oline = newoline
        else:
            oline = newoline + oline

        # Print stats
        sys.stdout.write(line + THEME['input'])

        if self.output and step == self.delay:
            if not os.path.exists(self.output) or not os.path.isfile(op.output):
                omode = 'wt'
            else:
                omode = 'at'
            outputfile = open(self.output, omode)
            outputfile.write(oline)

        if self.missed > 0 and self.nomissed is False:
            line = 'missed ' + str(self.missed + 1) + ' ticks'
            sys.stdout.write(' ' + THEME['error'] + line + THEME['input'])
            if self.output and step == self.delay:
                outputfile.write(',"' + line + '"')
        self.missed = 0
        # Finish the line
        if not op.update and self.novalues is False:
            sys.stdout.write('\n')
        if self.output and step == self.delay and self.novalues is False:
            outputfile.write('\n')

    def execute(self):
        """ Fetch and report """
        if self.debug:
            sys.stdout.write("Config file keywords: " + str(self.keys) + "\n")
            sys.stdout.write("Metric spec keywords: " + str(self.pmconfig.metricspec) + "\n")

        # Set delay mode for live sampling
        if self.context.type != PM_CONTEXT_ARCHIVE:
            scheduler = sched.scheduler(time.time, time.sleep)
            self.inittime = time.time()

        # Common preparations
        self.context.prepare_execute(self.opts, False, self.interpol, self.interval)

        update = 0.0
        interval = 1.0
        if not self.update:
            interval = op.delay

        while update <= self.delay * (self.samples - 1) or self.samples == -1:
            if self.context.type != PM_CONTEXT_ARCHIVE:
                scheduler.enterabs(self.inittime + update, 1, perform, (update,))
                scheduler.run()
            else:
                self.perform(update)
            sys.stdout.flush()
            update = update + interval


def perform(update):
    """Helper function for interfacing to the scheduler"""
    op.perform(update)


if __name__ == '__main__':
    global update
    try:
        dstat = DstatTool(sys.argv[1:])
        dstat.execute()
    except pmapi.pmErr as error:
        if error.args[0] == PM_ERR_EOL:
            sys.exit(0)
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
