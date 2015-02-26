#!/usr/bin/python
#
# Copyright (C) 2013-2015 Red Hat.
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

"""Advanced System & Process Monitor using the libpcp Wrapper module

Additional Information:

Performance Co-Pilot Web Site
http://www.performancecopilot.org
"""

# ignore line too long, missing docstring, method could be a function,
#        too many public methods
# pylint: disable=C0301
# pylint: disable=C0111
# pylint: disable=R0201
# pylint: disable=R0904

##############################################################################
#
# imports
#

import os
import datetime
import time
import sys
import select
import signal
import cpmapi as c_api
import cpmgui as c_gui
from pcp import pmapi, pmgui
from pcp.pmsubsys import Subsystem
try:
    import curses
except ImportError as e:
    print(e)
    print("pmatop requires curses.py")
    sys.exit(0)

ME = "pmatop"

def debug(mssg):
    import logging
    logging.basicConfig(filename='pmatop.log',level=logging.DEBUG)
    if type(mssg) == type(""):
        logging.debug(mssg)
    else:
        logging.debug(str(mssg) + "\n")


# scale  -------------------------------------------------------------


def scale(value, magnitude):
    return value / magnitude


# record ---------------------------------------------------------------

def record(context, config, duration, path, host):

    # -f saves the metrics in a directory
    if os.path.exists(path):
        return "playback directory %s already exists\n" % path
    try:
        # Non-graphical application using libpcp_gui services - never want
        # to see popup dialogs from pmlogger(1) here, so force the issue.
        os.environ['PCP_XCONFIRM_PROG'] = '/bin/true'
        interval = pmapi.timeval.fromInterval(str(duration) + " seconds")
        context.pmRecordSetup(path, ME, 0) # pylint: disable=W0621
        context.pmRecordAddHost(host, 1, config)
        deadhand = "-T" + str(interval) + "seconds"
        context.pmRecordControl(0, c_gui.PM_REC_SETARG, deadhand)
        context.pmRecordControl(0, c_gui.PM_REC_ON, "")
        interval.sleep()
        context.pmRecordControl(0, c_gui.PM_REC_OFF, "")
        # Note: pmlogger has a deadhand timer that will make it stop of its
        # own accord once -T limit is reached; but we send an OFF-recording
        # message anyway for cleanliness, just prior to pmcollectl exiting.
    except pmapi.pmErr as e:
        return "Cannot create PCP archive: " + path + " " + str(e)
    return ""

# record_add_creator ------------------------------------------------------

def record_add_creator(path):
    fdesc = open(path, "a+")
    args = ""
    for i in sys.argv:
        args = args + i + " "
    fdesc.write("# Created by " + args)
    fdesc.write("\n#\n")
    fdesc.close()

# minutes_seconds ----------------------------------------------------------


def minutes_seconds(milli):
    milli = abs(milli)
    sec, milli = divmod(milli, 1000)
    tenth, milli = divmod(milli, 100)
    milli = milli / 10
    minute, sec = divmod(sec, 60)
    hour, minute = divmod(minute, 60)
    day, hour = divmod(hour, 24)
    if day > 0:
        return "%dd" % (day)
    elif hour > 0:
        return "%dh%dm" % (hour, minute)
    elif minute > 0:
        return "%dm%ds" % (minute, sec)
    else:
        return "%d.%d%1ds" % (sec, tenth, milli)


# _StandardOutput --------------------------------------------------


class _StandardOutput(object):
    def width_write(self, value):
        if value > 0:
            self._width = value
    width = property(None, width_write, None, None)

    def __init__(self, out):
        if out == sys.stdout:
            self._width = 80
            self.stdout = True
        else:
            self.stdout = False
            self.so_stdscr = out
            self._width = self.so_stdscr.getmaxyx()[1]
    def addstr(self, str, clrtoeol=False):
        if self.stdout:
            sys.stdout.write(str)
        else:
            if self.getyx()[1] + len(str) <= self._width:
                self.so_stdscr.addstr(str)
                if clrtoeol:
                    self.so_stdscr.clrtoeol()
            elif len(str) > self._width:
                self.so_stdscr.addstr(str[:self._width])
    def clear(self):
        if not self.stdout:
            self.so_stdscr.clear()
    def move(self, y, x):
        if not self.stdout:
            self.so_stdscr.move(y, x)
    def getyx(self):
        if self.stdout:
            return (0, 0)
        else:
            return self.so_stdscr.getyx()
    def getmaxyx(self):
        if self.stdout:
            return (1000, self._width)
        else:
            return self.so_stdscr.getmaxyx()
    def nodelay(self, tf):
        if not self.stdout:
            self.so_stdscr.nodelay(tf)
    def timeout(self, milliseconds):
        if not self.stdout:
            self.so_stdscr.timeout(milliseconds)
        else:
            time.sleep(milliseconds / 1000)
    def refresh(self):
        if not self.stdout:
            self.so_stdscr.refresh()
    def clrtobot(self):
        if not self.stdout:
            self.so_stdscr.clrtobot()
    def clear(self):
        if not self.stdout:
            self.so_stdscr.clear()
    def getch(self):
        if not self.stdout:
            return self.so_stdscr.getch()
        else:
            while sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
                char = sys.stdin.read(1)
                if len(char) == 0:
                    return -1
                else:
                    return ord(char)
            return -1


