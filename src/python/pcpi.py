#
# pcpi.py
#
# Copyright (C) 2009-2012 Michael T. Werner
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

"""Convenience classes building on the base pcp extension module

"""

##
#

##############################################################################
#
# imports
#

import pcp
from pcp import *


##############################################################################
#
# classes
#

##
# MetricCore
#
# core metric information that can be queried from the PMAPI
# PMAPI metrics are unique by name, and MetricCores should be also
# rarely, some PMAPI metrics with different names might have identical PMIDs
# PMAPI metrics are unique by (name) and by (name,pmid) - & _usually_ by (pmid)
# 

class MetricCore( object ):
    def __init__( self, ctx, name, pmid ):
        self.ctx = ctx
        self.name = name
        self.pmid = pmid
        self.desc = None
        self.text = None
        self.help = None

##
# Metric
#
# additional metric information, such as conversion factors and result values
# several instances of Metric may share a MetricCore instance

class Metric( object ):

    ##
    # constructor

    def __init__( self, core ):
        self._core = core
        self._result = None
        self._prev = None
        self._convType = core.desc.type
        self._convUnits = core.desc.units
        self._errorStatus = None
        self._netValue = None
        self._netPrev = None

    ##
    # core property read methods

    def _R_ctx( self ):
        return self._core.ctx
    def _R_name( self ):
        return self._core.name
    def _R_pmid( self ):
        return self._core.pmid
    def _R_desc( self ):
        return self._core.desc
    def _R_text( self ):
        return self._core.text
    def _R_help( self ):
        return self._core.help

    ## 
    # instance property read methods

    def computeVal( self, inResult ):
        # compute value
        timestamp, vset = inResult
        ctx = self.ctx
        instD = ctx.mcGetInstD( self.desc.indom )
        numval = vset.numval
        valL = []
        print 'AA'
        for i in range( numval ):
            instval = vset.vlist[i].inst
            name = instD[ instval ]
            outAtom = self.ctx.pmExtractValue( vset.valfmt,
                     vset.vlist[i],
                     self.desc.type, self._convType )
            atom = outAtom[1]
            if self._convUnits:
                atom = self.ctx.pmConvScale( self._convType, atom,
                     self.desc.units, self._convUnits )
            print 'AA' + str(atom)
            x = atom.dref( self._convType )
            valL.append( (instval, name, x) )
        return valL


    def _R_result( self ):
        return self._result
    def _R_prev( self ):
        return self._prev
    def _R_convType( self ):
        return self._convType
    def _R_convUnits( self ):
        return self._convUnits
    def _R_errorStatus( self ):
        return self._errorStatus

    def _R_netPrev( self ):
        print 'AA0'
        if not self._prev:
            return None
        if type(self._netPrev) == type(None):
            self._netPrev = self.computeVal( self._prev )
        return self._netPrev
    def _R_netValue( self ):
        print 'AA1 '  + str(self._result)
        if not self._result:
            print 'AA2'
            return None
        if type(self._netValue) == type(None):
            print 'AA3'
            self._netValue = self.computeVal( self._result )
        return self._netValue

    def _W_result( self, value ):
        print 'BB ' + str(self._result)
        self._prev = self._result
        self._result = value
        self._netPrev = self._netValue
        self._netValue = None
    def _W_convType( self, value ):
        self._convType = value
    def _W_convUnits( self, value ):
        self._convUnits = value

    # interface to properties in MetricCore
    ctx = property( _R_ctx, None, None, None )
    name = property( _R_name, None, None, None )
    pmid = property( _R_pmid, None, None, None )
    desc = property( _R_desc, None, None, None )
    text = property( _R_text, None, None, None )
    help = property( _R_help, None, None, None )

    # properties specific to this instance
    result = property( _R_result, _W_result, None, None )
    prev = property( _R_prev, None, None, None )
    convType = property( _R_convType, _W_convType, None, None )
    convUnits = property( _R_convUnits, _W_convUnits, None, None )
    errorStatus = property( _R_errorStatus, None, None, None )
    netValue = property( _R_netValue, None, None, None )
    netPrev = property( _R_netPrev, None, None, None )

    def metricPrint( self ):
        print self.ctx.pmIDStr( self.pmid ), self.name
        ts, vset = self.result
        print "   ", "sec =", ts.tv_sec, " usec =", ts.tv_usec
        indomstr = self.ctx.pmInDomStr( self.desc.indom )
        print "   ", "indom:", indomstr
        instD = self.ctx.mcGetInstD( self.desc.indom )
        for inst, name, val in self.netValue:
            print "   ", name, val



