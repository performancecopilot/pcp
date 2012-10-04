/*
 * Copyright (c) 2012, Red Hat.
 * Copyright (c) 2012, Nathan Scott.  All Rights Reserved.
 * Copyright (c) 2006-2010, Aconex.  All Rights Reserved.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
#include <qmc_desc.h>
#include "main.h"
#include "tracing.h"
#include "sampling.h"
#include "saveviewdialog.h"

#include <QtCore/QPoint>
#include <QtCore/QRegExp>
#include <QtGui/QApplication>
#include <QtGui/QWhatsThis>
#include <QtGui/QPainter>
#include <QtGui/QCursor>
#include <QtGui/QLabel>
#include <qwt_plot_layout.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_picker.h>
#include <qwt_picker_machine.h>
#include <qwt_plot_renderer.h>
#include <qwt_legend.h>
#include <qwt_legend_item.h>
#include <qwt_scale_widget.h>
#include <qwt_text.h>
#include <qwt_text_label.h>

#define DESPERATE 0

ChartItem::ChartItem(QmcMetric *mp,
		pmMetricSpec *msp, pmDesc *dp, const char *legend)
{
    my.metric = mp;
    my.units = dp->units;

    my.name = QString(msp->metric);
    if (msp->ninst == 1)
	my.name.append("[").append(msp->inst[0]).append("]");

    //
    // Build the legend label string, even if the chart is declared
    // "legend off" so that subsequent Edit->Chart Title and Legend
    // changes can turn the legend on and off dynamically
    //
    if (legend != NULL) {
	my.legend = strdup(legend);
	my.label = QString(legend);
    } else {
	my.legend = NULL;
	// show name as ...[end of name]
	if (my.name.size() > PmChart::maximumLegendLength()) {
	    int size = PmChart::maximumLegendLength() - 3;
	    my.label = QString("...");
	    my.label.append(my.name.right(size));
	} else {
	    my.label = my.name;
	}
    }

    my.removed = false;
    my.hidden = false;
}

Chart::Chart(Tab *chartTab, QWidget *parent) : QwtPlot(parent), Gadget()
{
    Gadget::setWidget(this);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    plotLayout()->setCanvasMargin(0);
    plotLayout()->setAlignCanvasToScales(true);
    plotLayout()->setFixedAxisOffset(54, QwtPlot::yLeft);
    setAutoReplot(false);
    setCanvasBackground(globalSettings.chartBackground);
    enableAxis(xBottom, false);

    setLegendVisible(true);
    legend()->contentsWidget()->setFont(*globalFont);
    connect(this, SIGNAL(legendChecked(QwtPlotItem *, bool)),
		    SLOT(legendChecked(QwtPlotItem *, bool)));

    my.tab = chartTab;
    my.title = NULL;
    my.eventType = false;
    my.rateConvert = true;
    my.antiAliasing = true;
    my.style = NoStyle;
    my.scheme = QString::null;
    my.sequence = 0;
    my.tracingScaleDraw = NULL;
    my.tracingScaleEngine = NULL;
    my.tracingPickerMachine = NULL;
    my.samplingScaleDraw = NULL;
    my.samplingScaleEngine = NULL;
    my.samplingPickerMachine = NULL;
    setAxisFont(QwtPlot::yLeft, *globalFont);
    setAxisAutoScale(QwtPlot::yLeft);
    setAxisScale(QwtPlot::yLeft, 0.0, 1.0, 0.0);
    setScaleEngine();

    my.picker = new QwtPlotPicker(QwtPlot::xBottom, QwtPlot::yLeft,
			QwtPicker::CrossRubberBand, QwtPicker::AlwaysOff,
			canvas());
    setPickerMachine();
    connect(my.picker, SIGNAL(selected(const QPolygon &)),
			 SLOT(selected(const QPolygon &)));
    connect(my.picker, SIGNAL(selected(const QPointF &)),
			 SLOT(selected(const QPointF &)));
    connect(my.picker, SIGNAL(moved(const QPointF &)),
			 SLOT(moved(const QPointF &)));

    replot();

    console->post("Chart::ctor complete(%p)", this);
}

void Chart::setScaleEngine()
{
    if (!my.samplingScaleDraw)	// stash the default Qwt made
	my.samplingScaleDraw = axisScaleDraw(QwtPlot::yLeft);

    if (my.eventType && !my.tracingScaleEngine) {
	if (my.tracingScaleDraw)
	    delete my.tracingScaleDraw;
	my.tracingScaleDraw = new TracingScaleDraw(this);
	setAxisScaleDraw(QwtPlot::yLeft, my.tracingScaleDraw);

	my.tracingScaleEngine = new TracingScaleEngine(this);
	setAxisScaleEngine(QwtPlot::yLeft, my.tracingScaleEngine);

	my.samplingScaleEngine = NULL;	// deleted in setAxisScaleEngine
    } else if (!my.eventType && !my.samplingScaleEngine) {
	setAxisScaleDraw(QwtPlot::yLeft, my.samplingScaleDraw);

	my.samplingScaleEngine = new SamplingScaleEngine();
	setAxisScaleEngine(QwtPlot::yLeft, my.samplingScaleEngine);

	my.tracingScaleEngine = NULL;	// deleted in setAxisScaleEngine
    }
}

void Chart::setPickerMachine()
{
    if (my.eventType && !my.tracingPickerMachine) {
	// use a rectangular point picker for event tracing
	my.tracingPickerMachine = new QwtPickerDragRectMachine();
	my.picker->setStateMachine(my.tracingPickerMachine);
	my.picker->setRubberBand(QwtPicker::RectRubberBand);
	my.picker->setRubberBandPen(QColor(Qt::green));
	my.samplingPickerMachine = NULL;
    } else if (!my.eventType && !my.samplingPickerMachine) {
	// use an individual point picker for sampled data
	my.samplingPickerMachine = new QwtPickerDragPointMachine();
	my.picker->setStateMachine(my.samplingPickerMachine);
	my.picker->setRubberBand(QwtPicker::CrossRubberBand);
	my.picker->setRubberBandPen(QColor(Qt::green));
	my.tracingPickerMachine = NULL;
    }
}

Chart::~Chart()
{
    console->post("Chart::~Chart() for chart %p", this);

    if (my.tracingScaleDraw)
	delete my.tracingScaleDraw;

    for (int i = 0; i < my.items.size(); i++)
	delete my.items[i];
    delete my.picker;
}

void Chart::setCurrent(bool enable)
{
    QwtScaleWidget *sp;
    QPalette palette;
    QwtText t;

    console->post("Chart::setCurrent(%p) %s", this, enable ? "true" : "false");

    // (Re)set title and y-axis highlight for new/old current chart.
    // For title, have to set both QwtText and QwtTextLabel because of
    // the way attributes are cached and restored when printing charts

    t = titleLabel()->text();
    t.setColor(enable ? globalSettings.chartHighlight : "black");
    setTitle(t);
    palette = titleLabel()->palette();
    palette.setColor(QPalette::Active, QPalette::Text,
		enable ? globalSettings.chartHighlight : QColor("black"));
    titleLabel()->setPalette(palette);

    sp = axisWidget(QwtPlot::yLeft);
    t = sp->title();
    t.setColor(enable ? globalSettings.chartHighlight : "black");
    sp->setTitle(t);
}

void Chart::preserveLiveData(int index, int oldindex)
{
#if DESPERATE
    console->post("Chart::preserveLiveData %d/%d (%d items)",
			index, oldindex, my.items.size());
#endif
    for (int i = 0; i < my.items.size(); i++)
	my.items[i]->preserveLiveData(index, oldindex);
}

void Chart::punchoutLiveData(int index)
{
#if DESPERATE
    console->post("Chart::punchoutLiveData=%d (%d items)",
			index, my.items.size());
#endif
    for (int i = 0; i < my.items.size(); i++)
	my.items[i]->punchoutLiveData(index);
}

void Chart::adjustValues(void)
{
    redoChartItems();
    replot();
}

SamplingItem *Chart::samplingItem(int index)
{
    Q_ASSERT(my.eventType == false);
    return (SamplingItem *)my.items[index];
}

TracingItem *Chart::tracingItem(int index)
{
    Q_ASSERT(my.eventType == true);
    return (TracingItem *)my.items[index];
}

void Chart::updateValues(bool forward, bool visible, int size, int points,
			 double left, double right, double delta)
{
    int		itemCount = my.items.size();
    int		i, index = forward ? 0 : -1;	/* first or last data point */

