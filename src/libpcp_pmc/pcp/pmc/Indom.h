/* -*- C++ -*-
 * Copyright (c) 1997-2005 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifndef _PMC_INDOM_H_
#define _PMC_INDOM_H_

#ident "$Id: Indom.h,v 1.10 2005/05/10 01:59:58 kenmcd Exp $"

#include <iostream.h>
#include <assert.h>
#include <pcp/pmc/PMC.h>
#include <pcp/pmc/String.h>

struct PMC_Instance
{
    int		_inst;		// Instance internal id
    PMC_String	_name;		// Instance external id
    int		_refCount;	// Reference count
    int		_index;		// Index into pmResult of last fetch
				// May also be used to index the next NULL inst
    PMC_Bool	_active;	// Instance was listed in last indom lookup

    ~PMC_Instance()
	{}
    PMC_Instance();
    PMC_Instance(int id, const char* name);
    PMC_Instance const& operator=(PMC_Instance const&);

    PMC_Bool null() const
	{ return (_inst == (int)PM_IN_NULL ? PMC_true : PMC_false); }

    PMC_Instance(PMC_Instance const&); // Never defined
};

typedef PMC_Vector<PMC_Instance> PMC_InstVector;
typedef PMC_List<PMC_Instance> PMC_InstList;

class PMC_Indom
{

private:

    int			_sts;
    int			_type;
    pmInDom		_id;
    PMC_InstList	_instances;	// Sparse list of instances
    PMC_Bool		_profile;	// Does the profile need to be updated
    PMC_Bool		_changed;	// Did indom change in the last fetch?
    PMC_Bool		_updated;	// Has the indom been updated?
    uint_t		_count;		// Number of referenced instances
    uint_t		_nullCount;	// Count of NULL instances
    uint_t		_nullIndex;	// Index to first NULL instance
    uint_t		_numActive;	// Number of active instances
    uint_t		_numActiveRef;	// Number of active referenced insts
    
public:

    ~PMC_Indom();

    // Create a instance domain description for context <type>
    PMC_Indom(int type, PMC_Desc &desc);

    // Normal PCP status, <0 is an error
    int status() const
	{ return _sts; }

    // Internal indom id
    int id() const
	{ return _id; }

    // Number of instances
    uint_t numInsts() const
	{ return _instances.length() - _nullCount; }

    // Number of active instances
    uint_t numActiveInsts() const
	{ return _numActive; }

    // Length of instance list - some of the instances may be NULL hence
    // this may be larger than numInsts()
    uint_t listLen() const
	{ return _instances.length(); }

    // Internal instance id for instance <index>
    int inst(uint_t index) const
	{ return _instances[index]._inst; }

    // External instance name for instance <index>
    const PMC_String &name(uint_t index) const
	{ return _instances[index]._name; }

    // Is this instance null?
    PMC_Bool nullInst(uint_t index) const
	{ return _instances[index].null(); }

    // Was this instance listed in the last update?
    PMC_Bool activeInst(uint_t index) const
	{ return _instances[index]._active; }

    // Is this instance referenced by any metrics?
    PMC_Bool refInst(uint_t index) const
	{ return (_instances[index]._refCount > 0 ? PMC_true : PMC_false); }

    // Return index into table for instance with external <name>
    // Also adds a reference to this instance for the profile
    int lookup(PMC_String const& name);

    // Add a reference to all instances in this indom
    // Id <active> is set, only reference active instances
    void refAll(PMC_Bool active = PMC_false);

    // Remove a reference to an instance
    void removeRef(uint_t index);

    // Number of instances referenced
    uint_t refCount() const
	{ return _count; }

    // Was the indom different on the last fetch?
    PMC_Bool changed() const
	{ return _changed; }

    // About to fetch, mark this indom as unchanged
    void newFetch()
	{ _changed = PMC_false; _updated = PMC_true; }

    // Mark that the indom was different in a previous fetch
    void hasChanged()
	{ _changed = PMC_true; _updated = PMC_false; }

    // Update indom with latest instances
    int update();

    // Has profile changed since last call to genProfile
    PMC_Bool diffProfile() const
	{ return _profile; }

    // Generate profile for current context
    int genProfile();

    // Get likely index into pmResult for instance <inst>
    int index(uint_t inst) const
	{ return _instances[inst]._index; }
    // Change the <index> for instance <inst>
    void setIndex(uint_t inst, int index)
	{ _instances[inst]._index = index; }
    
    // Dump some debugging output for this indom
    void dump(ostream &os) const;
};

#endif /* _PMC_INDOM_H_ */
