#!/usr/bin/env pmpython
#
# Copyright (C) 2018 Red Hat.
# Copyright 2004-2016 Dag Wieers <dag@wieers.com>
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
from cpmapi import PM_TYPE_DOUBLE, PM_TYPE_FLOAT
from cpmapi import PM_ERR_EOL, PM_IN_NULL, pmUsageMessage

if sys.version >= '3':
    long = int

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

    'darkgray': '\033[1;30m',
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
    'save_all': '\0337',
    'restore_all': '\0338',
    'linewrap': '\033[7h',
    'nolinewrap': '\033[7l',

    'up': '\033[1A',
    'down': '\033[1B',
    'right': '\033[1C',
    'left': '\033[1D',

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
                import curses
                curses.setupterm()
                curses.tigetnum('lines'), curses.tigetnum('cols')
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
                import curses
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
                import curses
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
    def set_title(arguments):
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
        import getpass
        user = getpass.getuser()
        host = os.uname()[1]    # TODO: hostname via PMAPI context
        host = host.split('.')[0]
        path = os.path.basename(sys.argv[0])    # TODO: pmProgname
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
        if metric[1] == None:
            metric[1] = self.label
        if metric[2] == []:
            metric[2] = self.instances
        if metric[3] == None:
            metric[3] = self.unit
        if metric[4] == None:
            metric[4] = self.type
        if metric[5] == None:
            metric[5] = self.width
        # slot six also magic - fetchgroup state
        if metric[7] == None:
            metric[7] = self.precision
        if metric[8] == None:
            metric[8] = self.limit
        if metric[9] == None:
            metric[9] = self.printtype
        if metric[10] == None:
            metric[10] = self.colorstep
        if metric[11] == None:
            metric[11] = self.grouptype
        if metric[12] == None:
            metric[12] = self.cullinsts
        metric[13] = self   # back-pointer to this from metric dict

    def prepare_grouptype(self, instlist):
        """Setup a list of instances from the command line"""
        if instlist == None:
            instlist = ['total']
        if 'total' in instlist:
            self.grouptype = 2 if (len(instlist) == 1) else 3
            instlist.remove('total') # remove command line arg
        else:
            self.grouptype = 1
        self.instances = instlist

    def statwidth(self):
        """Return complete width for this plugin"""
        return len(self.names) * self.colwidth() + len(self.names) - 1

    def colwidth(self):
        """Return column width"""
        return self.width

    def instlist(self):
        if self.grouptype > 2:
            return self.igroup + ['total']
        elif self.grouptype > 1:
            return ['total']
        return self.igroup

    def title(self):
        if self.grouptype == None:
            width = self.statwidth()
            label = self.label[0:width].center(width).replace(' ', '-')
            return THEME['title'] + label + THEME['default']
        ret = ''
        ilist = self.instlist()
        for i, name in enumerate(ilist):
            width = self.statwidth()
            name = self.label.replace('%I', name)
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
        if self.grouptype == None:
            for i, nick in enumerate(self.names):
                label = nick[0:self.width].center(self.width)
                ret = ret + THEME['subtitle'] + label + THEME['default']
                if i + 1 != len(self.names): ret = ret + CHAR['space']
            return ret
        ilist = self.instlist()
        for i, name in enumerate(ilist):
            for j, nick in enumerate(self.names):
                label = nick[0:self.width].center(self.width)
                ret = ret + THEME['subtitle'] + label + THEME['default']
                if j + 1 != len(self.names): ret = ret + CHAR['space']
            if i + 1 != len(ilist): ret = ret + THEME['frame'] + CHAR['colon']
        return ret