#if DESPERATE
    console->post(PmChart::DebugForce,
		  "Chart::updateValues(forward=%d,visible=%d) sz=%d pts=%d (%d items)",
		  forward, visible, size, points, itemCount);
#endif

    if (visible) {
	double scale = pmchart->timeAxis()->scaleValue(delta, points);
	setAxisScale(QwtPlot::xBottom, left, right, scale);
    }

    if (itemCount < 1)
	goto updated;

    for (i = 0; i < itemCount; i++)
	my.items[i]->updateValues(forward, my.rateConvert, &my.units, size, points, left, right, delta);

    if (my.style == BarStyle || my.style == AreaStyle || my.style == LineStyle) {
	for (i = 0; i < itemCount; i++)
	    samplingItem(i)->copyRawDataPoint(index);
    }
    // Utilisation: like Stack, but normalize value to a percentage (0,100)
    else if (my.style == UtilisationStyle) {
	double	sum = 0.0;
	// compute sum
	for (i = 0; i < itemCount; i++)
	    sum = samplingItem(i)->sumData(index, sum);
	// scale all components
	for (i = 0; i < itemCount; i++)
	    samplingItem(i)->setPlotUtil(index, sum);
	// stack components
	sum = 0.0;
	for (i = 0; i < itemCount; i++)
	    sum = samplingItem(i)->setPlotStack(index, sum);
    }
    else if (my.style == StackStyle) {
	double	sum = 0.0;
	for (i = 0; i < itemCount; i++)
	    sum = samplingItem(i)->setDataStack(index, sum);
    }

