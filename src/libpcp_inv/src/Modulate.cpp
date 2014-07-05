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


#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSelection.h>
#include "Modulate.h"
#include "ModList.h"
#include "Record.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

double			theNormError = 1.05;

const OMC_String	INV_Modulate::theErrorText = "Metric Unavailable";
const OMC_String	INV_Modulate::theStartText = "Metric has not been fetched from source";
const float		INV_Modulate::theDefErrorColor[] = {0.2, 0.2, 0.2};
const float		INV_Modulate::theDefSaturatedColor[] = {1.0, 1.0, 1.0};
const double		INV_Modulate::theMinScale = 0.01;

INV_Modulate::~INV_Modulate()
{
}

INV_Modulate::INV_Modulate(const char *metric, double scale,
			   INV_MetricList::AlignColor align)
: _sts(0), _metrics(0), _root(0)
{
    _metrics = new INV_MetricList();
    _sts = _metrics->add(metric, scale);
    if (_sts >= 0) {
	_metrics->resolveColors(align);
	_saturatedColor.setValue(theDefSaturatedColor);
    }
    _errorColor.setValue(theDefErrorColor);
}

INV_Modulate::INV_Modulate(const char *metric, double scale, 
			   const SbColor &color,
			   INV_MetricList::AlignColor align)
:  _sts(0), _metrics(0), _root(0)
{
    _metrics = new INV_MetricList();
    _sts = _metrics->add(metric, scale);
    if (_sts >= 0) {
	_metrics->add(color),
	_metrics->resolveColors(align);
    }
    _saturatedColor.setValue(theDefSaturatedColor);
    _errorColor.setValue(theDefErrorColor);
}

INV_Modulate::INV_Modulate(INV_MetricList *list)
:  _sts(0), _metrics(list), _root(0)
{
    _saturatedColor.setValue(theDefSaturatedColor);
    _errorColor.setValue(theDefErrorColor);
}

const char *
INV_Modulate::add()
{
    const char *str = theModList->add(this);
    _root->setName((SbName)str);
    return str;
}

ostream &
operator<<(ostream & os, const INV_Modulate &rhs)
{
    rhs.dump(os);
    return os;
}

void
INV_Modulate::dumpState(ostream &os, INV_Modulate::State state) const
{
    switch(state) {
    case INV_Modulate::start:
	os << "Start";
	break;
    case INV_Modulate::error:
	os << "Error";
	break;
    case INV_Modulate::saturated:
	os << "Saturated";
	break;
    case INV_Modulate::normal:
	os << "Normal";
	break;
    default:
	os << "Unknown";
	break;
    }
}

void
INV_Modulate::record(INV_Record &rec) const
{
    uint_t	i;

    if (_metrics != NULL)
	for (i = 0; i < _metrics->numMetrics(); i++) {
	    const OMC_Metric &metric = _metrics->metric(i);
	    rec.add(metric.host().ptr(), 
		    metric.spec(OMC_false, OMC_true).ptr());
	}
}

void
INV_Modulate::selectAll()
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_Modulate::selectAll: selectAll for " << *this << endl;
#endif

    theModList->selectAllId(_root, 1);
    theModList->selectSingle(_root);
}
