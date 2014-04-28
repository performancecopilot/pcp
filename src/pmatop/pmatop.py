#!/usr/bin/python

#
# pmatop.py
#
# Copyright (C) 2013 Red Hat Inc.
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
import re
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

def usage ():
    return "\nUsage:" + sys.argv[0] + "\n\t[-g|-m] [-w FILE] [-r FILE] [-L width] Interval Trials"


def debug (mssg):
    fdesc = open("/tmp/,python", mode="a")
    if type(mssg) == type(""):
        fdesc.write(mssg)
    else:
        fdesc.write(str(mssg) + "\n")
    fdesc.close()


# scale  -------------------------------------------------------------


def scale (value, magnitude):
    return value / magnitude


# record ---------------------------------------------------------------

def record (host, context, config, interval, path):

    # -f saves the metrics in a directory
    if os.path.exists(path):
        return "playback directory %s already exists\n" % path
    try:
        status = context.pmRecordSetup (path, ME, 0) # pylint: disable=W0621
        (status, rhp) = context.pmRecordAddHost (host, 1, config)
        status = context.pmRecordControl (0, c_gui.PM_REC_SETARG, "-T" + str(interval) + "sec")
        status = context.pmRecordControl (0, c_gui.PM_REC_ON, "")
        time.sleep(interval)
        context.pmRecordControl (0, c_gui.PM_REC_STATUS, "")
        status = context.pmRecordControl (rhp, c_gui.PM_REC_OFF, "")
    except pmapi.pmErr, e:
        return "Cannot create PCP archive: " + path + " " + str(e)
    return ""

# record_add_creator ------------------------------------------------------

def record_add_creator (path):
    fdesc = open (path, "r+")
    args = ""
    for i in sys.argv:
        args = args + i + " "
    fdesc.write("# Created by " + args)
    fdesc.write("\n#\n")
    fdesc.close()

# minutes_seconds ----------------------------------------------------------


def minutes_seconds (milli):
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
        self._width = value
    width = property(None, width_write, None, None)

    def __init__(self, out):
        if (out == sys.stdout):
            self._width = 80
            self.stdout = True
        else:
            self.stdout = False
            self.so_stdscr = out
    def addstr(self, str):
        if self.stdout:
            sys.stdout.write(str)
        else:
            self.so_stdscr.addstr (str)
    def clear(self):
        if not self.stdout:
            self.so_stdscr.clear()
    def move(self, y, x):
        if not self.stdout:
            self.so_stdscr.move(y,x)
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
        self.apyx = a_stdscr.getmaxyx()
    def end_of_screen(self):
        return (self.p_stdscr.getyx()[0] >= self.apyx[0]-1)
    def set_line(self):
        self.command_line = self.p_stdscr.getyx()[0]
        self.p_stdscr.addstr ('\n')
    def next_line(self):
        if self.p_stdscr.stdout:
            print
            return
        line = self.p_stdscr.getyx()
        apy = line[0]
        if line[1] > 0:
            apy += 1
        self.p_stdscr.addstr (' ' * (self.apyx[1] - line[1]))
        self.p_stdscr.move(apy, 0)
    def put_value(self, form, value, try_percentd=0, try_magnitude=""):
        # Increase units of value
        if try_magnitude == "KM" and abs(value) > 1000:
            value /= 1000
            form = form.replace(try_magnitude[0], try_magnitude[1])
        # Use %d instead
        if try_percentd > 0 and abs(value) < try_percentd:
            iform = form.replace(form[form.index("."):form.index(".")+3],"d")
            return iform % value
        if value > 0:
            return re.sub ("([0-9]*\.*[0-9]+)e\+0*", " \\1e", form % value)
        else:
            return re.sub ("([0-9]*\.*[0-9]+)e\+0*", "-\\1e", form % abs(value))


# _ProcessorPrint --------------------------------------------------