#if DESPERATE
    if (!my.eventType) {
	for (i = 0; i < my.items.size(); i++)
	    console->post(PmChart::DebugForce, "metric[%d] value %f",
		  i, samplingItem(i)->metric()->currentValue(0));
    }
#endif

updated:
    if (visible) {
	replot();	// done first so Value Axis range is updated
	redoScale();
    }
}

int Chart::getTraceSpan(const QString &spanID, int slot) const
{
    return my.traceSpanMapping.value(spanID, slot);
}

void Chart::addTraceSpan(const QString &spanID, int slot)
{
    Q_ASSERT(spanID != QString::null && spanID != "");
    Q_ASSERT(my.traceSpanMapping.size() == my.labelSpanMapping.size());

    console->post("Chart::addTraceSpan: \"%s\" <=> slot %d (%d total)",
			(const char *)spanID.toAscii(), slot, my.traceSpanMapping.size());
    my.traceSpanMapping.insert(spanID, slot);
    my.labelSpanMapping.insert(slot, spanID);
}

QString Chart::getSpanLabel(int slot) const
{
    return my.labelSpanMapping.value(slot);
}

void Chart::redoScale(void)
{
    if (my.eventType)
	redoTracingScale();
    else
	redoSamplingScale();
}

void Chart::redoTracingScale(void)
{
    double minValue = 0.0, maxValue = 1.0;

    Q_ASSERT(my.eventType == true);
    for (int i = 0; i < my.items.size(); i++)
	tracingItem(i)->rescaleValues(&minValue, &maxValue);
    my.tracingScaleEngine->setScale(minValue, maxValue);
    my.tracingScaleDraw->invalidate();
    replot();
}

void Chart::redoSamplingScale(void)
{
    bool	rescale = false;
    pmUnits	oldunits = my.units;

    Q_ASSERT(my.eventType == false);

    // The 1,000 and 0.1 thresholds are just a heuristic guess.
    //
    // We're assuming lBound() plays no part in this, which is OK as
    // the upper bound of the y-axis range (hBound()) drives the choice
    // of appropriate units scaling.
    //
    if (my.samplingScaleEngine->autoScale() &&
	axisScaleDiv(QwtPlot::yLeft)->upperBound() > 1000) {
	double scaled_max = axisScaleDiv(QwtPlot::yLeft)->upperBound();
	if (my.units.dimSpace == 1) {
	    switch (my.units.scaleSpace) {
		case PM_SPACE_BYTE:
		    my.units.scaleSpace = PM_SPACE_KBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_KBYTE:
		    my.units.scaleSpace = PM_SPACE_MBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_MBYTE:
		    my.units.scaleSpace = PM_SPACE_GBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_GBYTE:
		    my.units.scaleSpace = PM_SPACE_TBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_TBYTE:
		    my.units.scaleSpace = PM_SPACE_PBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_PBYTE:
		    my.units.scaleSpace = PM_SPACE_EBYTE;
		    rescale = true;
		    break;
	    }
	    if (rescale) {
		// logic here depends on PM_SPACE_* values being consecutive
		// integer values as the scale increases
		scaled_max /= 1024;
		while (scaled_max > 1000) {
		    my.units.scaleSpace++;
		    scaled_max /= 1024;
		    if (my.units.scaleSpace == PM_SPACE_EBYTE) break;
		}
	    }
	}
	else if (my.units.dimTime == 1) {
	    switch (my.units.scaleTime) {
		case PM_TIME_NSEC:
		    my.units.scaleTime = PM_TIME_USEC;
		    rescale = true;
		    scaled_max /= 1000;
		    break;
		case PM_TIME_USEC:
		    my.units.scaleTime = PM_TIME_MSEC;
		    rescale = true;
		    scaled_max /= 1000;
		    break;
		case PM_TIME_MSEC:
		    my.units.scaleTime = PM_TIME_SEC;
		    rescale = true;
		    scaled_max /= 1000;
		    break;
		case PM_TIME_SEC:
		    my.units.scaleTime = PM_TIME_MIN;
		    rescale = true;
		    scaled_max /= 60;
		    break;
		case PM_TIME_MIN:
		    my.units.scaleTime = PM_TIME_HOUR;
		    rescale = true;
		    scaled_max /= 60;
		    break;
	    }
	    if (rescale) {
		// logic here depends on PM_TIME* values being consecutive
		// integer values as the scale increases
		while (scaled_max > 1000) {
		    my.units.scaleTime++;
		    if (my.units.scaleTime <= PM_TIME_SEC)
			scaled_max /= 1000;
		    else
			scaled_max /= 60;
		    if (my.units.scaleTime == PM_TIME_HOUR) break;
		}
	    }
	}
    }

    if (rescale == false &&
	my.samplingScaleEngine->autoScale() &&
	axisScaleDiv(QwtPlot::yLeft)->upperBound() < 0.1) {
	double scaled_max = axisScaleDiv(QwtPlot::yLeft)->upperBound();
	if (my.units.dimSpace == 1) {
	    switch (my.units.scaleSpace) {
		case PM_SPACE_KBYTE:
		    my.units.scaleSpace = PM_SPACE_BYTE;
		    rescale = true;
		    break;
		case PM_SPACE_MBYTE:
		    my.units.scaleSpace = PM_SPACE_KBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_GBYTE:
		    my.units.scaleSpace = PM_SPACE_MBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_TBYTE:
		    my.units.scaleSpace = PM_SPACE_GBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_PBYTE:
		    my.units.scaleSpace = PM_SPACE_TBYTE;
		    rescale = true;
		    break;
		case PM_SPACE_EBYTE:
		    my.units.scaleSpace = PM_SPACE_PBYTE;
		    rescale = true;
		    break;
	    }
	    if (rescale) {
		// logic here depends on PM_SPACE_* values being consecutive
		// integer values (in reverse) as the scale decreases
		scaled_max *= 1024;
		while (scaled_max < 0.1) {
		    my.units.scaleSpace--;
		    scaled_max *= 1024;
		    if (my.units.scaleSpace == PM_SPACE_BYTE) break;
		}
	    }
	}
	else if (my.units.dimTime == 1) {
	    switch (my.units.scaleTime) {
		case PM_TIME_USEC:
		    my.units.scaleTime = PM_TIME_NSEC;
		    rescale = true;
		    scaled_max *= 1000;
		    break;
		case PM_TIME_MSEC:
		    my.units.scaleTime = PM_TIME_USEC;
		    rescale = true;
		    scaled_max *= 1000;
		    break;
		case PM_TIME_SEC:
		    my.units.scaleTime = PM_TIME_MSEC;
		    rescale = true;
		    scaled_max *= 1000;
		    break;
		case PM_TIME_MIN:
		    my.units.scaleTime = PM_TIME_SEC;
		    rescale = true;
		    scaled_max *= 60;
		    break;
		case PM_TIME_HOUR:
		    my.units.scaleTime = PM_TIME_MIN;
		    rescale = true;
		    scaled_max *= 60;
		    break;
	    }
	    if (rescale) {
		// logic here depends on PM_TIME* values being consecutive
		// integer values (in reverse) as the scale decreases
		while (scaled_max < 0.1) {
		    my.units.scaleTime--;
		    if (my.units.scaleTime < PM_TIME_SEC)
			scaled_max *= 1000;
		    else
			scaled_max *= 60;
		    if (my.units.scaleTime == PM_TIME_NSEC) break;
		}
	    }
	}
    }

    if (rescale) {
	//
	// need to rescale ... we transform all of the historical (raw)
	// data, new data will be taken care of by changing my.units.
	//
	for (int i = 0; i < my.items.size(); i++)
	    samplingItem(i)->rescaleValues(&my.units);
	if (my.style == UtilisationStyle)
	    setYAxisTitle("% utilization");
	else
	    setYAxisTitle(pmUnitsStr(&my.units));
	replot();
    }
}

