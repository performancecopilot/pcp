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
http://oss.sgi.com/projects/pcp
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
import curses
import select
import signal
import pwd
import cpmapi as c_api
import cpmgui as c_gui
from pcp import pmapi, pmgui
from pcp.pmsubsys import Processor, Interrupt, Disk, Memory, Network, Process, Subsystem

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

def record (context, config, interval, path):

    # -f saves the metrics in a directory
    if os.path.exists(path):
        return "playback directory %s already exists\n" % path
    status = context.pmRecordSetup (path, ME, 0) # pylint: disable=W0621
    (status, rhp) = context.pmRecordAddHost ("localhost", 1, config)
    status = context.pmRecordControl (0, c_gui.PM_REC_SETARG, "-T" + str(interval) + "sec")
    status = context.pmRecordControl (0, c_gui.PM_REC_ON, "")
    time.sleep(interval)
    context.pmRecordControl (0, c_gui.PM_REC_STATUS, "")
    status = context.pmRecordControl (rhp, c_gui.PM_REC_OFF, "")
    if status < 0 and status != c_api.PM_ERR_IPC:
        check_status (status)


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
    sec, milli = divmod(milli, 1000)
    tenth, milli = divmod(milli, 100)
    milli = milli / 10
    min, sec = divmod(sec, 60)
    hour, min = divmod(min, 60)
    day, hour = divmod(hour, 24)
    if hour > 0:
        return "%dh%dm" % (day, hour)
    elif min > 0:
        return "%dm%ds" % (min, sec)
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
    def set_line(self):
        self.command_line = self.p_stdscr.getyx()[0]
        self.p_stdscr.addstr ('\n')
    def set_stdscr(self, a_stdscr):
        self.p_stdscr = a_stdscr
        self.apyx = a_stdscr.getmaxyx()
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
    def put_value(self, form, value, try_percentd=0):
        if try_percentd > 0 and abs(value) < try_percentd:
            iform = form.replace(form[form.index("."):form.index(".")+3],"d")
            return iform % value
        if value > 0:
            return re.sub ("([0-9]*\.*[0-9]+)e\+0", " \\1e", form % value)
        else:
            return re.sub ("([0-9]*\.*[0-9]+)e\+0", "-\\1e", form % abs(value))


# _ProcessorPrint --------------------------------------------------


class _ProcessorPrint(_AtopPrint, Processor):
# Missing: #trun (total # running threads) 
# Missing: #exit (requires accounting)
# Substitutions: proc.runq.sleeping for #tslpi (threads sleeping) 
# Substitutions: proc.runq.blocked for #tslpu (threads uninterrupt sleep)
    def prc(self):
        self.p_stdscr.addstr ('PRC |')
        self.p_stdscr.addstr (' sys %8s |' % (minutes_seconds(self.get_metric_value('kernel.all.cpu.sys'))))
        self.p_stdscr.addstr (' user %7s |' % (minutes_seconds(self.get_metric_value('kernel.all.cpu.user'))))
        self.p_stdscr.addstr (' #proc %6d |' % (self.get_metric_value('kernel.all.nprocs')))
        if (self.apyx[1] >= 95):
            self.p_stdscr.addstr (' #tslpi %5d |' % (self.get_metric_value('proc.runq.sleeping')))
        if (self.apyx[1] >= 110):
            self.p_stdscr.addstr (' #tslpu %5d |' % (self.get_metric_value('proc.runq.blocked')))
        self.p_stdscr.addstr (' #zombie %4d' % (self.get_metric_value('proc.runq.defunct')))
        self.next_line()
