/*
 * Copyright (c) 2012, Red Hat.
 * Copyright (c) 2012, Nathan Scott.  All Rights Reserved.
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
#include "tracing.h"

TracingItem::TracingItem(Chart *parent,
	QmcMetric *mp, pmMetricSpec *msp, pmDesc *dp, const char *legend)
	: ChartItem(mp, msp, dp, legend)
{
    (void)parent;
}

TracingItem::~TracingItem(void)
{
}

QwtPlotItem* TracingItem::item(void)
{
    return NULL;
}

void TracingItem::preserveLiveData(int, int)
{
}

void TracingItem::punchoutLiveData(int)
{
}

void TracingItem::resetValues(int)
{
}

void TracingItem::updateValues(bool, bool, int, pmUnits*)
{
}

void TracingItem::rescaleValues(pmUnits*)
{
}

void TracingItem::setStroke(Chart::Style, QColor, bool)
{
}

void TracingItem::replot(int, double*)
{
}

void TracingItem::revive(Chart *parent)
{
}

void TracingItem::remove(void)
{
}

void TracingItem::setPlotEnd(int index)
{
    (void)index;	// extend any active trace to the right
}


TracingScaleEngine::TracingScaleEngine()
{
}

void TracingScaleEngine::setScale(bool autoScale, double minValue, double maxValue)
{
    (void)autoScale;
    (void)minValue;
    (void)maxValue;
}

void TracingScaleEngine::autoScale(int maxSteps, double &minValue,
                           double &maxValue, double &stepSize) const
{
    (void)maxSteps;
    (void)minValue;
    (void)maxValue;
    (void)stepSize;
}
