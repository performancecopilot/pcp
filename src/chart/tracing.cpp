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
#include <qwt_plot_marker.h>

TracingItem::TracingItem(Chart *parent,
	QmcMetric *mp, pmMetricSpec *msp, pmDesc *dp, const char *legend)
	: ChartItem(mp, msp, dp, legend)
{
    // some dummy data
    my.records.append(QwtIntervalSample(10, QwtInterval(800000, 800100)));
    my.records.append(QwtIntervalSample(2,  QwtInterval(800400, 800500)));
    my.records.append(QwtIntervalSample(18, QwtInterval(800600, 800700)));

    my.curve = new QwtPlotIntervalCurve("XXX TODO");
    QwtIntervalSymbol *symbol = new QwtIntervalSymbol(QwtIntervalSymbol::Box);
    my.curve->setSymbol(symbol);
    my.curve->setSamples(my.records);
    my.curve->setStyle(QwtPlotIntervalCurve::NoCurve);
    my.curve->setOrientation(Qt::Horizontal);
    my.curve->setPen(QPen(Qt::blue));
    my.curve->attach(parent);
}

TracingItem::~TracingItem(void)
{
    delete my.curve;
}

QwtPlotItem* TracingItem::item(void)
{
    return my.curve;
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

void TracingItem::revive(Chart *)
{
}

void TracingItem::remove(void)
{
}

void TracingItem::setPlotEnd(int)
{
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
