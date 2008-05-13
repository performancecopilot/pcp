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

#ident "$Id: Metric.c++,v 1.9 2005/05/10 01:59:58 kenmcd Exp $"

#include <pcp/pmc/Metric.h>
#include <pcp/pmc/Group.h>
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif
#include <strings.h>

PMC_MetricValue::PMC_MetricValue()
: _instance(PM_ERR_INST),
  _value(0.0), 
  _prevValue(0.0),
  _currValue(0.0),
  _error(PM_ERR_VALUE),
  _prevError(PM_ERR_VALUE),
  _currError(PM_ERR_VALUE),
  _strValue(1)
{
}

PMC_MetricValue const&
PMC_MetricValue::operator=(PMC_MetricValue const& rhs)
{
    if (this != &rhs) {
	_instance = rhs._instance;
	_value = rhs._value;
	_currValue = rhs._currValue;
	_prevValue = rhs._prevValue;
	_strValue = rhs._strValue;
	_error = rhs._error;
	_currError = rhs._currError;
	_prevError = rhs._prevError;
    }
    return *this;
}

PMC_Metric::~PMC_Metric()
{
    uint_t	i;

    if (hasInstances())
	for (i = 0; i < _values.length(); i++)
	    indomRef()->removeRef(_values[i]._instance);
}

PMC_Metric::PMC_Metric(PMC_Group *group, 
		       const char *str, 
		       double theScale,
		       PMC_Bool active)
: _sts(0),
  _name(),
  _group(group),
  _values(1),
  _scale(theScale),
  _contextIndex(UINT_MAX),
  _idIndex(UINT_MAX),
  _descIndex(UINT_MAX),
  _indomIndex(UINT_MAX),
  _explicit(PMC_false),
  _active(active)
{
    pmMetricSpec	*theMetric;
    char		*msg;

    _sts = pmParseMetricSpec((char *)str, 0, (char *)0, &theMetric, &msg);
    if (_sts < 0) {
	pmprintf("%s: Error: Unable to parse metric spec:\n%s\n", 
		 pmProgname, msg);
	free(msg);
	return;
	/*NOTREACHED*/
    }

    if (_sts >= 0) {
	_name = theMetric->metric;
	setup(group, theMetric);
    }

    free(theMetric);
}

PMC_Metric::PMC_Metric(PMC_Group *group, pmMetricSpec *theMetric, 
		       double theScale, PMC_Bool active)
: _sts(0), 
  _name(theMetric->metric),
  _group(group),
  _values(1),
  _scale(theScale),
  _contextIndex(UINT_MAX),
  _idIndex(0),
  _descIndex(UINT_MAX),
  _indomIndex(UINT_MAX),
  _explicit(PMC_false),
  _active(active)
{
    setup(group, theMetric);
}

void
PMC_Metric::setup(PMC_Group* group, pmMetricSpec *theMetric)
{
    if (_sts >= 0)
	setupDesc(group, theMetric);
    if (_sts >= 0)
	setupIndom(theMetric);
    if (_sts < 0) {
	return;
	/*NOTREACHED*/
    }

 #ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMC)
	dumpAll();
#endif
}

void
PMC_Metric::setupDesc(PMC_Group* group, pmMetricSpec *theMetric)
{
    int		contextType = PM_CONTEXT_HOST;
    int		descType;

    if (theMetric->isarch == 1)
	contextType = PM_CONTEXT_ARCHIVE;
    else if (theMetric->isarch == 2)
	contextType = PM_CONTEXT_LOCAL;

    _sts = group->use(contextType, theMetric->source);
    _contextIndex = group->whichIndex();

    if (_sts >= 0) {
	contextType = context().source().type();

	_sts = contextRef().lookupDesc(theMetric->metric, _descIndex, 
				       _indomIndex);

	if (_sts < 0)
	    pmprintf("%s: Error: %s%c%s: %s\n", 
		     pmProgname,
		     contextType == PM_CONTEXT_LOCAL ? "@" : context().source().source().ptr(),
		     (contextType == PM_CONTEXT_ARCHIVE ? '/' : ':'),
		     _name.ptr(), pmErrStr(_sts));
    }
    else 
	pmprintf("%s: Error: %s: %s\n", pmProgname,
		 context().source().desc().ptr(), pmErrStr(_sts));

    if (_sts >= 0) {
	descType = desc().desc().type;
	if (descType == PM_TYPE_NOSUPPORT) {
	    _sts = PM_ERR_CONV;
	    pmprintf("%s: Error: %s%c%s is not supported on %s\n",
		     pmProgname,
		     contextType == PM_CONTEXT_LOCAL ? "@" : context().source().source().ptr(),
		     (contextType == PM_CONTEXT_ARCHIVE ? '/' : ':'),
		     _name.ptr(), context().source().host().ptr());
	}
	else if (descType == PM_TYPE_AGGREGATE ||
		 descType == PM_TYPE_AGGREGATE_STATIC ||
		 descType == PM_TYPE_UNKNOWN) {

	    _sts = PM_ERR_CONV;
	    pmprintf("%s: Error: %s%c%s has type \"%s\", which is not a number or a string\n",
		     pmProgname,
		     contextType == PM_CONTEXT_LOCAL ? "@" : context().source().source().ptr(),
		     (contextType == PM_CONTEXT_ARCHIVE ? '/' : ':'),
		     _name.ptr(), pmTypeStr(descType));
	}
    }
}