#    def csvtitle(self):
#        if isinstance(self.name, types.StringType):
#            return '"' + self.name + '"' + CHAR['sep'] * (len(self.nick) - 1)
#        else:
#            ret = ''
#            for i, name in enumerate(self.name):
#                ret = ret + '"' + name + '"' + CHAR['sep'] * (len(self.nick) - 1)
#                if i + 1 != len(self.name): ret = ret + CHAR['sep']
#            return ret
#
#    def csvsubtitle(self):
#        ret = ''
#        if isinstance(self.name, types.StringType):
#            for i, nick in enumerate(self.nick):
#                ret = ret + '"' + nick + '"'
#                if i + 1 != len(self.nick): ret = ret + CHAR['sep']
#        elif len(self.name) == 1:
#            for i, name in enumerate(self.name):
#                for j, nick in enumerate(self.nick):
#                    ret = ret + '"' + nick + '"'
#                    if j + 1 != len(self.nick): ret = ret + CHAR['sep']
#                if i + 1 != len(self.name): ret = ret + CHAR['sep']
#        else:
#            for i, name in enumerate(self.name):
#                for j, nick in enumerate(self.nick):
#                    ret = ret + '"' + name + ':' + nick + '"'
#                    if j + 1 != len(self.nick): ret = ret + CHAR['sep']
#                if i + 1 != len(self.name): ret = ret + CHAR['sep']
#        return ret
#
#    def showcsv(self):
#        def printcsv(var):
#            if var != round(var):
#                return '%.3f' % var
#            return '%d' % long(round(var))
#
#        line = ''
#        for i, name in enumerate(self.vars):
#            if isinstance(self.val[name], types.ListType) or isinstance(self.val[name], types.TupleType):
#                for j, val in enumerate(self.val[name]):
#                    line = line + printcsv(val)
#                    if j + 1 != len(self.val[name]):
#                        line = line + CHAR['sep']
#            elif isinstance(self.val[name], types.StringType):
#                line = line + self.val[name]
#            else:
#                line = line + printcsv(self.val[name])
#            if i + 1 != len(self.vars):
#                line = line + CHAR['sep']
#        return line
#
#    def showcsvend(self, totlist, vislist):
#        if vislist and self is not vislist[-1]:
#            return CHAR['sep']
#        elif totlist and self is not totlist[-1]:
#            return CHAR['sep']
#        return ''

def dchg(var, width, base):
    "Convert decimal to string given base and length"
    c = 0
    while True:
        ret = str(long(round(var)))
        if len(ret) <= width:
            break
        var = var / base
        c = c + 1
    else:
        c = -1
    return ret, c

def fchg(var, width, base):
    "Convert float to string given scale and length"
    c = 0
    while True:
        if var == 0:
            ret = str('0')
            break
        ret = str(long(round(var, width)))
        if len(ret) <= width:
            i = width - len(ret) - 1
            while i > 0:
                ret = ('%%.%df' % i) % var
                if len(ret) <= width and ret != str(long(round(var, width))):
                    break
                i = i - 1
            else:
                ret = str(long(round(var)))
            break
        var = var / base
        c = c + 1
    else:
        c = -1
    return ret, c

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

TIMEFMT = os.getenv('DSTAT_TIMEFMT') or '%d-%m %H:%M:%S'

def tshow(plugin, stamp):
    "Display sample time stamp"
    if plugin.name in ['epoch', 'epoch-adv']:    # time in seconds
        value = str(int(stamp.value))
    elif plugin.name in ['time', 'time-adv']:    # formatted time
        value = stamp().strftime(TIMEFMT)
    if plugin.name in ['epoch-adv', 'time-adv']: # with milliseconds
        value = value + '.' + str(stamp.value.tv_usec * 1000)[:3]
    line = cprint(value, NOUNITS, 's', None, plugin.width, None)
    #sys.stderr.write("tshow result line:\n%s%s\n" % (line, THEME['default']))
    return line

def mshow(plugin, index, result):
    "Display stat results"
    #sys.stderr.write("Result metric: %s\n" % metric)
    metric = op.metrics[plugin.mgroup[index]]
    label = metric[0]
    insts = metric[1]
    units = metric[2][1]
    width = metric[4]
    pmtype = metric[5].pmtype
    printtype = metric[8]
    colorstep = metric[9]

    line = ''
    count = 0
    sep = CHAR['space']
    for inst, name, value in result:
        if count > 0:
            line = line + sep
        line = line + cprint(value, units, printtype, pmtype, width, colorstep)
        count += 1
    #sys.stderr.write("mshow result line:\n%s%s\n" % (line, THEME['default']))
    return line

