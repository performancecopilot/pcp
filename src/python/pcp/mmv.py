# pylint: disable=C0103
"""Wrapper module for libpcp_mmv - PCP Memory Mapped Values library
#
# Copyright (C) 2013-2016,2019 Red Hat.
#
# This file is part of the "pcp" module, the python interfaces for the
# Performance Co-Pilot toolkit.
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
# Example use of this module for instrumenting a python application:

        from pcp import mmv, pmapi
        import cpmapi as pcpapi
        import cmmv as mmvapi

        instances = [
                mmv.mmv_instance(0, "Anvils"),
                mmv.mmv_instance(1, "Rockets"),
                mmv.mmv_instance(2, "Giant_Rubber_Bands")
        ]
        ACME_PRODUCTS_INDOM = 61
        indoms = [
                mmv.mmv_indom(
                    serial = ACME_PRODUCTS_INDOM,
                    shorttext = "Acme products",
                    helptext = "Most popular products produced by the Acme Corporation")
        ]
        indoms[0].set_instances(instances)

        metrics = [
                mmv.mmv_metric(
                    name = "products.count",
                    item = 7,
                    typeof = mmvapi.MMV_TYPE_U64,
                    semantics = mmvapi.MMV_SEM_COUNTER,
                    dimension = pmapi.pmUnits(0,0,1,0,0,pcpapi.PM_COUNT_ONE),
                    indom = ACME_PRODUCTS_INDOM,
                    shorttext = "Acme factory product throughput",
                    helptext =
        "Monotonic increasing counter of products produced in the Acme Corporation\n" +
        "factory since starting the Acme production application.  Quality guaranteed."),

                mmv.mmv_metric(
                    name = "products.time",
                    item = 8,
                    typeof = mmvapi.MMV_TYPE_U64,
                    semantics = mmvapi.MMV_SEM_COUNTER,
                    dimension = pmapi.pmUnits(0,1,0,0,pcpapi.PM_TIME_USEC,0),
                    indom = ACME_PRODUCTS_INDOM,
                    shorttext = "Machine time spent producing Acme products")
        ]

        values = mmv.MemoryMappedValues("acme")
        values.add_indoms(indoms)
        values.add_metrics(metrics)

        values.start()
        anvils = values.lookup_mapping("products.count", "Anvils")
        values.set(anvils, 41)
        values.inc(anvils)
        values.stop()
"""

from pcp.pmapi import pmUnits, pmAtomValue
from cmmv import MMV_NAMEMAX

import ctypes
from ctypes import Structure, POINTER
from ctypes import c_int, c_uint, c_long, c_char, c_char_p, c_double, c_void_p

# Performance Co-Pilot MMV library (C)
LIBPCP_MMV = ctypes.CDLL(ctypes.util.find_library("pcp_mmv"))

##############################################################################
#
# definition of structures used by libpcp, derived from <pcp/pmapi.h>
#
# This section defines the data structures for accessing and manuiplating
# metric information and values.  Detailed information about these data
# structures can be found in the MMV(5) manual page.
#

class mmv_instance(Structure):
    """ Maps internal to external instance identifiers, within an
        instance domain.
    """
    _fields_ = [("internal", c_int),
                ("external", c_char * MMV_NAMEMAX)]

    def __init__(self, inst, name):
        Structure.__init__(self)
        if not isinstance(name, bytes):
            name = name.encode('utf-8')
        self.external = name
        self.internal = inst

class mmv_indom(Structure):
    """ Represents an instance domain (for set valued metrics)
        Instance domains have associated instances - integer/string pairs.
        Defines complete indom metadata (instances, count, text and so on)
    """
    _fields_ = [("serial", c_uint),
                ("count", c_uint),
                ("instances", POINTER(mmv_instance)),
                ("shorttext", c_char_p),
                ("helptext", c_char_p)]

    def __init__(self, serial, shorttext='', helptext=''):
        Structure.__init__(self)
        if helptext is not None and not isinstance(helptext, bytes):
            helptext = helptext.encode('utf-8')
        if shorttext is not None and not isinstance(shorttext, bytes):
            shorttext = shorttext.encode('utf-8')
        self.shorttext = shorttext
        self.helptext = shorttext
        self.serial = serial

    def set_instances(self, instances):
        """ Update the instances and counts fields for this indom """
        self.count = len(instances)
        instance_array = (mmv_instance * self.count)()
        for i in range(self.count):
            instance_array[i].internal = instances[i].internal
            instance_array[i].external = instances[i].external
        self.instances = instance_array