# _AtopPrint --------------------------------------------------

class _AtopPrint(object):
    def __init__(self, ss, a_stdscr):
        self.ss = ss
        self.p_stdscr = a_stdscr
        self.command_line = self.p_stdscr.getyx()[0]
        self.apyx = a_stdscr.getmaxyx()
        self.ONEKBYTE = 1024
        self.ONEMBYTE = 1048576
        self.ONEGBYTE = 1073741824
        self.ONETBYTE = 1099511627776
        self.MAXBYTE	= 1024
        self.MAXKBYTE = self.ONEKBYTE*99999
        self.MAXMBYTE = self.ONEMBYTE*999
        self.MAXGBYTE = self.ONEGBYTE*999
        self.ANYFORMAT = 0
        self.KBFORMAT = 1
        self.MBFORMAT = 2
        self.GBFORMAT = 3
        self.TBFORMAT = 4
        self.OVFORMAT = 9

    def end_of_screen(self):
        return self.p_stdscr.getyx()[0] >= self.apyx[0]-1

    def set_line(self):
        self.command_line = self.p_stdscr.getyx()[0]
        self.p_stdscr.addstr('\n')

    def next_line(self):
        if self.p_stdscr.stdout:
            print('')
            return
        line = self.p_stdscr.getyx()
        apy = line[0]
        if line[1] > 0:
            apy += 1
        self.p_stdscr.addstr(' ' * (self.apyx[1] - line[1]))
        self.p_stdscr.move(apy, 0)

    def cpupct(self, value):
        ''' Return integer percentage of aggregate CPU utilization '''
        return int(100 * value / self.ss.cpu_total)

    def valstr(self, value, width, avg_secs = 0):
        '''
        Function valstr() converts 'value' to a string of 'width' fixed
        number of positions.  If 'value' does not fit, it will be formatted to
        exponent-notation.
        '''
        maxval = 0
        remain = 0
        exp = 0
        suffix = ""
        strvalue = ""

        if avg_secs:
            value = (value + (avg_secs/2)) / avg_secs
            width = width - 2
            suffix = "/s"

        maxval = pow(10.0, width) - 1
        if value < 0:
            sign = -1
            value = abs(value)
        else:
            sign = 1

        if value == 0:
            strvalue = "%*d" % (width, value)
        elif abs(value) >= 1: # exponent, if needed, will be positive
            maxval = pow(10.0, width) - 1
            if value > maxval:
                # convert to E format: canonical form, fit width
                maxval = pow(10.0, width-2) - 1
                while value > maxval:
                    exp += 1
                    remain = value % 10
                    value /= 10

                width -= 2
                if remain >= 5:
                    value += 1
                strvalue = "%*de%d%s" % (width, value * sign, exp, suffix)
            else:
                # E format not needed: split int and fraction, fit width
                intval = str(int(value * sign))
                fractional = str(value%1)[1:width-len(intval)+1]
                if fractional == ".":
                    fractional = ""

                prval = "%s%s" % (intval, fractional)
                strvalue = "%*s%s" % (width, prval, suffix)
        else:                   # exponent, if needed, will be negative
            if value < 0.01:
                # convert to E format: canonical form, fit width
                width -= 3
                while value < 1:
                    exp += 1
                    value *= 10

                fractional = str(value%1)[1:width]
                if fractional == ".":
                    fractional = ""
                prval = "%d%s" % (int(value * sign), fractional)
                strvalue = "%*se-%d%s" % (width, prval, int(exp), suffix)
            else:
                # E format not needed: reduce precision, remove trailing 0s
                svalue = str(value * sign).replace("0.",".")[0:width]
                strvalue = "%*s" % (width, svalue.rstrip('0'))
        return strvalue

    def memstr(self, value, width=6, pformat=-1, avg_secs=0):
        '''
        Function memstr() converts 'value' to a string of 'width' fixed
        number of positions and a memory size unit specifier which may
        optionally be specified 'pformat'; otherwise it is deduced.
        '''
        if pformat == -1:
            pformat = self.ANYFORMAT
        aformat = ""
        verifyval = 0
        suffix = ""
        strvalue = ""

        if value < 0:
            verifyval = -value * 10
        else:
            verifyval = value

        if avg_secs:
            value /= avg_secs
            verifyval *= 100
            width -= 2
            suffix = "/s"

        if verifyval <= self.MAXBYTE:		# bytes ?
            aformat = self.ANYFORMAT
        elif verifyval <= self.MAXKBYTE:	# kbytes ?
            aformat = self.KBFORMAT
        elif verifyval <= self.MAXMBYTE:	# mbytes ?
            aformat = self.MBFORMAT
        elif verifyval <= self.MAXGBYTE:	# mbytes ?
            aformat = self.GBFORMAT
        else:
            aformat = self.TBFORMAT

        if aformat <= pformat:
            aformat = pformat

        if aformat == self.ANYFORMAT:
            strvalue = "%s%s" % (self.valstr(int(value), width), suffix)
        elif aformat == self.KBFORMAT:
            strvalue = "%sK%s" % (self.valstr(int(value/self.ONEKBYTE), width-1), suffix)
        elif aformat == self.MBFORMAT:
            strvalue = "%sM%s" % (self.valstr(int(value/self.ONEMBYTE), width-1), suffix)
        elif aformat == self.GBFORMAT:
            strvalue = "%sG%s" % (self.valstr(int(value/self.ONEGBYTE), width-1), suffix)
        elif aformat == self.TBFORMAT:
            strvalue = "%sT%s" % (self.valstr(int(value/self.ONETBYTE), width-1), suffix)
        else:
            strvalue = "*****"

        return strvalue