void
PMC_Metric::setupIndom(pmMetricSpec *theMetric)
{
    uint_t	i;
    int		j;
    PMC_Indom*	indomPtr = indomRef();
    
    if (desc().desc().indom == PM_INDOM_NULL) {
	if (theMetric->ninst > 0) {
	    _sts = PM_ERR_INST;
	    dumpErr(theMetric->inst[0]);
	}
	else
	    setupValues(1);
    }
    else if (theMetric->ninst) {
	assert(hasInstances());
	setupValues(theMetric->ninst);
	
	for (i = 0 ; i < (uint_t)theMetric->ninst && _sts >= 0; i++) {
	    j = indomPtr->lookup(theMetric->inst[i]);
	    if (j >= 0)
		_values[i]._instance = j;
	    else {
		_sts = PM_ERR_INST;
		_values[i]._instance = PM_ERR_INST;
		dumpErr(theMetric->inst[i]);
	    }
	}
	_explicit = PMC_true;
    }
    else {
	assert(hasInstances());

	if (_active) {
	    setupValues(indomPtr->numActiveInsts());
	    indomPtr->refAll(_active);

	    for (i = 0, j = 0; i < indomPtr->listLen(); i++)
		if (!indomPtr->nullInst(i) && indomPtr->activeInst(i))
		    _values[j++]._instance = i;
	}
	else {
	    setupValues(indomPtr->numInsts());
	    indomPtr->refAll(_active);
	    
	    for (i = 0, j = 0; i < indomPtr->listLen(); i++)
		if (!indomPtr->nullInst(i))
		    _values[j++]._instance = i;
	    
	}
    }
}

void
PMC_Metric::setupValues(uint_t num)
{
    uint_t	i;
    uint_t	oldLen = _values.length();

    if (num == 0) {
	_values.resize(1);
	_values.removeAll();
    }
    else {
	if (_values.size() < num)
	    _values.resize(num);
	if (_values.length() > num)
	    _values.remove(num, _values.length() - num);
	for (i = oldLen; i < num; i++)
	    _values.append(PMC_MetricValue());
    }
}

PMC_String
PMC_Metric::spec(PMC_Bool srcFlag, PMC_Bool instFlag, uint_t instance) const
{
    PMC_String	str;
    uint_t	len = 4;
    uint_t	i;

    if (srcFlag)
	len += context().source().source().length();
    len += name().length();
    if (hasInstances() && instFlag) {
	if (instance != UINT_MAX)
	    len += instName(instance).length() + 2;
	else {
	    for (i = 0; i < numInst(); i++)
		len += instName(i).length() + 4;
	}
    }
    str.resize(len);

    if (srcFlag) {
	str.append(context().source().source());
	if (context().source().type() == PM_CONTEXT_ARCHIVE)
	    str.appendChar('/');
	else
	    str.appendChar(':');
    }
    str.append(name());
    if (hasInstances() && instFlag) {
	str.appendChar('[');
	str.appendChar('\"');
	if (instance != UINT_MAX)
	    str.append(instName(instance));
	else if (numInst()) {
	    str.append(instName(0));
	    for (i = 1; i < numInst(); i++) {
		str.append("\", \"");
		str.append(instName(i));
	    }
	}
	str.append("\"]");
    }

    return str;
}