void Chart::replot()
{
    if (my.eventType == false) {
	int     vh = my.tab->group()->visibleHistory();
	double *vp = my.tab->group()->timeAxisData();
	for (int i = 0; i < my.items.size(); i++)
	    samplingItem(i)->replot(vh, vp);
    }
    QwtPlot::replot();
}

void Chart::showItem(QwtPlotItem *item, bool visible)
{
    item->setVisible(visible);
    replot();
}

void Chart::legendChecked(QwtPlotItem *item, bool down)
{
#ifdef DESPERATE
    console->post(PmChart::DebugForce, "Chart::legendChecked %s for item %p",
		down? "down":"up", item);
#endif

    // find matching item and update hidden status if required
    for (int i = 0; i < my.items.size(); i++) {
	if (my.items[i]->item() != item)
	    continue;
	// if the state is changing, note it and update
	if (my.items[i]->hidden() != down) {
	    my.items[i]->setHidden(down);
	    redoChartItems();
	}
    }
    showItem(item, down == false);
}

void Chart::redoChartItems(void)
{
    int		i, m;
    int		maxCount = 0;
    int		itemCount = my.items.size();
    double	sum;

#if DESPERATE
    console->post(PmChart::DebugForce, "Chart::redoChartItems %d items)", itemCount);
#endif
    switch (my.style) {
	case BarStyle:
	case AreaStyle:
	case LineStyle:
	    for (i = 0; i < itemCount; i++)
		samplingItem(i)->copyRawDataArray();
	    break;

	case UtilisationStyle:
	    for (i = 0; i < itemCount; i++)
		maxCount = samplingItem(i)->maximumDataCount(maxCount);
	    for (m = 0; m < maxCount; m++) {
		sum = 0.0;
		for (i = 0; i < itemCount; i++)
		    sum = samplingItem(i)->sumData(m, sum);
		for (i = 0; i < itemCount; i++)
		    samplingItem(i)->setPlotUtil(m, sum);
		sum = 0.0;
		for (i = 0; i < itemCount; i++)
		    sum = samplingItem(i)->setPlotStack(m, sum);
	    }
	    break;

	case StackStyle:
	    for (i = 0; i < itemCount; i++)
		maxCount = samplingItem(i)->maximumDataCount(maxCount);
	    for (m = 0; m < maxCount; m++) {
		for (i = 0; i < itemCount; i++)
		    samplingItem(i)->copyDataPoint(m);
		sum = 0.0;
		for (i = 0; i < itemCount; i++)
		    sum = samplingItem(i)->setPlotStack(m, sum);
	    }
	    break;

	case EventStyle:
	case NoStyle:
	    break;
    }
}