# _ProcessorPrint --------------------------------------------------


class _ProcessorPrint(_AtopPrint):
# Missing: #trun (total # running threads)
# Missing: #exit (requires accounting)
# Substitutions: proc.runq.sleeping for #tslpi (threads sleeping)
# Substitutions: proc.runq.blocked for #tslpu (threads uninterrupt sleep)
    def prc(self):
        self.p_stdscr.addstr('PRC |')
        self.p_stdscr.addstr(' sys %8s |' % (minutes_seconds(self.ss.get_metric_value('kernel.all.cpu.sys'))))
        self.p_stdscr.addstr(' user %7s |' % (minutes_seconds(self.ss.get_metric_value('kernel.all.cpu.user'))))
        self.p_stdscr.addstr(' #proc %6d |' % (self.ss.get_metric_value('kernel.all.nprocs')))
        if self.apyx[1] >= 95:
            self.p_stdscr.addstr(' #tslpi %s |' % self.valstr(self.ss.get_metric_value('proc.runq.sleeping'), 5))
        if self.apyx[1] >= 110:
            self.p_stdscr.addstr(' #tslpu %s |' % self.valstr(self.ss.get_metric_value('proc.runq.blocked'), 5))
        self.p_stdscr.addstr(' #zombie %s' % self.valstr(self.ss.get_metric_value('proc.runq.defunct'), 4))
        self.next_line()
# Missing: curscal (current current scaling percentage)
    def cpu(self):
        self.ss.get_total()
        sys = self.cpupct(self.ss.get_metric_value('kernel.all.cpu.sys'))
        user = self.cpupct(self.ss.get_metric_value('kernel.all.cpu.user'))
        irq = self.cpupct(self.ss.get_metric_value('kernel.all.cpu.irq.hard')
              + self.ss.get_metric_value('kernel.all.cpu.irq.soft'))
        idle = self.cpupct(self.ss.get_metric_value('kernel.all.cpu.idle'))
        io = self.cpupct(self.ss.get_metric_value('kernel.all.cpu.wait.total'))
        self.p_stdscr.addstr('CPU |')
        self.p_stdscr.addstr(' sys %s%% |' % self.valstr(sys, 7))
        self.p_stdscr.addstr(' user %s%% |' % self.valstr(user, 6))
        self.p_stdscr.addstr(' irq %7d%% |' % irq)
        self.p_stdscr.addstr(' idle %s%% |' % self.valstr(idle, 6))
        self.p_stdscr.addstr(' wait %s%% |' % self.valstr(io, 6))
        self.next_line()
        ncpu = self.ss.get_metric_value('hinv.ncpu')
        max_display_cpus = int(self.apyx[0] / 4)
        for k in range(ncpu):
            sys = self.ss.get_scalar_value('kernel.percpu.cpu.sys', k)
            user = self.ss.get_scalar_value('kernel.percpu.cpu.user', k)
            sys = self.cpupct(sys)
            user = self.cpupct(user)
            if sys == 0 and user == 0:
                continue
            irq = (self.ss.get_scalar_value('kernel.percpu.cpu.irq.hard', k)
                 + self.ss.get_scalar_value('kernel.percpu.cpu.irq.soft', k))
            idle = self.ss.get_scalar_value('kernel.percpu.cpu.idle', k)
            wait = self.ss.get_scalar_value('kernel.percpu.cpu.wait.total', k)
            irq = self.cpupct(irq)
            idle = self.cpupct(idle)
            wait = self.cpupct(wait)
            self.p_stdscr.addstr('cpu |')
            self.p_stdscr.addstr(' sys %7d%% |' % sys)
            self.p_stdscr.addstr(' user %6d%% |' % user)
            self.p_stdscr.addstr(' irq %7d%% |' % irq)
            self.p_stdscr.addstr(' idle %6d%% |' % idle)
            self.p_stdscr.addstr(' cpu%02d %5d%% |' % (k, wait))
            if self.apyx[1] >= 95:
                mhz = scale(self.ss.get_scalar_value('hinv.cpu.clock', k), 1000)
                self.p_stdscr.addstr(' curf %sMHz |' % (self.valstr(mhz), 4))
            self.next_line()
            if ncpu > max_display_cpus and k >= max_display_cpus:
                break

        self.p_stdscr.addstr('CPL |')
        self.p_stdscr.addstr(' avg1 %s |' % self.valstr(self.ss.get_scalar_value('kernel.all.load', 0), 7))
        self.p_stdscr.addstr(' avg5 %s |' % self.valstr(self.ss.get_scalar_value('kernel.all.load', 1), 7))
        self.p_stdscr.addstr(' avg15 %s |' % self.valstr(self.ss.get_scalar_value('kernel.all.load', 2), 6))
        self.p_stdscr.addstr(' csw %s |' % self.valstr(self.ss.get_metric_value('kernel.all.pswitch'), 8))
        self.p_stdscr.addstr(' intr %s |' % self.valstr(self.ss.get_metric_value('kernel.all.intr'), 7))
        if self.apyx[1] >= 110:
            self.p_stdscr.addstr('              |')
        if self.apyx[1] >= 95:
            self.p_stdscr.addstr(' numcpu   %2d  |' % (self.ss.get_metric_value('hinv.ncpu')))
        self.next_line()

