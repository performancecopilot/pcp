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
#include <qwt_picker_machine.h>
#include <qwt_plot_marker.h>
#include <qwt_symbol.h>
#include "tracing.h"
#include "main.h"

#define DESPERATE 0

TracingItem::TracingItem(Chart *chart,
	QmcMetric *mp, pmMetricSpec *msp, pmDesc *dp, const QString &legend)
	: ChartItem(mp, msp, dp, legend)
{
    struct timeval	limit;
    my.chart = chart;
    my.minSpanID = 0;
    my.maxSpanID = 1;
    limit = mp->context()->source().start();
    my.minSpanTime = __pmtimevalToReal(&limit);
    if (mp->context()->source().isArchive()) {
	limit = mp->context()->source().end();
	my.maxSpanTime = __pmtimevalToReal(&limit);
    }
    else
	my.maxSpanTime = my.minSpanTime * 1.1;

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

QwtPlotItem *
TracingItem::item(void)
{
    return my.pointCurve;
}

QwtPlotCurve *
TracingItem::curve(void)
{
    return my.pointCurve;
}

TracingEvent::TracingEvent(QmcEventRecord const &record, pmID pmid, int inst)
{
    my.timestamp = __pmtimevalToReal(record.timestamp());
    my.missed = record.missed();
    my.flags = record.flags();
    my.pmid = pmid;
    my.inst = inst;
    my.spanID = record.identifier();
    my.rootID = record.parent();

    // details displayed about this record (on selection)
    my.description.append(timeHiResString(my.timestamp));
    my.description.append(": flags=");
    my.description.append(pmEventFlagsStr(record.flags()));
    if (record.missed()> 0) {
	my.description.append(" (");
	my.description.append(QString::number(record.missed()));
	my.description.append(" missed)");
    }
    my.description.append("\n");
    record.parameterSummary(my.description, inst);
}

TracingEvent::~TracingEvent()
{
    my.timestamp = 0.0;
}

void
TracingItem::rescaleValues(double *minValue, double *maxValue)
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
//

void
TracingItem::cullOutlyingSpans(double left, double right)
{
    // Start from the end so that we can remove as we go
    // without interfering with the index we are using.
    for (int i = my.spans.size() - 1; i >= 0; i--) {
	const QwtIntervalSample &span = my.spans.at(i);
	if (span.interval.maxValue() >= left ||
	    span.interval.minValue() <= right)
	    continue;
	my.spans.remove(i);
    }
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
TracingItem::resetValues(int, double left, double right)
{
    cullOutlyingSpans(left, right);
    cullOutlyingDrops(left, right);
    cullOutlyingPoints(left, right);
    cullOutlyingEvents(left, right);

    // update the display
    my.dropCurve->setSamples(my.drops);
    my.spanCurve->setSamples(my.spans);
    my.pointCurve->setSamples(my.points);
    my.selectionCurve->setSamples(my.selections);
}

//
// Requirement here is to merge in any event records that have
// just arrived in the last timestep (from my.metric) with the
// set already being displayed.
// Additionally, we need to cull those that are no longer within
// the time window of interest.
//
void
TracingItem::updateValues(TracingEngine *engine, double left, double right)
{
    QmcMetric *metric = ChartItem::my.metric;

#if DESPERATE
    console->post(PmChart::DebugForce, "TracingItem::updateValues: "
		"%d total events, left=%.2f right=%.2f\n"
		"Metadata counts: drops=%d spans=%d points=%d  "
		"Metric values: %d count\n",
		my.events.size(), left, right,
		my.drops.size(), my.spans.size(), my.points.size(), metric->numValues());
#endif

    cullOutlyingSpans(left, right);
    cullOutlyingDrops(left, right);
    cullOutlyingPoints(left, right);
    cullOutlyingEvents(left, right);

    // crack open newly arrived event records
    if (metric->numValues() > 0)
	updateEvents(engine, metric);

    // update the display
    my.dropCurve->setSamples(my.drops);
    my.spanCurve->setSamples(my.spans);
    my.pointCurve->setSamples(my.points);
    my.selectionCurve->setSamples(my.selections);
}

void
TracingItem::updateEvents(TracingEngine *engine, QmcMetric *metric)
{
    if (metric->hasIndom() && !metric->explicitInsts()) {
	for (int i = 0; i < metric->numInst(); i++)
	    updateEventRecords(engine, metric, i);
    } else {
	updateEventRecords(engine, metric, 0);
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
void
TracingItem::updateEventRecords(TracingEngine *engine, QmcMetric *metric, int index)
{
    if (metric->error(index) == 0) {
	QVector<QmcEventRecord> const &records = metric->eventRecords(index);
	int slot = 0, parentSlot = -1;
	QString name;

	// First lookup the event "slot" (aka "span" / y-axis-entry)
	// Strategy is to use the ID from the event (if one exists),
	// which must be mapped to a y-axis integer-based index.  If
	// no ID, we fall back to metric instance ID, else just zero.
	//
	if (metric->hasInstances()) {
	    if (metric->explicitInsts())
		index = 0;
	    slot = metric->instIndex(index);
	    name = metric->instName(index);
	} else {
	    name = metric->name();
	}
	addTraceSpan(engine, name, slot);

	for (int i = 0; i < records.size(); i++) {
	    QmcEventRecord const &record = records.at(i);

	    my.events.append(TracingEvent(record, metric->metricID(), index));
	    TracingEvent &event = my.events.last();

	    if (event.hasIdentifier() && name == QString::null) {
		addTraceSpan(engine, event.spanID(), slot);
	    }

	    // this adds the basic point (ellipse), all events get one
	    my.points.append(QPointF(event.timestamp(), slot));

	    parentSlot = -1;
	    if (event.hasParent()) {	// lookup parent in yMap
		parentSlot = engine->getTraceSpan(event.rootID(), parentSlot);
		if (parentSlot == -1)
		    addTraceSpan(engine, event.rootID(), parentSlot);
		// do this on start/end only?  (or if first?)
		my.drops.append(QwtIntervalSample(event.timestamp(),
				    QwtInterval(slot, parentSlot)));
	    }

#if DESPERATE
	    QString timestamp = QmcSource::timeStringBrief(record.timestamp());
	    console->post(PmChart::DebugForce, "TracingItem::updateEventRecords: "
		"[%s] span: %s (slot=%d) id=%s, root: %s (slot=%d,id=%s), start=%s end=%s",
		(const char *)timestamp.toAscii(),
		(const char *)event.spanID().toAscii(), slot,
		event.hasIdentifier() ? "y" : "n",
		(const char *)event.rootID().toAscii(), parentSlot,
		event.hasParent() ? "y" : "n",
		event.hasStartFlag() ? "y" : "n", event.hasEndFlag() ? "y" : "n");
#endif

	    if (event.hasStartFlag()) {
		if (!my.spans.isEmpty()) {
		    QwtIntervalSample &active = my.spans.last();
		    // did we get a start, then another start?
		    // (if so, just end the previous span now)
		    if (active.interval.maxValue() == my.maxSpanTime)
			active.interval.setMaxValue(event.timestamp());
		}
		// no matter what, we'll start a new span here
		my.spans.append(QwtIntervalSample(slot,
				    QwtInterval(event.timestamp(), my.maxSpanTime)));
	    }
	    if (event.hasEndFlag()) {
		if (!my.spans.isEmpty()) {
		    QwtIntervalSample &active = my.spans.last();
		    // did we get an end, then another end?
		    // (if so, move previous span end to now)
		    if (active.interval.maxValue() == my.maxSpanTime)
			active.interval.setMaxValue(event.timestamp());
		} else {
		    // got an end, but we haven't seen a start
		    my.spans.append(QwtIntervalSample(index,
				    QwtInterval(my.minSpanTime, event.timestamp())));
		}
	    }
	    // Have not yet handled missed events (i.e. event.missed())
	    // Could have a separate list of events? (render differently?)
	}
    } else {
#if DESPERATE
	//
	// TODO: need to track this failure point, and ensure that the
	// begin/end spans do not cross this boundary.
	//
	console->post(PmChart::DebugForce,
		"TracingItem::updateEventRecords: NYI error path: %d (%s)",
		metric->error(index), pmErrStr(metric->error(index)));
#endif
    }
}

void
TracingItem::addTraceSpan(TracingEngine *engine, const QString &span, int slot)
{
    double spanID = (double)slot;

    my.minSpanID = qMin(my.minSpanID, spanID);
    my.maxSpanID = qMax(my.maxSpanID, spanID);
    engine->addTraceSpan(span, slot);
}

void
TracingItem::setStroke(Chart::Style, QColor color, bool)
{
    const QColor black(Qt::black);
    const QPen outline(black);
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

bool
TracingItem::containsPoint(const QRectF &rect, int index)
{
    if (my.points.isEmpty())
	return false;
    return rect.contains(my.points.at(index));
}

void
TracingItem::updateCursor(const QPointF &, int index)
{
    Q_ASSERT(index <= my.points.size());
    Q_ASSERT(index <= (int)my.pointCurve->dataSize());

    my.selections.append(my.points.at(index));
    my.selectionInfo.append(my.events.at(index).description());

    // required for immediate chart update after selection
    QBrush pointBrush = my.pointSymbol->brush();
    my.pointSymbol->setBrush(my.selectionSymbol->brush());
    QwtPlotDirectPainter directPainter;
    directPainter.drawSeries(my.pointCurve, index, index);
    my.pointSymbol->setBrush(pointBrush);
}

void
TracingItem::clearCursor(void)
{
    // immediately clear any current visible selections
    for (int index = 0; index < my.selections.size(); index++) {
	QwtPlotDirectPainter directPainter;
	directPainter.drawSeries(my.pointCurve, index, index);
    }
    my.selections.clear();
    my.selectionInfo.clear();
}

//
// Display information text associated with selected events
//
const QString &
TracingItem::cursorInfo(void)
{
    if (my.selections.size() > 0) {
	QString preamble = metricName();
	if (metricHasInstances())
	    preamble.append("[").append(metricInstance()).append("]");
	preamble.append("\n");
	my.selectionInfo.prepend(preamble);
    }
    return my.selectionInfo;
}

void
TracingItem::revive(void)
{
    if (removed()) {
        setRemoved(false);
        my.dropCurve->attach(my.chart);
        my.spanCurve->attach(my.chart);
        my.pointCurve->attach(my.chart);
        my.selectionCurve->attach(my.chart);
    }
}

void
TracingItem::remove(void)
{
    setRemoved(true);
    my.dropCurve->detach();
    my.spanCurve->detach();
    my.pointCurve->detach();
    my.selectionCurve->detach();
}

void
TracingItem::redraw(void)
{
    if (removed() == false) {
	// point curve update by legend check, but not the rest:
	my.dropCurve->setVisible(hidden() == false);
	my.spanCurve->setVisible(hidden() == false);
	my.selectionCurve->setVisible(hidden() == false);
    }
}


TracingScaleEngine::TracingScaleEngine(TracingEngine *engine) : QwtLinearScaleEngine()
{
    my.engine = engine;
    my.minSpanID = 0.0;
    my.maxSpanID = 1.0;
    setMargins(0.5, 0.5);
}

void
TracingScaleEngine::getScale(double *minValue, double *maxValue)
{
    *minValue = my.minSpanID;
    *maxValue = my.maxSpanID;
}

void
TracingScaleEngine::setScale(double minValue, double maxValue)
{
    my.minSpanID = minValue;
    my.maxSpanID = maxValue;
}

bool
TracingScaleEngine::updateScale(double minValue, double maxValue)
{
    bool changed = false;

    if (minValue < my.minSpanID) {
	my.minSpanID = minValue;
	changed = true;
    }
    if (maxValue > my.maxSpanID) {
	my.maxSpanID = maxValue;
	changed = true;
    }
    return changed;
}

void
TracingScaleEngine::autoScale(int maxSteps, double &minValue,
                           double &maxValue, double &stepSize) const
{
    minValue = my.minSpanID;
    maxValue = my.maxSpanID;
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


//
// Use the hash map to provide event identifiers that map to given numeric IDs
// These values were mapped into the hash when we decoded the event records.
//
QwtText
TracingScaleDraw::label(double value) const
{
    int	slot = (int)value;
    const int LABEL_CUTOFF = 8;	// maximum width for label (units: characters)
    QString label = my.engine->getSpanLabel(slot);

#if DESPERATE
    console->post(PmChart::DebugForce,
		"TracingScaleDraw::label: lookup ID %d (=>\"%s\")",
		slot, (const char *)label.toAscii());
#endif

    // ensure label is not too long to fit
    label.truncate(LABEL_CUTOFF);
    // and only use up to the first space
    if ((slot = label.indexOf(' ')) >= 0)
	label.truncate(slot);
    return label;
}

void
TracingScaleDraw::getBorderDistHint(const QFont &f, int &start, int &end) const
{
    if (orientation() == Qt::Vertical)
	start = end = 0;
    else
	QwtScaleDraw::getBorderDistHint(f, start, end);
}


//
// The (chart-level) implementation of tracing charts
//
TracingEngine::TracingEngine(Chart *chart)
{
    QwtPlotPicker *picker = chart->my.picker;

    my.chart = chart;
    my.chart->my.style = Chart::EventStyle;

    my.scaleDraw = new TracingScaleDraw(this);
    chart->setAxisScaleDraw(QwtPlot::yLeft, my.scaleDraw);

    my.scaleEngine = new TracingScaleEngine(this);
    chart->setAxisScaleEngine(QwtPlot::yLeft, my.scaleEngine);

    // use a rectangular point picker for event tracing
    picker->setStateMachine(new QwtPickerDragRectMachine());
    picker->setRubberBand(QwtPicker::RectRubberBand);
    picker->setRubberBandPen(QColor(Qt::green));
}

ChartItem *
TracingEngine::addItem(QmcMetric *mp, pmMetricSpec *msp, pmDesc *desc, const QString &legend)
{
    return new TracingItem(my.chart, mp, msp, desc, legend);
}

TracingItem *
TracingEngine::tracingItem(int index)
{
    return (TracingItem *)my.chart->my.items[index];
}

void
TracingEngine::selected(const QPolygon &poly)
{
    my.chart->showPoints(poly);
}

void
TracingEngine::replot(void)
{
    for (int i = 0; i < my.chart->metricCount(); i++)
	tracingItem(i)->redraw();
}

void
TracingEngine::updateValues(bool, int, int, double left, double right, double)
{
    // Drive new values into each chart item
    for (int i = 0; i < my.chart->metricCount(); i++)
	tracingItem(i)->updateValues(this, left, right);
}

int
TracingEngine::getTraceSpan(const QString &spanID, int slot) const
{
    return my.traceSpanMapping.value(spanID, slot);
}

void
TracingEngine::addTraceSpan(const QString &spanID, int slot)
{
    Q_ASSERT(spanID != QString::null && spanID != "");
    console->post("TracingEngine::addTraceSpan: \"%s\" <=> slot %d (%d/%d span/label)",
			(const char *)spanID.toAscii(), slot,
			my.traceSpanMapping.size(), my.labelSpanMapping.size());
    my.traceSpanMapping.insert(spanID, slot);
    my.labelSpanMapping.insert(slot, spanID);
}

QString
TracingEngine::getSpanLabel(int slot) const
{
    return my.labelSpanMapping.value(slot);
}

void
TracingEngine::redoScale(void)
{
    double minValue, maxValue;

    my.scaleEngine->getScale(&minValue, &maxValue);

    for (int i = 0; i < my.chart->metricCount(); i++)
	tracingItem(i)->rescaleValues(&minValue, &maxValue);

    if (my.scaleEngine->updateScale(minValue, maxValue)) {
	my.scaleDraw->invalidate();
	replot();
    }
}

bool
TracingEngine::isCompatible(pmDesc &desc)
{
    return (desc.type == PM_TYPE_EVENT || desc.type == PM_TYPE_HIGHRES_EVENT);
}

void
TracingEngine::scale(bool *autoScale, double *yMin, double *yMax)
{
    *autoScale = true;
    my.scaleEngine->getScale(yMin, yMax);
}

void
TracingEngine::setScale(bool, double, double)
{
    my.chart->setAxisAutoScale(QwtPlot::yLeft);
}

void
TracingEngine::setStyle(Chart::Style)
{
    my.chart->setYAxisTitle("");
}
