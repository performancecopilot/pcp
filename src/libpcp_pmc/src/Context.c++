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

#ident "$Id: Context.c++,v 1.6 2007/09/11 01:38:10 kimbrr Exp $"

#include <iostream.h>
#include <limits.h>
#include <pcp/pmc/Context.h>
#include <pcp/pmc/Metric.h>

#ifdef __sgi
#pragma instantiate PMC_Vector<PMC_String>
#pragma instantiate PMC_Vector<PMC_Desc*>
#pragma instantiate PMC_Vector<PMC_Indom*>
#pragma instantiate PMC_Vector<PMC_Metric*>
#pragma instantiate PMC_Vector<PMC_NameToId>
#pragma instantiate PMC_List<PMC_String>
#pragma instantiate PMC_List<PMC_Desc*>
#pragma instantiate PMC_List<PMC_Indom*>
#pragma instantiate PMC_List<PMC_Metric*>
#pragma instantiate PMC_List<PMC_NameToId>
#endif

PMC_StrList* PMC_Context::theStrList = 0;

PMC_Context::~PMC_Context()
{
    uint_t	i;

    for (i = 0; i < _metrics.length(); i++)
	if (_metrics[i])
	    delete _metrics[i];
    for (i = 0; i < _descs.length(); i++)
	if (_descs[i])
	    delete _descs[i];
    for (i = 0; i < _indoms.length(); i++)
	if (_indoms[i])
	    delete _indoms[i];
    if (_context >= 0)
	_source->delContext(_context);
}

PMC_Context::PMC_Context(PMC_Source* source)
: _context(-1), 
  _source(source),
  _names(),
  _pmids(),
  _descs(),
  _indoms(),
  _metrics(),
  _delta(0.0),
  _needReconnect(PMC_false)
{
    if (_source->status() >= 0)
	_context = _source->dupContext();
    else
	_context = _source->status();

    _currTime.tv_sec = 0;
    _currTime.tv_usec = 0;
    _prevTime = _currTime;
}

int
PMC_Context::lookupDesc(const char *name, pmID& id)
{
    int		sts = 0;
    uint_t	i;
    uint_t	len = strlen(name);

    for (i = 0; i < _names.length(); i++) {
	const PMC_NameToId &item = _names[i];
	if (item._name.length() == len && item._name == name) {
	    sts = 1;
	    id = item._id;

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMC)
		cerr << "PMC_Context::lookupDesc: Matched \"" << name
		     << "\" to id " << pmIDStr(id) << endl;
#endif
	    break;
	}
    }

    if (i == _names.length()) {
	sts = pmLookupName(1, (char **)(&name), &id);
	if (sts >= 0) {
	    PMC_NameToId newName;
	    newName._name = name;
	    newName._id = id;
	    _names.append(newName);
	}
    }

    return sts;
}

int
PMC_Context::lookupDesc(const char *name, uint_t& desc, uint_t& indom)
{
    pmID	id;
    int		sts = lookupDesc(name, id);

    if (sts < 0)
	return sts;

    return lookupDesc(id, desc, indom);    
}

int
PMC_Context::lookupDesc(pmID pmid, uint_t& desc, uint_t& indom)
{
    int		sts = 0;
    uint_t	i;
    PMC_Desc*	descPtr;
    PMC_Indom*	indomPtr;

    desc = UINT_MAX;
    indom = UINT_MAX;

    for (i = 0; i < _descs.length(); i++)
	if (_descs[i]->desc().pmid == pmid)
	    break;

    if (i == _descs.length()) {
	descPtr = new PMC_Desc(pmid);
	if (descPtr->status() < 0) {
	    sts = descPtr->status();
	    delete descPtr;
	    return sts;
	    /*NOTREACHED*/
	}

	_descs.append(descPtr);
	desc = _descs.length() - 1;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC) {
	    cerr << "PMC_Context::lookupDesc: Add descriptor for "
		 << pmIDStr(descPtr->id()) << endl;
	}
#endif
    }
    else {
	descPtr = _descs[i];
	desc = i;
	
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PMC) {
	    cerr << "PMC_Context::lookupDesc: Reusing descriptor "
		 << pmIDStr(descPtr->id()) << endl;
	}
#endif
    }
	
    if (descPtr->desc().indom != PM_INDOM_NULL) {

	for (i = 0; i < _indoms.length(); i++)
	    if (_indoms[i]->id() == (int)descPtr->desc().indom)
		break;
	
	if (i == _indoms.length()) {
	    indomPtr = new PMC_Indom(_source->type(), *descPtr);
	    if (indomPtr->status() < 0) {
		sts = indomPtr->status();
		delete indomPtr;
		return sts;
		/*NOTREACHED*/
	    }

	    _indoms.append(indomPtr);
	    indom = _indoms.length() - 1;

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMC) {
		cerr << "PMC_Context::lookupDesc: Add indom for "
		     << pmInDomStr(indomPtr->id()) << endl;
	    }
#endif
	}
	else {
	    
	    indomPtr = _indoms[i];
	    indom = i;
	    
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMC) {
		cerr << "PMC_Context::lookupDesc: Reusing indom "
		     << pmInDomStr(indomPtr->id()) << endl;
	    }