# _InterruptPrint --------------------------------------------------


class _InterruptPrint(_AtopPrint):
    pass


# _DiskPrint --------------------------------------------------


class _DiskPrint(_AtopPrint):
    def interval_write(self, value):
        self._interval = value
    interval = property(None, interval_write, None, None)
    def replay_archive_write(self, value):
        self._replay_archive = value
    replay_archive = property(None, replay_archive_write, None, None)

    def disk(self, context):
        desc = self.ss.metric_descs[self.ss.metrics_dict['disk.partitions.read']]
        try:
            (inst, iname) = context.pmGetInDom(desc)
        except pmapi.pmErr as e:
            iname = iname = "X"

        # Missing: LVM avq (average queue depth)
        # TODO: switch to using disk.dm metrics?

        for j in range(self.ss.get_len(self.ss.get_metric_value('disk.partitions.read'))):
            if iname[j][:2] != "dm":
                continue
            lvm = iname[j]
            partitions_read = self.ss.get_scalar_value('disk.partitions.read', j)
            partitions_write = self.ss.get_scalar_value('disk.partitions.write', j)
            if partitions_read == 0 and partitions_write == 0:
                continue
            self.p_stdscr.addstr('LVM |')
            self.p_stdscr.addstr(' %-12s |' % (lvm[len(lvm)-12:]))
            # No disk.partitions.avactive thus no busy calculation
            self.p_stdscr.addstr('              |')
            self.p_stdscr.addstr(' read %s |' % self.valstr(partitions_read, 7))
            self.p_stdscr.addstr(' write %s |' % self.valstr(partitions_write, 6))
            if self.apyx[1] >= 95:
                val = (float(self.ss.get_scalar_value('disk.partitions.blkread', j)) / float(self._interval * 1000)) * 100
                self.p_stdscr.addstr(' MBr/s %s |' % self.valstr(val, 6))
            if self.apyx[1] >= 110:
                val = (float(self.ss.get_scalar_value('disk.partitions.blkwrite', j)) / float(self._interval * 1000)) * 100
                self.p_stdscr.addstr(' MBw/s %s |' % self.valstr(val, 6))
            # No disk.partitions.avactive thus no avio calculation
            if self.end_of_screen():
                break
            self.next_line()

        try:
            (inst, iname) = context.pmGetInDom(self.ss.metric_descs[self.ss.metrics_dict['disk.dev.read']])
        except pmapi.pmErr as e:
            iname = iname = "X"

        for j in range(self.ss.get_len(self.ss.get_metric_value('disk.dev.read_bytes'))):
            self.p_stdscr.addstr('DSK |')
            self.p_stdscr.addstr(' %-12s |' % (iname[j]))
            busy = (float(self.ss.get_scalar_value('disk.dev.avactive', j)) / float(self._interval * 1000)) * 100
            if busy > 100:
                busy = 0
            self.p_stdscr.addstr(' busy %6d%% |' % (busy))
            val = self.ss.get_scalar_value('disk.dev.read', j)
            self.p_stdscr.addstr(' read %s |' % self.valstr(val, 7))
            self.p_stdscr.addstr(' write %s |' % self.valstr(self.ss.get_scalar_value('disk.dev.write', j), 6))
            if self.apyx[1] >= 95:
                val = (float(self.ss.get_scalar_value('disk.partitions.blkread', j)) / float(self._interval * 1000)) * 100
                self.p_stdscr.addstr(' MBr/s %s |' % self.valstr(val, 6))
            if self.apyx[1] >= 110:
                val = (float(self.ss.get_scalar_value('disk.partitions.blkwrite', j)) / float(self._interval * 1000)) * 100
                self.p_stdscr.addstr(' MBw/s %s |' % self.valstr(val, 6))
            try:
                # (/proc/diskstats) time spent doing I/Os / (completed reads + completed writes)
                avio = (float(self.ss.get_scalar_value('disk.dev.avactive', j)) / float(self.ss.get_scalar_value('disk.dev.total', j)))
            except ZeroDivisionError:
                avio = 0
            self.p_stdscr.addstr(' avio %4.2g ms |' % (avio))
            if self.end_of_screen():
                break
            self.next_line()


# _MemoryPrint --------------------------------------------------