void
PMC_Metric::dumpSource(ostream &os) const
{
    switch(context().source().type()) {
    case PM_CONTEXT_LOCAL:
	os << "@:";
	break;
    case PM_CONTEXT_HOST:
	os << context().source().source() << ':';
	break;
    case PM_CONTEXT_ARCHIVE:
	os << context().source().source() << '/';
	break;
    }
}

void
PMC_Metric::dumpValue(ostream &os, uint_t inst) const
{
    if (error(inst) < 0)
	os << pmErrStr(error(inst));
    else if (!real())
	os << strValue(inst);
    else
	os << value(inst) << " " << desc().units();
}

void
PMC_Metric::dump(ostream &os, PMC_Bool srcFlag, uint_t instance) const
{
    uint_t	i;

    if (srcFlag == PMC_true)
	dumpSource(os);

    os << name();

    if (_sts < 0)
	os << ": " << pmErrStr(_sts) << endl;
    else if (hasInstances()) {
	if (instance == UINT_MAX) {
	    if (numInst() == 1)
		os << ": 1 instance";
	    else
		os << ": " << numInst() << " instances";

	    if (indom()->changed())
		os << " (indom has changed)";

	    os << endl;

	    for (i = 0; i < numInst(); i++) {
		os << "  [" << instID(i) << " or \"" << instName(i) << "\" ("
		   << _values[i]._instance << ")] = ";
		dumpValue(os, i);
		os << endl;
	    }
	}
	else {
	    os << '[' << instID(instance) << " or \"" << instName(instance) 
	       << "\" (" << _values[instance]._instance << ")] = ";
	    dumpValue(os, instance);
	    os << endl;
	}
    }
    else {
	os << " = ";
	dumpValue(os, 0);
	os << endl;
    }
}

ostream&
operator<<(ostream &os, const PMC_Metric &mtrc)
{
    uint_t	num = mtrc.numValues();
    uint_t	i;

    mtrc.dumpSource(os);

    os << mtrc.name();
    if (mtrc.numInst()) {
	os << "[\"" << mtrc.instName(0);
	for (i = 1; i < num; i++)
	    os << "\", \"" << mtrc.instName(i);
	os << "\"]";
    }

    return os;
}

int
PMC_Metric::update()
{
    uint_t	err = 0;
    uint_t	num = numValues();
    uint_t	i;
    int		sts;
    pmAtomValue	ival;
    pmAtomValue oval;
    static int	wrap = -1;
    double	delta = context().timeDelta();

    if (num == 0 || _sts < 0)
	return _sts;

    if (wrap == -1) {
	// PCP_COUNTER_WRAP in environment enables "counter wrap" logic
	       if (getenv("PCP_COUNTER_WRAP") == NULL)
            wrap = 0;
        else
	    wrap = 1;
    }

    for (i = 0; i < num; i++) {
	_values[i]._error = _values[i]._currError;
	if (_values[i]._error < 0)
	    err++;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_VALUE)
	    if (_values[i]._error < 0)
		cerr << "PMC_Metric::update: " << spec(PMC_true, PMC_true, i) 
		     << ": " << pmErrStr(_values[i]._error) << endl;
#endif
    }
    
    if (!real())
	return err;

    if (desc().desc().sem == PM_SEM_COUNTER) {

	for (i = 0; i < num; i++) {

	    PMC_MetricValue& value = _values[i];

	    if (value._error < 0) {		// we already know we
		value._value = 0.0;		// don't have this value
		continue;
		/*NOTREACHED*/
	    }

	    if (value._prevError < 0) {		// we need two values
		value._value = 0.0;		// for a rate
		value._error = value._prevError;
		err++;

#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_VALUE) {
		    cerr << "PMC_Metric::update: Previous: " 
			 << spec(PMC_true, PMC_true, i) << ": "
			 << pmErrStr(value._error) << endl;
		}
#endif

		continue;
		/*NOTREACHED*/
	    }

	    value._value = value._currValue - value._prevValue;

	    // wrapped going forwards
	    if (value._value < 0 && delta > 0) {
		if (wrap) {
		    switch(desc().desc().type) {
		    case PM_TYPE_32:
		    case PM_TYPE_U32:
			value._value += (double)UINT_MAX+1;
			break;
		    case PM_TYPE_64:
		    case PM_TYPE_U64:
			value._value += (double)ULONGLONG_MAX+1;
			break;
		    }
		}
		else {			// counter not montonic
		    value._value = 0.0;	// increasing
		    value._error = PM_ERR_VALUE;
		    err++;
		    continue;
		    /*NOTREACHED*/
		}
	    }
	    // wrapped going backwards
	    else if (value._value > 0 && delta < 0) {
		if (wrap) {
		    switch(desc().desc().type) {
		    case PM_TYPE_32:
		    case PM_TYPE_U32:
			value._value -= (double)UINT_MAX+1;
			break;
		    case PM_TYPE_64:
		    case PM_TYPE_U64:
			value._value -= (double)ULONGLONG_MAX+1;
			break;
		    }
		}
		else {			// counter not montonic
		    value._value = 0.0;	// increasing
		    value._error = PM_ERR_VALUE;
		    err++;
		    continue;
		    /*NOTREACHED*/
		}
	    }

	    if (delta != 0)			// sign of delta and v
		value._value /= delta;		// should be the same
	    else
		value._value = 0;		// nothing can have happened
	}	    
    }
    else {
	for (i = 0; i < num; i++) {
	    PMC_MetricValue& value = _values[i];
	    if (value._error < 0)
		value._value = 0.0;
	    else
		value._value = value._currValue;
	}
    }

    if (_scale != 0.0) {
	for (i = 0; i < num; i++) {
	    if (_values[i]._error >= 0)
		_values[i]._value /= _scale;
	}
    }

    if (desc().useScaleUnits()) {
	for (i = 0; i < num; i++) {
	    if (_values[i]._error < 0)
		continue;

	    ival.d = _values[i]._value;
	    pmUnits units = desc().desc().units;
	    sts = pmConvScale(PM_TYPE_DOUBLE, &ival, &units,
			      &oval, (pmUnits *)&(desc().scaleUnits()));
	    if (sts < 0)
		_values[i]._error = sts;
	    else {
		_values[i]._value = oval.d;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_VALUE)
		    cerr << "PMC_Metric::update: scaled " << _name
			 << " from " << ival.d << " to " << oval.d
			 << endl;
#endif
	    }
	}
    }

    return err;
}