class _ProcessorPrint(_AtopPrint):
# Missing: #trun (total # running threads) 
# Missing: #exit (requires accounting)
# Substitutions: proc.runq.sleeping for #tslpi (threads sleeping) 
# Substitutions: proc.runq.blocked for #tslpu (threads uninterrupt sleep)
    def prc(self):
        self.p_stdscr.addstr ('PRC |')
        self.p_stdscr.addstr (' sys %8s |' % (minutes_seconds(self.ss.get_metric_value('kernel.all.cpu.sys'))))
        self.p_stdscr.addstr (' user %7s |' % (minutes_seconds(self.ss.get_metric_value('kernel.all.cpu.user'))))
        self.p_stdscr.addstr (' #proc %6d |' % (self.ss.get_metric_value('kernel.all.nprocs')))
        if (self.apyx[1] >= 95):
            self.p_stdscr.addstr (' #tslpi %5d |' % (self.ss.get_metric_value('proc.runq.sleeping')))
        if (self.apyx[1] >= 110):
            self.p_stdscr.addstr (' #tslpu %5d |' % (self.ss.get_metric_value('proc.runq.blocked')))
        self.p_stdscr.addstr (' #zombie %4d' % (self.ss.get_metric_value('proc.runq.defunct')))
        self.next_line()
# Missing: curscal (current current scaling percentage)
    def cpu(self):
        self.ss.get_total()
        self.p_stdscr.addstr ('CPU |')
        self.p_stdscr.addstr (' sys %7d%% |' % (100 * self.ss.get_metric_value('kernel.all.cpu.sys') / self.ss.cpu_total))
        self.p_stdscr.addstr (' user %6d%% |' % (100 * self.ss.get_metric_value('kernel.all.cpu.user') / self.ss.cpu_total))
        self.p_stdscr.addstr (' irq %7d%% |' % (
                100 * self.ss.get_metric_value('kernel.all.cpu.irq.hard') / self.ss.cpu_total +
                100 * self.ss.get_metric_value('kernel.all.cpu.irq.soft') / self.ss.cpu_total))
        self.p_stdscr.addstr (' idle %6d%% |' % (100 * self.ss.get_metric_value('kernel.all.cpu.idle') / self.ss.cpu_total))
        self.p_stdscr.addstr (' wait %6d%% |' % (100 * self.ss.get_metric_value('kernel.all.cpu.wait.total') / self.ss.cpu_total))
        self.next_line()
        ncpu = self.ss.get_metric_value('hinv.ncpu')
        max_display_cpus = self.apyx[0] / 4
        for k in range(ncpu):
            percpu_sys = (100 * self.ss.get_scalar_value('kernel.percpu.cpu.sys', k) / self.ss.cpu_total)
            percpu_user = (100 * self.ss.get_scalar_value('kernel.percpu.cpu.user', k) / self.ss.cpu_total)
            if (percpu_sys == 0 and percpu_user == 0):
                continue
            self.p_stdscr.addstr ('cpu |')
            self.p_stdscr.addstr (' sys %7d%% |' % percpu_sys)
            self.p_stdscr.addstr (' user %6d%% |' % percpu_user)
            self.p_stdscr.addstr (' irq %7d%% |' % (
                    100 * self.ss.get_scalar_value('kernel.percpu.cpu.irq.hard', k) / self.ss.cpu_total +
                    100 * self.ss.get_scalar_value('kernel.percpu.cpu.irq.soft', k) / self.ss.cpu_total))
            self.p_stdscr.addstr (' idle %6d%% |' % (100 * self.ss.get_scalar_value('kernel.percpu.cpu.idle', k) / self.ss.cpu_total))
            self.p_stdscr.addstr (' cpu%02d %5d%% |' % (k, 100 * self.ss.get_scalar_value('kernel.percpu.cpu.wait.total', k) / self.ss.cpu_total))
            if (self.apyx[1] >= 95):
                self.p_stdscr.addstr (self.put_value(' curf %4.2gMHz |', scale(self.ss.get_scalar_value('hinv.cpu.clock', k), 1000)))
            self.next_line()
            if (ncpu > max_display_cpus and k >= max_display_cpus):
                break

        self.p_stdscr.addstr ('CPL |')
        self.p_stdscr.addstr (' avg1 %7.3g |' % (self.ss.get_scalar_value('kernel.all.load', 0)))
        self.p_stdscr.addstr (' avg5 %7.3g |' % (self.ss.get_scalar_value('kernel.all.load', 1)))
        self.p_stdscr.addstr (' avg15 %6.3g |' % (self.ss.get_scalar_value('kernel.all.load', 2)))
        self.p_stdscr.addstr (self.put_value(' csw %9.2g |', self.ss.get_metric_value('kernel.all.pswitch'), 10000))
        self.p_stdscr.addstr (self.put_value(' intr %8.2g |', self.ss.get_metric_value('kernel.all.intr'), 10000))
        if (self.apyx[1] >= 110):
            self.p_stdscr.addstr ('              |')
        if (self.apyx[1] >= 95):
            self.p_stdscr.addstr (' numcpu   %2d  |' % (self.ss.get_metric_value('hinv.ncpu')))
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
        try:
            (inst, iname) = context.pmGetInDom(self.ss.metric_descs [self.ss.metrics_dict['disk.partitions.read']])
        except pmapi.pmErr, e:
            iname = iname = "X"