class _MemoryPrint(_AtopPrint):
# Missing: shrss (resident shared memory size)
    def mem(self):
        self.p_stdscr.addstr('MEM |')
        self.p_stdscr.addstr(' tot %s |' % (self.memstr(self.ss.get_metric_value('mem.physmem') * self.ONEKBYTE, 8)))
        self.p_stdscr.addstr(' free %s |' % (self.memstr(self.ss.get_metric_value('mem.freemem') * self.ONEKBYTE, 7)))
        self.p_stdscr.addstr(' cache %s |' % (self.memstr(self.ss.get_metric_value('mem.util.cached') * self.ONEKBYTE, 6)))
        self.p_stdscr.addstr(' buff %s |' % (self.memstr(self.ss.get_metric_value('mem.util.bufmem') * self.ONEKBYTE, 7)))
        self.p_stdscr.addstr(' slab %s |' % (self.memstr(self.ss.get_metric_value('mem.util.slab') * self.ONEKBYTE, 7)))
        if self.apyx[1] >= 95:
            self.p_stdscr.addstr(' #shmem %s |' % (self.memstr(self.ss.get_metric_value('mem.util.shmem') * self.ONEKBYTE, 5)))
        self.next_line()

        self.p_stdscr.addstr('SWP |')
        self.p_stdscr.addstr(' tot %s |' % (self.memstr(self.ss.get_metric_value('mem.util.swapTotal') * self.ONEKBYTE, 8)))
        self.p_stdscr.addstr(' free %s |' % (self.memstr(self.ss.get_metric_value('mem.util.swapFree') * self.ONEKBYTE, 7)))
        self.p_stdscr.addstr('              |')
        self.p_stdscr.addstr(' vmcom %s |' % (self.memstr(self.ss.get_metric_value('mem.util.committed_AS') * self.ONEKBYTE, 6)))
        self.p_stdscr.addstr(' vmlim %s |' % (self.memstr(self.ss.get_metric_value('mem.util.commitLimit') * self.ONEKBYTE, 6)))
        self.next_line()

        self.p_stdscr.addstr('PAG |')
        self.p_stdscr.addstr(' scan %s |' % (self.valstr(self.ss.get_metric_value('mem.vmstat.slabs_scanned'), 7)))
        self.p_stdscr.addstr(' steal %s |' % (self.valstr(self.ss.get_metric_value('mem.vmstat.pginodesteal'), 6)))
        self.p_stdscr.addstr(' stall %s |' % (self.valstr(self.ss.get_metric_value('mem.vmstat.allocstall'), 6)))
        self.p_stdscr.addstr(' swin %s |' % (self.valstr(self.ss.get_metric_value('mem.vmstat.pswpin'), 7)))
        self.p_stdscr.addstr(' swout %s |' % (self.valstr(self.ss.get_metric_value('mem.vmstat.pswpout'), 6)))
        self.next_line()


# _NetPrint --------------------------------------------------


class _NetPrint(_AtopPrint):
    def net(self, context):
        if self.end_of_screen():
            return
        self.p_stdscr.addstr('NET | transport    |')
        self.p_stdscr.addstr(' tcpi %sM |' % self.valstr(self.ss.get_metric_value('network.tcp.insegs'), 6))
        self.p_stdscr.addstr(' tcpo %sM |' % self.valstr(self.ss.get_metric_value('network.tcp.outsegs'), 6))
        self.p_stdscr.addstr(' udpi %sM |' % self.valstr(self.ss.get_metric_value('network.udp.indatagrams'), 6))
        self.p_stdscr.addstr(' udpo %sM |' % self.valstr(self.ss.get_metric_value('network.udp.outdatagrams'), 6))
        if self.apyx[1] >= 95:
            self.p_stdscr.addstr(' tcpao %sM |' % self.valstr(self.ss.get_metric_value('network.tcp.activeopens'), 5))
        if self.apyx[1] >= 110:
            self.p_stdscr.addstr(' tcppo %sM |' % self.valstr(self.ss.get_metric_value('network.tcp.passiveopens'), 5))
        self.next_line()

# Missing: icmpi (internet control message protocol received datagrams)
# Missing: icmpo (internet control message protocol transmitted datagrams)
        self.p_stdscr.addstr('NET | network      |')
        self.p_stdscr.addstr(' ipi %sM |' % self.valstr(self.ss.get_metric_value('network.ip.inreceives'), 7))
        self.p_stdscr.addstr(' ipo %sM |' % self.valstr(self.ss.get_metric_value('network.ip.outrequests'), 7))
        self.p_stdscr.addstr(' ipfrw %sM |' % self.valstr(self.ss.get_metric_value('network.ip.forwdatagrams'), 5))
        self.p_stdscr.addstr(' deliv %sM |' % self.valstr(self.ss.get_metric_value('network.ip.indelivers'), 5))
        if self.apyx[1] >= 95:
            self.p_stdscr.addstr(' icmpi %s |' % self.valstr(self.ss.get_metric_value('network.icmp.inmsgs'), 6))
        if self.apyx[1] >= 110:
            self.p_stdscr.addstr(' icmpo %s |' % self.valstr(self.ss.get_metric_value('network.icmp.outmsgs'), 6))
        self.next_line()

        try:
            (inst, iname) = context.pmGetInDom(self.ss.metric_descs[self.ss.metrics_dict['network.interface.in.bytes']])
        except pmapi.pmErr as e:
            iname = iname = "X"
        net_metric = self.ss.get_metric_value('network.interface.in.bytes')
        if type(net_metric) == type([]):
            for j in range(len(self.ss.get_metric_value('network.interface.in.bytes'))):
                pcki = self.ss.get_scalar_value('network.interface.in.packets', j)
                pcko = self.ss.get_scalar_value('network.interface.out.packets', j)
                if pcki == 0 and pcko == 0:
                    continue
                self.p_stdscr.addstr('NET |')
                self.p_stdscr.addstr(' %-12s |' % (iname[j]))
                self.p_stdscr.addstr(' pcki %sM |' % self.valstr(pcki, 6))
                self.p_stdscr.addstr(' pcko %sM |' % self.valstr(pcko, 6))
                self.p_stdscr.addstr(' si %s Kbps |' % self.valstr(scale(self.ss.get_scalar_value('network.interface.in.bytes', j), 100000000), 4))
                self.p_stdscr.addstr(' so %s Kpbs |' % self.valstr(scale(self.ss.get_scalar_value('network.interface.out.bytes', j), 100000000), 4))
                if self.apyx[1] >= 95:
                    self.p_stdscr.addstr(' erri %sM |' % self.valstr(self.ss.get_scalar_value('network.interface.in.errors', j), 6))
                if self.apyx[1] >= 110:
                    self.p_stdscr.addstr(' erro %sM |' % self.valstr(self.ss.get_scalar_value('network.interface.out.errors', j), 6))
                if self.end_of_screen():
                    break
                self.next_line()


