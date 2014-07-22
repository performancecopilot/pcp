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
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoSeparator.h>
#include "main.h"
#include "colormod.h"
#include "modlist.h"
#include "launch.h"

#include <iostream>
using namespace std;

ColorMod::~ColorMod()
{
}

ColorMod::ColorMod(const char *metric, double scale,
			   const ColorScale &colors, SoNode *obj)
: Modulate(metric, scale), 
  _state(Modulate::start),
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
	    cerr << "ColorMod: Added " << metric << " (Id = " 
		 << _root->getName().getString() << ")" << endl;
#endif

    }
    else if (_metrics->numValues() > 1) {
	warningMsg(_POS_, 
		       "Color modulated metric (%s) has more than one value (%d)",
		       metric, _metrics->numValues());
    }
    else if (_scale.numSteps() == 0) {
	warningMsg(_POS_,
		       "No color steps for color modulated metric (%s)",
		       metric);
    }
}

void
ColorMod::refresh(bool fetchFlag)
{
    QmcMetric &metric = _metrics->metric(0);
    
    if (status() < 0)
	return;

    if (fetchFlag)
	metric.update();

    if (metric.error(0) <= 0) {
	if (_state != Modulate::error) {
	    _color->rgb.setValue(_errorColor.getValue());
	    _state = Modulate::error;
	}
    }
    else {
	double value = metric.value(0) * theScale;
	if (value > theNormError) {
	    if (_state != Modulate::saturated) {
		_color->rgb.setValue(Modulate::_saturatedColor);
		_state = Modulate::saturated;
	    }
	}
	else {
	    if (_state != Modulate::normal)
		_state = Modulate::normal;
	    _color->rgb.setValue(_scale.step(value).color().getValue());
	}
    }
}

void
ColorMod::dump(QTextStream &os) const
{
    os << "ColorMod: ";
    if (status() < 0)
    	os << "Invalid Metric: " << pmErrStr(status()) << endl;
    else {
	os << "state = ";
	dumpState(os, _state);
	os << ", scale = " << _scale << ": ";
        _metrics->metric(0).dump(os, true);
    }
}

void
ColorMod::infoText(QString &str, bool) const
{
    const QmcMetric &metric = _metrics->metric(0);
    str = metric.spec(true, true, 0);
    str.append(QChar('\n'));
    if (_state == Modulate::error)
	str.append(theErrorText);
    else if (_state == Modulate::start)
	str.append(theStartText);
    else {
	QString value;
	str.append(value.setNum(metric.realValue(0), 'g', 4));
	str.append(QChar(' '));
	if (metric.desc().units().length() > 0)
	    str.append(metric.desc().units());
	str.append(" [");
	str.append(value.setNum(metric.value(0) * 100.0 * theScale, 'g', 4));
	str.append("% of color scale]");
    }
}

void
ColorMod::launch(Launch &launch, bool) const
{
    if (status() < 0)
	return;
    launch.startGroup("point");
    launch.addMetric(_metrics->metric(0), _scale, 0);
    launch.endGroup();
}

int
ColorMod::select(SoPath *)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "ColorMod::select: " << _metrics->metric(0) << endl;
#endif
    return 1;
}

int
ColorMod::remove(SoPath *)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	cerr << "ColorMod::remove: " << _metrics->metric(0) << endl;
#endif
    return 0;
}