# Missing: curscal (current current scaling percentage)
    def cpu(self):
        self.get_total()
        self.p_stdscr.addstr ('CPU |')
        self.p_stdscr.addstr (' sys %7d%% |' % (100 * self.get_metric_value('kernel.all.cpu.sys') / self.cpu_total))
        self.p_stdscr.addstr (' user %6d%% |' % (100 * self.get_metric_value('kernel.all.cpu.user') / self.cpu_total))
        self.p_stdscr.addstr (' irq %7d%% |' % (
                100 * self.get_metric_value('kernel.all.cpu.irq.hard') / self.cpu_total +
                100 * self.get_metric_value('kernel.all.cpu.irq.soft') / self.cpu_total))
        self.p_stdscr.addstr (' idle %6d%% |' % (100 * self.get_metric_value('kernel.all.cpu.idle') / self.cpu_total))
        self.p_stdscr.addstr (' wait %6d%% |' % (100 * self.get_metric_value('kernel.all.cpu.wait.total') / self.cpu_total))
        self.next_line()
        for k in range(self.get_metric_value('hinv.ncpu')):
            self.p_stdscr.addstr ('cpu |')
            self.p_stdscr.addstr (' sys %7d%% |' % (100 * self.get_scalar_value('kernel.percpu.cpu.sys', k) / self.cpu_total))
            self.p_stdscr.addstr (' user %6d%% |' % (100 * self.get_scalar_value('kernel.percpu.cpu.user', k) / self.cpu_total))
            self.p_stdscr.addstr (' irq %7d%% |' % (
                    100 * self.get_scalar_value('kernel.percpu.cpu.irq.hard', k) / self.cpu_total +
                    100 * self.get_scalar_value('kernel.percpu.cpu.irq.soft', k) / self.cpu_total))
            self.p_stdscr.addstr (' idle %6d%% |' % (100 * self.get_scalar_value('kernel.percpu.cpu.idle', k) / self.cpu_total))
            self.p_stdscr.addstr (' cpu%02d %5d%% |' % (k, 100 * self.get_scalar_value('kernel.percpu.cpu.wait.total', k) / self.cpu_total))
            if (self.apyx[1] >= 95):
                self.p_stdscr.addstr (self.put_value(' curf %4.2gMHz |', scale(self.get_scalar_value('hinv.cpu.clock', k), 1000)))
            self.next_line()

        self.p_stdscr.addstr ('CPL |')
        self.p_stdscr.addstr (' avg1 %7.3g |' % (self.get_scalar_value('kernel.all.load', 0)))
        self.p_stdscr.addstr (' avg5 %7.3g |' % (self.get_scalar_value('kernel.all.load', 1)))
        self.p_stdscr.addstr (' avg15 %6.3g |' % (self.get_scalar_value('kernel.all.load', 2)))
        self.p_stdscr.addstr (self.put_value(' csw %9.3g |', self.get_metric_value('kernel.all.pswitch'), 10000))
        self.p_stdscr.addstr (self.put_value(' intr %7.3g |', self.get_metric_value('kernel.all.intr'), 10000))
        if (self.apyx[1] >= 110):
            self.p_stdscr.addstr ('              |')
        if (self.apyx[1] >= 95):
            self.p_stdscr.addstr (' numcpu   %2d  |' % (self.get_metric_value('hinv.ncpu')))
        self.next_line()

# _InterruptPrint --------------------------------------------------


class _InterruptPrint(_AtopPrint, Interrupt):
    pass


# _DiskPrint --------------------------------------------------


class _DiskPrint(_AtopPrint, Disk):
    def interval_write(self, value):
        self._interval = value
    interval = property(None, interval_write, None, None)

    def disk(self, context):
        try:
            (inst, iname) = context.pmGetInDom(self.metric_descs [self.metrics_dict['disk.partitions.read']])
        except pmapi.pmErr, e:
            iname = iname = "X"