# Missing: LVM avq (average queue depth)

        lvms = dict(map(lambda x: (os.path.realpath("/dev/mapper/" + x)[5:],x),
                         (os.listdir("/dev/mapper"))))

        for j in xrange(self.ss.get_len(self.ss.get_metric_value('disk.partitions.read'))):
            if (self._replay_archive == True):
                if iname[j][:2] != "dm":
                    continue
                lvm = iname[j]
            else:
                if iname[j] not in lvms:
                    continue
                lvm = lvms[iname[j]]
            partitions_read = self.ss.get_scalar_value('disk.partitions.read', j)
            partitions_write = self.ss.get_scalar_value('disk.partitions.write', j)
            if (partitions_read == 0 and partitions_write == 0):
                continue
            self.p_stdscr.addstr ('LVM |')
            self.p_stdscr.addstr (' %-12s |' % (lvm[len(lvm)-12:]))
            self.p_stdscr.addstr ('              |')
            self.p_stdscr.addstr (self.put_value(' read %7.3g |', partitions_read))
            self.p_stdscr.addstr (self.put_value(' write %6.2g |', partitions_write,10000))
            if (self.apyx[1] >= 95):
                val = (float(self.ss.get_scalar_value('disk.partitions.blkread', j)) / float(self._interval * 1000)) * 100
                self.p_stdscr.addstr (self.put_value(' MBr/s %6.3g |', val))
            if (self.apyx[1] >= 110):
                val = (float(self.ss.get_scalar_value('disk.partitions.blkwrite', j)) / float(self._interval * 1000)) * 100
                self.p_stdscr.addstr (self.put_value(' MBw/s %6.3g |', val))
            if (self.end_of_screen()):
                break;
            self.next_line()

        try:
            (inst, iname) = context.pmGetInDom(self.ss.metric_descs [self.ss.metrics_dict['disk.dev.read']])
        except pmapi.pmErr, e:
            iname = iname = "X"

        for j in xrange(self.ss.get_len(self.ss.get_metric_value('disk.dev.read_bytes'))):
            self.p_stdscr.addstr ('DSK |')
            self.p_stdscr.addstr (' %-12s |' % (iname[j]))
            busy = (float(self.ss.get_scalar_value('disk.dev.avactive', j)) / float(self._interval * 1000)) * 100
            if (busy > 100): busy = 0
            self.p_stdscr.addstr (' busy %6d%% |' % (busy)) # self.ss.get_scalar_value('disk.dev.avactive', j)
            val = self.ss.get_scalar_value('disk.dev.read', j)
            self.p_stdscr.addstr (' read %7d |' % (val))
            self.p_stdscr.addstr (self.put_value(' write %6.2g |', self.ss.get_scalar_value('disk.dev.write', j),1000))
            if (self.apyx[1] >= 95):
                val = (float(self.ss.get_scalar_value('disk.partitions.blkread', j)) / float(self._interval * 1000)) * 100
                self.p_stdscr.addstr (self.put_value(' MBr/s %6.3g |', val))
            if (self.apyx[1] >= 110):
                val = (float(self.ss.get_scalar_value('disk.partitions.blkwrite', j)) / float(self._interval * 1000)) * 100
                self.p_stdscr.addstr (self.put_value(' MBw/s %6.3g |', val))
            try:
                avio = (float(self.ss.get_scalar_value('disk.dev.avactive', j)) / float(self.ss.get_scalar_value('disk.dev.total', j)))
            except ZeroDivisionError:
                avio = 0
            self.p_stdscr.addstr (' avio %4.2g ms |' % (avio))
            if (self.end_of_screen()):
                break;
            self.next_line()


# _MemoryPrint --------------------------------------------------