##
# a cache for metric cores
#
# a cache of MetricCores is kept to reduce calls into the PMAPI library
# this also slightly reduces the memory footprint of Metric instances
# that share a common MetricCore
# a cache of instance domain information is also kept, which further
# reduces calls into the PMAPI and reduces the memory footprint of
# Metric objects that share a common instance domain
#
class MetricCache( pmContext ):

    ##
    # overloads
  
    def __init__( self, type=PM_CONTEXT_HOST, target="localhost" ):
        pmContext.__init__( self, type, target )
        self._mcIndomD = {}
        self._mcByNameD = {}
        self._mcByPmidD = {}

    ##
    # methods
  
    def mcGetInstD( self, indom ):
        return self._mcIndomD[ indom ]

    def _mcAdd( self, core ):
#        print "XXX" + str((core.desc[1]))
#        print get_indom(core.desc[1][0])
#        i = get_indom(core.desc[1][0])
        i = core.desc.indom
        if not self._mcIndomD.has_key( i ):
            # if i == PM_INDOM_NULL:
            #     d = { -1 : "(null)" }
            if i == PM_INDOM_NULL:
                d = { PM_IN_NULL : "PM_IN_NULL" }
            else:
                instL, nameL = self.pmGetInDom( i )
                d = dict( zip( instL, nameL ) )
            self._mcIndomD.update( { i : d } )

        self._mcByNameD.update( { core.name : core } )
        self._mcByPmidD.update( { core.pmid : core } )

    def mcGetCoresByName( self, nameL ):
        coreL = []
        missD = None
        errL = None
        # lookup names in cache
        for index, name in enumerate( nameL ):
            # lookup metric core in cache
            core = self._mcByNameD.get( name )
            if not core:
                # cache miss
                if not missD:
                    missD = {}
                missD.update( { name : index } )
            coreL.append( core )

        # some cache lookups missed, fetch pmids and build missing MetricCores
        if missD:
            idL, errL = self.mcFetchPmids( missD.keys() )
            for name, pmid in idL:
                if pmid == PM_ID_NULL:
                    # fetch failed for the given metric name
                    if not errL:
                       errL = []
                    errL.append( name )
                else:
                    # create core
                    newcore = self._mcCreateCore( name, pmid )
                    # update core ref in return list
                    coreL[ missD[name] ] = newcore

        return coreL, errL
    
    def _mcCreateCore( self, name, pmid ):
        newcore = MetricCore( self, name, pmid )
        d = self.pmLookupDesc( pmid )[1]
        newcore.desc = d[0].contents
        # insert core into cache
        self._mcAdd( newcore )
        return newcore

    def mcFetchPmids( self, nameL ):
        # note: some names have identical pmids
        errL = None
        nameA = (c_char_p * len(nameL))()
        for index, name in enumerate( nameL ):
            nameA[index] = c_char_p( name )
        status, pmidA = self.pmLookupName( nameA )
        if status < len( nameA ):
            print "lookup failed: got ", status, " of ", len(nameA)
            sys.exit()

        return zip( nameA, pmidA ), errL


##
# a handle for a pointer to a pmResult structure
# this provides access to the pmResult structure returned by pmFetch
#

