/* -*- C++ -*- */

#ifndef _PMC_SOURCE_H_
#define _PMC_SOURCE_H_

/*
 * Copyright (c) 1998,2005 Silicon Graphics, Inc.  All Rights Reserved.
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

#ident "$Id: Source.h,v 1.3 2005/05/10 00:46:37 kenmcd Exp $"

#include <iostream.h>
#include "PMC.h"
#include "String.h"
#include "List.h"

typedef PMC_List<PMC_Source*> PMC_SourceList;

class PMC_Source
{
private:

    // Local State

    int			_sts;
    int			_type;
    PMC_String		_source;
    PMC_String		_host;
    PMC_String		_desc;
    PMC_String		_timezone;
    int			_tz;
    PMC_IntList		_hndls;		// Contexts created for this source
    struct timeval	_start;
    struct timeval	_end;
    PMC_Bool		_dupFlag;	// Dup has been called and the first
					// context is in use

    // List of all sources

    static PMC_SourceList	_sourceList;
    static PMC_String		_localHost;

public:

    ~PMC_Source();

    // Get the source description by searching the list of existing sources
    // and returning a new source only if required.
    // If matchHosts is true, then it will attempt to map a live context
    // to an archive source. If no matching archive context is found,
    // a NULL pointer is returned.
    static PMC_Source* getSource(int type, 
				 const char* source, 
				 PMC_Bool matchHosts = PMC_false);

    // retry context/connection (e.g. if it failed in the constructor)
    void retryConnect(int type, const char *source);

    // Is this a valid source
    int status() const
	{ return _sts; }

    // Context type
    int type() const
	{ return _type; }

    // Source - same as host for all but archive contexts
    PMC_String const& source() const
	{ return _source; }

    // Host
    PMC_String const& host() const
	{ return _host; }

    // Timezone handle of host
    int tzHndl() const
	{ return _tz; }

    // Timezone of host
    PMC_String timezone() const
	{ return _timezone; }

    struct timeval const& start() const
	{ return _start; }

    struct timeval const& end() const
	{ return _end; }

    // Description of source
    PMC_String const& desc() const
	{ return _desc; }

    // Number of active contexts to this source
    uint_t numContexts() const
	{ return _hndls.length(); }

    // Create a new context to this source
    int dupContext();

    // Delete context to this source
    int delContext(int hndl);

    // Output the source
    friend ostream &operator<<(ostream &os, const PMC_Source & rhs);

    // Dump all info about a source
    void dump(ostream& os);

    // Dump list of known sources
    static void dumpList(ostream& os);

private:

    // Create a new source
    PMC_Source(int type, const char* source);

    PMC_Source();
    PMC_Source(const PMC_Source &);
    const PMC_Source &operator=(const PMC_Source &);
    // Never defined
};

#endif /* _PMC_SOURCE_H_ */