def gshow(plugin, results):
    "Display stat group results"
    metric = op.metrics[plugin.mgroup[0]]
    units = metric[2][1]
    width = metric[4]
    pmtype = metric[5].pmtype
    printtype = metric[8]
    colorstep = metric[9]

    line = ''
    count = 0
    totals = [0] * len(plugin.mgroup)
    col = THEME['frame'] + CHAR['colon']

    def instance_match(inst, plugin):
        if plugin.cullinsts != None and re.match(plugin.cullinsts, inst):
            return False
        if plugin.instances and inst in plugin.instances:
            return True
        return False

    for inst in plugin.igroup:      # e.g. [cpu0, cpu1, total]
        for i, name in enumerate(plugin.mgroup):        # e.g. [usr, sys, idl]
            result = results[name]
            value = None
            for instid, instname, val in result:
                if instname == inst:
                    value = val
            #sys.stderr.write("[%s] inst=%s name=%s value=%s\n" % (name, instid, instname, str(value)))
            if not instance_match(inst, plugin):
                continue
            if plugin.grouptype == 2:   # total only
                continue
            if count > 0 and (count % len(plugin.mgroup)) == 0:
                line = line + col
            elif count > 0:
                line = line + CHAR['space']
            line = line + cprint(value, units, printtype, pmtype, width, colorstep)
            count += 1

    if plugin.grouptype > 1:   # report 'total' (sum) calculation
        for i, name in enumerate(plugin.mgroup):        # e.g. [usr, sys, idl]
            values = 0
            result = results[name]
            for instid, instname, val in result:
                totals[i] += val
                values += 1
        if values and plugin.printtype == 'p':
            for i in range(0, len(plugin.mgroup)):
                totals[i] /= values
        if line != '':
            line = line + col
        line = line + cprintlist(totals, units, printtype, pmtype, width, colorstep)
    #sys.stderr.write("gshow result line:\n%s%s\n" % (line, THEME['default']))
    return line

def cprintlist(values, units, prtype, pmtype, width, colorstep):
    """Return all columns color printed"""
    ret = sep = ''
    for value in values:
        ret = ret + sep + cprint(value, units, prtype, pmtype, width, colorstep)
        sep = CHAR['space']
    return ret

def cprint(value, units, printtype, pmtype, width, colorstep):
    """Color print one column"""

    if printtype == None:
        if pmtype in [PM_TYPE_32, PM_TYPE_U32, PM_TYPE_64, PM_TYPE_U64]:
            printtype = 'd'
        elif pmtype in [PM_TYPE_DOUBLE, PM_TYPE_FLOAT]:
            printtype = 'f'
        else:
            printtype = 's'
    base = 1000
    if units.dimTime:
        value *= base * units.scaleTime
    if units.dimSpace and units.scaleSpace:
        base = 1024
        value *= base * units.scaleSpace
    if units.dimCount and units.scaleCount:
        value *= units.scaleCount

    ### Display units when base is exact 1000 or 1024
    showunit = False
    if colorstep == None and width >= len(str(base)) and type(value) != type(''):
        showunit = True
        width = width - 1

    ### If this is a negative value, return a dash
    if printtype in ('b', 'd', 'f') and value < 0:
        if showunit:
            return THEME['error'] + '-'.rjust(width) + CHAR['space'] + THEME['default']
        else:
            return THEME['error'] + '-'.rjust(width) + THEME['default']

    if base != 1024:
        units = (CHAR['space'], 'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y')
    elif op.bits and printtype in ('b', ):
        units = ('b', 'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y')
        base = 1000
        value = value * 8.0
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

    #sys.stderr.write("printtype: %s\n" % str(printtype))
    #sys.stderr.write("colorstep: %s\n" % str(colorstep))

    ### Convert value to string given base and field-length
    if op.integer and printtype in ('b', 'd', 'p' 'f'):
        ret, c = dchg(value, width, base)
    elif op.float and printtype in ('b', 'd', 'p', 'f'):
        ret, c = fchg(value, width, base)
    elif printtype in ('b', 'd', 'p'):
        ret, c = dchg(value, width, base)
    elif printtype in ('f',):
        ret, c = fchg(value, width, base)
    elif printtype in ('s',):
        ret, c = str(value), ctext
    elif printtype in ('t',):
        ret, c = tchg(value, width), ctext
    else:
        raise Exception('printtype %s not known to pcp-dstat.' % printtype)

    ### Set the metrics color
    if ret == '0':
        color = cunit
    elif printtype in ('p') and round(value) >= 100.0:
        color = cdone
    elif colorstep != None:
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
        if c != -1 and round(value) != 0:
            ret += cunit + units[c]
        else:
            ret += CHAR['space']

    return ret