# Missing: LVM avq (average queue depth)

        lvms = dict(map(lambda x: (os.path.realpath("/dev/mapper/" + x)[5:],x),
                         (os.listdir("/dev/mapper"))))

        for j in xrange(self.get_len(self.get_metric_value('disk.partitions.read'))):
            if iname[j] not in lvms:
                continue
            self.p_stdscr.addstr ('LVM |')
            lvm = lvms[iname[j]]
            self.p_stdscr.addstr (' %-12s |' % (lvm[len(lvm)-12:]))
            self.p_stdscr.addstr ('              |')
            self.p_stdscr.addstr (self.put_value(' read %7.3g |', self.get_scalar_value('disk.partitions.read', j)))
            self.p_stdscr.addstr (self.put_value(' write %6.3g |', self.get_scalar_value('disk.partitions.write', j)))
            if (self.apyx[1] >= 95):
                val = (float(self.get_scalar_value('disk.partitions.blkread', j)) / float(self._interval * 1000)) * 100
                self.p_stdscr.addstr (self.put_value(' MBr/s %6.3g |', val))
            if (self.apyx[1] >= 110):
                val = (float(self.get_scalar_value('disk.partitions.blkwrite', j)) / float(self._interval * 1000)) * 100
                self.p_stdscr.addstr (self.put_value(' MBw/s %6.3g |', val))
            self.next_line()

        try:
            (inst, iname) = context.pmGetInDom(self.metric_descs [self.metrics_dict['disk.dev.read']])
        except pmapi.pmErr, e:
            iname = iname = "X"

        for j in xrange(self.get_len(self.get_metric_value('disk.dev.read_bytes'))):
            self.p_stdscr.addstr ('DSK |')
            self.p_stdscr.addstr (' %-12s |' % (iname[j]))
            busy = (float(self.get_scalar_value('disk.dev.avactive', j)) / float(self._interval * 1000)) * 100
            self.p_stdscr.addstr (' busy %6d%% |' % (busy)) # self.get_scalar_value('disk.dev.avactive', j)
            val = self.get_scalar_value('disk.dev.read', j)
            self.p_stdscr.addstr (' read %7d |' % (val))
            self.p_stdscr.addstr (self.put_value(' write %6.3g |', self.get_scalar_value('disk.dev.write', j)))
            if (self.apyx[1] >= 95):
                val = (float(self.get_scalar_value('disk.partitions.blkread', j)) / float(self._interval * 1000)) * 100
                self.p_stdscr.addstr (self.put_value(' MBr/s %6.3g |', val))
            if (self.apyx[1] >= 110):
                val = (float(self.get_scalar_value('disk.partitions.blkwrite', j)) / float(self._interval * 1000)) * 100
                self.p_stdscr.addstr (self.put_value(' MBw/s %6.3g |', val))
            try:
                avio = (float(self.get_scalar_value('disk.dev.avactive', j)) / float(self.get_scalar_value('disk.dev.total', j)))
            except ZeroDivisionError:
                avio = 0
            self.p_stdscr.addstr (' avio %4.2g ms |' % (avio))
            self.next_line()


# _MemoryPrint --------------------------------------------------


class _MemoryPrint(_AtopPrint, Memory):
# Missing: shrss (resident shared memory size)
    def mem(self):
        self.p_stdscr.addstr ('MEM |')
        self.p_stdscr.addstr (' tot %7dM |' % (scale(self.get_metric_value('mem.physmem'), 1000)))
        self.p_stdscr.addstr (' free %6dM |' % (scale(self.get_metric_value('mem.freemem'), 1000)))
        self.p_stdscr.addstr (' cache %5dM |' % (scale(self.get_metric_value('mem.util.cached'), 1000)))
        self.p_stdscr.addstr (' buff %6dM |' % (scale(self.get_metric_value('mem.util.bufmem'), 1000)))
        self.p_stdscr.addstr (' slab %6dM |' % (scale(self.get_metric_value('mem.util.slab'), 1000)))
        if (self.apyx[1] >= 95):
            self.p_stdscr.addstr (' #shmem %4dM |' % (scale(self.get_metric_value('mem.util.shmem'), 1000)))
        self.next_line()

        self.p_stdscr.addstr ('SWP |')
        self.p_stdscr.addstr (' tot %7dG |' % (scale(self.get_metric_value('mem.util.swapTotal'), 1000000)))
        self.p_stdscr.addstr (' free %6dG |' % (scale(self.get_metric_value('mem.util.swapFree'), 1000000)))
        self.p_stdscr.addstr ('              |')
        self.p_stdscr.addstr (' vmcom %5dG |' % (scale(self.get_metric_value('mem.util.committed_AS'), 1000000)))
        self.p_stdscr.addstr (' vmlim %5dG |' % (scale(self.get_metric_value('mem.util.commitLimit'), 1000000)))
        self.next_line()

        self.p_stdscr.addstr ('PAG |')
        self.p_stdscr.addstr (' scan %7d |' % (self.get_metric_value('mem.vmstat.slabs_scanned')))
        self.p_stdscr.addstr (' steal %6d |' % (self.get_metric_value('mem.vmstat.pginodesteal')))
        self.p_stdscr.addstr (' stall %6d |' % (self.get_metric_value('mem.vmstat.allocstall')))
        self.p_stdscr.addstr (' swin %7d |' % (self.get_metric_value('mem.vmstat.pswpin')))
        self.p_stdscr.addstr (' swout %6d |' % (self.get_metric_value('mem.vmstat.pswpout')))
        self.next_line()