void
PMC_Metric::dumpAll() const
{
    cerr << *this << " from " << context().source().desc() 
	 << " with scale = "  << _scale << " and units = " << desc().units() 
	 << endl;
}

void
PMC_Metric::dumpErr() const
{
    pmprintf("%s: Error: %s: %s\n", 
	     pmProgname, spec(PMC_true).ptr(), pmErrStr(_sts));
}

// Instance list may not be valid, so pass inst as a string rather than
// as an index

void
PMC_Metric::dumpErr(const char *inst) const
{
    pmprintf("%s: Error: %s[%s]: %s\n", 
	     pmProgname, spec(PMC_true).ptr(), inst, pmErrStr(_sts));
}

const char *
PMC_Metric::formatNumber(double value)
{
    static char buf[8];

    if (value >= 0.0) {
	if (value > 99950000000000.0)
	    strcpy(buf, "  inf?");
	else if (value > 99950000000.0)
	    sprintf(buf, "%5.2fT", value / 1000000000000.0);
	else if (value > 99950000.0)
	    sprintf(buf, "%5.2fG", value / 1000000000.0);
	else if (value > 99950.0)
	    sprintf(buf, "%5.2fM", value / 1000000.0);
	else if (value > 99.95)
	    sprintf(buf, "%5.2fK", value / 1000.0);
	else if (value > 0.005)
	    sprintf(buf, "%5.2f ", value);
	else
	    strcpy(buf, " 0.00 ");
    }
    else {
	if (value < -9995000000000.0)
	    strcpy(buf, " -inf?");
	else if (value < -9995000000.0)
	    sprintf(buf, "%.2fT", value / 1000000000000.0);
	else if (value < -9995000.0)
	    sprintf(buf, "%.2fG", value / 1000000000.0);
	else if (value < -9995.0)
	    sprintf(buf, "%.2fM", value / 1000000.0);
	else if (value < -9.995)
	    sprintf(buf, "%.2fK", value / 1000.0);
	else if (value < -0.005)
	    sprintf(buf, "%.2f ", value);
	else
	    strcpy(buf, " 0.00  ");
    }

    return buf;
}

void
PMC_Metric::shiftValues()
{
    uint_t	i;

    for (i = 0; i < _values.length(); i++) {
	PMC_MetricValue& value = _values[i];
	value._prevValue = value._currValue;
	value._prevError = value._currError;
	value._currError = 0;
    }
}

