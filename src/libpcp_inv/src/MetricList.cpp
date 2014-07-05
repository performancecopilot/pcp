/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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
 */


#include "MetricList.h"

static OMC_Bool		doMetricFlag = OMC_true;
static INV_MetricList	*currentList = NULL;

INV_MetricList::~INV_MetricList()
{
    uint_t	i;

    for (i = 0; i < _metrics.length(); i++)
	delete _metrics[i];
    for (i = 0; i < _colors.length(); i++)
	delete _colors[i];
}

INV_MetricList::INV_MetricList()
: _metrics(), _colors(), _values(0)
{
}

void
INV_MetricList::toString(const SbColor &color, OMC_String &str)
{
    char buf[48];

    const float *values = color.getValue();
    sprintf(buf, "rgbi:%f/%f/%f", values[0], values[1], values[2]);
    str = buf;
}

int
INV_MetricList::add(char const* metric, double scale)
{
    OMC_Metric *ptr = new OMC_Metric(metric, scale);

    if (ptr->status() >= 0) {
	_metrics.append(ptr);
	_values += ptr->numValues();
    }

    return ptr->status();
}

int
INV_MetricList::add(char const* metric, double scale, int history)
{
    OMC_Metric *ptr = new OMC_Metric(metric, scale, history);

    if (ptr->status() >= 0) {
	_metrics.append(ptr);
	_values += ptr->numValues();
    }

    return ptr->status();
}

int
INV_MetricList::add(pmMetricSpec *metric, double scale)
{
    OMC_Metric *ptr = new OMC_Metric(metric, scale);

    if (ptr->status() >= 0) {
	_metrics.append(ptr);
	_values += ptr->numValues();
    }

    return ptr->status();
}

int
INV_MetricList::add(pmMetricSpec *metric, double scale, int history)
{
    OMC_Metric *ptr = new OMC_Metric(metric, scale, history);

    if (ptr->status() >= 0) {
	_metrics.append(ptr);
	_values += ptr->numValues();
    }

    return ptr->status();
}

void
INV_MetricList::add(SbColor const& color)
{
    SbColor *ptr = new SbColor;

    ptr->setValue(color.getValue());
    _colors.append(ptr);
}

void
INV_MetricList::add(uint32_t packedcol)
{
    float	tran = 0.0;
    SbColor	*ptr = new SbColor;

    ptr->setPackedValue(packedcol, tran);
    _colors.append(ptr);
}

void
INV_MetricList::resolveColors(AlignColor align)
{
    uint_t	need = 0;

    switch(align) {
    case perMetric:
	need = _metrics.length();
	break;
    case perValue:
	need = _values;
	break;
    case noColors:
    default:
	need = 0;
	break;
    }

    if (_metrics.length() == 0)
	return;

    if (_colors.length() == 0 && need > 0)
	add(SbColor(0.0, 0.0, 1.0));

    if (_colors.length() < need) {
	uint_t	o = 0;
	uint_t	n = 0;

	while (n < need) {
	    for (o = 0; o < _colors.length() && n < need; o++, n++) {
		SbColor *ptr = new SbColor;
                ptr->setValue(((SbColor *)_colors[o])->getValue());
                _colors.append(ptr);
	    }
	}
    }
}

ostream&
operator<<(ostream &os, INV_MetricList const &list)
{
    uint_t	i;
    float	r, g, b;

    for (i = 0; i < list._metrics.length(); i++) {
	os << '[' << i << "]: ";
	if (i < list._colors.length()) {
	    list._colors[i]->getValue(r, g, b);
	    os << r << ',' << g << ',' << b << ": ";
	}	    
	os << *(list._metrics[i]) << endl;
    }
    return os;
}

static void
dometric(const char *name)
{
    if (currentList->add(name, 0.0) < 0)
	doMetricFlag = OMC_false;
}

int
INV_MetricList::traverse(const char *str)
{
    pmMetricSpec	*theMetric;
    char		*msg;
    int			type = PM_CONTEXT_HOST;
    int			sts = 0;

    sts = pmParseMetricSpec((char *)str, 0, (char *)0, &theMetric, &msg);
    if (sts < 0) {
	pmprintf("%s: Error: Unable to parse metric spec:\n%s\n", 
		 pmProgname, msg);
	free(msg);
	return sts;
	/*NOTREACHED*/
    }

    // If the metric has instances, then it cannot be traversed
    if (theMetric->ninst) {
	sts = add(theMetric, 0.0);
    }
    else {
	if (theMetric->source && strlen(theMetric->source) > 0) {
	    if (theMetric->isarch)
		type = PM_CONTEXT_ARCHIVE;
	}
	
	sts = theSource.use(type, theMetric->source);
	
	currentList = this;

	if (sts >= 0) {
	    sts = pmTraversePMNS(theMetric->metric, dometric);
	    if (sts >= 0 && doMetricFlag == OMC_false)
		sts = -1;
	    else if (sts < 0)
		pmprintf("%s: Error: %s%c%s: %s\n",
			 pmProgname, 
			 theSource.which()->source().ptr(),
			 (theSource.which()->type() == PM_CONTEXT_ARCHIVE ?
			  '/' : ':'),
			 theMetric->metric,
			 pmErrStr(sts));
	}
    }

    free(theMetric);

    return sts;
}