#endif
	}
    }
	
    return sts;
}

ostream&
operator<<(ostream &os, const PMC_Context &cntx)
{
    os << cntx.source().desc() << " has " 
       << cntx._metrics.length() << " metrics";
    return os;
}

int
PMC_Context::useTZ()
{
    if (_source->tzHndl() >= 0)
	return pmUseZone(_source->tzHndl());
    return 0;
}

void
PMC_Context::dump(ostream& os)
{
    os << "Context " << _context << " has " << _names.length()
       << " metric names for source:" << endl;
    _source->dump(os);
}

void
PMC_Context::dumpMetrics(ostream& os)
{
    uint_t	i;
    for (i = 0; i < _metrics.length(); i++)
	os << "        [" << i << "] " 
	   << _metrics[i]->spec(PMC_false, PMC_true) << endl;
}

void
PMC_Context::addMetric(PMC_Metric* metric)
{
    pmID	id;
    uint_t	i;

    _metrics.append(metric);
    if (metric->status() >= 0) {
	id = metric->desc().desc().pmid;
	for (i = 0; i < _pmids.length(); i++)
	    if (_pmids[i] == (int)id)
		break;
	if (i == _pmids.length())
	    _pmids.append(id);
	metric->setIdIndex(i);
    }
}

int
PMC_Context::fetch(PMC_Bool update)
{
    int		sts;
    pmResult*	result;
    uint_t	i;

    for (i = 0; i < _metrics.length(); i++) {
	PMC_Metric* metric = _metrics[i];
	if (metric->status() < 0)
	    continue;
	metric->shiftValues();
    }

    // Inform each indom that we are about to do a new fetch so any
    // indom changes are now irrelevant
    for (i = 0; i < _indoms.length(); i++)
	_indoms[i]->newFetch();

    sts = pmUseContext(_context);

    if (sts >= 0) {
	for (i = 0; i < _indoms.length(); i++) {
	    if (_indoms[i]->diffProfile())
		sts = _indoms[i]->genProfile();
	}
    }
#ifdef PCP_DEBUG
    else if (pmDebug & DBG_TRACE_OPTFETCH)
	cerr << "PMC_Context::fetch: Unable to switch to this context: "
	     << pmErrStr(sts) << endl;
#endif

    if (sts >= 0 && _needReconnect) {
	
	sts = pmReconnectContext(_context);

	if (sts >= 0) {
	    _needReconnect = PMC_false;

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PMC) {
		cerr << "PMC_Context::fetch: Reconnected context \""
		     << *_source << endl;
	    }
#endif
	}
#ifdef PCP_DEBUG
	else if (pmDebug & DBG_TRACE_PMC) {
	    cerr << "PMC_Context::fetch: Reconnect failed: "
		 << pmErrStr(sts) << endl;
	}
#endif
    }

    if (sts >= 0 && _pmids.length()) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_OPTFETCH)
	    cerr << "PMC_Context::fetch: fetching context " << *this
		 << endl;
#endif

	sts = pmFetch(_pmids.length(), 
		      (pmID *)(_pmids.ptr()), &result);

	if (sts >= 0) {

	    _prevTime = _currTime;
	    _currTime = result->timestamp;
	    _delta = __pmtimevalSub(&_currTime, &_prevTime);

	    for (i = 0; i < _metrics.length(); i++) {
		PMC_Metric* metric = _metrics[i];
		if (metric->status() < 0)
		    continue;
		
		assert((int)metric->idIndex() < result->numpmid);
		metric->extractValues(result->vset[metric->idIndex()]);
	    }

	    pmFreeResult(result);
	}
	else {

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_OPTFETCH)
		cerr << "PMC_Context::fetch: pmFetch: " << pmErrStr(sts) 
		     << endl;
#endif

	    for (i = 0; i < _metrics.length(); i++) {
		PMC_Metric* metric = _metrics[i];
		if (metric->status() < 0)
		    continue;
		metric->setError(sts);
	    }

	    if (sts == PM_ERR_IPC || sts == PM_ERR_TIMEOUT)
		_needReconnect = PMC_true;
	}

	if (update) {

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_OPTFETCH)
		cerr << "PMC_Context::fetch: Updating metrics" << endl;
#endif

	    for (i = 0; i < _metrics.length(); i++) {
		PMC_Metric* metric = _metrics[i];
		if (metric->status() < 0)
		    continue;
		metric->update();
	    }
	}
    }
#ifdef PCP_DEBUG
    else if (pmDebug & DBG_TRACE_OPTFETCH)
	cerr << "PMC_Context::fetch: nothing to fetch" << endl;
#endif

    return sts;
}

void
PMC_Context::dometric(char const* name)
{
    theStrList->append(name);
}

int
PMC_Context::traverse(char const* name, PMC_StrList& list)
{
    int	sts;

    theStrList = &list;
    theStrList->removeAll();

    sts = pmTraversePMNS(name, PMC_Context::dometric);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMC) {
	if (sts >= 0) {
	    cerr << "PMC_Context::traverse: Found " << list.length()
		<< " names from " << name << endl;
	}
	else
	    cerr << "PMC_Context::traverse: Failed: " << pmErrStr(sts)
		<< endl;
    }	    
#endif

    return sts;
}
