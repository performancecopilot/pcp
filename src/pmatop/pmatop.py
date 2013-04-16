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

##############################################################################
#
# imports
#

import pmapi
import time
import sys
import curses
from pcp import *
from ctypes import *
from pmsubsys import cpu, interrupt, disk, memory, net, proc, subsys

me = "pmatop"

def usage ():
    print "\nUsage:", sys.argv[0], "\n\t[-d|-c|-n|-s|-v|-c|-y|-u|-p] [-C|-M|-D|-N|-A] [-f|--filename FILE] [-p|--playback FILE]"


def debug (str):
    f=open("/tmp/,python",mode="a")
    f.write(str)
    f.close()


# round  -----------------------------------------------------------------


def round (value, magnitude):
    return (value + (magnitude / 2)) / magnitude


# record ---------------------------------------------------------------

def record (pm, config, duration, file):
    global me

    # -f saves the metrics in a directory
    if os.path.exists(file):
        print me + "playback directory %s already exists\n" % file
        sys.exit(1)
    os.mkdir (file)
    status = pm.pmRecordSetup (file + "/" + me + ".pcp", me, 0)
    (status, rhp) = pm.pmRecordAddHost ("localhost", 1, config)
    status = pm.pmRecordControl (0, pmapi.PM_REC_SETARG, "-T" + str(duration) + "sec")
    status = pm.pmRecordControl (0, pmapi.PM_REC_ON, "")
    time.sleep(duration)
    pm.pmRecordControl (0, pmapi.PM_REC_STATUS, "")
    status = pm.pmRecordControl (rhp, pmapi.PM_REC_OFF, "")


# record_add_creator ------------------------------------------------------

def record_add_creator (fn):
    f = open (fn + "/" + me + ".pcp", "r+")
    args = ""
    for i in sys.argv:
        args = args + i + " "
    f.write("# Created by " + args)
    f.write("\n#\n")
    f.close()

# record_check_creator ------------------------------------------------------

def record_check_creator (fn, doc):
    f = open (fn + "/" + me + ".pcp", "r")
    line = f.readline()
    if line.find("# Created by ") == 0:
        print doc + line[13:]
    f.close()


# minutes_seconds ----------------------------------------------------------


def minutes_seconds (millis):
    try:
        dt = datetime.timedelta(0,millis/1000)
    except OverflowError:
        dt = datetime.timedelta(0,0)
    hours = dt.days * 24
    minutes = dt.seconds / 60
    hours += minutes / 60
    minutes = minutes % 60
    return "%dh%dm" % (hours,minutes)


# _atop_print --------------------------------------------------

class _atop_print(object):
    def set_line(self):
        self.command_line = self.p_stdscr.getyx()[0]
        self.p_stdscr.addstr ('\n')
    def set_stdscr(self, a_stdscr):
        self.p_stdscr = a_stdscr
        self.yx = a_stdscr.getmaxyx()
    def next_line(self):
        line = self.p_stdscr.getyx()
        y = line[0]
        if line[1] > 0:
            y += 1
        self.p_stdscr.addstr (' ' * (self.yx[1] - line[1]))
        self.p_stdscr.move(y,0)
    def put_value(self, format, value):
# 8e+03M
        return re.sub ("([0-9]*\.*[0-9]+)e\+0", " \\1e", format % value)


# _cpu_print --------------------------------------------------


