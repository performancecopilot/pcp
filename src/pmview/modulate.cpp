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
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSelection.h>
#include "modulate.h"
#include "modlist.h"

#include <iostream>
using namespace std;

double			theNormError = 1.05;

const QString	Modulate::theErrorText = "Metric Unavailable";
const QString	Modulate::theStartText = "Metric has not been fetched from source";
const float	Modulate::theDefErrorColor[] = {0.2, 0.2, 0.2};
const float	Modulate::theDefSaturatedColor[] = {1.0, 1.0, 1.0};
const double	Modulate::theMinScale = 0.01;

Modulate::~Modulate()
{
}

Modulate::Modulate(const char *metric, double scale,
			   MetricList::AlignColor align)
: _sts(0), _metrics(0), _root(0)
{
    _metrics = new MetricList();
    _sts = _metrics->add(metric, scale);
    if (_sts >= 0) {
	_metrics->resolveColors(align);
	_saturatedColor.setValue(theDefSaturatedColor);
    }
    _errorColor.setValue(theDefErrorColor);
}

Modulate::Modulate(const char *metric, double scale, 
			   const SbColor &color,
			   MetricList::AlignColor align)
:  _sts(0), _metrics(0), _root(0)
{
    _metrics = new MetricList();
    _sts = _metrics->add(metric, scale);
    if (_sts >= 0) {
	_metrics->add(color),
	_metrics->resolveColors(align);
    }
    _saturatedColor.setValue(theDefSaturatedColor);
    _errorColor.setValue(theDefErrorColor);
}

Modulate::Modulate(MetricList *list)
:  _sts(0), _metrics(list), _root(0)
{
    _saturatedColor.setValue(theDefSaturatedColor);
    _errorColor.setValue(theDefErrorColor);
}

const char *
Modulate::add()
{
    const char *str = theModList->add(this);
    _root->setName((SbName)str);
    return str;
}

QTextStream &
operator<<(QTextStream & os, const Modulate &rhs)
{
    rhs.dump(os);
    return os;
}

void
Modulate::dumpState(QTextStream &os, Modulate::State state) const
{
    switch(state) {
    case Modulate::start:
	os << "Start";
	break;
    case Modulate::error:
	os << "Error";
	break;
    case Modulate::saturated:
	os << "Saturated";
	break;
    case Modulate::normal:
	os << "Normal";
	break;
    default:
	os << "Unknown";
	break;
    }
}

void
Modulate::record(Record &rec) const
{
#if 1	// TODO
     (void)rec;
#else	// TODO
    int		i;

    if (_metrics != NULL)
	for (i = 0; i < _metrics->numMetrics(); i++) {
	    const QmcMetric &metric = _metrics->metric(i);
	    rec.add(metric.context()->source().sourceAscii(), 
		    (const char *)metric.spec(false, true).toAscii());
	}
#endif
}

void
Modulate::selectAll()
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "Modulate::selectAll: selectAll for " << *this << endl;
#endif

    theModList->selectAllId(_root, 1);
    theModList->selectSingle(_root);
}
