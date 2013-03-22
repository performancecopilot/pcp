'''
Python implementation of the "simple" Performance Metrics Domain Agent.
'''
#
# Copyright (c) 2013 Red Hat.
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

import os
import time
import pmapi
from pcp import pmda

class SimplePMDA(pcp.PMDA):
    '''
    A simple Performance Metrics Domain Agent with very simple metrics.
    Install it and make basic use of it, as follows:

    # $PCP_PMDAS_DIR/simple/Install
    [select python option]
    $ pminfo -fmdtT simple

    Then experiment with the simple.conf file (see simple.now metrics).
    '''

    # all indoms in this PMDA
    color_indom = 0
    now_indom = 1

    # simple.color instance domain
    red = 0
    green = 100
    blue = 200

    # simple.now instance domain
    configfile = ''
    timeslices = {}

    # simple.numfetch properties
    numfetch = 0
    oldfetch = -1


    def simple_instance(self):
        ''' Called once per "instance request" PDU '''
        self.simple_timenow_check()


    def simple_fetch(self):
        ''' Called once per "fetch" PDU, before callbacks '''
        self.numfetch += 1
        self.simple_timenow_check()

    def simple_fetch_color_callback(self, item, inst):
        '''
        Returns a list of value,status (single pair) for color cluster
        Helper for the fetch callback
        '''
        if (item == 0):
            return [self.numfetch, 1]
        elif (item == 1):
            if (inst == 0):
                self.red = (self.red + 1) % 255
                return [self.red, 1]
            elif (inst == 1):
                self.green = (self.green + 1) % 255
                return [self.green, 1]
            elif (inst == 2):
                self.blue = (self.blue + 1) % 255
                return [self.blue, 1]
            else:
                return [pmapi.PM_ERR_INST, 0]
        else:
            return [pmapi.PM_ERR_PMID, 0]

    def simple_fetch_times_callback(self, item):
        '''
        Returns a list of value,status (single pair) for times cluster
        Helper for the fetch callback
        '''
        times = ()
        if (self.oldfetch < self.numfetch):     # get current values, if needed
            times = os.times()
            self.oldfetch = self.numfetch
        if (item == 2):
            return [times[0], 1]
        elif (item == 3):
            return [times[1], 1]
        return [pmapi.PM_ERR_PMID, 0]

    def simple_fetch_now_callback(self, item, inst):
        '''
        Returns a list of value,status (single pair) for "now" cluster
        Helper for the fetch callback
        '''
        if (item == 4):
            value = self.pmda_inst_lookup(self.now_indom, inst)
            if (value == None):
                return [pmapi.PM_ERR_INST, 0]
            return [value, 1]
        return [pmapi.PM_ERR_PMID, 0]

    def simple_fetch_callback(self, cluster, item, inst):
        '''
        Main fetch callback, defers to helpers for each cluster.
        Returns a list of value,status (single pair) for requested pmid/inst
        '''
        if (not (inst == pmapi.PM_IN_NULL or
            (cluster == 0 and item == 1) or (cluster == 2 and item == 4))):
            return [pmapi.PM_ERR_INST, 0]
        if (cluster == 0):
            return self.simple_fetch_color_callback(item, inst)
        elif (cluster == 1):
            return self.simple_fetch_times_callback(item)
        elif (cluster == 2):
            return self.simple_fetch_now_callback(item, inst)
        return [pmapi.PM_ERR_PMID, 0]


    def simple_store_count_callback(self, val):
        ''' Helper for the store callback, handles simple.numfetch '''
        sts = 0
        if (val < 0):
            sts = pmapi.PM_ERR_SIGN
            val = 0
        self.numfetch = val
        return sts

    def simple_store_color_callback(self, val, inst):
        ''' Helper for the store callback, handles simple.color '''
        sts = 0
        if (val < 0):
            sts = pmapi.PM_ERR_SIGN
            val = 0
        elif (val > 255):
            sts = pmapi.PM_ERR_CONV
            val = 255

        if (inst == 0):
            self.red = val
        elif (inst == 1):
            self.green = val
        elif (inst == 2):
            self.blue = val
        else:
            sts = pmapi.PM_ERR_INST
        return sts

    def simple_store_callback(self, cluster, item, inst, val):
        '''
        Store callback, executed when a request to write to a metric happens
        Defers to helpers for each storable metric.  Returns a single value.
        '''
        if (cluster == 0):
            if (item == 0):
                return self.simple_store_count_callback(val)
            elif (item == 1):
                return self.simple_store_color_callback(val, inst)
            else:
                return pmapi.PM_ERR_PMID
        elif ((cluster == 1 and (item == 2 or item == 3))
            or (cluster == 2 and item == 4)):
            return pmapi.PM_ERR_PERMISSION
        return pmapi.PM_ERR_PMID


    def simple_timenow_check(self):
        '''
        Read our configuration file and update instance domain
        '''
        self.timeslices.clear()
        with open(self.configfile) as cfg:
            fields = time.localtime()
            values = {'sec': fields[5], 'min': fields[4], 'hour': fields[3]}
            config = cfg.readline().strip()
            for key in config.split(','):
                if (values[key] != None):
                    self.timeslices[key] = values[key]
        self.replace_indom(self.now_indom, self.timeslices)


    def __init__(self, name, domain):
        pcp.PMDA.__init__(name, domain)

        helpfile = pcp.pmGetConfig('PCP_PMDAS_DIR')
        helpfile += '/' + name + '/' + 'help'
        self.set_helpfile(helpfile)

        self.configfile = pcp.pmGetConfig('PCP_PMDAS_DIR')
        self.configfile += '/' + name + '/' + name + '.conf'

        self.add_metric(name + '.numfetch', pcp.pmda_pmid(0, 0),
                pmapi.PM_TYPE_U32, pmapi.PM_INDOM_NULL, pmapi.PM_SEM_INSTANT,
                pcp.pmda_units(0, 0, 0, 0, 0, 0))
        self.add_metric(name + '.color', pcp.pmda_pmid(0, 1),
                pmapi.PM_TYPE_32, self.color_indom, pmapi.PM_SEM_INSTANT,
                pcp.pmda_units(0, 0, 0, 0, 0, 0))
        self.add_metric(name + '.time.user', pcp.pmda_pmid(1, 2),
                pmapi.PM_TYPE_DOUBLE, pmapi.PM_INDOM_NULL, pmapi.PM_SEM_COUNTER,
                pcp.pmda_units(0, 1, 0, 0, pmapi.PM_TIME_SEC, 0))
        self.add_metric(name + '.time.sys', pcp.pmda_pmid(1, 3),
                pmapi.PM_TYPE_DOUBLE, pmapi.PM_INDOM_NULL, pmapi.PM_SEM_COUNTER,
                pcp.pmda_units(0, 1, 0, 0, pmapi.PM_TIME_SEC, 0))
        self.add_metric(name + '.now', pcp.pmda_pmid(2, 4),
                pmapi.PM_TYPE_U32, self.now_indom, pmapi.PM_SEM_INSTANT,
                pcp.pmda_units(0, 0, 0, 0, 0, 0))

        self.color_indom = self.add_indom(self.color_indom,
                {0: 'red', 1: 'green', 2: 'blue'})
        self.now_indom = self.add_indom(self.now_indom, {}) # initialized on-the-fly

        self.set_fetch(simple_fetch)
        self.set_instance(simple_instance)
        self.set_fetch_callback(simple_fetch_callback)
        self.set_store_callback(simple_store_callback)
        self.set_user(pcp.pmGetConfig('PCP_USER'))
        self.simple_timenow_check()


if __name__ == '__main__':

    SimplePMDA('simple', 253).run()