# _ProcPrint --------------------------------------------------


class _ProcPrint(_AtopPrint):
    def type_write(self, value):
        self._output_type = value
    output_type = property(None, type_write, None, None)

    def proc(self):
        if self._output_type in ['g']:
            self.p_stdscr.addstr('  PID  SYSCPU USRCPU   VGROW  RGROW RUID    THR ST EXC S CPU  CMD')
        elif self._output_type in ['m']:
            self.p_stdscr.addstr('PID ')
            if self.apyx[1] >= 110:
                self.p_stdscr.addstr('MINFLT MAJFLT ')
            else:
                self.p_stdscr.addstr('     ')
            if self.apyx[1] >= 95:
                self.p_stdscr.addstr('VSTEXT VSLIBS ')
            self.p_stdscr.addstr('VDATA VSTACK VGROW  RGROW  VSIZE   RSIZE MEM   CMD')
        self.next_line()

        # TODO Remember this state for Next/Previous Page
        cpu_time_sorted = list()
        for j in range(self.ss.get_metric_value('proc.nprocs')):
            cpu_time_sorted.append((j, self.ss.get_scalar_value('proc.psinfo.utime', j)
                                    +  self.ss.get_scalar_value('proc.psinfo.stime', j)))
        cpu_time_sorted.sort(key=lambda proc: proc[1], reverse=True)

        for i in range(len(cpu_time_sorted)):
            j = cpu_time_sorted[i][0]
            if self._output_type in ['g', 'm']:
                self.p_stdscr.addstr('%5d  ' % (self.ss.get_scalar_value('proc.psinfo.pid', j)))
            if self._output_type in ['g']:
                self.p_stdscr.addstr('%6s ' % minutes_seconds(self.ss.get_scalar_value('proc.psinfo.stime', j)))
                self.p_stdscr.addstr(' %6s ' % minutes_seconds(self.ss.get_scalar_value('proc.psinfo.utime', j)))
                self.p_stdscr.addstr('%s ' % self.memstr(self.ss.get_scalar_value('proc.psinfo.vsize', j), 5))
                self.p_stdscr.addstr('%s ' % self.memstr(self.ss.get_scalar_value('proc.psinfo.rss', j), 5))
                self.p_stdscr.addstr('%6s ' % (self.ss.get_scalar_value('proc.id.uid_nm', j)[0:6]))
                self.p_stdscr.addstr('%4d ' % self.ss.get_scalar_value('proc.psinfo.threads', j))
                self.p_stdscr.addstr('%3s ' % '--')
                state = self.ss.get_scalar_value('proc.psinfo.sname', j)
                self.p_stdscr.addstr(' %2s ' % '-')
                if state not in  ('D', 'R', 'S', 'T', 'W', 'X', 'Z'):
                    state = 'S'
                self.p_stdscr.addstr('%2s ' % (state))
                cpu_total = float(self.ss.cpu_total - self.ss.get_metric_value('kernel.all.cpu.idle'))
                proc_cpu_total = (self.ss.get_scalar_value('proc.psinfo.utime', j)
                                + self.ss.get_scalar_value('proc.psinfo.stime', j))
                if proc_cpu_total > cpu_total:
                    proc_percent = 0
                else:
                    proc_percent = (100 * proc_cpu_total / cpu_total)
                proc_command = self.ss.get_scalar_value('proc.psinfo.cmd', j)
                self.p_stdscr.addstr('%2d%% ' % proc_percent)
                self.p_stdscr.addstr('%-15s ' % proc_command)
            if self._output_type in ['m']:
                # Missing: SWAPSZ, proc.psinfo.nswap frequently returns -1
                if self.apyx[1] >= 110:
                    minf = self.ss.get_scalar_value('proc.psinfo.minflt', j)
                    majf = self.ss.get_scalar_value('proc.psinfo.maj_flt', j)
                    if minf < 0:
                        minf = 0
                    if majf < 0:
                        majf = 0
                    self.p_stdscr.addstr("%s " % self.valstr(minf, 3))
                    self.p_stdscr.addstr("%s " % self.valstr(majf, 3))
                if self.apyx[1] >= 95:
                    self.p_stdscr.addstr("%s " % self.memstr(self.ss.get_scalar_value('proc.memory.textrss', j) * self.ONEKBYTE, 7))
                    self.p_stdscr.addstr("%s " % self.memstr(self.ss.get_scalar_value('proc.memory.librss', j) * self.ONEKBYTE, 6))
                self.p_stdscr.addstr("%s " % self.memstr(self.ss.get_scalar_value('proc.memory.datrss', j) * self.ONEKBYTE, 6))
                self.p_stdscr.addstr("%s " % self.memstr(self.ss.get_scalar_value('proc.memory.vmstack', j) * self.ONEKBYTE, 6))
                self.p_stdscr.addstr("%s " % self.memstr(self.ss.get_scalar_value('proc.psinfo.vsize', j) * self.ONEKBYTE, 6))
                self.p_stdscr.addstr("%s " % self.memstr(self.ss.get_scalar_value('proc.psinfo.rss', j) * self.ONEKBYTE, 6))
                self.p_stdscr.addstr("%s " % self.memstr(self.ss.get_scalar_value('proc.psinfo.vsize', j) * self.ONEKBYTE, 6))
                self.p_stdscr.addstr("%s " % self.memstr(self.ss.get_scalar_value('proc.psinfo.rss', j) * self.ONEKBYTE, 6))
                val = float(self.ss.get_old_scalar_value('proc.psinfo.rss', j)) / float(self.ss.get_metric_value('mem.physmem')) * 100
                if val > 100:
                    val = 0
                self.p_stdscr.addstr('%2d%% ' % val)
                self.p_stdscr.addstr('%-15s' % (self.ss.get_scalar_value('proc.psinfo.cmd', j)))
            if self.end_of_screen():
                break
            self.next_line()


