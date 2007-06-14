/*
 * Copyright (c) 1997,2005 Silicon Graphics, Inc.  All Rights Reserved.
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

#ident "$Id: Indom.c++,v 1.9 2005/05/10 01:59:58 kenmcd Exp $"

#include <iostream.h>
#include <ctype.h>
#include "Indom.h"
#include "Desc.h"

#ifdef __sgi
#pragma instantiate PMC_Vector<PMC_Instance>
#pragma instantiate PMC_List<PMC_Instance>
#endif

PMC_Instance::PMC_Instance()
: _inst(PM_IN_NULL),
  _name(),
  _refCount(0),
  _index(-1),
  _active(PMC_false)
{
}

PMC_Instance::PMC_Instance(int id, const char* name)
: _inst(id),
  _name(name),
  _refCount(0),
  _index(-1),
  _active(PMC_true)
{
}

PMC_Instance const&
PMC_Instance::operator=(PMC_Instance const& rhs)
{
    if (this != &rhs) {
	_inst = rhs._inst;
	_name = rhs._name;
	_refCount = rhs._refCount;
	_index = rhs._index;
	_active = rhs._active;
    }
    return *this;
}

PMC_Indom::~PMC_Indom()
{
}

PMC_Indom::PMC_Indom(int type, PMC_Desc &desc)
: _sts(0), 
  _type(type),
  _id(desc.desc().indom), 
  _instances(4),
  _profile(PMC_false),
  _changed(PMC_false),
  _updated(PMC_true),
  _count(0),
  _nullCount(0),
  _nullIndex(UINT_MAX),
  _numActive(0),
  _numActiveRef(0)
{
    int		*instList;
    char	**nameList;
    uint_t	i;

    if (_id == PM_INDOM_NULL)
	_sts = PM_ERR_INDOM;
    else if (_type == PM_CONTEXT_HOST || _type == PM_CONTEXT_LOCAL)
	_sts = pmGetInDom(_id, &(instList), &(nameList));
    else if (_type == PM_CONTEXT_ARCHIVE)
	_sts = pmGetInDomArchive(_id, &(instList), &(nameList));
    else
	_sts = PM_ERR_NOCONTEXT;

    if (_sts > 0) {
	_instances.resize(_sts);

	for (i = 0; i < (uint_t)_sts; i++)
	    _instances.append(PMC_Instance(instList[i],
					   nameList[i]));

	_numActive = _sts;

	free(instList);
	free(nameList);

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_INDOM) {
	    cerr << "PMC_Indom::PMC_Indom: indom ";
	    dump(cerr);
	}
#endif

    }

#ifdef PCP_DEBUG
    else if (_sts < 0 && pmDebug & DBG_TRACE_PMC) {
	cerr << "PMC_Indom::PMC_Indom: unable to lookup "
	     << pmInDomStr(_id) << " from "
	     << (_type == PM_CONTEXT_ARCHIVE ? "archive" : "host/local")
	     << " source: " << pmErrStr(_sts) << endl;
    }
#endif
}

int
PMC_Indom::lookup(PMC_String const& name)
{
    uint_t	i;
    char	*p = NULL;
    const char	*q = NULL;

    for (i = 0; i < _instances.length(); i++) {
	if (_instances[i].null())
	    continue;
	if (_instances[i]._name == name) {
	    if (_instances[i]._refCount == 0) {
		_profile = PMC_true;
		_count++;
		if (_instances[i]._active)
		    _numActiveRef++;
	    }
	    _instances[i]._refCount++;
	    return i;
	    /*NOTREACHED*/
	}
    }

    // Match up to the first space
    // Need this for proc and similiar agents

    for (i = 0; i < _instances.length(); i++) {
	if (_instances[i].null())
	    continue;

	p = strchr(_instances[i]._name.ptr(), ' ');
		    
	if (p != NULL &&
	    (int)name.length() == (p - _instances[i]._name.ptr()) &&
	    strncmp(name.ptr(), _instances[i]._name.ptr(), 
		    p - _instances[i]._name.ptr()) == 0) {

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMC)
		cerr << "PMC_Indom::lookup: inst \"" << name << "\"(" << i
		     << ") matched to \"" << _instances[i]._name << "\"(" << i 
		     << ')' << endl;
#endif

	    if (_instances[i]._refCount == 0) {
		_profile = PMC_true;
		_count++;
		if (_instances[i]._active)
		    _numActiveRef++;
	    }
	    _instances[i]._refCount++;
	    return i;
	    /*NOTREACHED*/
	}
    }

    // If the instance requested is numeric, then ignore leading
    // zeros in the instance up to the first space

    for (i = 0; i < name.length(); i++)
	if (!isdigit(name[i]))
	    break;

    // The requested instance is numeric
    if (i == name.length()) {
	for (i = 0; i < _instances.length(); i++) {
	    if (_instances[i].null())
		continue;

	    p = strchr(_instances[i]._name.ptr(), ' ');

	    if (p == NULL)
		continue;

	    for (q = _instances[i]._name.ptr(); 
		 isdigit(*q) && *q == '0' && q < p;
		 q++);

	    if (q < p && isdigit(*q) && (int)name.length() == (p - q) &&
		strncmp(name.ptr(), q, p - q) == 0) {
			
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_PMC)
		    cerr << "PMC_Metric::setupDesc: numerical inst \""
			 << name << " matched to \"" << _instances[i]._name 
			 << "\"(" << i << ')' << endl;
#endif
		
		if (_instances[i]._refCount == 0) {
		    _profile = PMC_true;
		    _count++;
		    if (_instances[i]._active)
			_numActiveRef++;
		}
		_instances[i]._refCount++;
		return i;
		/*NOTREACHED*/
	    }
	}
    }

    // Looks like I don't know about that instance, I tried my best
		
    return -1;
}