// add a new chart item
// the pmMetricSpec has been filled in, and ninst is always 0
// (PM_INDOM_NULL) or 1 (one instance at a time)
//
int Chart::addItem(pmMetricSpec *msp, const char *legend)
{
    QmcMetric	*mp;
    pmDesc	desc;
    ChartItem	*item;

    console->post("Chart::addItem src=%s", msp->source);
    if (msp->ninst == 0)
	console->post("addItem metric=%s", msp->metric);
    else
	console->post("addItem instance %s[%s]", msp->metric, msp->inst[0]);

    mp = my.tab->group()->addMetric(msp, 0.0, true);
    if (mp->status() < 0)
	return mp->status();
    desc = mp->desc().desc();
    if (my.rateConvert && desc.sem == PM_SEM_COUNTER) {
	if (desc.units.dimTime == 0) {
	    desc.units.dimTime = -1;
	    desc.units.scaleTime = PM_TIME_SEC;
	}
	else if (desc.units.dimTime == 1) {
	    desc.units.dimTime = 0;
	    // don't play with scaleTime, need native per item scaleTime
	    // so we can apply correct scaling via item->scale, e.g. in
	    // the msec -> msec/sec after rate conversion ... see the
	    // calculation for item->scale below
	}
    }

    if (my.items.size() == 0) {
	my.units = desc.units;
	my.eventType = (desc.type == PM_TYPE_EVENT);
	my.style = my.eventType ? EventStyle : my.style;
	setPickerMachine();
	setScaleEngine();
    }
    else {
	// error reporting handled in caller
	if (checkCompatibleUnits(&desc.units) == false)
	    return PM_ERR_CONV;
	if (checkCompatibleTypes(desc.type) == false)
	    return PM_ERR_CONV;
    }

    if (my.eventType) {
	item = (ChartItem *)new TracingItem(this, mp, msp, &desc, legend);
    } else {
	int	i, size = 0;

	item = (ChartItem *)new SamplingItem(this, mp, msp, &desc, legend,
		my.style, my.tab->group()->sampleHistory(), my.items.size());
	// Find current max count for all plot items, then zero any plot
	// from there to the end, so that Stack<->Line transitions work.
	for (i = 0; i < my.items.size(); i++)
	    size = samplingItem(i)->maximumDataCount(size);
	for (i = 0; i < my.items.size(); i++)
	    samplingItem(i)->truncateData(size);
    }

    // set the prevailing chart style and default color, show it
    setStroke(item, my.style, nextColor(my.scheme, &my.sequence));
    showItem(item->item(), true);
    my.items.append(item);

    console->post("addItem %p nitems=%d", item, my.items.size());
    return my.items.size() - 1;
}

void Chart::reviveItem(int i)
{
    console->post("Chart::reviveItem=%d (%d)", i, my.items[i]->removed());
    my.items[i]->revive();
}

void Chart::removeItem(int i)
{
    console->post("Chart::removeItem=%d", i);
    my.items[i]->remove();
}

void Chart::resetValues(int i, int v)
{
    console->post("Chart::resetValues=%d (%d)", i, v);
    my.items[i]->resetValues(v);
}

int Chart::metricCount() const
{
    return my.items.size();
}

char *Chart::title()
{
    return my.title;
}

// expand is true to expand %h to host name in title
//
void Chart::changeTitle(char *title, int expand)
{
    bool hadTitle = (my.title != NULL);

    if (my.title) {
	free(my.title);
	my.title = NULL;
    }
    if (title != NULL) {
	if (hadTitle)
	    pmchart->updateHeight(titleLabel()->height());
	QwtText t = titleLabel()->text();
	t.setFont(*globalFont);
	setTitle(t);
	// have to set font for both QwtText and QwtTextLabel because of
	// the way attributes are cached and restored when printing charts
	QFont titleFont = *globalFont;
	titleFont.setBold(true);
	titleLabel()->setFont(titleFont);
	my.title = strdup(title);

	if (expand && (strstr(title, "%h")) != NULL) {
	    QString titleString = title;
	    QString shortHost = activeGroup->context()->source().host();
	    QStringList::Iterator host;

	    /* shorten hostname(s) - may be multiple (proxied) */
	    QStringList hosts = shortHost.split(QChar('@'));
	    for (host = hosts.begin(); host != hosts.end(); ++host) {
		/* decide whether or not to truncate this hostname */
		int dot = host->indexOf(QChar('.'));
		if (dot != -1)
		    /* no change if it looks even vaguely like an IP address */
		    if (!host->contains(QRegExp("^\\d+\\.")) &&	/* IPv4 */
			!host->contains(QChar(':')))		/* IPv6 */
			host->remove(dot, host->size());
	    }
	    host = hosts.begin();
	    shortHost = *host++;
	    for (; host != hosts.end(); ++host)
		shortHost.append(QString("@")).append(*host);
	    titleString.replace(QRegExp("%h"), shortHost);
	    setTitle(titleString);
	}
	else 
	    setTitle(my.title);
    }
    else {
	if (hadTitle)
	    pmchart->updateHeight(-(titleLabel()->height()));
	setTitle(NULL);
    }
}