class _cpu_print(_atop_print, cpu):
# Missing: #trun (total # running threads) 
# Missing: #exit (requires accounting)
# Substitutions: proc.runq.sleeping for #tslpi (threads sleeping) 
# Substitutions: proc.runq.blocked for #tslpu (threads uninterrupt sleep)
    def prc(self):
        self.p_stdscr.addstr ('PRC |')
        self.p_stdscr.addstr (' sys %8s |' % (minutes_seconds(self.get_metric_value('kernel.all.cpu.sys'))))
        self.p_stdscr.addstr (' user %7s |' % (minutes_seconds(self.get_metric_value('kernel.all.cpu.user'))))
        self.p_stdscr.addstr (' #proc %6d |' % (self.get_metric_value('kernel.all.nprocs')))
        if (self.yx[1] >= 95):
            self.p_stdscr.addstr (' #tslpi %5d |' % (self.get_metric_value('proc.runq.sleeping')))
        if (self.yx[1] >= 110):
            self.p_stdscr.addstr (' #tslpu %5d |' % (self.get_metric_value('proc.runq.blocked')))
        self.p_stdscr.addstr (' #zombie %4d' % (self.get_metric_value('proc.runq.defunct')))
        self.next_line()
# Missing: curf (current frequency)
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
        for k in range(self.get_len(self.get_metric_value('kernel.percpu.cpu.user'))):
            self.p_stdscr.addstr ('cpu |')
            self.p_stdscr.addstr (' sys %7d%% |' % (100 * self.get_scalar_value('kernel.percpu.cpu.sys',k) / self.cpu_total))
            self.p_stdscr.addstr (' user %6d%% |' % (100 * self.get_scalar_value('kernel.percpu.cpu.user',k) / self.cpu_total))
            self.p_stdscr.addstr (' irq %7d%% |' % (
                    100 * self.get_scalar_value('kernel.percpu.cpu.irq.hard',k) / self.cpu_total +
                    100 * self.get_scalar_value('kernel.percpu.cpu.irq.soft',k) / self.cpu_total))
            self.p_stdscr.addstr (' idle %6d%% |' % (100 * self.get_scalar_value('kernel.percpu.cpu.idle',k) / self.cpu_total))
            self.p_stdscr.addstr (' wait %6d%% |' % (100 * self.get_scalar_value('kernel.percpu.cpu.wait.total',k) / self.cpu_total))
            self.next_line()

        self.p_stdscr.addstr ('CPL |')
        self.p_stdscr.addstr (' avg1 %7.3g |' % (self.get_scalar_value('kernel.all.load',0)))
        self.p_stdscr.addstr (' avg5 %7.3g |' % (self.get_scalar_value('kernel.all.load',1)))
        self.p_stdscr.addstr (' avg15 %6.3g |' % (self.get_scalar_value('kernel.all.load',2)))
        self.p_stdscr.addstr (self.put_value(' csw %8.3g |', self.get_metric_value('kernel.all.pswitch')))
        self.p_stdscr.addstr (self.put_value(' intr %7.3g |', self.get_metric_value('kernel.all.intr')))
        self.p_stdscr.addstr ('\n')

# _interrupt_print --------------------------------------------------


class _interrupt_print(_atop_print, interrupt):
        True


# _disk_print --------------------------------------------------


class _disk_print(_atop_print, disk):
    def disk(self, pm):
        try:
            (inst, iname) = pm.pmGetInDom(self.metric_descs [self.metrics_dict['disk.partitions.read']])
        except pmErr, e:
            iname = iname = "X"