#def csvheader(totlist):
#    "Return the CVS header for a set of module counters"
#    line = ''
#    ### Process title
#    for o in totlist:
#        line = line + o.csvtitle()
#        if o is not totlist[-1]:
#            line = line + CHAR['sep']
#    line += '\n'
#    ### Process subtitle
#    for o in totlist:
#        line = line + o.csvsubtitle()
#        if o is not totlist[-1]:
#            line = line + CHAR['sep']
#    return line + '\n'


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

        self.context = None
        self.opts = self.options()
        self.arguments = arguments
        self.pmconfig = pmconfig.pmConfig(self)

        ### Add additional dstat metric specifiers
        dspec = (None, 'printtype', 'colorstep', 'grouptype', 'cullinsts', 'plugin')
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
        self.missed = 0
        self.delay = 1.0
        self.type = 0
        self.type_prefer = self.type
        self.ignore_incompat = 0
        self.precision = 5 # .5f
        self.timefmt = self.TIMEFMT
        self.interpol = 0
        self.leaf_only = True

        # Internal
        self.missed = 0
        self.runtime = -1
        self.plugins = []     # list of requested plugin names
        self.allplugins = []  # list of all known plugin names
        self.timeplugins = [] # list of the time plugin names
        self.timelist = []    # DstatPlugin time objects list
        self.totlist = []     # active DstatPlugin object list
        self.vislist = []     # visible DstatPlugin object list
        self.mapping = {}     # maps 'section/label' to plugin

        self.bits = False
        self.blackonwhite = False
        self.color = None
        self.debug = False
        self.verify = False
        self.header = 1
        self.output = True
        self.update = True
        self.pidfile = False
        self.float = False
        self.integer = False

        # Options for specific plugins
        self.cpulist = None
        self.disklist = None
        self.intlist = None
        self.netlist = None
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
        #           12:plugin <- Dstat extras ]
        self.metrics = OrderedDict()
        self.pmfg = None
        self.pmfg_ts = None

        ### Initialise output device
        self.term = DstatTerminal()

        ### Read configuration and initialise plugins
        configs = self.prepare_plugins()
        self.create_time_plugins()

        ### Complete command line processing and terminal/file setup
        self.pmconfig.read_cmd_line()
        self.prepare_metrics(configs)
        if self.verify:
            sys.exit(0)
        self.prepare_output()

    def prepare_output(self):
        """ Complete all initialisation and get ready to begin sampling """
        self.pmconfig.set_signal_handler()
        self.term.set_title(self.arguments)
        self.term.set_theme(self.blackonwhite)
        if self.color == None:
            self.color = self.term.get_color()

        ### Empty ansi and theme databases when colors not in use
        if not self.color:
            for key in COLOR:
                COLOR[key] = ''
            for key in THEME:
                THEME[key] = ''
            for key in ANSI:
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
            pidfile = open(self.pidfile, 'w', 0)
            pidfile.write(str(os.getpid()))
            pidfile.close()
        except Exception as e:
            sys.stderr.write('Failed to create pidfile %s\n' % self.pidfile, e)
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
        except Exception as error:
            sys.stderr.write("Invalid regex '%s': %s.\n" % (value, error))
            sys.exit(1)
        return value

    def prepare_plugins(self):
        paths = self.config_files(self.DEFAULT_CONFIGS)
        if not paths or len(paths) < 1:
            sys.stderr.write("No configs found in: %s\n" % self.DEFAULT_CONFIGS)
            sys.exit(1)

        config = ConfigParser.RawConfigParser()
        config.optionxform = str
        try:
            found = config.read(paths)
        except ConfigParser.Error as error:
            sys.stderr.write("Config parse failure: %s\n" % error.message())
            sys.exit(1)
        except Exception as error:
            sys.stderr.write("Cannot parse configs in %s\n" % paths)
            sys.exit(1)

        #print("Found configs: %s" % found)
        #print("with sections: %s" % config.sections())
        for plugin in config.sections():
            self.allplugins.append(plugin)
            self.opts.pmSetLongOption(plugin, 0, '', '', '')
        return config

    def prepare_metrics(self, config):
        """ Using the list of requested plugins, prepare for sampling """

        if not self.plugins:
            print('You did not select any stats, using -cdngy by default.')
            self.plugins = ['cpu', 'disk', 'net', 'page', 'sys']

        lib = self.pmconfig
        for section in self.plugins:
            metrics = OrderedDict()
            if section in self.timeplugins:
                index = self.timeplugins.index(section)
                plugin = self.timelist[index]
                name = 'dstat.' + section + '.' + plugin.name # metric name
                value = 'event.missed'  # a valid metric that always exists
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
                        elif key in ['colorstep']:
                            value = float(value)
                        elif key in ['printtype']:
                            value = value[0]    # first character suffices
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

                # Instance logic for -C/-D/-I/-N/-S options
                if section == 'cpu':
                    plugin.prepare_grouptype(self.cpulist)
                elif section in ['disk', 'disk-tps']:
                    plugin.prepare_grouptype(self.disklist)
                elif section == 'int':
                    plugin.prepare_grouptype(self.intlist)
                elif section == 'net':
                    plugin.prepare_grouptype(self.netlist)
                elif section == 'swap':
                    plugin.prepare_grouptype(self.swaplist)

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
        if len(operands) > 0:
            try:
                self.delay = float(operands[0])
            except Exception as e:
                sys.stderr.write("Invalid sample delay '%s'\n" % operands[0])
                sys.exit(1)
        if len(operands) > 1:
            try:
                self.samples = int(operands[1])
            except Exception as e:
                sys.stderr.write("Invalid sample count '%s'\n" % operands[1])
                sys.exit(1)
        if len(operands) > 2:
            sys.stderr.write("Incorrect argument list, try --help\n")
            sys.exit(1)

        if not self.update:
            self.interval = pmapi.timeval(self.delay)
        if not self.samples:
            self.samples = -1

    def options(self):
        """ Setup default command line argument option handling """
        opts = pmapi.pmOptions()
        opts.pmSetOptionCallback(self.option)
        opts.pmSetOverrideCallback(self.option_override)
        opts.pmSetShortOptions("acC:dD:fghiI:lmnN:o:pqrsS:tT:vVy?")
        opts.pmSetShortUsage("[-afv] [options...] [delay [count]]")
        opts.pmSetLongOptionText('Versatile tool for generating system resource statistics')

        opts.pmSetLongOptionHeader("Dstat options")
        opts.pmSetLongOption('cpu', 0, 'c', '', 'enable cpu stats')
        opts.pmSetLongOptionText(' '*5 + '-C 0,3,total' + ' '*10 + 'include cpu0, cpu3 and total')
        opts.pmSetLongOption('disk', 0, 'd', '', 'enable disk stats')
        opts.pmSetLongOptionText(' '*5 + '-D total,hda' + ' '*10 + 'include hda and total')
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
        opts.pmSetLongOption('noupdate', 0, '', '', 'disable intermediate headers')
        opts.pmSetLongOption('output', 0, '', 'file', 'write CSV output to file')
        opts.pmSetLongOption('profile', 0, '', '', 'show profiling statistics when exiting dstat')
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
        if opt in ('a', 'D', 'g', 'h', 'n', 'N', 'p', 's', 'S', 't', 'T', 'V'):
            return 1
        return 0

    def option(self, opt, arg, index):
        """ Perform setup for an individual command line option """
        if opt in ['dbg']:
            self.debug = True
        elif opt in ['c']:
            self.plugins.append('cpu')
        elif opt in ['C']:
            insts = arg.split(',')
            self.cpulist = sorted({'cpu' + str(x) for x in insts if x != 'total'})
            if 'total' in insts:
                self.cpulist.append('total')
        elif opt in ['d']:
            self.plugins.append('disk')
        elif opt in ['D']:
            insts = arg.split(',')
            self.disklist = sorted({x for x in insts if x != 'total'})
            if 'total' in insts:
                self.disklist.append('total')
        elif opt in ['--filesystem']:
            self.plugins.append('fs')
        elif opt in ['g']:
            self.plugins.append('page')
        elif opt in ['i']:
            self.plugins.append('int')
        elif opt in ['I']:
            insts = arg.split(',')
            self.intlist = sorted({'line' + str(x) for x in insts if x != 'total'})
            if 'total' in insts:
                self.intlist.append('total')
        elif opt in ['l']:
            self.plugins.append('load')
        elif opt in ['m']:
            self.plugins.append('mem')
        elif opt in ['n']:
            self.plugins.append('net')
        elif opt in ['N']:
            insts = arg.split(',')
            self.netlist = sorted({x for x in insts if x != 'total'})
            if 'total' in insts:
                self.netlist.append('total')
        elif opt in ['p']:
            self.plugins.append('proc')
        elif opt in ['r']:
            self.plugins.append('io')
        elif opt in ['s']:
            self.plugins.append('swap')
        elif opt in ['S']:
            self.swaplist = list({'/dev/' + str(x) for x in arg.split(',')})
        elif opt in ['t']:
            self.plugins.append('time')
        elif opt in ['T']:
            self.plugins.append('epoch')
        elif opt in ['y']:
            self.plugins.append('sys')
        elif opt in ['a', 'all']:
            self.plugins += ['cpu', 'disk', 'net', 'page', 'sys']
        elif opt in ['v', 'vmstat']:
            self.plugins += ['proc', 'mem', 'page', 'disk', 'sys', 'cpu']
        elif opt in ['f', 'full']:
            self.full = True
        elif opt in ['all-plugins']:
            self.plugins += self.allplugins
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
            self.show_plugins()
            sys.exit(0)
        elif opt in ['nocolor']:
            self.color = False
        elif opt in ['noheaders']:
            self.header = False
        elif opt in ['noupdate']:
            self.update = False
        elif opt in ['o', 'output']:
            self.output = arg
        elif opt in ['pidfile']:
            self.pidfile = arg
        elif opt in ['profile']:
            self.profile = 'dstat_profile.log'
        elif opt in ['q']:
            self.verify = True
        elif opt in ['h']:
            self.usage()
        elif opt in ['V', 'version']:
            self.show_version()
            self.show_plugins()
            sys.exit(0)
        elif opt != '':
            self.plugins.append(opt)
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
        sys.stdout.write("internal:\n\t")
        sep = ''
        for i, name in enumerate(sorted(self.timeplugins)):
            if i != 0:
                sep = ', '
            sys.stdout.write("%s%s" % (sep, name))
        sys.stdout.write("\n")
        rows, cols = self.term.get_size()
        for i, path in enumerate(self.DEFAULT_CONFIGS):
            self.show_config_files(path, cols)

    def show_config_files(self, path, cols):
        files = self.config_files([path])
        if files == []:
            return
        path = self.config_paths(path)
        config = ConfigParser.RawConfigParser()
        config.optionxform = str
        try:
            config.read(files)
            sys.stdout.write("%s:\n\t" % path)
            self.show_config_plugins(config, cols)
        except:
            sys.stderr.write("%s: failed to read configuration file(s)" % path)

    def show_config_plugins(self, config, cols):
        plugins = sorted(config.sections())
        cols2 = cols - 8
        for mod in plugins:
            cols2 = cols2 - len(mod) - 2
            if cols2 <= 0:
                sys.stdout.write('\n\t')
                cols2 = cols - len(mod) - 10
            if mod != plugins[-1]:
                sys.stdout.write("%s, " % mod)
        if mod != None:
            sys.stdout.write("%s\n" % mod)

    def show_version(self):
        self.connect()
        print('pcp-dstat %s' % pmapi.pmContext.pmGetConfig('PCP_VERSION'))
        print('Written by the PCP team <pcp@groups.io> and Dag Wieers <dag@wieers.com>')
        print('Homepages at https://pcp.io/ and http://dag.wieers.com/home-made/dstat/')
        print()
        print('Platform %s/%s' % (os.name, sys.platform)) # kernel.uname.sysname
        print('Kernel %s' % os.uname()[2])  # kernel.uname.release
        print('Python %s' % sys.version)
        print()
        color = ""
        if not self.term.get_color():
            color = "no "
        print('Terminal type: %s (%scolor support)' % (os.getenv('TERM'), color))
        rows, cols = self.term.get_size()
        print('Terminal size: %d lines, %d columns' % (rows, cols))
        print()