void Chart::changeTitle(QString title, int expand)
{
    changeTitle((char *)(const char *)title.toAscii(), expand);
}

QString Chart::scheme() const
{
    return my.scheme;
}

void Chart::setScheme(QString scheme)
{
    my.sequence = 0;
    my.scheme = scheme;
}

int Chart::sequence()
{
    return my.sequence;
}

void Chart::setSequence(int sequence)
{
    my.sequence = sequence;
}

void Chart::setScheme(QString scheme, int sequence)
{
    my.sequence = sequence;
    my.scheme = scheme;
}

Chart::Style Chart::style()
{
    return my.style;
}

void Chart::setStyle(Style style)
{
    my.style = my.eventType ? EventStyle : style;
}

void Chart::setStroke(int index, Style style, QColor color)
{
    setStroke(my.items[index], style, color);
}

void Chart::setStroke(ChartItem *item, Style style, QColor color)
{
    item->setColor(color);
    item->setStroke(style, color, my.antiAliasing);

#if DESPERATE
    console->post(PmChart::DebugForce, "Chart::setStroke");
#endif

    // Y-Axis title choice is difficult.  A Utilisation plot by definition
    // is dimensionless and scaled to a percentage, so a label of just
    // "% utilization" makes sense ... there has been some argument in
    // support of "% time utilization" as a special case when the metrics
    // involve some aspect of time, but the base metrics in the common case
    // are counters in units of time (e.g. the CPU view), which after rate
    // conversion is indistinguishable from instantaneous or discrete
    // metrics of dimension time^0 which are units compatible ... so we're
    // opting for the simplest possible interpretation of utilization or
    // everything else.
    //
    if (style != my.style) {
	switch (style) {
	case BarStyle:
	case AreaStyle:
	case LineStyle:
	case StackStyle:
	    if (my.style == UtilisationStyle)
		my.samplingScaleEngine->setAutoScale(true);
	    setYAxisTitle(pmUnitsStr(&my.units));
	    break;
	case UtilisationStyle:
	    my.samplingScaleEngine->setScale(false, 0.0, 100.0);
	    setYAxisTitle("% utilization");
	    break;
	case EventStyle:
	case NoStyle:
	    setYAxisTitle("");
	    break;
	}
	my.style = style;
	adjustValues();
    }
}

QColor Chart::color(int index)
{
    if (index >= 0 && index < my.items.size())
	return my.items[index]->color();
    return QColor("white");
}

void Chart::setLabel(int index, QString s)
{
    if (index >= 0 && index < my.items.size())
	my.items[index]->setLabel(s);
}

void Chart::scale(bool *autoScale, double *yMin, double *yMax)
{
    if (my.eventType) {
	*autoScale = false;
	*yMin = 0.0;
	*yMax = 1.0;
    } else {
	*autoScale = my.samplingScaleEngine->autoScale();
	*yMin = my.samplingScaleEngine->minimum();
	*yMax = my.samplingScaleEngine->maximum();
    }
}

void Chart::setScale(bool autoScale, double yMin, double yMax)
{
    if (my.eventType) {
	setAxisAutoScale(QwtPlot::yLeft);
    } else {
	my.samplingScaleEngine->setScale(autoScale, yMin, yMin);
	if (autoScale)
	    setAxisAutoScale(QwtPlot::yLeft);
	else
	    setAxisScale(QwtPlot::yLeft, yMin, yMax);
    }
    replot();
    redoScale();
}

bool Chart::rateConvert()
{
    return my.rateConvert;
}

void Chart::setRateConvert(bool rateConvert)
{
    my.rateConvert = rateConvert;
}

void Chart::setYAxisTitle(const char *p)
{
    QwtText *t;
    bool enable = (my.tab->currentGadget() == this);

    if (!p || *p == '\0')
	t = new QwtText(" ");	// for y-axis alignment (space is invisible)
    else
	t = new QwtText(p);
    t->setFont(*globalFont);
    t->setColor(enable ? globalSettings.chartHighlight : "black");
    setAxisTitle(QwtPlot::yLeft, *t);
}

void Chart::selected(const QPolygon &poly)
{
    console->post(PmChart::DebugForce, "Chart::selected(polygon) chart=%p npoints=%d", this, poly.size());
    if (my.eventType)
	showPoints(poly);
    else
	showPoint(poly.point(0));
    my.tab->setCurrent(this);
}