# lvm partitions have names like dm-N; but we want the real name
# Missing: LVM name
# Missing: LVM busy (time handling requests)
# Missing: LVM MBr/s (per second read throughput)
# Missing: LVM MBw/s (per second write throughput)
# Missing: LVM avq (average queue depth)
# Missing: LVM avio (milliseconds per request)

        for j in xrange(self.get_len(self.get_metric_value('disk.partitions.read'))):
            self.p_stdscr.addstr ('LVM |')
            self.p_stdscr.addstr (' %-12s |' % (iname[j]))
            self.p_stdscr.addstr ('              |')
            self.p_stdscr.addstr (self.put_value(' read %7.3g |', self.get_scalar_value('disk.partitions.read',j)))
            self.p_stdscr.addstr (self.put_value(' write %6.3g |', self.get_scalar_value('disk.partitions.write',j)))
            if (self.yx[1] >= 95):
                if self.get_scalar_value('disk.partitions.read',j) != 0:
                    v = self.get_scalar_value('disk.partitions.read_bytes',j) / self.get_scalar_value('disk.partitions.read',j)
                else:
                    v = 0
                self.p_stdscr.addstr (self.put_value(' KiB/r %6.3g |', v))
            if (self.yx[1] >= 110):
                if self.get_scalar_value('disk.partitions.write',j) != 0:
                    v = self.get_scalar_value('disk.partitions.write_bytes',j) / self.get_scalar_value('disk.partitions.write',j)
                else:
                    v = 0
                self.p_stdscr.addstr (self.put_value(' KiB/w %6.3g |', v))
            self.next_line()

        try:
            (inst, iname) = pm.pmGetInDom(self.metric_descs [self.metrics_dict['disk.dev.read']])
        except pmErr, e:
            iname = iname = "X"

        for j in xrange(self.get_len(self.get_metric_value('disk.dev.read_bytes'))):
            self.p_stdscr.addstr ('DSK |')
            self.p_stdscr.addstr (' %-12s |' % (iname[j]))
            self.p_stdscr.addstr (' busy %6d%% |' % (0)) # self.get_scalar_value('disk.dev.avactive',j)
            xx=(self.get_scalar_value('disk.dev.read',j))
            self.p_stdscr.addstr (' read %7d |' % (xx))
            self.p_stdscr.addstr (self.put_value(' write %6.3g |', self.get_scalar_value('disk.dev.write',j)))
            self.p_stdscr.addstr (' avio %7.3g |' % (0))
            self.next_line()


# _memory_print --------------------------------------------------


class _memory_print(_atop_print, memory):
# Missing: shrss (resident shared memory size)
    def mem(self):
        self.p_stdscr.addstr ('MEM |')
        self.p_stdscr.addstr (' tot %7dM |' % (round(self.get_metric_value('mem.physmem'),1000)))
        self.p_stdscr.addstr (' free %6dM |' % (round(self.get_metric_value('mem.freemem'),1000)))
        self.p_stdscr.addstr (' cache %5dM |' % (round(self.get_metric_value('mem.util.cached'),1000)))
        self.p_stdscr.addstr (' buff %6dM |' % (round(self.get_metric_value('mem.util.bufmem'),1000)))
        self.p_stdscr.addstr (' slab %6dM |' % (round(self.get_metric_value('mem.util.slab'),1000)))
        if (self.yx[1] >= 95):
            self.p_stdscr.addstr (' #shmem %4dM |' % (round(self.get_metric_value('mem.util.shmem'),1000)))
        self.next_line()

        self.p_stdscr.addstr ('SWP |')
        self.p_stdscr.addstr (' tot %7dG |' % (round(self.get_metric_value('mem.util.swapTotal'), 1000000)))
        self.p_stdscr.addstr (' free %6dG |' % (round(self.get_metric_value('mem.util.swapFree'), 1000000)))
        self.p_stdscr.addstr ('              |')
        self.p_stdscr.addstr (' vmcom %5dG |' % (round(self.get_metric_value('mem.util.committed_AS'), 1000000)))
        self.p_stdscr.addstr (' vmlim %5dG |' % (round(self.get_metric_value('mem.util.commitLimit'), 1000000)))
        self.next_line()

        self.p_stdscr.addstr ('PAG |')
        self.p_stdscr.addstr (' scan %7d |' % (self.get_metric_value('mem.vmstat.slabs_scanned')))
        self.p_stdscr.addstr (' steal %6d |' % (self.get_metric_value('mem.vmstat.pginodesteal')))
        self.p_stdscr.addstr (' stall %6d |' % (self.get_metric_value('mem.vmstat.allocstall')))
        self.p_stdscr.addstr (' swin %7d |' % (self.get_metric_value('mem.vmstat.pswpin')))
        self.p_stdscr.addstr (' swout %6d |' % (self.get_metric_value('mem.vmstat.pswpout')))
        self.next_line()


# _net_print --------------------------------------------------


