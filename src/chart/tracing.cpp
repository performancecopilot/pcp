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

TracingItem::TracingItem(Chart *chart,
	QmcMetric *mp, pmMetricSpec *msp, pmDesc *dp, const char *legend)
	: ChartItem(mp, msp, dp, legend)
{
    my.chart = chart;
    my.minSpanID = 0;
    my.maxSpanID = 1;
    my.previousTimestamp = 0.0;

    my.spanSymbol = new QwtIntervalSymbol(QwtIntervalSymbol::Box);
    my.spanCurve = new QwtPlotIntervalCurve(label());
    my.spanCurve->setItemAttribute(QwtPlotItem::Legend, false);
    my.spanCurve->setStyle(QwtPlotIntervalCurve::NoCurve);
    my.spanCurve->setOrientation(Qt::Horizontal);
    my.spanCurve->setSymbol(my.spanSymbol);
    my.spanCurve->setZ(1);	// lowest/furthest

    my.dropSymbol = new QwtIntervalSymbol(QwtIntervalSymbol::Box);
    my.dropCurve = new QwtPlotIntervalCurve(label());
    my.dropCurve->setItemAttribute(QwtPlotItem::Legend, false);
    my.dropCurve->setStyle(QwtPlotIntervalCurve::NoCurve);
    my.dropCurve->setOrientation(Qt::Vertical);
    my.dropCurve->setSymbol(my.dropSymbol);
    my.dropCurve->setZ(2);	// middle/central

    my.pointSymbol = new QwtSymbol(QwtSymbol::Ellipse);
    my.pointCurve = new ChartCurve(label());
    my.pointCurve->setStyle(QwtPlotCurve::NoCurve);
    my.pointCurve->setSymbol(my.pointSymbol);
    my.pointCurve->setZ(3);	// higher/closer

    my.selectionSymbol = new QwtSymbol(QwtSymbol::Ellipse);
    my.selectionCurve = new QwtPlotCurve(label());
    my.selectionCurve->setItemAttribute(QwtPlotItem::Legend, false);
    my.selectionCurve->setStyle(QwtPlotCurve::NoCurve);
    my.selectionCurve->setSymbol(my.selectionSymbol);
    my.selectionCurve->setZ(4);	// highest/closest

    my.spanCurve->attach(chart);
    my.dropCurve->attach(chart);
    my.pointCurve->attach(chart);
    my.selectionCurve->attach(chart);
}

TracingItem::~TracingItem(void)
{
    delete my.spanCurve;
    delete my.spanSymbol;
    delete my.dropCurve;
    delete my.dropSymbol;
    delete my.pointCurve;
    delete my.pointSymbol;
    delete my.selectionCurve;
    delete my.selectionSymbol;
}

QwtPlotItem* TracingItem::item(void)
{
    return my.pointCurve;
}

QwtPlotCurve* TracingItem::curve(void)
{
    return my.pointCurve;
}

TracingEvent::TracingEvent(QmcEventRecord const &record)
{
    my.timestamp = tosec(*record.timestamp());
    my.missed = record.missed();
    my.flags = record.flags();
    my.slot = 0;
    my.spanID = record.identifier();
    my.rootID = record.parent();
    my.description = record.parameterSummary();
}

TracingEvent::~TracingEvent()
{
    my.timestamp = 0.0;
}

void
TracingItem::rescaleValues(int *minValue, int *maxValue)
{
    if (my.minSpanID < *minValue)
	*minValue = my.minSpanID;
    if (my.maxSpanID > *maxValue)
	*maxValue = my.maxSpanID;
}

//
// Walk the vectors/lists and drop no-longer-needed events.
// For points/events/drops (single time value):
//   Events arrive time-ordered, so we can short-circuit these
//   walks once we are within the time window.  Two phases -
//   walk once from the left, then a subsequent walk from the
//   right (note: done *after* we modify the original vector)
// For horizonal spans (i.e. two time values):
//   This is still true - events arrive in order - but we have
//   to walk the entire list as these ranges can overlap.  But
//   thats OK - we expect far fewer spans than total events.
//   While we're at it, we can keep track of min/max span ID.
//

void
TracingItem::cullOutlyingSpans(double left, double right)
{
    int minSpanID = 0, maxSpanID = 1;

    // Start from the end so that we can remove as we go
    // without interfering with the index we are using.
    for (int i = my.spans.size() - 1; i >= 0; i--) {
	const QwtIntervalSample &span = my.spans.at(i);
	if (span.interval.maxValue() >= left ||
	    span.interval.minValue() <= right) {
	    minSpanID = qMin(minSpanID, (int)span.value);
	    maxSpanID = qMax(maxSpanID, (int)span.value);
	} else {
	    my.spans.remove(i);
	}
    }
    my.minSpanID = minSpanID;
    my.maxSpanID = maxSpanID;
}