void Chart::selected(const QPointF &p)
{
    console->post(PmChart::DebugForce, "Chart::selected(point) chart=%p x=%f y=%f", this, p.x(), p.y());
    showPoint(p);
    my.tab->setCurrent(this);
}

void Chart::moved(const QPointF &p)
{
    if (!my.eventType)
	showPoint(p);
}

#define	PICK_TOLERANCE	10	// #pixels tolerance

void Chart::showPoint(const QPointF &p)
{
    ChartItem *selected = NULL;
    double dist, distance = 10e10;
    int index = -1;

    // pixel point
    QPoint pp = my.picker->transform(p);	// XXX: problematic!

    console->post(PmChart::DebugForce, "Chart::showPoint p=%.2f,%.2f pixel=%d,%d",
			p.x(), p.y(), pp.x(), pp.y());

    // seek the closest curve to the point selected
    for (int i = 0; i < my.items.size(); i++) {
	QwtPlotCurve *curve = my.items[i]->curve();
	int point = curve->closestPoint(pp, &dist);

	if (dist < distance) {
	    index = point;
	    distance = dist;
	    selected = my.items[i];
	}
    }

    // clear existing selections then show this one
    for (int i = 0; i < my.items.size(); i++) {
	ChartItem *item = my.items[i];

	item->clearCursor();
	if (item == selected && dist < PICK_TOLERANCE)
	    item->updateCursor(p, index);
    }
}

void Chart::showPoints(const QPolygon &poly)
{
    Q_ASSERT(poly.size() == 2);

    console->post(PmChart::DebugForce, "Chart::showPoints: %d,%d -> %d,%d",
	poly.at(0).x(), poly.at(0).y(), poly.at(1).x(), poly.at(1).y());

    // Transform selected (pixel) points to our coordinate system
    QRectF cp = my.picker->invTransform(poly.boundingRect());
    const QPointF &p = cp.topLeft();

    //
    // If a single-point selected, use showPoint instead
    // (this uses proximity checking, not bounding box).
    //
    if (cp.width() == 0 && cp.height() == 0) {
	showPoint(p);
    }
    else {
	// clear existing selections, find and show new ones
	for (int i = 0; i < my.items.size(); i++) {
	    ChartItem *item = my.items[i];
	    int itemDataSize = item->curve()->dataSize();

	    item->clearCursor();
	    for (int index = 0; index < itemDataSize; index++)
		if (item->containsPoint(cp, index))
		    item->updateCursor(p, index);
	}
    }

    showInfo();
}

//
// give feedback (popup) about the selection
//
void Chart::showInfo(void)
{
    QString info = QString::null;

    for (int i = 0; i < my.items.size(); i++) {
	ChartItem *item = my.items[i];
	if (info != QString::null)
	    info.append("\n");
	info.append(item->cursorInfo());
    }

    if (info != QString::null)
	QWhatsThis::showText(QCursor::pos(), info, this);
    else
	QWhatsThis::hideText();
}

bool Chart::legendVisible()
{
    // Legend is on or off for all items, only need to test the first item
    if (my.items.size() > 0)
	return legend() != NULL;
    return false;
}

// Use Edit->Chart Title and Legend to enable/disable the legend.
// Clicking on individual legend buttons will hide/show the
// corresponding item.
//
void Chart::setLegendVisible(bool on)
{
    console->post("Chart::setLegendVisible(%d) legend()=%p", on, legend());

    if (on) {
	if (legend() == NULL) {	// currently disabled, enable it
	    QwtLegend *l = new QwtLegend;

	    l->setItemMode(QwtLegend::CheckableItem);
	    insertLegend(l, QwtPlot::BottomLegend);
	    // force each Legend item to "checked" state matching
	    // the initial plotting state
	    for (int i = 0; i < my.items.size(); i++)
		showItem(my.items[i]->item(), my.items[i]->removed());
	}
    }
    else {
	QwtLegend *l = legend();
	if (l != NULL) {
	    // currently enabled, disable it
	    insertLegend(NULL, QwtPlot::BottomLegend);
	    // WISHLIST: this can cause a core dump - needs investigating
	    // [memleak].  Really, all of the legend code needs reworking.
	    // delete l;
	}
    }
}

void Chart::save(FILE *f, bool hostDynamic)
{
    SaveViewDialog::saveChart(f, this, hostDynamic);
}

void Chart::print(QPainter *qp, QRect &rect, bool transparent)
{
    QwtPlotRenderer renderer;

    if (transparent)
	renderer.setDiscardFlag(QwtPlotRenderer::DiscardBackground);
    renderer.render(this, qp, rect);
}

bool Chart::antiAliasing()
{
    return my.antiAliasing;
}

void Chart::setAntiAliasing(bool on)
{
    console->post("Chart::setAntiAliasing [%d -> %d]", my.antiAliasing, on);
    my.antiAliasing = on;
}

QString Chart::name(int index) const
{
    return my.items[index]->name();
}

char *Chart::legendSpec(int index) const
{
    return my.items[index]->legendSpec();
}

