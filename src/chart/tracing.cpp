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
#include <qwt_plot_directpainter.h>
#include <qwt_plot_marker.h>
#include <qwt_symbol.h>
#include <values.h>
#include "tracing.h"
#include "main.h"

TracingItem::TracingItem(Chart *parent,
	QmcMetric *mp, pmMetricSpec *msp, pmDesc *dp, const char *legend)
	: ChartItem(mp, msp, dp, legend)
{
    my.spanSymbol = new QwtIntervalSymbol(QwtIntervalSymbol::Box);
    my.spanCurve = new QwtPlotIntervalCurve(label());
    my.spanCurve->setStyle(QwtPlotIntervalCurve::NoCurve);
    my.spanCurve->setOrientation(Qt::Horizontal);
    my.spanCurve->setSymbol(my.spanSymbol);
    my.spanCurve->setZ(1);	// lowest/furthest
    my.spanCurve->attach(parent);

    my.dropSymbol = new QwtIntervalSymbol(QwtIntervalSymbol::Box);
    my.dropCurve = new QwtPlotIntervalCurve(label());
    my.dropCurve->setItemAttribute(QwtPlotItem::Legend, false);
    my.dropCurve->setStyle(QwtPlotIntervalCurve::NoCurve);
    my.dropCurve->setOrientation(Qt::Vertical);
    my.dropCurve->setSymbol(my.dropSymbol);
    my.dropCurve->setZ(2);	// middle/central
    my.dropCurve->attach(parent);

    my.pointSymbol = new QwtSymbol(QwtSymbol::Ellipse);
    my.pointCurve = new QwtPlotCurve(label());
    my.pointCurve->setItemAttribute(QwtPlotItem::Legend, false);
    my.pointCurve->setStyle(QwtPlotCurve::NoCurve);
    my.pointCurve->setSymbol(my.pointSymbol);
    my.pointCurve->setZ(3);	// highest/closest
    my.pointCurve->attach(parent);
}

TracingItem::~TracingItem(void)
{
    delete my.pointSymbol;
    delete my.pointCurve;

    delete my.spanSymbol;
    delete my.spanCurve;

    delete my.dropSymbol;
    delete my.dropCurve;
}

QwtPlotItem* TracingItem::item(void)
{
    return my.pointCurve;
}

QwtPlotCurve* TracingItem::curve(void)
{
    return my.pointCurve;
}

void TracingItem::resetValues(int)
{
    console->post(PmChart::DebugForce, "TracingItem::resetValue: %d events", my.events.size());
}

TraceEvent::TraceEvent(QmcEventRecord const &record)
{
    my.timestamp = tosec(*record.timestamp());
    my.missed = record.missed();
    my.flags = record.flags();
    my.spanID = record.identifier();
    my.rootID = record.parent();
    my.description = record.parameterSummary();
}

TraceEvent::~TraceEvent()
{
    my.timestamp = 0.0;
}

//
// Walk the vectors/lists and drop no-longer-needed events.
// Events arrive time-ordered, so we can short-circuit these
// walks once we are within the time window.  Two passes -
// one from the left, one (subsequent, after modification to
// original structure) from the right.
//

void TracingItem::cullOutlyingRanges(QVector<QwtIntervalSample> &range, double left, double right)
{
    int i, cull;

    for (i = cull = 0; i < range.size(); i++, cull++)
	if (range.at(i).value >= left)
	    break;
    if (cull)
	range.remove(0, cull); // cull from the start (0-index)
    for (i = range.size() - 1, cull = 0; i >= 0; i--, cull++)
	if (range.at(i).value <= right)
	    break;
    if (cull)
	range.remove(range.size() - cull, cull); // cull from end
}

void TracingItem::cullOutlyingPoints(QVector<QPointF> &points, double left, double right)
{
    int i, cull;

    for (i = cull = 0; i < points.size(); i++, cull++)
	if (points.at(i).y() >= left)
	    break;
    if (cull)
	points.remove(0, cull); // cull from the start (0-index)
    for (i = points.size() - 1, cull = 0; i >= 0; i--, cull++)
	if (points.at(i).y() <= right)
	    break;
    if (cull)
	points.remove(points.size() - cull, cull); // cull from end
}

void TracingItem::cullOutlyingEvents(QVector<TraceEvent> &events, double left, double right)
{
    int i, cull;

    for (i = cull = 0; i < events.size(); i++, cull++)
	if (events.at(i).timestamp() >= left)
	    break;
    if (cull)
	events.remove(0, cull); // cull from the start (0-index)
    for (i = events.size() - 1, cull = 0; i >= 0; i--, cull++)
	if (events.at(i).timestamp() <= right)
	    break;
    if (cull)
	events.remove(events.size() - cull, cull); // cull from end
}