class _Options(object):
    def __init__(self):
        self.output_file = ""
        self.output_type = "g"
        self.create_archive = False
        self.replay_archive = False
        self.have_interval_arg = False
        self.interval_arg = 5
        self.width = 0
        self.n_samples = 0
        self.opts = self.setup()

    def setup(self):
        """ Setup default command line argument option handling """
        opts = pmapi.pmOptions()
        opts.pmSetOptionCallback(self.option_callback)
        opts.pmSetOverrideCallback(self.override)
        # leading - returns args that are not options with leading ^A
        opts.pmSetShortOptions("-gmw:r:L:h:a:V?")
        opts.pmSetLongOptionText("Interactive: [-g|-m] [-L linelen] [-h host | -a archive] [ interval [ samples ]]")
        opts.pmSetLongOptionText("Write folio: pmatop -w folio [ interval [ samples ]]")
        opts.pmSetLongOptionText("Read folio: pmatop -r folio [-g|-m] [-L linelen] [-h host]")
        opts.pmSetLongOptionHeader("Reporting Options")
        opts.pmSetLongOption("generic", 0, 'g', '', "Display generic metrics")
        opts.pmSetLongOption("memory", 0, 'm', '', "Display memory metrics")
        opts.pmSetLongOption("width", 1, 'L', 'WIDTH', "Width of the output")
        opts.pmSetLongOptionHeader("Folio Options")
        opts.pmSetLongOption("write", 1, 'w', 'FILENAME', "Write metric data to PCP archive folio")
        opts.pmSetLongOption("read", 1, 'r', 'FILENAME', "Read metric data from PCP archive folio")
        opts.pmSetLongOptionHeader("General Options")
        opts.pmSetLongOptionAlign()
        opts.pmSetLongOptionArchive()
        opts.pmSetLongOptionDebug()
        opts.pmSetLongOptionHost()
        opts.pmSetLongOptionOrigin()
        opts.pmSetLongOptionStart()
        opts.pmSetLongOptionFinish()
        opts.pmSetLongOptionVersion()
        opts.pmSetLongOptionTimeZone()
        opts.pmSetLongOptionHostZone()
        opts.pmSetLongOptionHelp()
        return opts


    def override(self, opt):
        """ Override a few standard PCP options to match free(1) """
        # pylint: disable=R0201
        if opt == 'g':
            return 1
        elif opt == "a":
            self.replay_archive = True
        elif opt == 'L':
            return 1
        return 0

    def option_callback(self, opt, optarg, index):
        """ Perform setup for an individual command line option """
        # pylint: disable=W0613

        if opt == "g":
            self.output_type = "g"
        elif opt == "m":
            self.output_type = "m"
        elif opt == "w":
            self.output_file = optarg
            self.create_archive = True
        elif opt == "r":
            self.opts.pmSetOptionArchiveFolio(optarg)
            self.replay_archive = True
        elif opt == "L":
            self.width = int(optarg)
        elif opt == "":
            if self.have_interval_arg == False:
                self.interval_arg = optarg
                self.have_interval_arg = True
            else:
                self.n_samples = int(optarg)


# main ----------------------------------------------------------------------


