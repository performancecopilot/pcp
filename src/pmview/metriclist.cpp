/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#include "metriclist.h"
#include "scenegroup.h"
#include "main.h"

static bool		doMetricFlag = true;
static MetricList	*currentList;

MetricList::~MetricList()
{
    int		i;

    for (i = 0; i < _metrics.size(); i++)
	delete _metrics[i];
    for (i = 0; i < _colors.size(); i++)
	delete _colors[i];
}

MetricList::MetricList()
: _metrics(), _colors(), _values(0)
{
}

void
MetricList::toString(const SbColor &color, QString &str)
{
    char buf[48];

    const float *values = color.getValue();
    pmsprintf(buf, sizeof(buf), "rgbi:%f/%f/%f", values[0], values[1], values[2]);
    str = buf;
}

int
MetricList::add(char const* metric, double scale)
{
    QmcMetric *ptr = new QmcMetric(activeGroup, metric, scale);

    if (ptr->status() >= 0) {
	_metrics.append(ptr);
	_values += ptr->numValues();
    }

    return ptr->status();
}

int
MetricList::add(char const* metric, double scale, int history)
{
    QmcMetric *ptr = new QmcMetric(activeGroup, metric, scale, history);

    if (ptr->status() >= 0) {
	_metrics.append(ptr);
	_values += ptr->numValues();
    }

    return ptr->status();
}

int
MetricList::add(pmMetricSpec *metric, double scale)
{
    QmcMetric *ptr = new QmcMetric(activeGroup, metric, scale);

    if (ptr->status() >= 0) {
	_metrics.append(ptr);
	_values += ptr->numValues();
    }

    return ptr->status();
}

int
MetricList::add(pmMetricSpec *metric, double scale, int history)
{
    QmcMetric *ptr = new QmcMetric(activeGroup, metric, scale, history);

    if (ptr->status() >= 0) {
	_metrics.append(ptr);
	_values += ptr->numValues();
    }

    return ptr->status();
}

void
MetricList::add(SbColor const& color)
{
    SbColor *ptr = new SbColor;

    ptr->setValue(color.getValue());
    _colors.append(ptr);
}

void
MetricList::add(int packedcol)
{
    float	tran = 0.0;
    SbColor	*ptr = new SbColor;

    ptr->setPackedValue(packedcol, tran);
    _colors.append(ptr);
}

void
MetricList::resolveColors(AlignColor align)
{
    int		need = 0;

    switch(align) {
    case perMetric:
	need = _metrics.size();
	break;
    case perValue:
	need = _values;
	break;
    case noColors:
    default:
	need = 0;
	break;
    }

    if (_metrics.size() == 0)
	return;

    if (_colors.size() == 0 && need > 0)
	add(SbColor(0.0, 0.0, 1.0));

    if (_colors.size() < need) {
	int	o = 0;
	int	n = 0;

	while (n < need) {
	    for (o = 0; o < _colors.size() && n < need; o++, n++) {
		SbColor *ptr = new SbColor;
                ptr->setValue(((SbColor *)_colors[o])->getValue());
                _colors.append(ptr);
	    }
	}
    }
}

QTextStream&
operator<<(QTextStream &os, MetricList const &list)
{
    int		i;
    float	r, g, b;

    for (i = 0; i < list._metrics.size(); i++) {
	os << '[' << i << "]: ";
	if (i < list._colors.size()) {
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
	doMetricFlag = false;
}

int
MetricList::traverse(const char *str)
{
    pmMetricSpec	*theMetric;
    QString		source;
    char		*msg;
    int			type = PM_CONTEXT_HOST;
    SceneGroup		*group = liveGroup;
    int			sts = 0;

    sts = pmParseMetricSpec((char *)str, 0, (char *)0, &theMetric, &msg);
    if (sts < 0) {
	pmprintf("%s: Error: Unable to parse metric spec:\n%s\n", 
		 pmGetProgname(), msg);
	free(msg);
	return sts;
	/*NOTREACHED*/
    }

    // If the metric has instances, then it cannot be traversed
    if (theMetric->ninst) {
	sts = add(theMetric, 0.0);
    }
    else {
	if (theMetric->isarch == 2) {
	    type = PM_CONTEXT_LOCAL;
	}
	else if (theMetric->source && strlen(theMetric->source) > 0) {
	    if (theMetric->isarch == 1) {
		type = PM_CONTEXT_ARCHIVE;
		group = archiveGroup;
	    }
	}

	currentList = this;
	source = theMetric->source;
	sts = group->use(type, source);
	if (sts >= 0) {
	    sts = pmTraversePMNS(theMetric->metric, dometric);
	    if (sts >= 0 && doMetricFlag == false)
		sts = -1;
	    else if (sts < 0)
		pmprintf("%s: Error: %s%c%s: %s\n",
			 pmGetProgname(), 
			 group->context()->source().sourceAscii(),
			 type == PM_CONTEXT_ARCHIVE ? '/' : ':',
			 theMetric->metric,
			 pmErrStr(sts));
	}
    }

    free(theMetric);

    return sts;
}