# _NetPrint --------------------------------------------------


class _NetPrint(_AtopPrint, Network):
    def net(self, context):
        self.p_stdscr.addstr ('NET | transport    |')
        self.p_stdscr.addstr (self.put_value(' tcpi %6.2gM |', self.get_metric_value('network.tcp.insegs')))
        self.p_stdscr.addstr (self.put_value(' tcpo %6.2gM |', self.get_metric_value('network.tcp.outsegs')))
        self.p_stdscr.addstr (self.put_value(' udpi %6.2gM |', self.get_metric_value('network.udp.indatagrams')))
        self.p_stdscr.addstr (self.put_value(' udpo %6.2gM |', self.get_metric_value('network.udp.outdatagrams')))
        if (self.apyx[1] >= 95):
            self.p_stdscr.addstr (self.put_value(' tcpao %5.2gM |', self.get_metric_value('network.tcp.activeopens')))
        if (self.apyx[1] >= 110):
            self.p_stdscr.addstr (self.put_value(' tcppo %5.2gM |', self.get_metric_value('network.tcp.passiveopens')))
        self.next_line()

# Missing: icmpi (internet control message protocol received datagrams)
# Missing: icmpo (internet control message protocol transmitted datagrams)
        self.p_stdscr.addstr ('NET | network      |')
        self.p_stdscr.addstr (self.put_value(' ipi %7.2gM |', self.get_metric_value('network.ip.inreceives')))
        self.p_stdscr.addstr (self.put_value(' ipo %7.2gM |', self.get_metric_value('network.ip.outrequests')))
        self.p_stdscr.addstr (self.put_value(' ipfrw %5.2gM |', self.get_metric_value('network.ip.forwdatagrams')))
        self.p_stdscr.addstr (self.put_value(' deliv %5.2gM |', self.get_metric_value('network.ip.indelivers')))
        if (self.apyx[1] >= 95):
            self.p_stdscr.addstr (' icmpi %6d |' % (self.get_metric_value('network.icmp.inmsgs')))
        if (self.apyx[1] >= 110):
            self.p_stdscr.addstr (' icmpo %6d |' % (self.get_metric_value('network.icmp.outmsgs')))
        self.next_line()

        for k in xrange(len(self.metric_pmids)):
            try:
                (inst, iname) = context.pmGetInDom(self.metric_descs[k])
                break
            except pmapi.pmErr, e:
                iname = "X"
        net_metric = self.get_metric_value('network.interface.in.bytes')
        if type(net_metric) == type([]):
            for j in xrange(len(self.get_metric_value('network.interface.in.bytes'))):
                self.p_stdscr.addstr ('NET |')
                self.p_stdscr.addstr (' %-12s |' % (iname[j]))
                self.p_stdscr.addstr (self.put_value(' pcki %6.2gM |', self.get_scalar_value('network.interface.in.packets', j)))
                self.p_stdscr.addstr (self.put_value(' pcko %6.2gM |', self.get_scalar_value('network.interface.out.packets', j)))
                self.p_stdscr.addstr (self.put_value(' si %4d Kbps |', scale (self.get_scalar_value('network.interface.in.bytes', j), 100000000)))
                self.p_stdscr.addstr (self.put_value(' so %4d Kpbs |', scale (self.get_scalar_value('network.interface.out.bytes', j), 100000000)))
                if (self.apyx[1] >= 95):
                    self.p_stdscr.addstr (self.put_value(' erri %6.2gM |', self.get_scalar_value('network.interface.in.errors', j)))
                if (self.apyx[1] >= 110):
                    self.p_stdscr.addstr (self.put_value(' erro %6.2gM |', self.get_scalar_value('network.interface.out.errors', j)))
                self.next_line()


# _ProcPrint --------------------------------------------------