void
TracingItem::cullOutlyingDrops(double left, double right)
{
    int i, cull;

    for (i = cull = 0; i < my.drops.size(); i++, cull++)
	if (my.drops.at(i).value >= left)
	    break;
    if (cull)
	my.drops.remove(0, cull); // cull from the start (0-index)
    for (i = my.drops.size() - 1, cull = 0; i >= 0; i--, cull++)
	if (my.drops.at(i).value <= right)
	    break;
    if (cull)
	my.drops.remove(my.drops.size() - cull, cull); // cull from end
}

void
TracingItem::cullOutlyingPoints(double left, double right)
{
    int i, cull;

    for (i = cull = 0; i < my.points.size(); i++, cull++)
	if (my.points.at(i).x() >= left)
	    break;
    if (cull)
	my.points.remove(0, cull); // cull from the start (0-index)
    for (i = my.points.size() - 1, cull = 0; i >= 0; i--, cull++)
	if (my.points.at(i).x() <= right)
	    break;
    if (cull)
	my.points.remove(my.points.size() - cull, cull); // cull from end
}

void
TracingItem::cullOutlyingEvents(double left, double right)
{
    int i, cull;

    for (i = cull = 0; i < my.events.size(); i++, cull++)
	if (my.events.at(i).timestamp() >= left)
	    break;
    if (cull)
	my.events.remove(0, cull); // cull from the start (0-index)
    for (i = my.events.size() - 1, cull = 0; i >= 0; i--, cull++)
	if (my.events.at(i).timestamp() <= right)
	    break;
    if (cull)
	my.events.remove(my.events.size() - cull, cull); // cull from end
}

void
TracingItem::resetValues(int samples)
{
    // TODO
    console->post(PmChart::DebugForce, "TracingItem::resetValues: sample count change: NYI: %d", samples);
}