class MetricResultHandle(Structure):

    _fields_ = [ ("_data_p", POINTER(pmResult)) ]

    ##
    # overloads

    def __init__( self, result_p=0 ):
       if result_p:
          self.result_p = result_p
    def __del__( self ):
       if self._data_p and libpcp:
           libpcp.pmFreeResult( self._data_p )

    ##
    # property methods

    def data_write( self, value ):
        print 'EE ' + str(type(value))
        if self._data_p and libpcp:
            libpcp.pmFreeResult( self._data_p )
        self._data_p = value
    def data_read( self ):
        return self._data_p
    def ts_read( self ):
        return self._data_p.contents.timestamp
    def np_read( self ):
        return self._data_p.contents.numpmid
    def vs_read( self ):
        return self._data_p.contents.vset

    ##
    # property definitions

    timestamp = property( ts_read, None, None, None )
    numpmid = property( np_read, None, None, None )
    vset = property( vs_read, None, None, None )
    result_p = property( data_read, data_write, None, None )

##
# manages a group of metrics for fetching the values of
# a MetricGroup is a dictionary of Metric objects, for which data can
# be fetched from a target system using a single call to pmFetch
# the Metric objects are indexed by the metric name
# pmFetch fetches data for a list of pmIDs, so there is also a shadow
# dictionary keyed by pmID, along with a shadow list of pmIDs

class MetricGroup( dict ):

    ##
    # property read methods

    def _R_contextCache( self ):
        return self._ctx
    def _R_pmidA( self ):
        return self._pmidA
    def _R_result( self ):
        return self._result
    def _R_prev( self ):
        return self._prev

    ##
    # property write methods

    def _W_result( self, value ):
        print 'CC ' + str(value.result_p.contents.get_numval(0)) + " " + str(type(value))
        self._prev = self._result
        self._result = value

    ##
    # property definitions

    contextCache = property( _R_contextCache, None, None, None )
    pmidA = property( _R_pmidA, None, None, None )
    result = property( _R_result, _W_result, None, None )
    prev = property( _R_prev, None, None, None )

    ##
    # constructor

    def __init__( self, contextCache, inL=[] ):
        self._ctx = contextCache
        self._pmidA = None
        self._result = None
        self._prev = None
        self._altD = {}
        dict.__init__( self )
        self.mgAdd( inL )

    ##
    # methods

    def mgAdd( self, nameL ):
        coreL, errL = self._ctx.mcGetCoresByName( nameL )
        for core in coreL:
            metric = Metric( core )
            self.update( { metric.name : metric } )
            self._altD.update( { metric.pmid : metric } )
        n = len( self )
        self._pmidA = (c_uint * n)()
        for x, key in enumerate( self.keys() ):
            self._pmidA[x] = c_uint( self[key].pmid )

    def mgFetch( self ):
        # fetch the metric values
        print 'DD '
        status, result_p = self._ctx.pmFetch( self._pmidA )
        # handle errors
        # update the group's result pointers
        rh = MetricResultHandle( result_p )
        self.result = rh
        # update the result entries in each metric
        for i in range( rh.numpmid ):
            try:
                print "XX" + str(type(rh.vset[int(i)].contents.pmid))
                self._altD[ rh.vset[int(i)].contents.pmid ].result = (rh.timestamp, rh.vset[i])
            except IndexError:
                pass

##
# manages a dictionary of MetricGroups which can be pmFetch'ed
# inherits from MetricCache, which inherits from pmContext

class MetricGroupManager( dict, MetricCache ):

    ##
    # overloads

    def __init__( self, type=PM_CONTEXT_HOST, target="localhost" ):
        MetricCache.__init__( self, type, target )
        dict.__init__( self )

    def __setitem__( self, attr, value=[] ):
        if self.has_key( attr ):
            raise KeyError, "metric group with that key already exists"
        else:
            dict.__setitem__( self, attr, MetricGroup( self, inL=value ) )
    

