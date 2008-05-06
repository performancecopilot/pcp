/* -*- C++ -*- */

#ifndef _PMC_CONTEXT_H_
#define _PMC_CONTEXT_H_

/*
 * Copyright (c) 1998-2005 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: Context.h,v 1.3 2005/05/10 00:46:37 kenmcd Exp $"

#ifndef PCP_DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <pcp/pmc/PMC.h>
#include <pcp/pmc/Vector.h>
#include <pcp/pmc/String.h>
#include <pcp/pmc/Hash.h>
#include <pcp/pmc/Desc.h>
#include <pcp/pmc/Indom.h>
#include <pcp/pmc/Source.h>

#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

struct PMC_NameToId
{
    PMC_String	_name;
    pmID	_id;
};

typedef PMC_List<PMC_NameToId> PMC_NameList;
typedef PMC_List<PMC_Desc*> PMC_DescList;
typedef PMC_List<PMC_Indom*> PMC_IndomList;
typedef PMC_List<PMC_Metric*> PMC_MetricList;

class PMC_Context
{
private:

    int			_context;	// PMAPI Context handle
    PMC_Source*		_source;	// Handle to the source description
    PMC_NameList	_names;		// Mapping between names and PMIDs
    PMC_IntList		_pmids;		// List of valid PMIDs to be fetched
    PMC_DescList	_descs;		// List of requested metric descs
    PMC_IndomList	_indoms;	// List of requested indoms 
    PMC_MetricList	_metrics;	// List of metrics using this context
    struct timeval	_currTime;	// Time of current fetch
    struct timeval	_prevTime;	// Time of previous fetch
    double		_delta;		// Time between fetches
    PMC_Bool		_needReconnect;	// Need to reconnect the context

    static PMC_StrList*	theStrList;	// List of metric names in traversal

public:

    ~PMC_Context();

    // Create a new context within this group
    PMC_Context(PMC_Source* source);

    // Is this a valid context?
    int status() const
	{ return (_context < 0 ? _context : 0); }

    // The PMAPI context handle
    int hndl() const
	{ return _context; }

    // Source description
    PMC_Source const& source() const
	{ return *_source; }

    // Number of unique pmIDs in use
    uint_t numIDs() const
	{ return _pmids.length(); }

    // Access to each unique pmID
    pmID id(uint_t index) const
	{ return _pmids[index]; }

    // Number of descriptors
    uint_t numDesc() const
	{ return _descs.length(); }

    // Access to each descriptor
    PMC_Desc const& desc(uint_t index) const
	{ return *(_descs[index]); }

    // Access to each descriptor
    PMC_Desc& desc(uint_t index)
	{ return *(_descs[index]); }

    // Number of indom descriptors requested from this context
    uint_t numIndoms() const
	{ return _indoms.length(); }

    // Access to each indom
    PMC_Indom const& indom(uint_t index) const
	{ return *(_indoms[index]); }

    // Access to each indom
    PMC_Indom& indom(uint_t index)
	{ return *(_indoms[index]); }

    // Lookup the descriptor and indom for metric <name>
    int lookupDesc(const char *name, pmID& id);
    int lookupDesc(const char *name, uint_t& desc, uint_t& indom);
    int lookupDesc(pmID pmid, uint_t& desc, uint_t& indom);

    // Use this timezone
    int useTZ();

    // Number of metrics using this context
    uint_t numMetrics() const
	{ return _metrics.length(); }

    // Get a handle to a metric
    PMC_Metric const& metric(uint_t index) const
	{ return *(_metrics[index]); }
    PMC_Metric& metric(uint_t index)
	{ return *(_metrics[index]); }

    // Add a metric using this context
    void addMetric(PMC_Metric* metric);

    // Fetch metrics using this context
    int fetch(PMC_Bool update);

    struct timeval const& timeStamp() const
	{ return _currTime; }

     // Time since previous fetch
    double timeDelta() const
	{ return _delta; }

    // Traverse namespace
    int traverse(char const* name, PMC_StrList& list);

    // Output the source description
    friend ostream &operator<<(ostream &os, const PMC_Context & rhs);

    // Dump debugging information
    void dump(ostream& os);

    // Dump list of metrics
    void dumpMetrics(ostream& os);

private:

    static void dometric(char const* name);

    PMC_Context();
    PMC_Context(const PMC_Context &);
    const PMC_Context &operator=(const PMC_Context &);
    // Never defined
};

#endif /* _PMC_CONTEXT_H_ */
