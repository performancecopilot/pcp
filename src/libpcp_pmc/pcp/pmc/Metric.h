/* -*- C++ -*- 
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

#ifndef _PMC_METRIC_H_
#define _PMC_METRIC_H_

#ident "$Id: Metric.h,v 1.11 2005/05/10 00:46:37 kenmcd Exp $"

#include <pcp/pmc/PMC.h>
#include <pcp/pmc/Vector.h>
#include <pcp/pmc/String.h>
#include <pcp/pmc/Bool.h>
#include <pcp/pmc/Context.h>
#include <pcp/pmc/Desc.h>
#include <pcp/pmc/Group.h>

struct PMC_MetricValue
{
    int		_instance;
    double	_value;
    double	_prevValue;
    double	_currValue;
    int		_error;
    int		_prevError;
    int		_currError;
    PMC_String	_strValue;

    ~PMC_MetricValue()
	{}
    PMC_MetricValue();
    PMC_MetricValue const& operator=(PMC_MetricValue const& rhs);

    PMC_MetricValue(PMC_MetricValue const&); // Never defined
};

typedef PMC_List<struct PMC_MetricValue> PMC_ValueList;

class PMC_Metric
{

friend class PMC_Group;
friend class PMC_Context;

private:

    int			_sts;
    PMC_String		_name;
    PMC_Group*		_group;
    PMC_ValueList	_values;
    double		_scale;

    uint_t		_contextIndex;	// Index into the context list for the
    					// group
    uint_t		_idIndex;	// Index into the pmid list for the
					// context.
    uint_t		_descIndex;	// Index into the desc list for the
					// context.
    uint_t		_indomIndex;	// Index into the indom list for the
					// context.

    PMC_Bool		_explicit;	// Instances explicitly specified
    PMC_Bool		_active;	// Use only active implicit insts

public:

    ~PMC_Metric();

    // Error code, < 0 is an error
    int status() const
	{ return _sts; }

    // Name of the metric
    const PMC_String &name() const
	{ return _name; }

    // Context for this metric
    PMC_Context const& context() const
	{ return _group->context(_contextIndex); }

    // Metric descriptor
    const PMC_Desc &desc() const
	{ return _group->context(_contextIndex).desc(_descIndex); }

    // Does the metric have instances
    PMC_Bool hasInstances() const
	{ return (_sts >= 0 && _indomIndex < UINT_MAX ? PMC_true : PMC_false); }

    // Were the instances explicitly listed?
    PMC_Bool explicitInsts() const
	{ return _explicit; }

    // Are only active instances referenced
    PMC_Bool activeInsts() const
	{ return _active; }

    // How many instances does it have
    uint_t numInst() const
	{ return (_sts >= 0 && _indomIndex < UINT_MAX ? _values.length() : 0); }

    // How many values does it have (will not equal number of instances
    // if singular)
    uint_t numValues() const
	{ return (_sts >= 0 ? _values.length() : 0); }

    // The metric indom
    PMC_Indom const* indom() const
	{ return (_indomIndex < UINT_MAX ? 
		  &(_group->context(_contextIndex).indom(_indomIndex)) :
		  NULL); }

    // Internal instance id for instance <index>
    int instID(int index) const
	{ return _group->context(_contextIndex).indom(_indomIndex).inst(_values[index]._instance); }

    // External instance name for instance <index>
    const PMC_String &instName(int index) const
	{ return _group->context(_contextIndex).indom(_indomIndex).name(_values[index]._instance); }

    // Return the index for the instance in the indom list
    int instIndex(uint_t index) const
	{ return _values[index]._instance; }

    // Update the metric to include new instances
    // Returns PMC_true if the instance list changed
    // Metrics with implicit instances will be extended to include those
    // new instances. The position of instances may change.
    // If <active> is set, only those instances in the latest indom will
    // be listed, other instances will be removed
    PMC_Bool updateIndom();

    // Add an instance to this metric
    int addInst(PMC_String const& name);

    // Remove an instance from this metric
    void removeInst(uint_t index);

    // Scaling modifier applied to metric values
    double scale() const
	{ return _scale; }

    // Metric has real values (as opposed to string values)
    PMC_Bool real() const
	{ return (desc().desc().type == PM_TYPE_STRING ? PMC_false : PMC_true);}

    // Current rate-converted and scaled real value
    double value(int index) const
	{ return _values[index]._value; }

    // Current rate-converted real value
    double realValue(int index) const
	{ return _values[index]._value * _scale; }

    // Current raw value
    double currValue(int index) const
	{ return _values[index]._currValue; }

    // Current string value
    PMC_String const& strValue(int index) const
	{ return _values[index]._strValue; }

    // Current error code (after rate-conversion)
    int error(int index) const
	{ return _values[index]._error; }

    // Current raw error code
    int currError(int index) const
	{ return _values[index]._currError; }

    // Shift values in preparation for next fetch
    void shiftValues();

    // Set error code for all instances
    void setError(int sts);

    // Extract metrics values after a fetch
    void extractValues(pmValueSet const* set);

    // Index for context in group list
    uint_t contextIndex() const
	{ return _contextIndex; }

    // Index for desc in context list
    uint_t descIndex() const
	{ return _descIndex; }

    // Index for metric into pmResult
    uint_t idIndex() const
	{ return _idIndex; }

    // Index for indom in context list
    uint_t indomIndex() const
	{ return _indomIndex; }

    // Set the canonical units
    void setScaleUnits(pmUnits const& units)
	{ descRef().setScaleUnits(units); }

    // Generate a metric spec
    PMC_String spec(PMC_Bool srcFlag = PMC_false,
		    PMC_Bool instFlag = PMC_false,
		    uint_t instance = UINT_MAX) const;

    // Dump out the metric and its current value(s)
    void dump(ostream &os, 
	      PMC_Bool srcFlag = PMC_false,
	      uint_t instance = UINT_MAX) const;

    // Dump out the current value
    void dumpValue(ostream &os, uint_t instance) const;

    // Dump out the metric source
    void dumpSource(ostream &os) const;

    // Format a value into a fixed width format
    static const char *formatNumber(double value);

    // Determine the current errors and rate-converted scaled values
    int update();

    friend ostream &operator<<(ostream &os, const PMC_Metric &metric);

private:

    // Create new metrics 
    PMC_Metric(PMC_Group *group, const char *str, double theScale = 0.0,
	       PMC_Bool active = PMC_false);
    PMC_Metric(PMC_Group *group, pmMetricSpec *theMetric, double theScale = 0.0,
	       PMC_Bool active = PMC_false);
    void setup(PMC_Group *group, pmMetricSpec *theMetric);
    void setupDesc(PMC_Group *group, pmMetricSpec *theMetric);
    void setupIndom(pmMetricSpec *theMetric);
    void setupValues(uint_t num);

    void setIdIndex(uint_t index)
	{ _idIndex = index; }

    PMC_Context& contextRef()
	{ return _group->context(_contextIndex); }

    PMC_Desc& descRef()
	{ return _group->context(_contextIndex).desc(_descIndex); }


    PMC_Indom* indomRef()
	{ return (_indomIndex < UINT_MAX ? 
		  (PMC_Indom *)&(_group->context(_contextIndex).indom(_indomIndex)) :
		  NULL); }

    // Dump error messages
    void dumpAll() const;
    void dumpErr() const;
    void dumpErr(const char *inst) const;

    // Not defined
    PMC_Metric();
    PMC_Metric(const PMC_Metric &);
    const PMC_Metric &operator=(const PMC_Metric &);
};

#endif /* _PMC_METRIC_H_ */