void
PMC_Indom::refAll(PMC_Bool active)
{
    uint_t	i;

    _numActiveRef = 0;

    for (i = 0; i < _instances.length(); i++) {
	if (_instances[i].null() || (active && !_instances[i]._active))
	    continue;

	if (_instances[i]._refCount == 0)
	    _profile = PMC_true;
	if (_instances[i]._active)
	    _numActiveRef++;

	_instances[i]._refCount++;
    }
    _count = _instances.length() - _nullCount;
}

void
PMC_Indom::removeRef(uint_t index)
{
    assert(_instances[index]._refCount);

    _instances[index]._refCount--;
    if (_instances[index]._refCount == 0) {
	_profile = PMC_true;
	_count--;
	if (_instances[index]._active)
	    _numActiveRef--;
    }
}

int
PMC_Indom::genProfile()
{
    uint_t		i;
    uint_t		j;
    int			sts = 0;
    int*		ptr = NULL;
    PMC_IntVector	list;

#ifdef PCP_DEBUG
    char*		action = NULL;
#endif

    // If all instances are referenced or there are no instances
    // then request all instances
    if (_numActiveRef == _numActive || _numActive == 0) {
	sts = pmAddProfile(_id, 0, NULL);

#ifdef PCP_DEBUG
	action = "ALL";
#endif
    }

    // If the number of referenced instances is less than the number
    // of unreferenced active instances, then the smallest profile
    // is to add all the referenced instances
    else if (_count < (_numActive - _numActiveRef)) {

#ifdef PCP_DEBUG
	action = "ADD";
#endif

	sts = pmDelProfile(_id, 0, NULL);
	if (sts >= 0) {
	    list.resize(_count);
	    for (i = 0, j = 0; i < _instances.length(); i++)
		if (!_instances[i].null() && _instances[i]._refCount)
		    list[j++] = _instances[i]._inst;
	    ptr = list.ptr();
	    sts = pmAddProfile(_id, list.length(), ptr);
	}
    }
    
    // Delete those active instances that are not referenced
    else {

#ifdef PCP_DEBUG
	action = "DELETE";
#endif

	sts = pmAddProfile(_id, 0, NULL);
	if (sts >= 0) {
	    list.resize(_instances.length() - _count);
	    for (i = 0, j = 0; i < _instances.length(); i++)
		if (!_instances[i].null() && 
		    _instances[i]._refCount == 0 &&
		    _instances[i]._active)
		    list[j++] = _instances[i]._inst;
	    ptr = list.ptr();
	    sts = pmDelProfile(_id, list.length(), ptr);
	}
    }
    
#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMC || 
	pmDebug & DBG_TRACE_INDOM ||
	pmDebug & DBG_TRACE_PROFILE) {
	cerr << "PMC_Indom::genProfile: id = " << _id << ", count = " 
	     << _count << ", numInsts = " << numInsts() << ", active = "
	     << _numActive << ", activeRef = " << _numActiveRef 
	     << ": " << action << " ptr = " << ptr;
	if (sts < 0)
	    cerr << ", sts = " << sts << ": " << pmErrStr(sts);
	cerr << endl;
    }
#endif

    if (sts >= 0)
	_profile = PMC_false;

    return sts;
}

void
PMC_Indom::dump(ostream &os) const
{
    uint_t	i;

    os << pmInDomStr(_id) << ": " << numInsts() << " instances ("
       << _nullCount << " NULL)" << endl;
    for (i = 0; i < _instances.length(); i++)
	if (!_instances[i].null())
	    os << "  [" << _instances[i]._inst << "] = \""
	       << _instances[i]._name << "\" (" << _instances[i]._refCount 
	       << " refs) " << (_instances[i]._active ? "active" : "inactive")
	       << endl;
	else
	    os << "  NULL -> " << _instances[i]._index << endl;
}