def main(stdscr_p):
    global stdscr
    stdscr = _StandardOutput(stdscr_p)
    sort = ""
    duration = 0.0
    i = 1

    ss = Subsystem()
    ss.init_processor_metrics()
    ss.init_memory_metrics()
    ss.init_disk_metrics()
    ss.init_network_metrics()
    ss.init_process_metrics()

    cpu = _ProcessorPrint(ss, stdscr)
    mem = _MemoryPrint(ss, stdscr)
    disk = _DiskPrint(ss, stdscr)
    net = _NetPrint(ss, stdscr)
    proc = _ProcPrint(ss, stdscr)

    proc.output_type = opts.output_type
    stdscr.width = opts.width

    pmc = pmapi.pmContext.fromOptions(opts.opts, sys.argv)
    (delta, errmsg) = pmc.pmParseInterval(str(opts.interval_arg) + " seconds")
    if pmc.type == c_api.PM_CONTEXT_ARCHIVE:
        pmc.pmSetMode(c_api.PM_MODE_FORW, delta, 0)

    host = pmc.pmGetContextHostName()

    ss.setup_metrics(pmc)

    if opts.create_archive:
        delta_seconds = c_api.pmtimevalToReal(delta.tv_sec, delta.tv_usec)
        msec = str(int(1000.0 * delta_seconds))
        configuration = "log mandatory on every " + msec + " milliseconds { "
        configuration += ss.dump_metrics()
        configuration += "}"
        if opts.n_samples != 0:
            duration = float(opts.n_samples) * delta_seconds
        else:
            duration = float(10) * delta_seconds
        status = record(pmgui.GuiClient(), configuration, duration, opts.output_file, host)
        if status != "":
            return status
        record_add_creator(opts.output_file)
        sys.exit(0)

    i_samples = 0

    disk.interval = delta.tv_sec
    disk.replay_archive = opts.replay_archive

    try:
        elapsed = ss.get_metric_value('kernel.all.uptime')
        while (i_samples < opts.n_samples) or (opts.n_samples == 0):
            ss.get_stats(pmc)
            stamp = pmc.pmCtime(ss.timestamp)
            stdscr.move(0, 0)
            stdscr.addstr('ATOP - %s                %s elapsed\n\n' % (
                    stamp.rstrip(), datetime.timedelta(0, elapsed)))
            elapsed = delta.tv_sec
            stdscr.move(2, 0)

            try:
                cpu.prc()
                cpu.cpu()
                mem.mem()
                disk.disk(pmc)
                net.net(pmc)
                proc.set_line()
                proc.proc()
            except pmapi.pmErr as e:
                return str(e) + " while processing " + str(ssx[0])
            except Exception as e: # catch all errors, pcp or python or other
                pass
            stdscr.move(proc.command_line, 0)
            stdscr.refresh()

            stdscr.timeout(delta.tv_sec * 1000)
            char = stdscr.getch()

            if char != -1:       # user typed a command
                try:
                    cmd = chr(char)
                except ValueError:
                    cmd = None
                if cmd == "q":
                    raise KeyboardInterrupt
                elif cmd == "":
                    stdscr.clear()
                    stdscr.refresh()
                elif cmd == "z":
                    stdscr.timeout(-1)
                    # currently it just does "hit any key to continue"
                    char = stdscr.getch()
                elif cmd == "h" or cmd == "?":
                    stdscr.clear ()
                    stdscr.move (0, 0)
                    stdscr.addstr ('\nOptions shown for active processes:\n')
                    stdscr.addstr ( "'g'  - generic info (default)\n")
                    stdscr.addstr ( "'m'  - memory details\n")
                    stdscr.addstr ( "Miscellaneous commands:\n")
                    stdscr.addstr ("'z'  - pause-button to freeze current sample (toggle)\n")
                    stdscr.addstr ("^L   - redraw the screen\n")
                    stdscr.addstr ("hit any key to continue\n")
                    stdscr.timeout(-1)
                    char = stdscr.getch()
                    stdscr.clear()
                elif cmd in ['g', 'm']:
                    stdscr.clear()
                    proc.output_type = cmd
                # TODO Next/Previous Page
                else:
                    stdscr.move(proc.command_line, 0)
                    stdscr.addstr("Invalid command %s\n" % (cmd), True)
                    stdscr.addstr("Type 'h' to see a list of valid commands", True)
                    stdscr.refresh()
                    time.sleep(2)
            i_samples += 1
    except KeyboardInterrupt:
        pass
    stdscr.refresh()
    time.sleep(1)
    return ""

def sigwinch_handler(n, frame):
    global stdscr
    curses.endwin()
    curses.initscr()
    # consume any subsequent characters awhile
    while 1:
        char = stdscr.getch()
        if char == -1:
            break

if __name__ == '__main__':
    global opts
    opts = _Options()
    if c_api.pmGetOptionsFromList(sys.argv) != 0:
        c_api.pmUsageMessage()
        sys.exit(1)

    if sys.stdout.isatty():
        signal.signal(signal.SIGWINCH, sigwinch_handler)
        try:
            status = curses.wrapper(main)   # pylint: disable-msg=C0103
        except curses.error as e:
            status = "Error in the curses module.  Try running " + ME + " in a larger window."
    else:                       # Output is piped or redirected
        status = main(sys.stdout)
    if status != "":
        print(status)