void
PMC_Metric::setError(int sts)
{
    uint_t	i;

    for (i = 0; i < numValues(); i++) {
	PMC_MetricValue& value = _values[i];
	value._currError = sts;
	if (real())
	    value._currValue = 0.0;
	else
	    value._strValue = "";
    }
}

void
PMC_Metric::extractValues(pmValueSet const* set)
{
    uint_t		i;
    uint_t		j;
    int			index;
    int			inst;
    int			sts;
    pmValue const*	value;
    pmAtomValue		result;
    PMC_Bool		found;
    PMC_Indom*		indomPtr = indomRef();

    assert(set->pmid == desc().id());

    if (set->numval > 0) {

	if (hasInstances()) {

	    // If the number of instances are not the expected number
	    // then mark the indom as changed
	    if (!_explicit && ((int)_values.length() != set->numval)) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_INDOM)
		    cerr << "PMC_Metric::extractValues: implicit indom "
			 << pmInDomStr(indomPtr->id()) << " changed ("
			 << set->numval << " != " << _values.length() << ')'
			 << endl;
#endif
		indomPtr->hasChanged();
	    }

	    for (i = 0; i < numInst(); i++) {
		PMC_MetricValue& valueRef = _values[i];
		inst = _values[i]._instance;
		index = indomPtr->index(inst);
		found = PMC_false;

		// If the index is within range, try it first
		if (index >= 0 && index < set->numval) {
		    value = &(set->vlist[index]);

		    // Found it in the same spot as last time
		    if (value->inst == indomPtr->inst(inst))
			found = PMC_true;
		}

		// Search for it from the top
		for (j = 0; found == PMC_false && j < (uint_t)set->numval; j++) {
		    if (set->vlist[j].inst == indomPtr->inst(inst)) {
			value = &(set->vlist[j]);
			indomPtr->setIndex(inst, j);
			found = PMC_true;
		    }
		}
		
		if (found) {
		    if (!real()) {
			sts = pmExtractValue(set->valfmt, value,
					     desc().desc().type, &result,
					     PM_TYPE_STRING);
			if (sts >= 0) {
			    valueRef._strValue = result.cp;
			    if (result.cp)
				free(result.cp);
			}
			else {
			    valueRef._currError = sts;
			    valueRef._strValue = "";
			}
		    }
		    else {
			sts = pmExtractValue(set->valfmt, value,
					     desc().desc().type, &result,
					     PM_TYPE_DOUBLE);
			if (sts >= 0)
			    valueRef._currValue = result.d;
			else {
			    valueRef._currError = sts;
			    valueRef._currValue = 0.0;
			}
		    }
		}

		// Cannot find it
		else {

#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_OPTFETCH)
			cerr << "PMC_Metric::extractValues: "
			     << spec(PMC_true, PMC_true, i) << ": "
			     << pmErrStr(PM_ERR_VALUE) << endl;
#endif

		    if (valueRef._prevError != PM_ERR_VALUE)
			indomPtr->hasChanged();

		    valueRef._currError = PM_ERR_VALUE;
		    if (real())
			valueRef._currValue = 0.0;
		    else
			valueRef._strValue = ""; 
		}
	    }
	}
	else if (set->numval == 1) {

	    // We have no instances at this point in time
	    if (_values.length() == 0 && hasInstances())
		indomPtr->hasChanged();
	    else {
		PMC_MetricValue& valueRef = _values[0];
		value = &(set->vlist[0]);
		if (!real()) {
		    sts = pmExtractValue(set->valfmt, value,
					 desc().desc().type, &result,
					 PM_TYPE_STRING);
		    if (sts >= 0) {
			valueRef._strValue = result.cp;
			if (result.cp)
			    free(result.cp);
		    }
		    else {
			valueRef._currError = sts;
			valueRef._strValue = "";
		    }
		}
		else {
		    sts = pmExtractValue(set->valfmt, value,
					 desc().desc().type, &result,
					 PM_TYPE_DOUBLE);
		    if (sts >= 0)
			valueRef._currValue = result.d;
		    else {
			valueRef._currError = sts;
			valueRef._currValue = 0.0;
		    }
		}
	    }
	}

	// Did not expect any instances
	else {

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_OPTFETCH)
		cerr << "PMC_Metric::extractValues: " << spec(PMC_true) 
		     << " is a singular metric but result contained "
		     << set->numval << " values" << endl;
