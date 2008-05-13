/* -*- C++ -*- */

#ifndef _PMC_GROUP_H_
#define _PMC_GROUP_H_

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

#include <pcp/pmc/PMC.h>
#include <pcp/pmc/List.h>
#include <pcp/pmc/String.h>
#include <pcp/pmc/Bool.h>
#include <pcp/pmc/Context.h>
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

typedef PMC_List<PMC_Context*> PMC_ContextList;

class PMC_Group
{

public:

    enum TimeZoneFlag { localTZ, userTZ, groupTZ, unknownTZ };

private:

    // Contexts
    PMC_ContextList	_contexts;	// List of all contexts in this group
    PMC_Bool		_restrictArch;	// Only one archive per host
    int			_mode;		// Default context type
    int			_use;		// Context in use
    PMC_Source*		_localSource;	// Localhost source desc

    // Timezones
    TimeZoneFlag	_tzFlag;	// default TZ type
    int			_tzDefault;	// handle to default TZ
    int			_tzUser;	// handle to user defined TZ
    PMC_String		_tzUserStr;	// user defined TZ;
    uint_t		_tzGroupIndex;	// index to group context used for
					// current timezone

    // Timezone for localhost from environment
    static PMC_Bool	_tzLocalInit;	// got TZ from environment
    static int		_tzLocal;	// handle to environment TZ
    static PMC_String	_tzLocalStr;	// environment TZ string
    static PMC_String	_localHost;	// name of localhost

    // Archives
    struct timeval	_timeStart;	// Start of first archive
    struct timeval	_timeEnd;	// End of last archive
    double		_timeEndDbl;	// End of last archive
    
public:

    ~PMC_Group();

    // Create a new fetch group
    PMC_Group(PMC_Bool restrictArchives = PMC_false);

// Contexts

    // The number of contexts in this group
    uint_t numContexts() const
	{ return _contexts.length(); }

    // Return a handle to the contexts
    PMC_Context const& context(uint_t index) const
	{ return *(_contexts[index]); }
    PMC_Context& context(uint_t index)
	{ return *(_contexts[index]); }

    // The type of the default context
    int mode() const
	{ return _mode; }

    // Handle to the active context
    PMC_Context* which() const
	{ return _contexts[_use]; }

    // Index to the active context
    uint_t whichIndex() const
    	{ return _use; }

    // Use another context
    int use(int type, char const* source);

    // Use context from list
    int use(uint_t index)
	{ _use = index; return useContext(); }

    // Is the default context defined
    PMC_Bool defaultDefined() const
	{ return (numContexts() > 0 ? PMC_true : PMC_false); }

    // Use the default context
    int useDefault();

    // Create a context to the localhost
    void createLocalContext();

// Metrics

    // Add a new metric to the group
    PMC_Metric* addMetric(char const* str, double theScale = 0.0,
			  PMC_Bool active = PMC_false);
    PMC_Metric* addMetric(pmMetricSpec* theMetric, double theScale = 0.0,
			  PMC_Bool active = PMC_false);

    // Fetch all the metrics in this group
    // By default, do all rate conversions and counter wraps
    int fetch(PMC_Bool update = PMC_true);

    // Set the archive position and mode
    int setArchiveMode(int mode, const struct timeval *when, int interval);

// Timezones

    // Use TZ of current context as default
    int useTZ();

    // Use this TZ as default
    int useTZ(const PMC_String &tz);

    // Use local TZ as default
    int useLocalTZ();

    // Return the default context
    void defaultTZ(PMC_String &label, PMC_String &tz);

    // Which is the default timezone
    TimeZoneFlag defaultTZ() const
	{ return _tzFlag; }

    // Use the previously defined default timezone
    int useDefaultTZ();

// pmtime interaction

    // When is the start of the earliest log
    struct timeval const& logStart() const
    	{ return _timeStart; }

    // When the end of the last log
    struct timeval const& logEnd() const
    	{ return _timeEnd; }

    // Determine the archive start and finish times
    void updateBounds();

    void dump(ostream &os);
    
private:

    int useContext();

    PMC_Group(const PMC_Group &);
    const PMC_Group &operator=(const PMC_Group &);
    // Never defined
};

#endif /* _PMC_GROUP_H_ */
