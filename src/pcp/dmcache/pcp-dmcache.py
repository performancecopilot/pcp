#!/usr/bin/env pmpython
#
# Copyright (C) 2014-2017 Red Hat.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# DmCache Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# pylint: disable=C0103,R0914,R0902,W0141
""" Display device mapper cache statistics for the system """

import sys

from pcp import pmapi, pmcc

if sys.version >= '3':
    long = int  # python2 to python3 portability (no long() in python3)

CACHE_METRICS = ['dmcache.cache.used', 'dmcache.cache.total',
                 'dmcache.metadata.used', 'dmcache.metadata.total',
                 'dmcache.read_hits', 'dmcache.read_misses',
                 'dmcache.write_hits', 'dmcache.write_misses',
                 'disk.dm.read', 'disk.dm.write']

HEADING = ''
COL_HEADING = \
    ' ---%used--- ---------reads--------- --------writes---------'

SUBHEAD_IOPS = \
    ' meta  cache     hit    miss     ops     hit    miss     ops'
SUBHEAD_RATIO = \
    ' meta  cache     hit    miss   ratio     hit    miss   ratio'
RATIO = True                # default to displaying cache hit ratios
REPEAT = 10                # repeat heading after every N samples

def option(opt, optarg, index):
    """ Perform setup for an individual command line option """
    global RATIO
    global REPEAT
    if opt == 'R':
        REPEAT = int(optarg)
    elif opt == 'i':
        RATIO = False

def cache_value(group, device, width, values):
    """ Lookup value for device instance, return it in a short string """
    if device not in values:
        return '?'.rjust(width)
    result = group.contextCache.pmNumberStr(values[device])
    return result.strip(' ').rjust(width)

def cache_percent(device, width, used, total):
    """ From used and total values (dict), calculate 'percentage used' """
    if device not in used or device not in total:
        return '?%'.rjust(width)
    numerator = float(used[device])
    denominator = float(total[device])
    if denominator == 0.0:
        return '0%'.rjust(width)
    value = 100.0 * numerator / denominator
    if value >= 100.0:
        return '100%'.rjust(width)
    return ('%3.1f%%' % value).rjust(width)

def cache_dict(group, metric):
    """ Create an instance:value dictionary for the given metric """
    values = group[metric].netConvValues
    if not values:
        return {}
    return dict(map(lambda x: (x[1], x[2]), values))

def max_lv_length(group):
    """ look at the observation group and return the max length of all the lvnames """
    cache_used = cache_dict(group, 'dmcache.cache.used')
    if not cache_used:
        return 0
    lv_names = cache_used.keys()
    return len(max(lv_names, key=len))


class DmCachePrinter(pmcc.MetricGroupPrinter):
    """ Report device mapper cache statistics """

    def __init__(self, devices):
        """ Construct object - prepare for command line handling """
        pmcc.MetricGroupPrinter.__init__(self)
        self.hostname = None
        self.devices = devices

    def report_values(self, group, width=12):
        """ Report values for one of more device mapper cache devices """

        # Build several dictionaries, keyed on cache names, with the values
        cache_used = cache_dict(group, 'dmcache.cache.used')
        cache_total = cache_dict(group, 'dmcache.cache.total')
        meta_used = cache_dict(group, 'dmcache.metadata.used')
        meta_total = cache_dict(group, 'dmcache.metadata.total')
        read_hits = cache_dict(group, 'dmcache.read_hits')
        read_misses = cache_dict(group, 'dmcache.read_misses')
        read_ops = cache_dict(group, 'disk.dm.read')
        write_hits = cache_dict(group, 'dmcache.write_hits')
        write_misses = cache_dict(group, 'dmcache.write_misses')
        write_ops = cache_dict(group, 'disk.dm.write')

        devicelist = self.devices
        if not devicelist:
            devicelist = cache_used.keys()
        if devicelist:
            for name in sorted(devicelist):
                if RATIO:
                    read_column = cache_percent(name, 7, read_hits, read_ops)
                    write_column = cache_percent(name, 7, write_hits, write_ops)
                else:
                    read_column = cache_value(group, name, 7, read_ops)
                    write_column = cache_value(group, name, 7, write_ops)

                print('%s %s %s %s %s %s %s %s %s' % (name[:width],
                        cache_percent(name, 5, meta_used, meta_total),
                        cache_percent(name, 5, cache_used, cache_total),
                        cache_value(group, name, 7, read_hits),
                        cache_value(group, name, 7, read_misses),
                        read_column,
                        cache_value(group, name, 7, write_hits),
                        cache_value(group, name, 7, write_misses),
                        write_column))
        else:
            print('No values available')

    def report(self, groups):
        """ Report driver routine - headings, sub-headings and values """
        self.convert(groups)
        group = groups['dmcache']
        max_lv = max_lv_length(group)
        padding = " "*max_lv
        if groups.counter % REPEAT == 1:
            if not self.hostname:
                self.hostname = group.contextCache.pmGetContextHostName()
            stamp = group.contextCache.pmCtime(long(group.timestamp))
            title = '@ %s (host %s)' % (stamp.rstrip(), self.hostname)
            if RATIO:
                style = "%s%s" % (padding, SUBHEAD_RATIO)
            else:
                style = "%s%s" % (padding, SUBHEAD_IOPS)
            
            HEADING = ' device '.center(max_lv,'-') + COL_HEADING
            print('%s\n%s\n%s' % (title, HEADING, style))
        self.report_values(group, width=max_lv)

if __name__ == '__main__':
    try:
        options = pmapi.pmOptions('iR:?')
        options.pmSetShortUsage('[options] [device ...]')
        options.pmSetOptionCallback(option)
        options.pmSetLongOptionHeader('Options')
        options.pmSetLongOption('repeat', 1, 'R', 'N', 'repeat the header after every N samples')
        options.pmSetLongOption('iops', 0, 'i', '', 'display IOPs instead of cache hit ratio')
        options.pmSetLongOptionVersion()
        options.pmSetLongOptionHelp()
        manager = pmcc.MetricGroupManager.builder(options, sys.argv)
        missing = manager.checkMissingMetrics(CACHE_METRICS)
        if missing != None:
            sys.stderr.write('Error: not all required metrics are available\nMissing: %s\n' % (missing))
            sys.exit(1)
        manager.printer = DmCachePrinter(options.pmGetOperands())
        manager['dmcache'] = CACHE_METRICS
        manager.run()
    except pmapi.pmErr as error:
        print('%s: %s\n' % (error.progname(), error.message()))
    except pmapi.pmUsageErr as usage:
        usage.message()
    except KeyboardInterrupt:
        pass
