#!/usr/bin/pcp python
#
# Copyright (C) 2014 Red Hat.
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

""" Rewrite of pmgsys (originally a C++ application) in python

    Needs to handle the following options at some point:
    -h (host), -cpudelta (interval), -l (label), -v (verbose),
    -c (config), -zoom (factor), ... rest through to pmgadgets.
"""

from sys import argv
from pcp import pmapi
from math import sqrt
from cpmapi import PM_TYPE_U32

PCPARGS = ""	# for pmgadgets-launched child processes to use

# magic numbers, from original pmgsys algorithms
CPUDELTA = 0.5
LOADDELTA = 5
FONTASCENT = 7
DOLABEL = 1
HSPACE = 5
VSPACE = 2
CPUWIDTH = 30
CPUHEIGHT = 4
LOADWIDTH = CPUWIDTH
LOADHEIGHT = 4 * CPUHEIGHT + 3 * VSPACE
MEMWIDTH = CPUHEIGHT
MEMHEIGHT = LOADHEIGHT
DISKSIZE = 6
NETWIDTH = CPUWIDTH
NETHEIGHT = CPUHEIGHT

class Machine(object):
    def __init__(self, context):
        # pcp context
        self.context = context
        # hardware bits
        self.ncpu = 0
        self.ndisk = 0
        self.niface = 0
        self.ndiskmaps = 0
        self.memory = 0
        # external names
        self.cpus = []
        self.disks = []
        self.parts = []
        self.ifaces = []
        self.diskmaps = []

    def get_hinv(self):
        """ Extract counts of CPUs, disks, interfaces and memory size
        """
        hinv = ('hinv.ncpu', 'hinv.ndisk', 'hinv.ninterface', 'hinv.physmem')
        pmids = self.context.pmLookupName(hinv)
        descs = self.context.pmLookupDescs(pmids)
        result = self.context.pmFetch(pmids)
        hardware = [0, 0, 0, 0]
        for x in xrange(4):
            atom = self.context.pmExtractValue(
                                result.contents.get_valfmt(x),
                                result.contents.get_vlist(x, 0),
                                descs[x].contents.type, PM_TYPE_U32)
            hardware[x] = atom.ul
        context.pmFreeResult(result)
        self.ncpu = hardware[0]
        self.ndisk = hardware[1]
        self.niface = hardware[2]
        self.memory = hardware[3]

    def get_names(self):
        """ Extract names of CPUs, disks and network interfaces.
        """
        inst = ('kernel.percpu.cpu.user',               # expand CPU names
                'disk.dev.total',                       # expand disk names
                'disk.partitions.total',                # expand disk partition names
                'network.interface.total.bytes')        # expand network interface names
        pmids = self.context.pmLookupName(inst)
        descs = self.context.pmLookupDescs(pmids)
    
        (inst, self.cpus) = self.context.pmGetInDom(descs[0])
        (inst, self.disks) = self.context.pmGetInDom(descs[1])
        (inst, self.parts) = self.context.pmGetInDom(descs[2])
        (inst, self.ifaces) = self.context.pmGetInDom(descs[3])

    def details(self):
        print "CPUs: ", self.ncpu
        print "CPU names: ", self.cpus
        print "Disks: ", self.ndisk
        print "Disk names: ", self.disks
        print "Partition names: ", self.parts
        print "Interfaces: ", self.niface
        print "Interface names: ", self.ifaces
        print "Memory: ", self.memory

    def get_diskmaps(self):
        """ Produce disk -> partition mappings
            This means grouping "sda1 sda2 sda3 sdb1" into two mappings
            - "sda" -> (sda1, sda2, sda3) and "sdb" -> (sdb1); done via
            the disk.dev.total and disk.partitions.total metrics.
            (original: controller -> disk mappings, but ENODATA)
        """
        return 0

    def inventory(self):
        """ Wrap calls to getting counts and subsystem names
        """
        self.get_hinv()
        self.get_names()
        self.get_diskmaps()

    def gadgetize(self):
        """ Generate a pmgadgets configuration for this host
        """
        print "pmgadgets 1",	# follow with command line
        for arg in argv:
            print "\"%s\"" % (arg),
        print

        rows = 1
        ctiles = int((self.ncpu - 1) / 4 + 1)   # always at least one cpu
        ntiles = int((self.niface - 1) / 4 + 1)
        if ctiles > 3:
            cr = int(math.sqrt(ctiles))
            if cr > rows:
                rows = cr
        if ntiles > 3:
            nr = int(math.sqrt(ntiles))
            if nr > rows:
                rows = nr

        baseY = VSPACE
        if DOLABEL == 1:
            y = FONTASCENT + VSPACE
            hostname = self.context.pmGetContextHostName()
            print "_label %d %d \"%s\"" % (HSPACE, y, hostname)
            baseY += y
        baseX = maxX = HSPACE
        maxY = baseY

        print "_actions cpuActions ("
        print "    \"pmchart\"\t\t\"pmchart -c CPU%s\"" % (PCPARGS)
        print "    \"mpvis *\"\t\t\"mpvis%s\" _default" % (PCPARGS)
        # original had IRIX gr_top and gr_osview tools next;
        # perhaps some fine day we could implement these as
        # pmgadgets front-end tools (certainly the latter)
        print ")"

        y = baseY + FONTASCENT
        x = baseX
        print "_label %d %d \"CPU\"" % (x, y)
        print "    _actions cpuActions\n"
        # original: "these should match the colours in mpvis"
        print "_colourlist cpuColours (blue3 red3 yellow3 cyan3 green3)"

        # place the CPU bars
        y += VSPACE
        ccols = int((ctiles + rows - 1) / rows)

        cpu = 0
        for rc in range(0, self.ncpu):
            for ct in range(0, ccols):
                tc = 0
                while (cpu < self.ncpu and tc < 4):
                    print "_multibar %d %d %d %d" % (x, y, CPUWIDTH, CPUHEIGHT)
                    print("    _update %f" % (CPUDELTA)).rstrip('0').rstrip('.')
                    print "    _metrics ("
                    print "\tkernel.percpu.cpu.user[\"%s\"]" % (self.cpus[cpu])
                    print "\tkernel.percpu.cpu.sys[\"%s\"]" % (self.cpus[cpu])
                    print "\tkernel.percpu.cpu.intr[\"%s\"]" % (self.cpus[cpu])
                    print "\tkernel.percpu.cpu.wait.total[\"%s\"]" % (self.cpus[cpu])
                    print "\tkernel.percpu.cpu.idle[\"%s\"]" % (self.cpus[cpu])
                    print "    )"
                    print "    _maximum 0.0\n"
                    print "    _colourlist cpuColours"
                    print "    _actions cpuActions\n"
                    cpu += 1
                    tc += 1
                    y += VSPACE + CPUHEIGHT

                if maxY < y:
                    maxY = y
                y -= (VSPACE + CPUHEIGHT) * tc
                x += HSPACE + CPUWIDTH

            y += (CPUHEIGHT + VSPACE) * 4 + VSPACE
            if (maxX < x):
                maxX = x
            x = baseX

        baseX += (HSPACE + CPUWIDTH) * ccols

        # The load gadget and its label
        print "_actions loadActions ("
        print "    \"pmchart *\"",
        print "\t\"pmchart -c LoadAvg%s\" _default" % (PCPARGS)
        # original had IRIX gr_top here
        print ")"
        print "_label %d %d \"Load\"" % (baseX, baseY + FONTASCENT)
        print "    _actions loadActions"
        print

        y = VSPACE + baseY + FONTASCENT

        i = y + LOADHEIGHT
        if (i > maxY):
            maxY = i

        print "_bargraph %d %d %d %d" % (baseX, y, LOADWIDTH, LOADHEIGHT)
        print("    _update %f" % (LOADDELTA)).rstrip('0').rstrip('.')
        print "    _metric kernel.all.load[\"1 minute\"]"
        print "    _max 1.0"
        print "    _actions loadActions"

        # For more than one row, stack LoadAvg on top of Memory.
        #
        # Move baseX just after the right hand side of the memory, so
        # we don't have to do anything special for the netifs.  For the
        # sake of argument, consider total width occupied by memory
        # gauges equal to total width of loadavg graph
        if (rows > 1):
            y += LOADHEIGHT + VSPACE
            baseX += LOADWIDTH + HSPACE
        else:
            y += baseY
            baseX += LOADWIDTH * 2 + HSPACE * 2

        # The memory gadgets and their label (platform-specific!)
        x = baseX - LOADWIDTH - HSPACE
        y += FONTASCENT
        print "_label %d %d \"Mem\"\n" % (x, y)
        print "_colourlist memColours (cyan1 red yellow green)\n"
        y += VSPACE
        print "_multibar %d %d %d %d" % (x, y, MEMWIDTH, MEMHEIGHT)
        print "    _update 0.5"
        print "    _metrics ("
        print "\tmem.util.cached"
        print "\tmem.util.bufmem"
        print "\tmem.util.other"
        print "\tmem.util.free"
        print "    )"
        print "    _colourlist memColours"
        x += HSPACE + MEMWIDTH
        print "_bar %d %d %d %d" % (x, y, MEMWIDTH, MEMHEIGHT)
        print "    _metric swap.pagesout"
        print "    _vertical"

        # Check for the max horizontal offset
        i = y + MEMHEIGHT
        if (i > maxY):
            maxY = i

        # The network bars and their label
        print "_colourlist netColours (aquamarine orange)"
        print "_actions netActions ("
        print "    \"pmchart-packets *\"",
        print "\t\"pmchart -c NetPackets%s\" _default" % (PCPARGS)
        print "    \"pmchart-bytes\"",
        print "\t\t\"pmchart -c NetBytes%s\"" % (PCPARGS)
        # original had netstat within an xterm here, next
        print ")"

        y = baseY + FONTASCENT
        x = baseX

        print "_label %d %d \"Net\"" % (x, y)
        print "    _actions netActions\n"

        y += VSPACE

        ncols = int((ntiles + rows - 1) / rows)

        ni = 0
        while ni < self.niface:
            for nt in range(0, ncols):
                tc = 0
                while ni < self.niface and tc < 4:
                    print "_multibar %d %d %d %d" % (x, y, NETWIDTH, NETHEIGHT)
                    print "    _metrics ("
                    print "\tnetwork.interface.in.bytes[\"%s\"]" % (self.ifaces[ni])
                    print "\tnetwork.interface.out.bytes[\"%s\"]" % (self.ifaces[ni])
                    print "    )"
                    print "_colourlist netColours"
                    print "    _actions netActions"
                    y += NETHEIGHT + VSPACE

                    tc += 1
                    ni += 1

                if maxY < y:
                    maxY = y
                y -= (NETHEIGHT + VSPACE) * tc
                x += NETWIDTH + HSPACE
            if maxX < x:
                maxX = x
            y += (NETHEIGHT + VSPACE) * 4 + VSPACE
            x = baseX
        print

        # Disks
        dir = 1

        print "_actions diskActions ("
        print "    \"pmchart\"",
        print "\t\t\"pmchart -c Disk%s\"" % (PCPARGS)
        print "    \"dkvis *\"",
        print "\t\t\"dkvis%s\" _default" % (PCPARGS)
        print ")"

        x = HSPACE
        y = maxY + FONTASCENT + 2 * VSPACE
        print "_label %d %d \"Disk\"" % (x, y)
        print "    _actions diskActions\n"
        print "_legend diskLegend ("
        print "    _default green3"
        print "    15       yellow"
        print "    40       orange"
        print "    75       red"
        print ")"

        x += CPUWIDTH + HSPACE
        # this only works if FONTASCENT >= ledSize
        y -= DISKSIZE
        thickness = 4
        halfDiskSize = int((DISKSIZE - thickness) / 2)

        for i in range(0, self.ndiskmaps):
            mapping = self.diskmaps[i]
            for j in range(0, len(mappings)):
                if j > 0:
                    if oldX < x:	# moved to right
                        lx = x - VSPACE - 1
                        ly = y + halfDiskSize
                        lw = VSPACE + 2
                        lh = thickness
                    elif oldX > x:	# moved to left
                        lx = oldX - VSPACE - 1
                        ly = y + halfDiskSize
                        lw = VSPACE + 2
                        lh = thickness
                    else:		# moved down
                        lx = x + halfDiskSize
                        ly = oldY + DISKSIZE - 1
                        lw = thickness
                        lh = VSPACE + 2
                    print "_line %d %d %d %d" % (lx, ly, lw, lh)
                print "_led %d %d %d %d" % (x, y, DISKSIZE, DISKSIZE)
                print "    _metric disk.dev.total[\"%s\"]" % (mapping.name())
                print "    _legend diskLegend"
                print "    _actions diskActions"
                oldX = x
                oldY = y
                xStep = dir * (DISKSIZE + VSPACE)	# use VSPACE (tighter packing)
                x += xStep
                if x > maxX - DISKSIZE or x <= HSPACE:
                    x -= xStep
                    y += DISKSIZE + VSPACE
                    dir = -dir


if __name__ == '__main__':
    context = pmapi.pmContext()
    machine = Machine(context)
    machine.inventory()
    # machine.details()
    machine.gadgetize()