class _net_print(_atop_print, net):
    def net(self, pm):
        self.p_stdscr.addstr ('NET | transport    |')
        self.p_stdscr.addstr (self.put_value(' tcpi %6.2gM |', self.get_metric_value('network.tcp.insegs')))
        self.p_stdscr.addstr (self.put_value(' tcpo %6.2gM |', self.get_metric_value('network.tcp.outsegs')))
        self.p_stdscr.addstr (self.put_value(' udpi %6.2gM |', self.get_metric_value('network.udp.indatagrams')))
        self.p_stdscr.addstr (self.put_value(' udpo %6.2gM |', self.get_metric_value('network.udp.outdatagrams')))
        if (self.yx[1] >= 95):
            self.p_stdscr.addstr (self.put_value(' tcpao %5.2gM |', self.get_metric_value('network.tcp.activeopens')))
        if (self.yx[1] >= 110):
            self.p_stdscr.addstr (self.put_value(' tcppo %5.2gM |', self.get_metric_value('network.tcp.passiveopens')))
        self.next_line()

# Missing: icmpi (internet control message protocol received datagrams)
# Missing: icmpo (internet control message protocol transmitted datagrams)
        self.p_stdscr.addstr ('NET | network      |')
        self.p_stdscr.addstr (self.put_value(' ipi %7.2gM |', self.get_metric_value('network.ip.inreceives')))
        self.p_stdscr.addstr (self.put_value(' ipo %7.2gM |', self.get_metric_value('network.ip.outrequests')))
        self.p_stdscr.addstr (self.put_value(' ipfrw %5.2gM |', self.get_metric_value('network.ip.forwdatagrams')))
        self.p_stdscr.addstr (self.put_value(' deliv %5.2gM |', self.get_metric_value('network.ip.indelivers')))
        self.next_line()

        for k in xrange(len(self.metric_pmids)):
            try:
                (inst, iname) = pm.pmGetInDom(self.metric_descs[k])
                break
            except pmErr, e:
                iname = "X"
        net_metric = self.get_metric_value('network.interface.in.bytes')
        if type(net_metric) == type([]):
            for j in xrange(len(self.get_metric_value('network.interface.in.bytes'))):
                self.p_stdscr.addstr ('NET |')
                self.p_stdscr.addstr (' %-12s |' % (iname[j]))
                self.p_stdscr.addstr (self.put_value(' pcki %6.2gM |', self.get_scalar_value('network.interface.in.packets',j)))
                self.p_stdscr.addstr (self.put_value(' pcko %6.2gM |', self.get_scalar_value('network.interface.out.packets',j)))
                self.p_stdscr.addstr (self.put_value(' si %8.2gM |', self.get_scalar_value('network.interface.in.bytes',j)))
                self.p_stdscr.addstr (self.put_value(' so %8.2gM |', self.get_scalar_value('network.interface.out.bytes',j)))
                if (self.yx[1] >= 95):
                    self.p_stdscr.addstr (self.put_value(' erri %6.2gM |', self.get_scalar_value('network.interface.in.errors',j)))
                if (self.yx[1] >= 110):
                    self.p_stdscr.addstr (self.put_value(' erro %6.2gM |', self.get_scalar_value('network.interface.out.errors',j)))
                self.next_line()


# _proc_print --------------------------------------------------


class _proc_print(_atop_print, proc):
    def __init__(self):
        super(_proc_print, self).__init__()
        self.output_type = 'g'
    def proc(self):
        current_yx = self.p_stdscr.getyx()

        if self.output_type in ['g']:
            self.p_stdscr.addstr ('  PID SYSCPU USRCPU VGROW RGROW RUID   THR ST EXC S CPU  CMD\n')
        elif self.output_type in ['m']:
            self.p_stdscr.addstr ('  PID MAJFLT MINFLT\n')

        for j in xrange(len(self.get_metric_value('proc.psinfo.pid'))):
            if j > (self.yx[0] - current_yx[0]):
                break

            if self.output_type in ['g', 'm']:
                self.p_stdscr.addstr ('%4d  ' % (self.get_scalar_value('proc.psinfo.pid',j)))
            if self.output_type in ['g']:
# Missing: is proc.psinfo.stime correct?
                self.p_stdscr.addstr ('%5s ' % minutes_seconds (self.get_scalar_value('proc.psinfo.stime',j)))
# Missing: is proc.psinfo.utime correct?
                self.p_stdscr.addstr ('%5s ' % minutes_seconds (self.get_scalar_value('proc.psinfo.utime',j)))
                self.p_stdscr.addstr ('%5d ' % 0)
                self.p_stdscr.addstr ('%5d ' % 0)
                self.p_stdscr.addstr ('%5d ' % (self.get_scalar_value('proc.id.uid',j)))
                self.p_stdscr.addstr ('%5d ' % 0)
                self.p_stdscr.addstr ('%3d ' % 0)
#                self.p_stdscr.addstr ('%5d ' % (self.get_scalar_value('proc.psinfo.flags',j)))
                self.p_stdscr.addstr ('%3d ' % (self.get_scalar_value('proc.psinfo.exit_signal',j)))
                self.p_stdscr.addstr ('%2d ' % 0)
                self.p_stdscr.addstr ('%3d ' % 0)
                self.p_stdscr.addstr ('%-15s ' % (self.get_scalar_value('proc.psinfo.cmd',j)))
            if self.output_type in ['m']:
                self.p_stdscr.addstr ('%5d ' % (self.get_scalar_value('proc.psinfo.maj_flt',j)))
                self.p_stdscr.addstr ('%5d ' % (self.get_scalar_value('proc.psinfo.minflt',j)))
            self.next_line()



# _generic_print --------------------------------------------------


class _generic_print(_atop_print, subsys):
    True


# main ----------------------------------------------------------------------