class mmv_metric(Structure):
    """ Represents an individual metric to be exported by pmdammv
        Defines complete metric metadata (type, semantics, units and so on)
    """
    _fields_ = [("name", c_char * MMV_NAMEMAX),
                ("item", c_int),
                ("typeof", c_int),
                ("semantics", c_int),
                ("dimension", pmUnits),
                ("indom", c_uint),
                ("shorttext", c_char_p),
                ("helptext", c_char_p)]

    def __init__(self, name, item, typeof, semantics, dimension, indom=0, shorttext='', helptext=''): # pylint: disable=R0913
        Structure.__init__(self)
        if not isinstance(name, bytes):
            name = name.encode('utf-8')
        if helptext is not None and not isinstance(helptext, bytes):
            helptext = helptext.encode('utf-8')
        if shorttext is not None and not isinstance(shorttext, bytes):
            shorttext = shorttext.encode('utf-8')
        self.shorttext = shorttext
        self.helptext = shorttext
        self.typeof = typeof
        self.indom = indom
        self.item = item
        self.name = name

##
# PCP Memory Mapped Value Services

LIBPCP_MMV.mmv_stats_init.restype = c_void_p
LIBPCP_MMV.mmv_stats_init.argtypes = [
    c_char_p, c_int, c_int,
    POINTER(mmv_metric), c_int, POINTER(mmv_indom), c_int]

LIBPCP_MMV.mmv_stats_stop.restype = None
LIBPCP_MMV.mmv_stats_stop.argtypes = [c_char_p, c_void_p]

LIBPCP_MMV.mmv_lookup_value_desc.restype = POINTER(pmAtomValue)
LIBPCP_MMV.mmv_lookup_value_desc.argtypes = [c_void_p, c_char_p, c_char_p]

LIBPCP_MMV.mmv_inc_value.restype = None
LIBPCP_MMV.mmv_inc_value.argtypes = [c_void_p, POINTER(pmAtomValue), c_double]

LIBPCP_MMV.mmv_inc_atomvalue.restype = None
LIBPCP_MMV.mmv_inc_atomvalue.argtypes = [c_void_p, POINTER(pmAtomValue), POINTER(pmAtomValue)]

LIBPCP_MMV.mmv_set_value.restype = None
LIBPCP_MMV.mmv_set_value.argtypes = [c_void_p, POINTER(pmAtomValue), c_double]

LIBPCP_MMV.mmv_set_atomvalue.restype = None
LIBPCP_MMV.mmv_set_atomvalue.argtypes = [c_void_p, POINTER(pmAtomValue), POINTER(pmAtomValue)]

LIBPCP_MMV.mmv_set_string.restype = None
LIBPCP_MMV.mmv_set_string.argtypes = [
    c_void_p, POINTER(pmAtomValue), c_char_p, c_int]

LIBPCP_MMV.mmv_stats_add.restype = None
LIBPCP_MMV.mmv_stats_add.argtypes = [c_void_p, c_char_p, c_char_p, c_double]

LIBPCP_MMV.mmv_stats_inc.restype = None
LIBPCP_MMV.mmv_stats_inc.argtypes = [c_void_p, c_char_p, c_char_p]

LIBPCP_MMV.mmv_stats_set.restype = None
LIBPCP_MMV.mmv_stats_set.argtypes = [c_void_p, c_char_p, c_char_p, c_double]

LIBPCP_MMV.mmv_stats_add_fallback.restype = None
LIBPCP_MMV.mmv_stats_add_fallback.argtypes = [
    c_void_p, c_char_p, c_char_p, c_char_p, c_double]

