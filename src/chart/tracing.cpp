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
#include <qwt_plot_marker.h>
#include "tracing.h"
#include "main.h"

TracingItem::TracingItem(Chart *parent,
	QmcMetric *mp, pmMetricSpec *msp, pmDesc *dp, const char *legend)
	: ChartItem(mp, msp, dp, legend)
{
    my.curve = new QwtPlotIntervalCurve(label());
    my.symbol = new QwtIntervalSymbol(QwtIntervalSymbol::Box);
    my.curve->setStyle(QwtPlotIntervalCurve::NoCurve);
    my.curve->setOrientation(Qt::Horizontal);
    my.curve->setSymbol(my.symbol);
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

void TracingItem::resetValues(int)
{
    console->post(PmChart::DebugForce, "TracingItem::resetValue: %d events", my.events.size());
    my.curve->setSamples(my.intervals);
}

TraceEvent::TraceEvent(QmcEventRecord *record)
{
    my.timestamp = tosec(*record->timestamp());
    my.missed = record->missed();
    my.flags = record->flags();
    my.spanID = record->identifier();
    my.rootID = record->parent();
    my.description = record->parameterSummary();
}

void TracingItem::updateValues(bool, bool, pmUnits*, int size, double left, double right, double delta)
{
    QmcMetric *metric = ChartItem::my.metric;

    console->post(PmChart::DebugForce, "TracingItem::updateValues: %d events, size: %d left: %.2f right=%.2f delta=%.2f", my.events.size(), size, left, right, delta);

    //
    // Iterate over existing event set before updating with new values.
    // Takes advantage of the list of events geing temporally ordered.
    //
    for (int i = 0; i < my.events.size(); i++) {
	// 1. Handle those that have dropped off the left.
	//      If its an active begin/end pair, and end not yet reached,
	//      need to shift the (visible) beginning marker.
	// Only need to process until first still-valid entry found, then
	// short-circuit out.

    }

    for (int i = my.events.size(); i >= 0; i--) {
	//
	// 2. Handle those that need extension to the right.
	// 	    If its an active begin/end pair, and no end as yet (be aware
	// 	    it may be arriving here now), extend to the right.
	// 3. Handle error extension, also to the right, from previous interval.
	// Only need to process until first still-valid entry found, then
	// short-circuit out.
	//
	
    }

    // Now, fetch the new set of events, adding them to the existing set
    if (metric->error(0)) {
	// TODO: create a greyed-out chunk for this interval (QwtIntervalSample)
    } else {
	QVector<QmcEventRecord> const &records = metric->eventRecords(0);

	for (int i = 0; i < records.size(); i++) {
	    QmcEventRecord const &record = records.at(i);

	    if (record.flags())
		i = i;
	}
    }

    // finally, update the display

    double range = right - left;

    my.intervals.clear();
    my.intervals.append(QwtIntervalSample(0,
	QwtInterval((left + drand48() * range), right - drand48() * range)));
    my.intervals.append(QwtIntervalSample(1,
	QwtInterval((left + drand48() * range), right - drand48() * range)));
    my.intervals.append(QwtIntervalSample(2,
	QwtInterval((left + drand48() * range), right - drand48() * range)));

    my.curve->setSamples(my.intervals);
}

void TracingItem::rescaleValues(pmUnits*)
{
    console->post(PmChart::DebugForce, "TracingItem::rescaleValues");
}

void TracingItem::setStroke(Chart::Style, QColor color, bool)
{
    console->post(PmChart::DebugForce, "TracingItem::setStroke");

    QPen outline(QColor(Qt::black));
    QBrush brush(color);

    my.symbol->setWidth(8);
    my.symbol->setBrush(brush);
    my.symbol->setPen(outline);
}

void TracingItem::replot(int, double*)
{
    console->post(PmChart::DebugForce, "TracingItem::replot: %d event records", my.events.size());
}

void TracingItem::revive(Chart *parent)	// TODO: inheritance, move to ChartItem
{
    if (removed()) {
        setRemoved(false);
        my.curve->attach(parent);
    }
}

void TracingItem::remove(void)		// TODO: inheritance, move to ChartItem?
{
    setRemoved(true);
    my.curve->detach();
}

void TracingItem::setPlotEnd(int)
{
    console->post(PmChart::DebugForce, "TracingItem::setPlotEnd");
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