def main (stdscr):
    subsys = list()
    cpu = _cpu_print()
    cpu.set_stdscr(stdscr)
    mem = _memory_print()
    mem.set_stdscr(stdscr)
    disk = _disk_print()
    disk.set_stdscr(stdscr)
    net = _net_print()
    net.set_stdscr(stdscr)
    proc = _proc_print()
    proc.set_stdscr(stdscr)
    ss = _generic_print()

    output_file = ""
    input_file = ""
    duration = 0
    interval_arg = 1
    duration_arg = 0
    n_samples = 0
    i = 1
    sort = ""

    stdscr.nodelay(True)

    subsys_options = {"d":"disk",
                 "c":"cpu",
                 "n":"net",
                 "s":"scheduling",
                 "v":"various",
                 "c":"command",
                 "y":"threads",
                 "u":"user total",
                 "p":"process total",
                 }

    sort_options = {"C": "cpu",
                    "M": "mem",
                    "D": "disk",
                    "N": "net",
                    "A": "auto"}

    class nextOption ( Exception ):
        True

    while i < len(sys.argv):
        try:
            if (sys.argv[i][:1] == "-"):
                for s in subsys_options:
                    if sys.argv[i][1:] == s:
                        subsys.add([s[1]])
                        raise nextOption
                for s in sort_options:
                    if sys.argv[i][1:] == s:
                        sort = s[1]
                        raise nextOption
                if (sys.argv[i] == "-w"):
                    i += 1
                    output_file = sys.argv[i]
                elif (sys.argv[i] == "-r"):
                    i += 1
                    input_file = sys.argv[i]
                elif (sys.argv[i] == "--help" or sys.argv[i] == "-h"):
                    usage()
                    sys.exit(1)
                else:
                    return sys.argv[0] + ": Unknown option " + sys.argv[i] \
                        + "\nTry `" + sys.argv[0] + " --help' for more information."
                    sys.exit(1)
            else:
                interval_arg = int(sys.argv[i])
                i += 1
                if (i < len(sys.argv)):
                    n_samples = int(sys.argv[i])
            i += 1
        except nextOption:
            True

    if input_file == "":
        try:
            pm = pmContext()
        except pmErr, e:
            return "Cannot connect to pmcd on localhost"
            sys.exit(1)
    else:
        # -f saved the metrics in a directory, so get the archive basename
        lol = []
        if not os.path.exists(input_file):
            return input_file + " does not exist"
            sys.exit(1)
        if not os.path.isdir(input_file) or not os.path.exists(input_file + "/" + me + ".pcp"):
            return input_file + " is not a " + me + " playback directory"
            sys.exit(1)
        for line in open(input_file + "/" + me + ".pcp"):
            lol.append(line[:-1].split())
        archive = input_file + "/" + lol[len(lol)-1][2]
        try:
            pm = pmContext(pmapi.PM_CONTEXT_ARCHIVE, archive)
        except pmErr, e:
            debug(e.__str__())
            return "Cannot open PCP archive: " + archive
            sys.exit(1)

    if duration_arg != 0:
        (code, timeval, errmsg) = pm.pmParseInterval(duration_arg)
        if code < 0:
            return errmsg
            sys.exit(1)
        duration = timeval.tv_sec

    cpu.setup_metrics (pm)
    mem.setup_metrics (pm)
    disk.setup_metrics (pm)
    net.setup_metrics (pm)
    proc.setup_metrics (pm)

    if len(subsys) == 0:
        # method "pointers"
        subsys.append ([cpu.get_stats, pm])
        subsys.append ([cpu.prc, None])
        subsys.append ([cpu.cpu, None])
        subsys.append ([mem.get_stats, pm])
        subsys.append ([mem.mem, None])
        subsys.append ([disk.get_stats, pm])
        subsys.append ([disk.disk, pm])
        subsys.append ([net.get_stats, pm])
        subsys.append ([net.net, pm])
        subsys.append ([proc.get_stats, pm])
        subsys.append ([proc.set_line, None])
        subsys.append ([proc.proc, None])

    if output_file != "":
        configuration = "log mandatory on every " + str(interval_arg) + " seconds { "
        for s in (cpu, mem, disk, net, proc):
            configuration += s.dump_metrics()
        configuration += "}"
        if duration == 0:
            if n_samples != 0:
                duration = n_samples * interval_arg
            else:
                duration = 10 * interval_arg
        record (pm, configuration, duration, output_file)
        record_add_creator (output_file)
        sys.exit(0)

    host = pm.pmGetContextHostName()
    if host == "localhost":
        host = os.uname()[1]

    stdscr.move (0,0)
    stdscr.addstr ('ATOP - %s\t\t%s elapsed\n\n' % (time.strftime("%c"), datetime.timedelta(0, cpu.get_metric_value('kernel.all.uptime'))))

    n = 0
    subsys_cmds = ['g','m']

    try:
        while (n < n_samples) or (n_samples == 0):
            stdscr.move (2,0)
            for s in subsys:
                try:
                    if (s[1] == None):
                        # indirect call via method "pointers"
                        s[0]()
                    else:
                        s[0](s[1])
                except curses.error:
                    pass
            stdscr.move (proc.command_line,0)
            t = 1
            c = 0
            (code, delta, errmsg) = pm.pmParseInterval(str(interval_arg) + " seconds")
            (code, one_second, errmsg) = pm.pmParseInterval(str(1) + " seconds")
            while (t < delta.tv_sec):
                c = stdscr.getch()
                if (c != -1 and chr(c) == "q"):
                    raise KeyboardInterrupt
                pm.pmtimevalSleep(one_second)
                t += 1

            if (c != -1):       # user typed a command
                try:
                    cmd = chr(c)
                except ValueError:
                    cmd = None
                if cmd == "q":
                    raise KeyboardInterrupt
                elif cmd in subsys_cmds:
                    proc.output_type = cmd
            n += 1
    except KeyboardInterrupt:
        True
    stdscr.refresh()
    time.sleep(3)
    return ""

if __name__ == '__main__':
    status = curses.wrapper(main)
    if (status != ""):
        print status