LIBPCP_MMV.mmv_stats_inc_fallback.restype = None
LIBPCP_MMV.mmv_stats_inc_fallback.argtypes = [
    c_void_p, c_char_p, c_char_p, c_char_p]

LIBPCP_MMV.mmv_stats_interval_start.restype = POINTER(pmAtomValue)
LIBPCP_MMV.mmv_stats_interval_start.argtypes = [
    c_void_p, POINTER(pmAtomValue), c_char_p, c_char_p]

LIBPCP_MMV.mmv_stats_interval_end.restype = None
LIBPCP_MMV.mmv_stats_interval_end.argtypes = [c_void_p, POINTER(pmAtomValue)]

LIBPCP_MMV.mmv_stats_set_strlen.restype = None
LIBPCP_MMV.mmv_stats_set_strlen.argtypes = [
    c_void_p, c_char_p, c_char_p, c_char_p, c_long]


#
# class MemoryMappedValues
#
# This class wraps the MMV (Memory Mapped Values) library functions
#

class MemoryMappedValues(object):
    """ Defines a set of PCP Memory Mapped Value (MMV) metrics

        Creates PCP metrics from an instrumented python script
        via pmdammv (Performance Metrics Domain Agent for MMV)
    """

    def __init__(self, name, flags=0, cluster=42):
        if not isinstance(name, bytes):
            name = name.encode('utf-8')
        self._name = name
        self._cluster = cluster  # PMID cluster number (domain is MMV)
        self._flags = flags      # MMV_FLAGS_* flags
        self._metrics = []
        self._indoms = []
        self._handle = None      # pointer to the memory mapped area

    def start(self):
        """ Initialise the underlying library with metrics/instances.
            On completion of this call, we're all visible to pmdammv.
        """
        count_metrics = len(self._metrics)
        metrics = (mmv_metric * count_metrics)()
        for i in range(count_metrics):
            metrics[i] = self._metrics[i]
        count_indoms = len(self._indoms)
        indoms = (mmv_indom * count_indoms)()
        for i in range(count_indoms):
            indoms[i] = self._indoms[i]
        self._handle = LIBPCP_MMV.mmv_stats_init(self._name, self._cluster,
                                                 self._flags,
                                                 metrics, count_metrics,
                                                 indoms, count_indoms)

    def stop(self):
        """ Shut down the underlying library with metrics/instances.
            This closes the mmap file preventing any further updates.
        """
        if self._handle is not None:
            LIBPCP_MMV.mmv_stats_stop(self._name, self._handle)
        self._handle = None

    def restart(self):
        """ Cleanly stop-if-running and restart MMV export services. """
        self.stop()
        self.start()

    def started(self):
        """ Property flagging an active memory mapping """
        if self._handle is None:
            return 0
        return 1

    def add_indoms(self, indoms):
        """ Make a list of instance domains visible to the MMV export """
        self._indoms = indoms
        if self.started():
            self.restart()

    def add_indom(self, indom):
        """ Make an additional instance domain visible to the MMV export """
        self._indoms.append(indom)
        self.add_indoms(self._indoms)

    def add_metrics(self, metrics):
        """ Make a list of metrics visible to the MMV export """
        self._metrics = metrics
        if self.started():
            self.restart()

    def add_metric(self, metric):
        """ Make an additional metric visible to the MMV export """
        self._metrics.append(metric)
        self.add_metrics(self._metrics)


    def lookup_mapping(self, name, inst):
        """ Find the memory mapping for a given metric name and instance

            This handle can be used to directly manipulate metric values
            by other interfaces in this module.  This is the *preferred*
            technique for manipulating MMV values.  It is more efficient
            and the alternative (name/inst lookups) is made available as
            a convenience only for situations where performance will not
            be affected by repeated (linear) name/inst lookups.
        """
        if name is not None and not isinstance(name, bytes):
            name = name.encode('utf-8')
        if inst is not None and not isinstance(inst, bytes):
            inst = inst.encode('utf-8')
        return LIBPCP_MMV.mmv_lookup_value_desc(self._handle, name, inst)

    def add(self, mapping, value):
        """ Increment the mapped metric by a given value """
        LIBPCP_MMV.mmv_inc_value(self._handle, mapping, value)

    def inc(self, mapping):
        """ Increment the mapped metric by one """
        LIBPCP_MMV.mmv_inc_value(self._handle, mapping, 1)

    def set(self, mapping, value):
        """ Set the mapped metric to a given value """
        LIBPCP_MMV.mmv_set_value(self._handle, mapping, value)

    def set_string(self, mapping, value):
        """ Set the string mapped metric to a given value """
        if value is not None and not isinstance(value, bytes):
            value = value.encode('utf-8')
        LIBPCP_MMV.mmv_set_string(self._handle, mapping, value, len(value))

    def interval_start(self, mapping):
        """ Start a timed interval for the mapped metric
            The opaque handle (mapping) returned is passed to interval_end().
        """
        return LIBPCP_MMV.mmv_stats_interval_start(self._handle, mapping, 0, 0)

    def interval_end(self, mapping):
        """ End a timed interval, the metrics time is increased by interval """
        return LIBPCP_MMV.mmv_stats_interval_end(self._handle, mapping)


    def lookup_add(self, name, inst, value):
        """ Lookup the named metric[instance] and add a value to it """
        if name is not None and not isinstance(name, bytes):
            name = name.encode('utf-8')
        if inst is not None and not isinstance(inst, bytes):
            inst = inst.encode('utf-8')
        LIBPCP_MMV.mmv_stats_add(self._handle, name, inst, value)

    def lookup_inc(self, name, inst):
        """ Lookup the named metric[instance] and add one to it """
        if name is not None and not isinstance(name, bytes):
            name = name.encode('utf-8')
        if inst is not None and not isinstance(inst, bytes):
            inst = inst.encode('utf-8')
        LIBPCP_MMV.mmv_stats_inc(self._handle, name, inst)

    def lookup_set(self, name, inst, value):
        """ Lookup the named metric[instance] and set its value """
        if name is not None and not isinstance(name, bytes):
            name = name.encode('utf-8')
        if inst is not None and not isinstance(inst, bytes):
            inst = inst.encode('utf-8')
        LIBPCP_MMV.mmv_stats_set(self._handle, name, inst, value)

    def lookup_interval_start(self, name, inst):
        """ Lookup the named metric[instance] and start an interval
            The opaque handle returned is passed to interval_end().
        """
        if name is not None and not isinstance(name, bytes):
            name = name.encode('utf-8')
        if inst is not None and not isinstance(inst, bytes):
            inst = inst.encode('utf-8')
        return LIBPCP_MMV.mmv_stats_interval_start(self._handle,
                                                   None, name, inst)

    def lookup_set_string(self, name, inst, s):
        """ Lookup the named metric[instance] and set its string value """
        if name is not None and not isinstance(name, bytes):
            name = name.encode('utf-8')
        if inst is not None and not isinstance(inst, bytes):
            inst = inst.encode('utf-8')
        if not isinstance(s, bytes):
            s = s.encode('utf-8')
        LIBPCP_MMV.mmv_stats_set_strlen(self._handle, name, inst, s, len(s))

    def lookup_add_fallback(self, name, inst, fall, value):
        """ Lookup the named metric[instance] and set its value if found
            If instance is not found, fallback to using a second instance
            One example use is: add value to bucketN else use a catch-all
                                bucket such as "other"
        """
        if name is not None and not isinstance(name, bytes):
            name = name.encode('utf-8')
        if inst is not None and not isinstance(inst, bytes):
            inst = inst.encode('utf-8')
        LIBPCP_MMV.mmv_stats_add_fallback(self._handle, name, inst, fall, value)

    def lookup_inc_fallback(self, name, inst, fallback):
        """ Lookup the named metric[instance] and increment its value if found
            If instance is not found, fallback to using a second instance
            One sample use is: inc value of BucketA, else inc a catch-all
        """
        if name is not None and not isinstance(name, bytes):
            name = name.encode('utf-8')
        if inst is not None and not isinstance(inst, bytes):
            inst = inst.encode('utf-8')
        LIBPCP_MMV.mmv_stats_inc_fallback(self._handle, name, inst, fallback)