class _MemoryPrint(_AtopPrint):
# Missing: shrss (resident shared memory size)
    def mem(self):
        self.p_stdscr.addstr ('MEM |')
        self.p_stdscr.addstr (' tot %7dM |' % (scale(self.ss.get_metric_value('mem.physmem'), 1000)))
        self.p_stdscr.addstr (' free %6dM |' % (scale(self.ss.get_metric_value('mem.freemem'), 1000)))
        self.p_stdscr.addstr (' cache %5dM |' % (scale(self.ss.get_metric_value('mem.util.cached'), 1000)))
        self.p_stdscr.addstr (' buff %6dM |' % (scale(self.ss.get_metric_value('mem.util.bufmem'), 1000)))
        self.p_stdscr.addstr (' slab %6dM |' % (scale(self.ss.get_metric_value('mem.util.slab'), 1000)))
        if (self.apyx[1] >= 95):
            self.p_stdscr.addstr (' #shmem %4dM |' % (scale(self.ss.get_metric_value('mem.util.shmem'), 1000)))
        self.next_line()

        self.p_stdscr.addstr ('SWP |')
        self.p_stdscr.addstr (' tot %7dG |' % (scale(self.ss.get_metric_value('mem.util.swapTotal'), 1000000)))
        self.p_stdscr.addstr (' free %6dG |' % (scale(self.ss.get_metric_value('mem.util.swapFree'), 1000000)))
        self.p_stdscr.addstr ('              |')
        self.p_stdscr.addstr (' vmcom %5dG |' % (scale(self.ss.get_metric_value('mem.util.committed_AS'), 1000000)))
        self.p_stdscr.addstr (' vmlim %5dG |' % (scale(self.ss.get_metric_value('mem.util.commitLimit'), 1000000)))
        self.next_line()

        self.p_stdscr.addstr ('PAG |')
        self.p_stdscr.addstr (' scan %7d |' % (self.ss.get_metric_value('mem.vmstat.slabs_scanned')))
        self.p_stdscr.addstr (' steal %6d |' % (self.ss.get_metric_value('mem.vmstat.pginodesteal')))
        self.p_stdscr.addstr (' stall %6d |' % (self.ss.get_metric_value('mem.vmstat.allocstall')))
        self.p_stdscr.addstr (' swin %7d |' % (self.ss.get_metric_value('mem.vmstat.pswpin')))
        self.p_stdscr.addstr (' swout %6d |' % (self.ss.get_metric_value('mem.vmstat.pswpout')))
        self.next_line()


# _NetPrint --------------------------------------------------


class _NetPrint(_AtopPrint):
    def net(self, context):
        if (self.end_of_screen()):
            return
        self.p_stdscr.addstr ('NET | transport    |')
        self.p_stdscr.addstr (self.put_value(' tcpi %6.2gM |', self.ss.get_metric_value('network.tcp.insegs')))
        self.p_stdscr.addstr (self.put_value(' tcpo %6.2gM |', self.ss.get_metric_value('network.tcp.outsegs')))
        self.p_stdscr.addstr (self.put_value(' udpi %6.2gM |', self.ss.get_metric_value('network.udp.indatagrams')))
        self.p_stdscr.addstr (self.put_value(' udpo %6.2gM |', self.ss.get_metric_value('network.udp.outdatagrams')))
        if (self.apyx[1] >= 95):
            self.p_stdscr.addstr (self.put_value(' tcpao %5.2gM |', self.ss.get_metric_value('network.tcp.activeopens')))
        if (self.apyx[1] >= 110):
            self.p_stdscr.addstr (self.put_value(' tcppo %5.2gM |', self.ss.get_metric_value('network.tcp.passiveopens')))
        self.next_line()

