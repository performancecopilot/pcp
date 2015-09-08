#!/usr/bin/pcp python
#
# Copyright (C) 2014-2015 Red Hat.
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
# pylint: disable=C0103
""" Tell how long the system has been running """

import sys
from pcp import pmapi
from cpmapi import PM_TYPE_U32, PM_TYPE_FLOAT

def print_timestamp(stamp):
    """ Report the sample time (struct tm) in HH:MM:SS form """
    return " %02d:%02d:%02d" % (stamp.tm_hour, stamp.tm_min, stamp.tm_sec)

def print_uptime(seconds):
    """ Report on system up-time in days, hours and minutes """
    days = int(seconds / (60 * 60 * 24))
    minutes = int(seconds / 60)
    hours = int(minutes / 60)
    hours = int(hours % 24)
    minutes = int(minutes % 60)
    result = " up"
    if days > 1:
        result += " %d days," % days
    elif days != 0:
        result += " 1 day,"
    if hours != 0:
        result += ' %2d:%02d,' % (hours, minutes)
    else:
        result += ' %d min,' % minutes
    return result

def print_users(nusers):
    """ Report the number of logged in users at sample time """
    if nusers == 1:
        return ' 1 user, ' 
    else:
        return (' %2d users, ' % nusers)

def print_load(one, five, fifteen):
    """ Report 1, 5, 15 minute load averages at sample time """
    return ' load average: %.2f, %.2f, %.2f' % (one, five, fifteen)


class Uptime(object):
    """ Gives a one line display of the following information:
        The current time;
        How long the system has been running;
        How many users are currently logged on; and
        The system load averages for the past 1, 5, and 15 minutes.

        Knows about some of the default PCP arguments - can function
        using remote hosts or historical data, using the timezone of
        the metric source, at an offset within an archive, and so on.
    """

    def __init__(self):
        """ Construct object - prepare for command line handling """
        self.context = None
        self.opts = pmapi.pmOptions()
        self.opts.pmSetShortOptions("V?")
        self.opts.pmSetLongOptionHeader("Options")
        self.opts.pmSetLongOptionVersion()
        self.opts.pmSetLongOptionHelp()

    def execute(self):
        """ Using a PMAPI context (could be either host or archive),
            fetch and report a fixed set of values related to uptime.
        """
        metrics = ('kernel.all.uptime', 'kernel.all.nusers', 'kernel.all.load')
        pmids = self.context.pmLookupName(metrics)
        descs = self.context.pmLookupDescs(pmids)
        result = self.context.pmFetch(pmids)
        uptime = ''

        sample_time = result.contents.timestamp.tv_sec
        time_struct = self.context.pmLocaltime(sample_time)
        uptime += print_timestamp(time_struct)

        atom = self.context.pmExtractValue(
                        result.contents.get_valfmt(0),
                        result.contents.get_vlist(0, 0),
                        descs[0].contents.type, PM_TYPE_U32)
        uptime += print_uptime(atom.ul)

        atom = self.context.pmExtractValue(
                        result.contents.get_valfmt(1),
                        result.contents.get_vlist(1, 0),
                        descs[1].contents.type, PM_TYPE_U32)
        uptime += print_users(atom.ul)

        averages = [1, 5, 15]
        for inst in range(3):
            averages[inst] = self.context.pmExtractValue(
                                      result.contents.get_valfmt(2),
                                      result.contents.get_vlist(2, inst),
                                      descs[2].contents.type, PM_TYPE_FLOAT)
        uptime += print_load(averages[0].f, averages[1].f, averages[2].f)
        print(uptime)
        self.context.pmFreeResult(result)

    def connect(self):
        """ Establish a PMAPI context to archive, host or local, via args """
        self.context = pmapi.pmContext.fromOptions(self.opts, sys.argv)


if __name__ == '__main__':
    try:
        UPTIME = Uptime()
        UPTIME.connect()
        UPTIME.execute()
    except pmapi.pmErr as error:
        print("uptime:", error.message())
    except pmapi.pmUsageErr as usage:
        usage.message()
    except KeyboardInterrupt:
        pass