//
// Requirement here is to merge in any event records that have
// just arrived in the last timestep (from my.metric) with the
// set already being displayed.
// Additionally, we need to cull those that are no longer within
// the time window of interest.
//
void TracingItem::updateValues(bool, bool, pmUnits*, int size, int points, double left, double right, double delta)
{
    console->post(PmChart::DebugForce, "TracingItem::updateValues: "
		"%d events, chart size=%d points=%d, left: %.2f right=%.2f delta=%.2f",
		my.events.size(), size, points, left, right, delta);

    cullOutlyingRanges(my.drops, left, right);
    cullOutlyingRanges(my.spans, left, right);
    cullOutlyingPoints(my.points, left, right);
    cullOutlyingEvents(my.events, left, right);

    // crack open newly arrived event records
    QmcMetric *metric = ChartItem::my.metric;
    if (metric->numValues() > 0)
	updateEvents(metric);

    // update the display
    my.dropCurve->setSamples(my.drops);
    my.spanCurve->setSamples(my.spans);
    my.pointCurve->setSamples(my.points);
}

void TracingItem::updateEvents(QmcMetric *metric)
{
    if (metric->hasInstances() && !metric->explicitInsts()) {
	for (int i = 0; i < metric->numInst(); i++)
	    updateEventRecords(metric, i);
    } else {
	updateEventRecords(metric, 0);
    }
}

//
// Fetch the new set of events, merging them into the existing sets
// - "Points" curve has an entry for *every* event to be displayed.
// - "Span" curve has an entry for all *begin/end* flagged events.
//   These are initially unpaired, unless PMDA is playing games, and
//   so usually min/max is used as a placeholder until corresponding
//   begin/end event arrives.
// - "Drop" curve has an entry to match up events with the parents.
//   The parent is the root "span" (terminology on loan from Dapper)
//
void TracingItem::updateEventRecords(QmcMetric *metric, int index)
{
    if (metric->error(index) == false) {
	QVector<QmcEventRecord> const &records = metric->eventRecords(index);
	int slot = metric->instIndex(index), parentSlot = -1;

	// First lookup the event "slot" (aka "span" / y-axis-entry)
	// Strategy is to use the ID from the event (if one exists),
	// which must be mapped to a y-axis zero-indexed value.  If
	// no ID, we fall back to metric instance ID, else just zero.
	//
	if (metric->hasInstances() && metric->explicitInsts())
	    slot = metric->instIndex(0);

	for (int i = 0; i < records.size(); i++) {
	    QmcEventRecord const &record = records.at(i);

	    my.events.append(TraceEvent(record));
	    TraceEvent &event = my.events.last();

	    // find the "slot" (y-axis value) for this identifier
	    if (event.hasIdentifier())
		slot = my.yMap.value(event.spanID(), slot);
	    event.setSlot(slot);

	    // this adds the basic point (ellipse), all events get one
	    my.points.append(QPointF(event.timestamp(), slot));
	    my.yMap.insert(event.spanID(), slot);

	    if (event.hasParent()) {	// lookup parent in yMap
		parentSlot = my.yMap.value(event.rootID(), parentSlot);
		// do this on start/end only?  (or if first?)
		my.drops.append(QwtIntervalSample(event.timestamp(),
				    QwtInterval(slot, parentSlot)));
	    }

	    if (event.hasStartFlag()) {
		if (!my.spans.isEmpty()) {
		    QwtIntervalSample &active = my.spans.last();
		    // did we get a start, then another start?
		    // (if so, just end the previous span now)
		    if (active.interval.maxValue() == DBL_MAX)
			active.interval.setMaxValue(event.timestamp());
		}
		// no matter what, we'll start a new span here
		my.spans.append(QwtIntervalSample(index,
				    QwtInterval(event.timestamp(), DBL_MAX)));
	    }
	    if (event.hasEndFlag()) {
		if (!my.spans.isEmpty()) {
		    QwtIntervalSample &active = my.spans.last();
		    // did we get an end, then another end?
		    // (if so, move previous span end to now)
		    if (active.interval.maxValue() != DBL_MAX)
			active.interval.setMaxValue(event.timestamp());
		} else {
		    // got an end, but we haven't seen a start
		    my.spans.append(QwtIntervalSample(index,
				    QwtInterval(DBL_MIN, event.timestamp())));
		}
	    }
	    // Have not yet handled missed events (i.e. event.missed())
	    // Could have a separate list of events? (render differently?)
	}
    } else {
	//
	// Hmm, what does an error here mean?  (e.g. host down).
	// We need some visual representation that makes sense.
	// Perhaps a background gridline (e.g. just y-axis dots)
	// and then leave a blank section when this happens?
	// Might need to have (another) curve to implement this.
	//
    }
}

