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


#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSeparator.h>
#include "Inv.h"
#include "ScaleMod.h"
#include "ModList.h"
#include "Metric.h"
#include "Launch.h"

INV_ScaleMod::~INV_ScaleMod()
{
}

INV_ScaleMod::INV_ScaleMod(const char *str,
			   double scale,
			   const SbColor &color,
			   SoNode *obj,
			   float xScale,
			   float yScale,
			   float zScale)
: INV_Modulate(str, scale, color),
  _state(INV_Modulate::start),
  _color(0),
  _scale(0),
  _xScale(xScale),
  _yScale(yScale),
  _zScale(zScale)
{
    _root = new SoSeparator;

    _color = new SoBaseColor;
    _color->rgb.setValue(_errorColor.getValue());
    _root->addChild(_color);

    if (_metrics->numValues() == 1 && status() >= 0) {

        _scale = new SoScale;
        _scale->scaleFactor.setValue(1.0, 1.0, 1.0);
        _root->addChild(_scale);
        _root->addChild(obj);

	add();

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "INV_ScaleMod: Added " << str << " (Id = " 
		 << _root->getName().getString() << ")" << endl;
#endif
    }

    // Invalid metric
    else
        _root->addChild(obj);
}

void
INV_ScaleMod::refresh(OMC_Bool fetchFlag)
{
    OMC_Metric &metric = _metrics->metric(0);

    if (status() < 0)
	return;

    if (fetchFlag)
        metric.update();

    if (metric.error(0) <= 0) {
        if (_state != INV_Modulate::error) {
            _color->rgb.setValue(_errorColor.getValue());
            _scale->scaleFactor.setValue((_xScale==0.0f ? 1.0 : theMinScale),
					 (_yScale==0.0f ? 1.0 : theMinScale),
					 (_zScale==0.0f ? 1.0 : theMinScale));
            _state = INV_Modulate::error;
        }
    }
    else {
        double value = metric.value(0) * theScale;
        if (value > theNormError) {
            if (_state != INV_Modulate::saturated) {
                _color->rgb.setValue(_saturatedColor.getValue());
                _scale->scaleFactor.setValue(1.0, 1.0, 1.0);
                _state = INV_Modulate::saturated;
            }
        }
        else {
            if (_state != INV_Modulate::normal) {
                _color->rgb.setValue(_metrics->color(0).getValue());
                _state = INV_Modulate::normal;
            }
            if (value < INV_Modulate::theMinScale)
                value = INV_Modulate::theMinScale;
            else if (value > 1.0)
                value = 1.0;
            _scale->scaleFactor.setValue((_xScale==0.0f ? 1.0 : _xScale*value),
					 (_yScale==0.0f ? 1.0 : _yScale*value),
					 (_zScale==0.0f ? 1.0 : _zScale*value));
        }
    }
}

void
INV_ScaleMod::dump(ostream &os) const
{
    os << "INV_ScaleMod: ";

    if (status() < 0)
        os << "Invalid metric";
    else {
        os << "state = ";
	dumpState(os, _state);
        os << ", scale = " << _xScale << ',' << _yScale << ',' << _zScale 
	   << ": ";
        _metrics->metric(0).dump(os, OMC_true);
    }
}

void
INV_ScaleMod::infoText(OMC_String &str, OMC_Bool) const
{
    const OMC_Metric &metric = _metrics->metric(0);
    str = metric.spec(OMC_true, OMC_true, 0);
    str.appendChar('\n');
    if (_state == INV_Modulate::error)
	str.append(theErrorText);
    else if (_state == INV_Modulate::start)
	str.append(theStartText);
    else {
	str.appendReal(metric.realValue(0), 4);
	str.appendChar(' ');
	if (metric.units().length() > 0)
	    str.append(metric.units());
	str.append(" [");
	str.appendReal(metric.value(0) * 100.0, 4);
	str.append("% of expected max]");
    }
}

void
INV_ScaleMod::launch(INV_Launch &launch, OMC_Bool) const
{
    if (status() < 0)
	return;

    launch.startGroup("point");
    launch.addMetric(_metrics->metric(0), _metrics->color(0), 0);
    launch.endGroup();
}

uint_t
INV_ScaleMod::select(SoPath *)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_ScaleMod::select: " << _metrics->metric(0) << endl;
#endif
    return 1;
}

uint_t
INV_ScaleMod::remove(SoPath *)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_ScaleMod::remove: " << _metrics->metric(0) << endl;
#endif
    return 0;
}