int
PMC_Indom::update()
{
    int		*instList;
    char	**nameList;
    uint_t	i;
    uint_t	j;
    uint_t	count;
    uint_t	oldLen = _instances.length();
    int		sts;

#ifdef PCP_DEBUG
    uint_t	oldNullCount = _nullCount;
#endif

    // If the indom has already been updated, just check that all instances
    // are referenced and remove any that have gone away.
    if (!_changed || _updated) {
	for (i = 0; i < oldLen; i++) {

	    PMC_Instance &inst = _instances[i];
	    if (inst._refCount || inst.null() || inst._active)
		continue;

	    inst._inst = PM_IN_NULL;
	    inst._name = "";
	    inst._index = _nullIndex;
	    inst._refCount = 0;
	    inst._active = PMC_false;
	    _nullIndex = i;
	    _nullCount++;
	    _profile = PMC_true;
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_INDOM && _nullCount != oldNullCount) {
	    cerr << "PMC_Indom::update: Cleaning indom " << pmInDomStr(_id)
		 << ": Removed " << _nullCount - oldNullCount 
		 << " instances" << endl;
	}
#endif

	return 0;
    }

    _updated = PMC_true;

    if (_type == PM_CONTEXT_ARCHIVE)
	return 0;

    if (_type == PM_CONTEXT_HOST || _type == PM_CONTEXT_LOCAL)
	sts = pmGetInDom(_id, &(instList), &(nameList));

    _numActive = 0;
    _numActiveRef = 0;

    if (sts > 0) {
	count = sts;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC)
	    cerr << "PMC_Indom::update: Updating indom " << pmInDomStr(_id)
		 << ": Got " << count << " instances (vs " << numInsts()
		 << ")" << endl;
#endif

	// Any instances which are not in the new indom AND are not
	// referenced can be removed
	for (i = 0; i < oldLen; i++) {

	    PMC_Instance &inst = _instances[i];
	    inst._active = PMC_false;

	    if (inst._refCount || inst.null())
		continue;
	    j = 0;
	    if (i < count && inst._inst == instList[i])
		if (inst._name == nameList[i])
		    continue;
		else
		    j = count;
	    for (; j < count; j++) {
		if (inst._inst == instList[j])
		    if (inst._name == nameList[j])
			break;
		    else
			j = count;
	    }

	    // If j >= count, then instance i has either changed or gone away
	    if (j >= count) {
		inst._inst = PM_IN_NULL;
		inst._name = "";
		inst._index = _nullIndex;
		inst._refCount = 0;
		inst._active = PMC_false;
		_nullIndex = i;
		_nullCount++;
		_profile = PMC_true;
	    }
	}

	for (i = 0; i < count; i++) {
	    // Quick check to see if they are the same
	    if (i < _instances.length() && 
		_instances[i]._inst == instList[i] &&
		_instances[i]._name == nameList[i]) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_INDOM)
		    cerr << "PMC_Indom::update: Unchanged \"" << nameList[i]
			 << "\"(" << instList[i] << ')' << endl;
#endif
		_instances[i]._active = PMC_true;
		_numActive++;
		if (_instances[i]._refCount)
		    _numActiveRef++;
		continue;
	    }

	    for (j = 0; j < oldLen; j++) {
		if (_instances[j].null())
		    continue;

		if (_instances[j]._inst == instList[i]) {
		    // Same instance and same external name but different
		    // order, mark as active. If it has a different
		    // external name just ignore it
		    if (_instances[j]._name == nameList[i]) {
			_instances[j]._active = PMC_true;
			_numActive++;
			if (_instances[j]._refCount)
			    _numActiveRef++;
		    }
#ifdef PCP_DEBUG
		    else if (pmDebug & DBG_TRACE_PMC)
			cerr << "PMC_Indom::update: Ignoring \""
			     << nameList[i] 
			     << "\" with identical internal identifier ("
			     << instList[i] << ")" << endl;
#endif
		    break;
		}
	    }

	    if (j == oldLen) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_INDOM)
		    cerr << "PMC_Indom::update: Adding \"" << nameList[i] 
			 << "\"(" << instList[i] << ")" << endl;
#endif
		
		if (_nullCount) {
		    uint_t newindex = _instances[_nullIndex]._index;
		    _instances[_nullIndex] = PMC_Instance(instList[i],
							  nameList[i]);
		    _nullIndex = newindex;
		    _nullCount--;
		}
		else
		    _instances.append(PMC_Instance(instList[i],
						   nameList[i]));
		_profile = PMC_true;
		_numActive++;
	    }
	}

	free(instList);
	free(nameList);

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_INDOM) {
	    if (_instances.length() == oldLen && _nullCount == oldNullCount)
		cerr << "PMC_Indom::update: indom size unchanged" << endl;
	    else {
		cerr << "PMC_Indom::update: indom changed from "
		     << oldLen - oldNullCount << " to " << numInsts() << endl;
		dump(cerr);
	    }
	}
#endif
    }
    else {
	for (i = 0; i < _instances.length(); i++)
	    _instances[i]._active = PMC_false;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC) {
	    if (sts == 0)
		cerr << "PMC_Indom::update: indom empty!" << endl;
	    else
		cerr << "PMC_Indom::update: unable to lookup "
		     << pmInDomStr(_id) << " from host/local source: "
		     << pmErrStr(sts) << endl;
	}
#endif
    }

    return sts;
}
