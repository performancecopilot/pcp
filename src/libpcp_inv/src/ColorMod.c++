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
#include <Inventor/nodes/SoSeparator.h>
#include "Inv.h"
#include "ColorMod.h"
#include "ModList.h"
#include "Metric.h"
#include "Launch.h"

INV_ColorMod::~INV_ColorMod()
{
}

INV_ColorMod::INV_ColorMod(const char *metric, double scale,
			   const INV_ColorScale &colors, SoNode *obj)
: INV_Modulate(metric, scale), 
  _state(INV_Modulate::start),
  _scale(colors), 
  _color(0)
{
    _root = new SoSeparator;
    _color = new SoBaseColor;

    _color->rgb.setValue(_errorColor.getValue());
    _root->addChild(_color);
    _root->addChild(obj);

    if (_metrics->numValues() == 1 && _scale.numSteps() && status() >= 0) {
	add();
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    cerr << "INV_ColorMod: Added " << metric << " (Id = " 
		 << _root->getName().getString() << ")" << endl;
#endif

    }
    else if (_metrics->numValues() > 1) {
	INV_warningMsg(_POS_, 
		       "Color modulated metric (%s) has more than one value (%d)",
		       metric, _metrics->numValues());
    }
    else if (_scale.numSteps() == 0) {
	INV_warningMsg(_POS_,
		       "No color steps for color modulated metric (%s)",
		       metric);
    }
}

void
INV_ColorMod::refresh(OMC_Bool fetchFlag)
{
    OMC_Metric &metric = _metrics->metric(0);
    
    if (status() < 0)
	return;

    if (fetchFlag)
	metric.update();

    if (metric.error(0) <= 0) {
	if (_state != INV_Modulate::error) {
	    _color->rgb.setValue(_errorColor.getValue());
	    _state = INV_Modulate::error;
	}
    }
    else {
	double value = metric.value(0) * theScale;
	if (value > theNormError) {
	    if (_state != INV_Modulate::saturated) {
		_color->rgb.setValue(INV_Modulate::_saturatedColor);
		_state = INV_Modulate::saturated;
	    }
	}
	else {
	    if (_state != INV_Modulate::normal)
		_state = INV_Modulate::normal;
	    _color->rgb.setValue(_scale.step(value).color().getValue());
	}
    }
}

void
INV_ColorMod::dump(ostream &os) const
{
    os << "INV_ColorMod: ";
    if (status() < 0)
    	os << "Invalid Metric: " << pmErrStr(status()) << endl;
    else {
	os << "state = ";
	dumpState(os, _state);
	os << ", scale = " << _scale << ": ";
        _metrics->metric(0).dump(os, OMC_true);
    }
}

void
INV_ColorMod::infoText(OMC_String &str, OMC_Bool) const
{
    const OMC_Metric &metric = _metrics->metric(0);
    str = metric.spec(OMC_true, OMC_true, 0);
    str.appendChar('\n');
    if (_state == INV_Modulate::error)
	str.append(theErrorText);
    else if (_state == INV_Modulate::start)
	str.append(theStartText);
    else {
	str.appendReal(metric.realValue(0));
	str.appendChar(' ');
	if (metric.units().length() > 0)
	    str.append(metric.units());
	str.append(" [");
	str.appendReal(metric.value(0) * 100.0 * theScale);
	str.append("% of color scale]");
    }
}

void
INV_ColorMod::launch(INV_Launch &launch, OMC_Bool) const
{
    if (status() < 0)
	return;

    launch.startGroup("point");
    launch.addMetric(_metrics->metric(0), _scale, 0);
    launch.endGroup();
}

uint_t
INV_ColorMod::select(SoPath *)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_ColorMod::select: " << _metrics->metric(0) << endl;
#endif
    return 1;
}

uint_t
INV_ColorMod::remove(SoPath *)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "INV_ColorMod::remove: " << _metrics->metric(0) << endl;
#endif
    return 0;
}