#       print('Processors: %d' % hinv.ncpu)    - TODO
#       print('Pagesize: %d' % hinv.pagesize)
#       print('Clock ticks per secs: %d' % kernel.all.hz)
#       print()

    def connect(self):
        """ Establish a PMAPI context """
        context, self.source = pmapi.pmContext.set_connect_options(self.opts, self.source, self.speclocal)

        if context == PM_CONTEXT_HOST:
            try:
                self.pmfg = pmapi.fetchgroup(context, self.source)
            except pmapi.pmErr:
                context = PM_CONTEXT_LOCAL
        if self.pmfg == None:
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

    def execute(self):
        """ Fetch and report """
        if self.debug:
            sys.stdout.write("Config file keywords: " + str(self.keys) + "\n")
            sys.stdout.write("Metric spec keywords: " + str(self.pmconfig.metricspec) + "\n")

        # Set delay mode, interpolation
        if self.context.type != PM_CONTEXT_ARCHIVE:
            self.interpol = 1

        # Common preparations
        self.context.prepare_execute(self.opts, False, self.interpol, self.interval)
        scheduler = sched.scheduler(time.time, time.sleep)
        inittime = time.time()
        try:
            self.pmfg.fetch()    # prime initial values (TODO: fix, somehow)
        except:
            pass

        update = 0.0
        while update <= self.delay * (self.samples-1) or self.samples == -1:
            scheduler.enterabs(inittime + update, 1, perform, (update,))
            scheduler.run()
            sys.stdout.flush()
            update = update + float(self.interval)


    @staticmethod
    def finalize():
        """ Finalize and clean up (atexit) """
        sys.stdout.write('\n' + ANSI['reset'])
        sys.stdout.flush()
        if op.pidfile:
            os.remove(op.pidfile)

    def ticks(self):
        "Return the number of 'ticks' since bootup"
        # TODO: extract kernel.all.uptime and inject into fetchgroup fetches
        for line in open('/proc/uptime', 'r').readlines():
            l = line.split()
            if len(l) < 2: continue
            return float(l[0])
        return 0

    def show_header(self, vislist):
        "Return the header for a set of module counters"
        line = ''
        ### Process title
        for o in vislist:
            line += o.title()
            if o is not vislist[-1]:
                line += THEME['frame'] + CHAR['space']
            elif self.totlist != vislist:
                line += THEME['title'] + CHAR['gt']
        line += '\n'
        ### Process subtitle
        for o in vislist:
            line += o.subtitle()
            if o is not vislist[-1]:
                line += THEME['frame'] + CHAR['pipe']
            elif self.totlist != vislist:
                line += THEME['title'] + CHAR['gt']
        return line + '\n'

    def perform(self, update):
        "Inner loop that calculates counters and constructs output"
        global oldvislist, vislist, showheader, rows, cols
        global elapsed, totaltime, starttime
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
        if loop != 0 and starttime - inittime - update > 1:
            self.missed = self.missed + 1
            return 0

        # Initialise certain variables
        if loop == 0:
            elapsed = self.ticks()
            rows, cols = 0, 0
            vislist = []
            oldvislist = []
            showheader = True
        else:
            elapsed = step

        if sys.stdout.isatty():
            oldcols = cols
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

        # Prepare the colors for intermediate updates
        # (last step in a loop is definitive)
        if step == op.delay:
            THEME['default'] = ANSI['reset']
        else:
            THEME['default'] = THEME['text_lo']

        ### The first step is to show the definitive line if necessary
        newline = ''
        if op.update:
            if step == 1 and update != 0:
                newline = '\n' + ANSI['reset'] + ANSI['clearline'] + ANSI['save']
            elif loop != 0:
                newline = ANSI['restore']

        ### Display header
        if showheader:
            if loop == 0 and self.totlist != vislist:
                sys.stderr.write('Terminal width too small, trimming output.\n')
            showheader = False
            sys.stdout.write(newline)
            newline = self.show_header(vislist)

        ### Fetch values
        try:
            self.pmfg.fetch()
        except pmapi.pmErr as error:
            if error.args[0] == PM_ERR_EOL:
                return
            raise error

        ### Calculate all objects (visible, invisible)
        i = 0
        line = newline
        oline = ''
        metric = None

        ### Walk the result dict reporting on visible plugins.
        ### In conjuntion, we walk through the ordered results
        ### dictionary - matching up to each plugin as we go.
        ### Note that some plugins (time-based) will not have
        ### any corresponding entry in the results.

        results = self.pmconfig.get_sorted_results()
        for i, plugin in enumerate(self.totlist):
            if i == 0:
                sep = ''
            else:
                sep = THEME['frame'] + CHAR['pipe']
            if plugin in self.timelist:
                line = line + sep + tshow(plugin, self.pmfg_ts)
            elif plugin.grouptype == None:
                for m, name in enumerate(plugin.mgroup):
                    line = line + sep + mshow(plugin, m, results[name])
                    sep = CHAR['space']
            else:
                line = line + sep + gshow(plugin, results)
            if self.totlist == vislist:
                continue
            if plugin in self.totlist and plugin not in vislist:
                line = line + THEME['frame'] + CHAR['gt']
                break

#         for o in self.totlist:
#            if o in vislist:
#                line = line + o.show() + o.showend(self.totlist, vislist)
#            if op.output and step == op.delay:
#                oline = oline + o.showcsv() + o.showcsvend(self.totlist, vislist)

        # Print stats
        sys.stdout.write(line + THEME['input'])
#        if op.output and step == op.delay:
#            outputfile.write(oline + '\n')

        if self.missed > 0:
            sys.stdout.write(' ' + THEME['error'] + 'missed ' + str(self.missed+1) + ' ticks' + THEME['input'])
            self.missed = 0
        # Finish the line
        if not op.update:
            sys.stdout.write('\n')


def perform(update):
    """Helper function for interfacing to the scheduler"""
    op.perform(update)


if __name__ == '__main__':
    global outputfile
    global inittime, update, missed
    try:
        inittime = time.time()
        dstat = DstatTool(sys.argv[1:])
        dstat.connect()
        dstat.validate()
        dstat.execute()

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