# Missing: icmpi (internet control message protocol received datagrams)
# Missing: icmpo (internet control message protocol transmitted datagrams)
        self.p_stdscr.addstr ('NET | network      |')
        self.p_stdscr.addstr (self.put_value(' ipi %7.2gM |', self.ss.get_metric_value('network.ip.inreceives')))
        self.p_stdscr.addstr (self.put_value(' ipo %7.2gM |', self.ss.get_metric_value('network.ip.outrequests')))
        self.p_stdscr.addstr (self.put_value(' ipfrw %5.2gM |', self.ss.get_metric_value('network.ip.forwdatagrams')))
        self.p_stdscr.addstr (self.put_value(' deliv %5.2gM |', self.ss.get_metric_value('network.ip.indelivers')))
        if (self.apyx[1] >= 95):
            self.p_stdscr.addstr (' icmpi %6d |' % (self.ss.get_metric_value('network.icmp.inmsgs')))
        if (self.apyx[1] >= 110):
            self.p_stdscr.addstr (' icmpo %6d |' % (self.ss.get_metric_value('network.icmp.outmsgs')))
        self.next_line()

        try:
            (inst, iname) = context.pmGetInDom(self.ss.metric_descs [self.ss.metrics_dict['network.interface.in.bytes']])
        except pmapi.pmErr, e:
            iname = iname = "X"
        net_metric = self.ss.get_metric_value('network.interface.in.bytes')
        if type(net_metric) == type([]):
            for j in xrange(len(self.ss.get_metric_value('network.interface.in.bytes'))):
                pcki = self.ss.get_scalar_value('network.interface.in.packets', j)
                pcko = self.ss.get_scalar_value('network.interface.out.packets', j)
                if (pcki == 0 and pcko == 0):
                    continue
                self.p_stdscr.addstr ('NET |')
                self.p_stdscr.addstr (' %-12s |' % (iname[j]))
                self.p_stdscr.addstr (self.put_value(' pcki %6.2gM |', pcki))
                self.p_stdscr.addstr (self.put_value(' pcko %6.2gM |', pcko))
                self.p_stdscr.addstr (self.put_value(' si %4d Kbps |', scale (self.ss.get_scalar_value('network.interface.in.bytes', j), 100000000)))
                self.p_stdscr.addstr (self.put_value(' so %4d Kpbs |', scale (self.ss.get_scalar_value('network.interface.out.bytes', j), 100000000)))
                if (self.apyx[1] >= 95):
                    self.p_stdscr.addstr (self.put_value(' erri %6.2gM |', self.ss.get_scalar_value('network.interface.in.errors', j)))
                if (self.apyx[1] >= 110):
                    self.p_stdscr.addstr (self.put_value(' erro %6.2gM |', self.ss.get_scalar_value('network.interface.out.errors', j)))
                if (self.end_of_screen()):
                    break;
                self.next_line()


# _ProcPrint --------------------------------------------------