//
// Requirement here is to merge in any event records that have
// just arrived in the last timestep (from my.metric) with the
// set already being displayed.
// Additionally, we need to cull those that are no longer within
// the time window of interest.
//
void
TracingItem::updateValues(bool, bool, pmUnits*, int, int, double left, double right, double)
{
#ifdef DESPERATE
    console->post(PmChart::DebugForce, "TracingItem::updateValues: "
		"%d total events, left=%.2f right=%.2f\n"
		"Metadata counts: drops=%d spans=%d points=%d"
		my.events.size(), left, right,
		my.drops.size(), my.spans.size(), my.points.size());
#endif

    cullOutlyingSpans(left, right);
    cullOutlyingDrops(left, right);
    cullOutlyingPoints(left, right);
    cullOutlyingEvents(left, right);

    // crack open newly arrived event records
    QmcMetric *metric = ChartItem::my.metric;
    if (metric->numValues() > 0)
	updateEvents(metric);

    // update the display
    my.dropCurve->setSamples(my.drops);
    my.spanCurve->setSamples(my.spans);
    my.pointCurve->setSamples(my.points);
    my.selectionCurve->setSamples(my.selections);
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

	    my.events.append(TracingEvent(record));
	    TracingEvent &event = my.events.last();

	    // find the "slot" (y-axis value) for this identifier
	    // do not modify slot inside the loop (TODO: abstract
	    // the loop internals out into a separate routine?)
	    if (event.hasIdentifier()) {
		int childSlot = my.chart->getTraceSlot(event.spanID(), slot);
		event.setSlot(childSlot);
	    } else {
		event.setSlot(slot);
	    }

	    // this adds the basic point (ellipse), all events get one
	    my.points.append(QPointF(event.timestamp(), event.slot()));
	    my.chart->setTraceSlot(event.spanID(), event.slot());

	    if (event.hasParent()) {	// lookup parent in yMap
		parentSlot = my.chart->getTraceSlot(event.rootID(), parentSlot);
		// do this on start/end only?  (or if first?)
		my.drops.append(QwtIntervalSample(event.timestamp(),
				    QwtInterval(event.slot(), parentSlot)));
	    } else {
		parentSlot = -1;
	    }

	    console->post(PmChart::DebugForce, "TracingItem::updateEventRecords: "
		"[%.2f] span: %s (slot=%d) id=%s, root: %s (slot=%d,id=%s), start=%s end=%s",
		event.timestamp() - my.previousTimestamp,
		(const char *)event.spanID().toAscii(), event.slot(),
		event.hasIdentifier() ? "y" : "n",
		(const char *)event.rootID().toAscii(), parentSlot,
		event.hasParent() ? "y" : "n",
		event.hasStartFlag() ? "y" : "n", event.hasEndFlag() ? "y" : "n");
	    my.previousTimestamp = event.timestamp();

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

void TracingItem::setStroke(Chart::Style, QColor color, bool)
{
    console->post(PmChart::DebugForce, "TracingItem::setStroke");

    QPen outline(QColor(Qt::black));
    QColor darkColor(color);
    QColor alphaColor(color);

    darkColor.dark(180);
    alphaColor.setAlpha(196);
    QBrush alphaBrush(alphaColor);

    my.pointCurve->setLegendColor(color);

    my.spanSymbol->setWidth(6);
    my.spanSymbol->setBrush(alphaBrush);
    my.spanSymbol->setPen(outline);

    my.dropSymbol->setWidth(1);
    my.dropSymbol->setBrush(Qt::NoBrush);
    my.dropSymbol->setPen(outline);

    my.pointSymbol->setSize(8);
    my.pointSymbol->setColor(color);
    my.pointSymbol->setPen(outline);

    my.selectionSymbol->setSize(8);
    my.selectionSymbol->setColor(color.dark(180));
    my.selectionSymbol->setPen(outline);
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

    my.selections.append(my.points.at(index));

    // required for immediate chart update after selection
    QBrush pointBrush = my.pointSymbol->brush();
    my.pointSymbol->setBrush(my.selectionSymbol->brush());
    QwtPlotDirectPainter directPainter;
    directPainter.drawSeries(my.pointCurve, index, index);
    my.pointSymbol->setBrush(pointBrush);
}

void TracingItem::clearCursor(void)
{
    // immediately clear any current visible selections
    for (int index = 0; index < my.points.size(); index++) {
	QwtPlotDirectPainter directPainter;
	directPainter.drawSeries(my.pointCurve, index, index);
    }
    my.selections.clear();
}

//
// Display information text associated with selected events
//
const QString &TracingItem::cursorInfo(void)
{
    my.selectionInfo = QString::null;

    for (int i = 0; i < my.selections.size(); i++) {
	TracingEvent const &event = my.events.at(i);
	double stamp = event.timestamp();
	if (i)
	    my.selectionInfo.append("\n");
	my.selectionInfo.append(timeHiResString(stamp));
	my.selectionInfo.append(": ");
	my.selectionInfo.append(event.description());
    }
    return my.selectionInfo;
}

void TracingItem::revive(void)
{
    if (removed()) {
        setRemoved(false);
        my.dropCurve->attach(my.chart);
        my.spanCurve->attach(my.chart);
        my.pointCurve->attach(my.chart);
        my.selectionCurve->attach(my.chart);
    }
}

void TracingItem::remove(void)
{
    setRemoved(true);
    my.dropCurve->detach();
    my.spanCurve->detach();
    my.pointCurve->detach();
    my.selectionCurve->detach();
}

TracingScaleEngine::TracingScaleEngine(Chart *chart) : QwtLinearScaleEngine()
{
    my.chart = chart;
    setMargins(0.5, 0.5);
}

void
TracingScaleEngine::setScale(int minValue, int maxValue)
{
    console->post(PmChart::DebugForce, "TracingItem::setScale: min=%d max=%d",
		minValue, maxValue);
}

void
TracingScaleEngine::autoScale(int maxSteps, double &minValue,
                           double &maxValue, double &stepSize) const
{
    console->post(PmChart::DebugForce, "TracingItem::aut0Scale: maxSteps=%d min=%.2f max=%.2f sizeSteps=%.2f",
		maxSteps, minValue, maxValue, stepSize);
    maxSteps = 1;
    minValue = 0.0;
    maxValue = maxSteps * 1.0;
    stepSize = 1.0;
    console->post(PmChart::DebugForce, "TracingItem::autoScale: maxSteps=%d min=%.2f max=%.2f sizeSteps=%.2f",
		maxSteps, minValue, maxValue, stepSize);
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


//
// Use the hash map to provide event identifiers that map to given numeric IDs
// These values were mapped into the hash when we decoded the event records.
//
QwtText
TracingScaleDraw::label(double v) const
{
    int	slot = (int)v;
    console->post(PmChart::DebugForce, "TracingScaleDraw::label: lookup ID=%d", slot);
    return my.chart->getTraceID(slot);
}

void
TracingScaleDraw::getBorderDistHint(const QFont &f, int &start, int &end) const
{
    if (orientation() == Qt::Vertical) {
	start = 0;
	end = 0;
    }
    else {
	QwtScaleDraw::getBorderDistHint(f, start, end);
    }
}