class _ProcPrint(_AtopPrint, Process):
    def __init__(self):
        super(_ProcPrint, self).__init__()
        self._output_type = 'g'
    def type_write(self, value):
        self._output_type = value
    output_type = property(None, type_write, None, None)

    def set_cpu(self, value):
        self._cpu = value

    def set_mem(self, value):
        self._mem = value

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
        for j in xrange(self.get_metric_value('proc.nprocs')):
            cpu_time_sorted.append((j, self.get_scalar_value('proc.schedstat.cpu_time', j)))
        cpu_time_sorted.sort(self.sort_l,reverse=True)

        for i in xrange(len(cpu_time_sorted)):
            j = cpu_time_sorted[i][0]
            if (self.apyx[0] == current_yx[0]):
                break

            if self._output_type in ['g', 'm']:
                self.p_stdscr.addstr ('%5d  ' % (self.get_scalar_value('proc.psinfo.pid', j)))
            if self._output_type in ['g']:
                self.p_stdscr.addstr ('%5s ' % minutes_seconds (self.get_scalar_value('proc.psinfo.stime', j)))
                self.p_stdscr.addstr (' %5s ' % minutes_seconds (self.get_scalar_value('proc.psinfo.utime', j)))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.get_scalar_value('proc.psinfo.vsize', j), 10000))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.get_scalar_value('proc.psinfo.rss', j), 10000))
                self.p_stdscr.addstr ('%5s ' % (pwd.getpwuid(self.get_scalar_value('proc.id.uid', j))[0]))
                # Missing: THR # threads is in /proc/NNN/status
                self.p_stdscr.addstr ('%5s ' % '-')
                self.p_stdscr.addstr ('%3s ' % '--')
                state = self.get_scalar_value('proc.psinfo.sname', j)
                self.p_stdscr.addstr ('%2s ' % '-')
                if state not in  ('D', 'R', 'S', 'T', 'W', 'X', 'Z'):
                    state = 'S'
                self.p_stdscr.addstr ('%2s ' % (state))
                cpu_total = float(self._cpu.cpu_total - self._cpu.get_metric_value('kernel.all.cpu.idle'))
                self.p_stdscr.addstr ('%2d%% ' % ((float(self.get_scalar_value('proc.schedstat.cpu_time', j) / 1000000) / cpu_total) * 100))
                self.p_stdscr.addstr ('%-15s ' % (self.get_scalar_value('proc.psinfo.cmd', j)))
            if self._output_type in ['m']:
                # Missing: SWAPSZ, proc.psinfo.nswap frequently returns -1
                if (self.apyx[1] >= 110):
                    minf = self.get_scalar_value('proc.psinfo.minflt', j)
                    majf = self.get_scalar_value('proc.psinfo.maj_flt', j)
                    if minf < 0:
                        minf = 0
                    if majf < 0:
                        majf = 0
                    self.p_stdscr.addstr (self.put_value('%3.0g ', minf))
                    self.p_stdscr.addstr (self.put_value('%3.0g ', majf))
                if (self.apyx[1] >= 95):
                    self.p_stdscr.addstr (self.put_value('%6.3gK ', self.get_scalar_value('proc.memory.textrss', j), 10000))
                    # Missing: VSLIBS librss seems to always be 0
                    self.p_stdscr.addstr (self.put_value('%4.2gK ', self.get_scalar_value('proc.memory.librss', j)))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.get_scalar_value('proc.memory.datrss', j), 10000))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.get_scalar_value('proc.memory.vmstack', j), 10000))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.get_scalar_value('proc.psinfo.vsize', j), 10000))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.get_scalar_value('proc.psinfo.rss', j), 10000))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.get_old_scalar_value('proc.psinfo.vsize', j), 10000))
                self.p_stdscr.addstr (self.put_value('%6.3gK ', self.get_old_scalar_value('proc.psinfo.rss', j), 10000))
                val = float(self.get_old_scalar_value('proc.psinfo.rss', j)) / float(self._mem.get_metric_value('mem.physmem')) * 100
                if val > 100: val = 0
                self.p_stdscr.addstr (self.put_value('%2d%% ', val))
                self.p_stdscr.addstr ('%-15s' % (self.get_scalar_value('proc.psinfo.cmd', j)))
            self.next_line()



# _GenericPrint --------------------------------------------------


class _GenericPrint(_AtopPrint, Subsystem):
    pass