class _ProcPrint(_AtopPrint):
    def type_write(self, value):
        self._output_type = value
    output_type = property(None, type_write, None, None)

    @staticmethod
    def sort_l(l1, l2):
        if (l1[1] < l2[1]):
            return -1
        elif (l1[1] > l2[1]):
            return 1
        else: return 0

    def proc(self):
        current_yx = self.p_stdscr.getyx()
        if self._output_type in ['g']:
            self.p_stdscr.addstr ('  PID SYSCPU USRCPU VGROW    RGROW  RUID   THR ST EXC S CPU  CMD')
        elif self._output_type in ['m']:
            self.p_stdscr.addstr ('PID ')
            if (self.apyx[1] >= 110):
                self.p_stdscr.addstr ('MINFLT MAJFLT ')
            else:
                self.p_stdscr.addstr ('     ')
            if (self.apyx[1] >= 95):
                self.p_stdscr.addstr ('VSTEXT VSLIBS ')
            self.p_stdscr.addstr ('VDATA  VSTACK     VGROW   RGROW  VSIZE    RSIZE     MEM   CMD')
        self.next_line()

        # TODO Remember this state for Next/Previous Page
        cpu_time_sorted = list()
        for j in xrange(self.ss.get_metric_value('proc.nprocs')):
            cpu_time_sorted.append((j, self.ss.get_scalar_value('proc.psinfo.utime', j)
                                    +  self.ss.get_scalar_value('proc.psinfo.stime', j)))
        cpu_time_sorted.sort(self.sort_l,reverse=True)

        for i in xrange(len(cpu_time_sorted)):
            j = cpu_time_sorted[i][0]
            if self._output_type in ['g', 'm']:
                self.p_stdscr.addstr ('%5d  ' % (self.ss.get_scalar_value('proc.psinfo.pid', j)))
            if self._output_type in ['g']:
                self.p_stdscr.addstr ('%5s ' % minutes_seconds (self.ss.get_scalar_value('proc.psinfo.stime', j)))
                self.p_stdscr.addstr (' %5s ' % minutes_seconds (self.ss.get_scalar_value('proc.psinfo.utime', j)))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.ss.get_scalar_value('proc.psinfo.vsize', j), 10000, "KM"))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.ss.get_scalar_value('proc.psinfo.rss', j), 10000, "KM"))
                self.p_stdscr.addstr ('%5s ' % (self.ss.get_scalar_value('proc.id.uid_nm', j)))
                self.p_stdscr.addstr ('%5d ' % self.ss.get_scalar_value('proc.psinfo.threads', j))
                self.p_stdscr.addstr ('%3s ' % '--')
                state = self.ss.get_scalar_value('proc.psinfo.sname', j)
                self.p_stdscr.addstr ('%2s ' % '-')
                if state not in  ('D', 'R', 'S', 'T', 'W', 'X', 'Z'):
                    state = 'S'
                self.p_stdscr.addstr ('%2s ' % (state))
                cpu_total = float(self.ss.cpu_total - self.ss.get_metric_value('kernel.all.cpu.idle'))
                proc_cpu_total = (self.ss.get_scalar_value('proc.psinfo.utime', j) + self.ss.get_scalar_value('proc.psinfo.stime', j))
                if proc_cpu_total > cpu_total:
                    proc_percent = 0
                else:
                    proc_percent = (100 * proc_cpu_total / cpu_total)
                self.p_stdscr.addstr ('%2d%% ' % proc_percent)
                self.p_stdscr.addstr ('%-15s ' % (self.ss.get_scalar_value('proc.psinfo.cmd', j)))
            if self._output_type in ['m']:
                # Missing: SWAPSZ, proc.psinfo.nswap frequently returns -1
                if (self.apyx[1] >= 110):
                    minf = self.ss.get_scalar_value('proc.psinfo.minflt', j)
                    majf = self.ss.get_scalar_value('proc.psinfo.maj_flt', j)
                    if minf < 0:
                        minf = 0
                    if majf < 0:
                        majf = 0
                    self.p_stdscr.addstr (self.put_value('%3.0g ', minf, 1000))
                    self.p_stdscr.addstr (self.put_value('%3.0g ', majf, 1000))
                if (self.apyx[1] >= 95):
                    self.p_stdscr.addstr (self.put_value('%6.3gK ', self.ss.get_scalar_value('proc.memory.textrss', j), 10000))
                    # Missing: VSLIBS librss seems to always be 0
                    self.p_stdscr.addstr (self.put_value('%4.2gK ', self.ss.get_scalar_value('proc.memory.librss', j)))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.ss.get_scalar_value('proc.memory.datrss', j), 10000, "KM"))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.ss.get_scalar_value('proc.memory.vmstack', j), 10000, "KM"))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.ss.get_scalar_value('proc.psinfo.vsize', j), 10000, "KM"))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.ss.get_scalar_value('proc.psinfo.rss', j), 10000, "KM"))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.ss.get_old_scalar_value('proc.psinfo.vsize', j), 10000, "KM"))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.ss.get_old_scalar_value('proc.psinfo.rss', j), 10000, "KM"))
                val = float(self.ss.get_old_scalar_value('proc.psinfo.rss', j)) / float(self.ss.get_metric_value('mem.physmem')) * 100
                if val > 100: val = 0
                self.p_stdscr.addstr (self.put_value('%2d%% ', val))
                self.p_stdscr.addstr ('%-15s' % (self.ss.get_scalar_value('proc.psinfo.cmd', j)))
            if (self.end_of_screen()):
                break;
            self.next_line()


# main ----------------------------------------------------------------------