#endif

	    setError(PM_ERR_VALUE);
	}
    }
    else if (set->numval == 0) {

	if (!(hasInstances() && numInst() == 0)) {

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_OPTFETCH)
		cerr << "PMC_Metric::extractValues: numval == 0: " << spec(PMC_true, PMC_false)
		     << ": " << pmErrStr(PM_ERR_VALUE) << endl;
#endif

	    setError(PM_ERR_VALUE);
	    if (hasInstances())
		indomPtr->hasChanged();
	}
    }
    else {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_OPTFETCH)
	    cerr << "PMC_Metric::extractValues: numval < 0: " << spec(PMC_true, PMC_false)
		 << ": " << pmErrStr(set->numval) << endl;
#endif

	setError(set->numval);
	if (hasInstances())
	    indomPtr->hasChanged();
    }
}

PMC_Bool
PMC_Metric::updateIndom(void)
{
    uint_t		i;
    uint_t		j;
    uint_t		oldNum = numInst();
    uint_t		newNum;
    int			newInst;
    PMC_Indom*		indomPtr = indomRef();

    if (status() < 0 || !hasInstances())
	return PMC_false;

    if (indomPtr->changed())
	indomPtr->update();

    _explicit = PMC_false;

    if (_active)
	newNum = indomPtr->numActiveInsts();
    else
	newNum = indomPtr->numInsts();

    // If the number of instances are the same then we know that no
    // modifications to the metric instance list is required as these
    // instances are all referenced in the indom
    //
    // If the instance list is only active instances, then we need to
    // check all the instances as the number may be the same
    //
    if (newNum == oldNum) {
	if (_active) {
	    for (i = 0; i < _values.length(); i++)
		if (!indomPtr->activeInst(_values[i]._instance))
		    break;
	}

	if (!_active || i == _values.length()) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_INDOM)
		cerr << "PMC_Metric::updateIndom: No change required" << endl;
#endif
	    return PMC_false;
	    /*NOTREACHED*/
	}
    }

    // Duplicate the current values
    // Replace the old index into the indom instance list with the
    // internal instance identifiers so that these can be correlated
    // if the order of instances changes
    PMC_ValueList oldValues = _values;
    for (i = 0; i < oldNum; i++) {
	oldValues[i]._instance = indomPtr->inst(_values[i]._instance);
	indomPtr->removeRef(_values[i]._instance);
    }

    setupValues(newNum);
    indomPtr->refAll(_active);

    if (_active) {
	for (i = 0, j = 0; i < indomPtr->listLen(); i++)
	    if (!indomPtr->nullInst(i) && indomPtr->activeInst(i))
		_values[j++]._instance = i;
    }
    else {
	for (i = 0, j = 0; i < indomPtr->listLen(); i++)
	    if (!indomPtr->nullInst(i))
		_values[j++]._instance = i;
    }

    // Copy values of instances that have not gone away
    // Note that their position may have changed
    for (i = 0; i < _values.length(); i++) {
	if (i < oldValues.length() &&
	    indomPtr->inst(_values[i]._instance) == oldValues[i]._instance) {
	    newInst = _values[i]._instance;
	    _values[i] = oldValues[i];
	    _values[i]._instance = newInst;
	    continue;
	}
	for (j = 0; j < oldValues.length(); j++)
	    if (indomPtr->inst(_values[i]._instance) == oldValues[j]._instance) {
		newInst = _values[i]._instance;
		_values[i] = oldValues[j];
		_values[i]._instance = newInst;
		break;
	    }

	// Need to set all error flags to avoid problems with rate conversion
	if (j == oldValues.length()) {
	    _values[i]._error = PM_ERR_VALUE;
	    _values[i]._currError = PM_ERR_VALUE;
	    _values[i]._prevError = PM_ERR_VALUE;
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PMC)
	cerr << "PMC_Metric::updateIndom: " << spec(PMC_true) << ": Had " 
	     << oldNum << " instances, now have " << numInst() << endl;
#endif

    indomPtr->update();

    return PMC_true;
}

int
PMC_Metric::addInst(PMC_String const& name)
{
    int	i;

    if (_sts < 0)
	return _sts;

    if (!hasInstances())
	return PM_ERR_INDOM;

    i = indomRef()->lookup(name);
    if (i >= 0) {
	setupValues(_values.length() + 1);
	_values.tail()._instance = i;
    }

    return i;
}

void
PMC_Metric::removeInst(uint_t index)
{
    assert(hasInstances());
    indomRef()->removeRef(_values[index]._instance);
    _values.remove(index);
}