class _MiscMetrics(Subsystem):
    def __init__(self):
        super(_MiscMetrics, self).__init__()
        self.metrics += ['kernel.all.uptime']
        self.diff_metrics += ['kernel.all.uptime']


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
                elif (sys.argv[i] == "--help" or sys.argv[i] == "-h"):
                    return usage()
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

    subsys = list()
    cpu = _ProcessorPrint()
    cpu.set_stdscr(stdscr)
    mem = _MemoryPrint()
    mem.set_stdscr(stdscr)
    disk = _DiskPrint()
    disk.set_stdscr(stdscr)
    net = _NetPrint()
    net.set_stdscr(stdscr)
    proc = _ProcPrint()
    proc.set_stdscr(stdscr)
    proc.output_type = output_type
    mm = _MiscMetrics()

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
        try:
            pmc = pmapi.pmContext()
        except pmapi.pmErr, e:
            return "Cannot connect to pmcd on localhost"

    if duration_arg != 0:
        (timeval, errmsg) = pmc.pmParseInterval(duration_arg)
        if code < 0:
            return errmsg
        duration = timeval.tv_sec

    cpu.setup_metrics (pmc)
    mem.setup_metrics (pmc)
    disk.setup_metrics (pmc)
    net.setup_metrics (pmc)
    proc.setup_metrics (pmc)

    if len(subsys) == 0:
        # method "pointers"
        subsys.append ([cpu.get_stats, pmc])
        subsys.append ([cpu.prc, None])
        subsys.append ([cpu.cpu, None])
        subsys.append ([mem.get_stats, pmc])
        subsys.append ([mem.mem, None])
        subsys.append ([disk.get_stats, pmc])
        subsys.append ([disk.disk, pmc])
        subsys.append ([net.get_stats, pmc])
        subsys.append ([net.net, pmc])
        subsys.append ([proc.get_stats, pmc])
        subsys.append ([proc.set_line, None])
        subsys.append ([proc.set_cpu, cpu])
        subsys.append ([proc.set_mem, mem])
        subsys.append ([proc.proc, None])

    if create_archive:
        configuration = "log mandatory on every " + \
            str(interval_arg) + " seconds { "
        for ssx in (mm, cpu, mem, disk, net, proc):
            configuration += ssx.dump_metrics()
        configuration += "}"
        if duration == 0:
            if n_samples != 0:
                duration = n_samples * interval_arg
            else:
                duration = 10 * interval_arg
        status = record (pmgui.GuiClient(), configuration, duration, output_file)
        if status != "":
            return status
        record_add_creator (output_file)
        sys.exit(0)

    host = pmc.pmGetContextHostName()
    if host == "localhost":
        host = os.uname()[1]

    i_samples = 0
    subsys_cmds = ['g', 'm']

    mm.setup_metrics(pmc)
    mm.get_stats(pmc)
    (delta, errmsg) = pmc.pmParseInterval(str(interval_arg) + " seconds")
    disk.interval = delta.tv_sec

    try:
        while (i_samples < n_samples) or (n_samples == 0):
            stdscr.move (0, 0)
            stdscr.addstr ('ATOP - %s\t\t%s elapsed\n\n' % (
                    time.strftime("%c"), 
                    datetime.timedelta(0, mm.get_metric_value('kernel.all.uptime'))))
            mm.get_stats(pmc)
            stdscr.move (2, 0)
            for ssx in subsys:
                try:
                    if (ssx[1] == None):
                        # indirect call via method "pointers"
                        ssx[0]()
                    else:
                        ssx[0](ssx[1])
                except pmapi.pmErr, e:
                    return str(e) + " while processing " + str(ssx[0])
                except: # catch all errors, pcp or python or otherwise
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
                    stdscr.move (proc.command_line, 0)
                    stdscr.clrtobot()
                    stdscr.move (proc.command_line, 0)
                    stdscr.addstr ('\nOptions shown for active processes:\n')
                    stdscr.addstr ( "'g'  - generic info (default)\n")
                    stdscr.addstr ( "'m'  - memory details\n")
                    stdscr.addstr ( "Miscellaneous commands:\n")
                    stdscr.addstr ("'z'  - pause-button to freeze current sample (toggle)\n")
                    stdscr.addstr ("^L   - redraw the screen\n")
                    stdscr.addstr ("hit any key to continue\n")
                    stdscr.timeout(-1)
                    char = stdscr.getch()
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