void TracingItem::rescaleValues(pmUnits*)
{
    console->post(PmChart::DebugForce, "TracingItem::rescaleValues");
}

void TracingItem::setStroke(Chart::Style, QColor color, bool)
{
    QColor alphaColor = color;
    console->post(PmChart::DebugForce, "TracingItem::setStroke");

    QPen outline(QColor(Qt::black));
    alphaColor.setAlpha(196);
    QBrush brush(alphaColor);

    my.spanSymbol->setWidth(6);
    my.spanSymbol->setBrush(brush);
    my.spanSymbol->setPen(outline);

    my.dropSymbol->setWidth(1);
    my.dropSymbol->setBrush(Qt::NoBrush);
    my.dropSymbol->setPen(outline);

    my.pointSymbol->setSize(8);
    my.pointSymbol->setColor(color);
    my.pointSymbol->setPen(outline);
}

bool TracingItem::containsPoint(const QRectF &rect, int index)
{
    if (my.points.isEmpty())
	return false;
    return rect.contains(my.points.at(index));
}

void TracingItem::updateCursor(const QPointF &, int index)
{
    Q_ASSERT(index <= my.points.size());
    Q_ASSERT(index <= (int)my.pointCurve->dataSize());

    my.selections.append(index);
}

void TracingItem::clearCursor(void)
{
    my.selections.clear();
}

void TracingItem::showCursor()
{
    const QBrush brush = my.pointSymbol->brush();

    my.pointSymbol->setBrush(my.pointSymbol->brush().color().dark(180));
    for (int i = 0; i < my.selections.size(); i++) {
	int index = my.selections.at(i);
	QwtPlotDirectPainter directPainter;
	directPainter.drawSeries(my.pointCurve, index, index);
    }
    my.pointSymbol->setBrush(brush);   // reset brush
}

//
// Display information text associated with selected events
//
const QString &TracingItem::cursorInfo()
{
    my.selectionInfo = QString::null;

    for (int i = 0; i < my.selections.size(); i++) {
	TraceEvent const &event = my.events.at(i);
	double stamp = event.timestamp();
	if (i)
	    my.selectionInfo.append("\n");
	my.selectionInfo.append(timeHiResString(stamp));
	my.selectionInfo.append(": ");
	my.selectionInfo.append(event.description());
    }
    return my.selectionInfo;
}

void TracingItem::replot(int history, double*)
{
    console->post(PmChart::DebugForce,
		  "TracingItem::replot: %d event records (%d samples)",
		  my.events.size(), history);

    my.dropCurve->setSamples(my.drops);
    my.spanCurve->setSamples(my.spans);
    my.pointCurve->setSamples(my.points);
}

void TracingItem::revive(Chart *parent)
{
    if (removed()) {
        setRemoved(false);
        my.dropCurve->attach(parent);
        my.spanCurve->attach(parent);
        my.pointCurve->attach(parent);
    }
}

void TracingItem::remove(void)
{
    setRemoved(true);
    my.dropCurve->detach();
    my.spanCurve->detach();
    my.pointCurve->detach();
}

void TracingItem::setPlotEnd(int)
{
    console->post(PmChart::DebugForce, "TracingItem::setPlotEnd");
}


TracingScaleEngine::TracingScaleEngine() : QwtLinearScaleEngine()
{
    setMargins(0.5, 0.5);
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
    maxSteps = 1;
    minValue = 0.0;
    maxValue = maxSteps * 1.0;
    stepSize = 1.0;
    QwtLinearScaleEngine::autoScale(maxSteps, minValue, maxValue, stepSize);
}

QwtScaleDiv
TracingScaleEngine::divideScale(double x1, double x2, int numMajorSteps,
                           int /*numMinorSteps*/, double /*stepSize*/) const
{
    // discard minor steps - y-axis is displaying trace identifiers;
    // sub-divisions of an identifier makes no sense
    return QwtLinearScaleEngine::divideScale(x1, x2, numMajorSteps, 0, 1.0);
}