def main (stdscr_p):
    global stdscr
    stdscr = _StandardOutput(stdscr_p)
    output_file = ""
    input_file = ""
    sort = ""
    duration = 0
    interval_arg = 5
    duration_arg = 0
    n_samples = 0
    output_type = "g"
    host = ""
    create_archive = False
    replay_archive = False
    i = 1

    subsys_options = ("g", "m")

    class NextOption(Exception):
        pass

    while i < len(sys.argv):
        try:
            if (sys.argv[i][:1] == "-"):
                for ssx in subsys_options:
                    if sys.argv[i][1:] == ssx:
                        output_type = ssx
                        raise NextOption
                if (sys.argv[i] == "-w"):
                    i += 1
                    output_file = sys.argv[i]
                    create_archive = True
                elif (sys.argv[i] == "-r"):
                    i += 1
                    input_file = sys.argv[i]
                    replay_archive = True
                elif (sys.argv[i] == "-L"):
                    i += 1
                    stdscr.width = int(sys.argv[i])
                elif (sys.argv[i] == "--help"):
                    return usage()
                elif (sys.argv[i] == "-h"):
                    i += 1
                    host = sys.argv[i]
                else:
                    return sys.argv[0] + ": Unknown option " + sys.argv[i] \
                        + "\nTry `" + sys.argv[0] + " --help' for more information."
            else:
                interval_arg = int(sys.argv[i])
                i += 1
                if (i < len(sys.argv)):
                    n_samples = int(sys.argv[i])
            i += 1
        except NextOption:
            i += 1
            pass

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
    proc.output_type = output_type

    if replay_archive:
        archive = input_file
        if not os.path.exists(input_file):
            return input_file + " does not exist"
        for line in open(input_file):
            if (line[:8] == "Archive:"):
                tokens = line[:-1].split()
                archive = os.path.join(os.path.dirname(input_file), tokens[2])
        try:
            pmc = pmapi.pmContext(c_api.PM_CONTEXT_ARCHIVE, archive)
        except pmapi.pmErr, e:
            return "Cannot open PCP archive: " + archive
    else:
        if host == "":
            host = "local:"
        try:
            pmc = pmapi.pmContext(target=host)
        except pmapi.pmErr, e:
            return "Cannot connect to pmcd on " + host

    if duration_arg != 0:
        (timeval, errmsg) = pmc.pmParseInterval(duration_arg)
        if code < 0:
            return errmsg
        duration = timeval.tv_sec

    ss.setup_metrics (pmc)

    if create_archive:
        configuration = "log mandatory on every " + \
            str(interval_arg) + " seconds { "
        configuration += ss.dump_metrics()
        configuration += "}"
        if duration == 0:
            if n_samples != 0:
                duration = n_samples * interval_arg
            else:
                duration = 10 * interval_arg
        status = record (host, pmgui.GuiClient(), configuration, duration, output_file)
        if status != "":
            return status
        record_add_creator (output_file)
        sys.exit(0)

    i_samples = 0
    subsys_cmds = ['g', 'm']

    (delta, errmsg) = pmc.pmParseInterval(str(interval_arg) + " seconds")
    disk.interval = delta.tv_sec
    disk.replay_archive = replay_archive

    try:
        elapsed = ss.get_metric_value('kernel.all.uptime')
        while (i_samples < n_samples) or (n_samples == 0):
            ss.get_stats (pmc)
            stdscr.move (0, 0)
            stdscr.addstr ('ATOP - %s\t\t%s elapsed\n\n' % (
                    time.strftime("%c"), 
                    datetime.timedelta(0, elapsed)))
            elapsed = delta.tv_sec
#            mm.get_stats(pmc)
            stdscr.move (2, 0)

            try:
                cpu.prc()
                cpu.cpu()
                mem.mem()
                disk.disk(pmc)
                net.net(pmc)
                proc.set_line()
                proc.proc()
            except pmapi.pmErr, e:
                return str(e) + " while processing " + str(ssx[0])
            except Exception, e: # catch all errors, pcp or python or otherwise
                pass
            stdscr.move (proc.command_line, 0)
            stdscr.refresh()

            stdscr.timeout(delta.tv_sec * 1000)
            char = stdscr.getch()

            if (char != -1):       # user typed a command
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
                elif cmd == "h":
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
                    stdscr.clear ()
                elif cmd in subsys_cmds:
                    stdscr.clear()
                    proc.output_type = cmd
                # TODO Next/Previous Page
                else:
                    stdscr.move (proc.command_line, 0)
                    stdscr.addstr ("Invalid command %s\nType 'h' to see a list of valid commands" % (cmd))
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
        if (char == -1):
            break

if __name__ == '__main__':
    if sys.stdout.isatty():
        signal.signal(signal.SIGWINCH, sigwinch_handler)
        status = curses.wrapper(main)   # pylint: disable-msg=C0103
        # You're running in a real terminal
    else:
        status = main(sys.stdout)
        # You're being piped or redirected
    if (status != ""):
        print status