QmcDesc *Chart::metricDesc(int index) const
{
    return my.items[index]->metricDesc();
}

QString Chart::metricName(int index) const
{
    return my.items[index]->metricName();
}

QString Chart::metricInstance(int index) const
{
    return my.items[index]->metricInstance();
}

QmcContext *Chart::metricContext(int index) const
{
    return my.items[index]->metricContext();
}

QmcMetric *Chart::metric(int index) const
{
    return my.items[index]->metric();
}

QSize Chart::minimumSizeHint() const
{
    return QSize(10,10);
}

QSize Chart::sizeHint() const
{
    return QSize(150,100);
}

bool Chart::checkCompatibleUnits(pmUnits *newUnits)
{
    console->post("Chart::check units plot units %s", pmUnitsStr(newUnits));
    if (my.units.dimSpace != newUnits->dimSpace ||
	my.units.dimTime != newUnits->dimTime ||
	my.units.dimCount != newUnits->dimCount)
	return false;
    return true;
}

bool Chart::checkCompatibleTypes(int newType)
{
    console->post("Chart::check plot event type %s", pmTypeStr(newType));
    if (my.eventType == true && newType != PM_TYPE_EVENT)
	return false;
    if (my.eventType == false && newType == PM_TYPE_EVENT)
	return false;
    return true;
}

bool Chart::activeItem(int index)
{
    if (index >= 0 && index < my.items.size())
	return (my.items[index]->removed() == false);
    return false;
}

void Chart::setupTree(QTreeWidget *tree)
{
    for (int i = 0; i < my.items.size(); i++) {
	ChartItem *item = my.items[i];

	if (item->removed())
	    continue;
	addToTree(tree, item->name(),
		  item->metricContext(), item->metricHasInstances(), 
		  item->color(), item->label());
    }
}

void Chart::addToTree(QTreeWidget *treeview, QString metric,
	const QmcContext *context, bool isInst, QColor color, QString label)
{
    QRegExp regexInstance("\\[(.*)\\]$");
    QRegExp regexNameNode(tr("\\."));
    QString source = context->source().source();
    QString inst, name = metric;
    QStringList	namelist;
    int depth;

    console->post("Chart::addToTree src=%s metric=%s, isInst=%d",
		(const char *)source.toAscii(), (const char *)metric.toAscii(),
		isInst);

    depth = name.indexOf(regexInstance);
    if (depth > 0) {
	inst = name.mid(depth+1);	// after '['
	inst.chop(1);			// final ']'
	name = name.mid(0, depth);	// prior '['
    }

    namelist = name.split(regexNameNode);
    namelist.prepend(source);	// add the host/archive root as well.
    if (depth > 0)
	namelist.append(inst);
    depth = namelist.size();

    // Walk through each component of this name, creating them in the
    // target tree (if not there already), right down to the leaf.

    NameSpace *tree = (NameSpace *)treeview->invisibleRootItem();
    NameSpace *item = NULL;

    for (int b = 0; b < depth; b++) {
	QString text = namelist.at(b);
	bool foundMatchingName = false;
	for (int i = 0; i < tree->childCount(); i++) {
	    item = (NameSpace *)tree->child(i);
	    if (text == item->text(0)) {
		// No insert at this level necessary, move down a level
		tree = item;
		foundMatchingName = true;
		break;
	    }
	}

	// When no more children and no match so far, we create & insert
	if (foundMatchingName == false) {
	    NameSpace *n;
	    if (b == 0) {
		n = new NameSpace(treeview, context);
		n->expand();
		n->setExpanded(true, true);
		n->setSelectable(false);
	    }
	    else {
		bool isLeaf = (b == depth-1);
		n = new NameSpace(tree, text, isLeaf && isInst);
		if (isLeaf) {
		    n->setLabel(label);
		    n->setOriginalColor(color);
		    n->setCurrentColor(color, NULL);
		}
		n->expand();
		n->setExpanded(!isLeaf, true);
		n->setSelectable(isLeaf);
		if (!isLeaf)
		    n->setType(NameSpace::NonLeafName);
		else if (isInst)	// Constructor sets Instance type
		    tree->setType(NameSpace::LeafWithIndom);
		else
		    n->setType(NameSpace::LeafNullIndom);
	    }
	    tree = n;
	}
    }
}


//
// Override behaviour from QwtPlotCurve legend rendering
// Gives us fine-grained control over the colour that we
// display in the legend boxes for each ChartItem.
//
void ChartCurve::drawLegendIdentifier(QPainter *painter, const QRectF &rect) const
{
    if (rect.isEmpty())
        return;

    QRectF r(0, 0, rect.width()-1, rect.height()-1);
    r.moveCenter(rect.center());

    QPen pen(QColor(Qt::black));
    pen.setCapStyle(Qt::FlatCap);
    QBrush brush(legendColor, Qt::SolidPattern);

    painter->setPen(pen);
    painter->setBrush(brush);
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->drawRect(r.x(), r.y(), r.width(), r.height());
}
