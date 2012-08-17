/*
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 2012 Nathan Scott.  All Rights Reserved.
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
#include <QtGui/QPainter>
#include <QtGui/QLabel>
#include <qwt_plot_layout.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_picker.h>
#include <qwt_double_rect.h>
#include <qwt_legend.h>
#include <qwt_legend_item.h>
#include <qwt_scale_widget.h>
#include <qwt_text.h>
#include <qwt_text_label.h>

#define DESPERATE 0

Chart::Chart(Tab *chartTab, QWidget *parent) : QwtPlot(parent), Gadget()
{
    Gadget::setWidget(this);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    plotLayout()->setCanvasMargin(0);
    plotLayout()->setAlignCanvasToScales(true);
    plotLayout()->setFixedAxisOffset(54, QwtPlot::yLeft);
    setAutoReplot(false);
    setMargin(1);
    setCanvasBackground(globalSettings.chartBackground);
    canvas()->setPaintAttribute(QwtPlotCanvas::PaintPacked, true);
    enableAxis(xBottom, false);

    setLegendVisible(true);
    legend()->contentsWidget()->setFont(*globalFont);
    connect(this, SIGNAL(legendChecked(QwtPlotItem *, bool)),
	    SLOT(showCurve(QwtPlotItem *, bool)));

    // start with autoscale y axis
    my.engine = new SamplingScaleEngine();
    setAxisAutoScale(QwtPlot::yLeft);
    setAxisScaleEngine(QwtPlot::yLeft, my.engine);
    setAxisFont(QwtPlot::yLeft, *globalFont);

    my.tab = chartTab;
    my.title = NULL;
    my.eventType = false;
    my.rateConvert = true;
    my.antiAliasing = true;
    my.style = NoStyle;
    my.scheme = QString::null;
    my.sequence = 0;
    my.picker = new QwtPlotPicker(QwtPlot::xBottom, QwtPlot::yLeft,
			QwtPicker::PointSelection | QwtPicker::DragSelection,
			QwtPlotPicker::CrossRubberBand, QwtPicker::AlwaysOff,
			canvas());
    my.picker->setRubberBandPen(QColor(Qt::green));
    my.picker->setRubberBand(QwtPicker::CrossRubberBand);
    my.picker->setTrackerPen(QColor(Qt::white));
    connect(my.picker, SIGNAL(selected(const QwtDoublePoint &)),
			 SLOT(selected(const QwtDoublePoint &)));
    connect(my.picker, SIGNAL(moved(const QwtDoublePoint &)),
			 SLOT(moved(const QwtDoublePoint &)));

    replot();

    console->post("Chart::ctor complete(%p)", this);
}

Chart::~Chart()
{
    console->post("Chart::~Chart() for chart %p", this);

    for (int m = 0; m < my.plots.size(); m++) {
	Plot *plot = my.plots[m];
	if (plot->data != NULL)
	    free(plot->data);
	if (plot->plotData != NULL)
	    free(plot->plotData);
	free(plot);
    }
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

void Chart::preserveLiveData(int i, int oi)
{
#if DESPERATE
    console->post("Chart::preserveLiveData %d/%d (%d plots)",
			i, oi, my.plots.size());
#endif

    if (my.plots.size() < 1)
	return;
    for (int m = 0; m < my.plots.size(); m++) {
	Plot *plot = my.plots[m];
	if (plot->dataCount > oi) {
	    plot->plotData[i] = plot->data[i] = plot->data[oi];
	}
	else {
	    plot->plotData[i] = plot->data[i] = SamplingCurve::NaN();
	}
    }
}

void Chart::punchoutLiveData(int i)
{
#if DESPERATE
    console->post("Chart::punchoutLiveData=%d (%d plots)", i, my.plots.size());
#endif

    if (my.plots.size() < 1)
	return;
    for (int m = 0; m < my.plots.size(); m++) {
	Plot *plot = my.plots[m];
	plot->data[i] = plot->plotData[i] = SamplingCurve::NaN();
    }
}

void Chart::adjustedLiveData()
{
    redoPlotData();
    replot();
}

void Chart::updateTimeAxis(double leftmost, double rightmost, double delta)
{
    setAxisScale(QwtPlot::xBottom, leftmost, rightmost, delta);
}

void Chart::updateValues(bool forward, bool visible)
{
    int		sh = my.tab->group()->sampleHistory();
    int		idx, m;

#if DESPERATE
    console->post(PmChart::DebugForce,
		  "Chart::updateValues(forward=%d,visible=%d) sh=%d (%d plots)",
		  forward, visible, sh, my.plots.size());
#endif

    if (my.plots.size() < 1)
	return;

    for (m = 0; m < my.plots.size(); m++) {
	Plot *plot = my.plots[m];

	double value;
	if (plot->metric->error(0))
	    value = SamplingCurve::NaN();
	else {
	    // convert raw value to current chart scale
	    pmAtomValue	raw;
	    pmAtomValue	scaled;
	    raw.d = my.rateConvert ?
			plot->metric->value(0) : plot->metric->currentValue(0);
	    pmConvScale(PM_TYPE_DOUBLE, &raw, &plot->units, &scaled, &my.units);
	    value = scaled.d * plot->scale;
	}

	int sz;
	if (plot->dataCount < sh)
	    sz = qMax(0, (int)(plot->dataCount * sizeof(double)));
	else
	    sz = qMax(0, (int)((plot->dataCount - 1) * sizeof(double)));

#if DESPERATE
	console->post(PmChart::DebugForce,
		"BEFORE Chart::update (%s) 0-%d (sz=%d,v=%.2f):",
		(const char *)plot->metric->name().toAscii(),
		plot->dataCount, sz, value);
	for (int i = 0; i < plot->dataCount; i++)
	    console->post("\t[%d] data=%.2f", i, plot->data[i]);
#endif

	if (forward) {
	    memmove(&plot->data[1], &plot->data[0], sz);
	    memmove(&plot->plotData[1], &plot->plotData[0], sz);
	    plot->data[0] = value;
	}
	else {
	    memmove(&plot->data[0], &plot->data[1], sz);
	    memmove(&plot->plotData[0], &plot->plotData[1], sz);
	    plot->data[plot->dataCount - 1] = value;
	}
	if (plot->dataCount < sh)
	    plot->dataCount++;

#if DESPERATE
	console->post(PmChart::DebugForce, "AFTER Chart::update (%s) 0-%d:",
			(const char *)plot->name.toAscii(), plot->dataCount);
	for (int i = 0; i < plot->dataCount; i++)
	    console->post(PmChart::DebugForce, "\t[%d] data=%.2f time=%s",
				i, plot->data[i],
				timeString(my.tab->timeAxisData()[i]));
#endif
    }

    if (my.style == BarStyle || my.style == AreaStyle || my.style == LineStyle) {
	idx = 0;
	for (m = 0; m < my.plots.size(); m++) {
	    if (!forward)
		idx = my.plots[m]->dataCount - 1;
	    my.plots[m]->plotData[idx] = my.plots[m]->data[idx];
	}
    }
    else if (my.style == UtilisationStyle) {
	// like Stack, but normalize value to a percentage (0,100)
	double sum = 0.0;
	idx = 0;
	// compute sum
	for (m = 0; m < my.plots.size(); m++) {
	    if (!forward)
		idx = my.plots[m]->dataCount - 1;
	    if (!SamplingCurve::isNaN(my.plots[m]->data[idx]))
		sum += my.plots[m]->data[idx];
	}
	// scale all components
	for (m = 0; m < my.plots.size(); m++) {
	    if (!forward)
		idx = my.plots[m]->dataCount - 1;
	    if (sum == 0 || my.plots[m]->hidden ||
		SamplingCurve::isNaN(my.plots[m]->data[idx]))
		my.plots[m]->plotData[idx] = SamplingCurve::NaN();
	    else
		my.plots[m]->plotData[idx] = 100 * my.plots[m]->data[idx] / sum;
	}
	// stack components
	sum = 0.0;
	for (m = 0; m < my.plots.size(); m++) {
	    if (!forward)
		idx = my.plots[m]->dataCount - 1;
	    if (!SamplingCurve::isNaN(my.plots[m]->plotData[idx])) {
		sum += my.plots[m]->plotData[idx];
		my.plots[m]->plotData[idx] = sum;
	    }
	}
    }
    else if (my.style == StackStyle) {
	double sum = 0.0;
	idx = 0;
	for (m = 0; m < my.plots.size(); m++) {
	    if (!forward)
		idx = my.plots[0]->dataCount - 1;
	    if (my.plots[m]->hidden || SamplingCurve::isNaN(my.plots[m]->data[idx])) {
		my.plots[m]->plotData[idx] = SamplingCurve::NaN();
	    }
	    else {
		sum += my.plots[m]->data[idx];
		my.plots[m]->plotData[idx] = sum;
	    }
	}
    }

#if DESPERATE
    for (m = 0; m < my.plots.size(); m++)
	console->post(PmChart::DebugForce, "metric[%d] value %f plot %f", m,
		my.plots[m]->metric->value(0), my.rateConvert ?
			plot->metric->value(0) : plot->metric->currentValue(0),
		my.plots[m]->plotData[0]);
#endif

    if (visible) {
	// replot() first so Value Axis range is updated
	replot();
	redoScale();
    }
}

void Chart::redoScale(void)
{
    bool	rescale = false;
    pmUnits	oldunits = my.units;
    int		m;

    // The 1,000 and 0.1 thresholds are just a heuristic guess.
    //
    // We're assuming lBound() plays no part in this, which is OK as
    // the upper bound of the y-axis range (hBound()) drives the choice
    // of appropriate units scaling.
    //
    if (my.engine->autoScale() &&
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

    if (rescale == false && my.engine->autoScale() &&
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
	pmAtomValue	old_av;
	pmAtomValue	new_av;

	console->post("Chart::update change units %s", pmUnitsStr(&my.units));
	// need to rescale ... we transform all of the historical (raw)
	// data[] and the plotData[] ... new data will be taken care of
	// by changing my.units
	//
	for (m = 0; m < my.plots.size(); m++) {
	    for (int i = my.plots[m]->dataCount-1; i >= 0; i--) {
		if (my.plots[m]->data[i] != SamplingCurve::NaN()) {
		    old_av.d = my.plots[m]->data[i];
		    pmConvScale(PM_TYPE_DOUBLE, &old_av, &oldunits, &new_av, &my.units);
		    my.plots[m]->data[i] = new_av.d;
		}
		if (my.plots[m]->plotData[i] != SamplingCurve::NaN()) {
		    old_av.d = my.plots[m]->plotData[i];
		    pmConvScale(PM_TYPE_DOUBLE, &old_av, &oldunits, &new_av, &my.units);
		    my.plots[m]->plotData[i] = new_av.d;
		}
	    }
	}
	if (my.style == UtilisationStyle) {
	    setYAxisTitle("% utilization");
	}
	else {
	    setYAxisTitle(pmUnitsStr(&my.units));
	}

	replot();
    }
}

void Chart::replot()
{
    int	vh = my.tab->group()->visibleHistory();

#if DESPERATE
    console->post("Chart::replot vh=%d, %d plots)", vh, my.plots.size());
#endif

    for (int m = 0; m < my.plots.size(); m++)
	my.plots[m]->curve->setRawData(my.tab->group()->timeAxisData(),
					my.plots[m]->plotData,
					qMin(vh, my.plots[m]->dataCount));
    QwtPlot::replot();
}

void Chart::showCurve(QwtPlotItem *item, bool on)
{
    item->setVisible(on);
    if (legend()) {
	QWidget *w = legend()->find(item);
	if (w && w->inherits("QwtLegendItem")) {
	    QwtLegendItem *li = (QwtLegendItem *)w;
	    li->setChecked(on);
	    li->setFont(*globalFont);
	}
    }
    // find matching plot and update hidden status if required
    for (int m = 0; m < my.plots.size(); m++) {
	if (item == my.plots[m]->curve) {
	    if (my.plots[m]->hidden == on) {
		// boolean sense is reversed here, on == true => show plot
		my.plots[m]->hidden = !on;
		redoPlotData();
	    }
	    break;
	}
    }
    replot();
}

void Chart::resetDataArrays(Plot *plot, int v)
{
    size_t size;

    // Reset sizes of pcp data array, the plot data array, and the time array
    size = v * sizeof(plot->data[0]);
    if ((plot->data = (double *)realloc(plot->data, size)) == NULL)
	nomem();
    size = v * sizeof(plot->plotData[0]);
    if ((plot->plotData = (double *)realloc(plot->plotData, size)) == NULL)
	nomem();
    if (plot->dataCount > v)
	plot->dataCount = v;
}

void Chart::resetDataArrays(int m, int v)
{
    // Reset sizes of pcp data array, the plot data array, and the time array
    resetDataArrays(my.plots[m], v);
}

// add a new plot
// the pmMetricSpec has been filled in, and ninst is always 0
// (PM_INDOM_NULL) or 1 (one instance at a time)
//
int Chart::addPlot(pmMetricSpec *pmsp, const char *legend)
{
    int		maxCount;
    QmcMetric	*mp;
    pmDesc	desc;

    console->post("Chart::addPlot src=%s", pmsp->source);
    if (pmsp->ninst == 0)
	console->post("addPlot metric=%s", pmsp->metric);
    else
	console->post("addPlot instance %s[%s]", pmsp->metric, pmsp->inst[0]);

    mp = my.tab->group()->addMetric(pmsp, 0.0, true);
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
	    // don't play with scaleTime, need native per plot scaleTime
	    // so we can apply correct scaling via plot->scale, e.g. in
	    // the msec -> msec/sec after rate conversion ... see the
	    // calculation for plot->scale below
	}
    }

    if (my.plots.size() == 0) {
	my.units = desc.units;
	my.eventType = (desc.type == PM_TYPE_EVENT);
	console->post("Chart::addPlot initial units %s", pmUnitsStr(&my.units));
    }
    else {
	// error reporting handled in caller
	if (checkCompatibleUnits(&desc.units) == false)
	    return PM_ERR_CONV;
	if (checkCompatibleTypes(desc.type) == false)
	    return PM_ERR_CONV;
    }

    Plot *plot = new Plot;
    my.plots.append(plot);
    console->post("addPlot plot=%p nplots=%d", plot, my.plots.size());

    plot->metric = mp;
    plot->name = QString(pmsp->metric);
    if (pmsp->ninst == 1)
	plot->name.append("[").append(pmsp->inst[0]).append("]");
    plot->units = desc.units;

    //
    // Build the legend label string, even if the chart is declared
    // "legend off" so that subsequent Edit->Chart Title and Legend
    // changes can turn the legend on and off dynamically
    //
    if (legend != NULL) {
	plot->legend = strdup(legend);
	plot->label = QString(legend);
    }
    else {
	plot->legend = NULL;
	if (plot->name.size() > PmChart::maximumLegendLength()) {
	    // show name as ...[end of name]
	    int		size;
	    plot->label = QString("...");
	    size = PmChart::maximumLegendLength() - 3;
	    plot->label.append(plot->name.right(size));
	}
	else
	    plot->label = plot->name;
    }

    // initialize the pcp data and plot data arrays
    plot->dataCount = 0;
    plot->data = NULL;
    plot->plotData = NULL;
    resetDataArrays(plot, my.tab->group()->sampleHistory());

    // create and attach the plot right here
    plot->curve = new SamplingCurve(plot->label);
    plot->curve->attach(this);

    // the 1000 is arbitrary ... just want numbers to be monotonic
    // decreasing as plots are added
    plot->curve->setZ(1000 - my.plots.size() - 1);

    // force plot to be visible, legend visibility is controlled by
    // legend() to a state matching the initial state
    showCurve(plot->curve, true);
    plot->removed = false;
    plot->hidden = false;

    // set the prevailing chart style and the default color
    setStroke(plot, my.style, nextColor(my.scheme, &my.sequence));

    maxCount = 0;
    for (int m = 0; m < my.plots.size(); m++)
	maxCount = qMax(maxCount, my.plots[m]->dataCount);
    // Set all the values for all plots from dataCount to maxCount to zero
    // so that the Stack <--> Line transitions work correctly
    for (int m = 0; m < my.plots.size(); m++) {
	for (int i = my.plots[m]->dataCount+1; i < maxCount; i++)
	    my.plots[m]->data[i] = 0;
	// don't re-set dataCount ... so we don't plot these values,
	// we just want them to count 0 towards any Stack aggregation
    }

    plot->scale = 1;
    if (desc.sem == PM_SEM_COUNTER && desc.units.dimTime == 0 &&
	my.style != UtilisationStyle) {
	// value to plot is time / time ... set scale
	if (desc.units.scaleTime == PM_TIME_USEC)
	    plot->scale = 0.000001;
	else if (desc.units.scaleTime == PM_TIME_MSEC)
	    plot->scale = 0.001;
    }

    return my.plots.size() - 1;
}

void Chart::revivePlot(int m)
{
    console->post("Chart::revivePlot=%d (%d)", m, my.plots[m]->removed);

    if (my.plots[m]->removed) {
	my.plots[m]->removed = false;
	my.plots[m]->curve->attach(this);
    }
}

void Chart::delPlot(int m)
{
    console->post("Chart::delPlot plot=%d", m);

    my.plots[m]->removed = true;
    my.plots[m]->curve->detach();

    // We can't really do this properly (free memory, etc) - working around
    // metrics class limit (its using an ordinal index for metrics, remove any
    // and we'll get problems.  Which means the plots array must also remain
    // unchanged, as we drive things via the metriclist at times.  D'oh.
    // This blows - it means we have to continue to fetch metrics for those
    // metrics that have been removed from the chart, which may be remote
    // hosts, hosts which are down (introducing retry issues...).  Bother.

    //delete my.plots[m]->curve;
    //delete my.plots[m]->label;
    //free(my.plots[m]->legend);
    //my.plots.removeAt(m);
}

int Chart::metricCount() const
{
    return my.plots.size();
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
    my.style = style;
}

void Chart::setStroke(int m, Style style, QColor color)
{
    if (m < 0 || m >= my.plots.size())
	abort();
    setStroke(my.plots[m], style, color);
}

bool Chart::isStepped(Plot *plot)
{
    int sem = plot->metric->desc().desc().sem;
    return (sem == PM_SEM_INSTANT || sem == PM_SEM_DISCRETE);
}

void Chart::setStroke(Plot *plot, Style style, QColor color)
{
    console->post("Chart::setStroke [style %d->%d]", my.style, style);

    setColor(plot, color);

    QPen p(color);
    p.setWidth(8);
    plot->curve->setLegendPen(p);
    plot->curve->setRenderHint(QwtPlotItem::RenderAntialiased, my.antiAliasing);

    switch (style) {
	case BarStyle:
	    plot->curve->setPen(color);
	    plot->curve->setBrush(QBrush(color, Qt::SolidPattern));
	    plot->curve->setStyle(QwtPlotCurve::Sticks);
	    if (my.style == UtilisationStyle)
		my.engine->setAutoScale(true);
	    break;

	case AreaStyle:
	    plot->curve->setPen(color);
	    plot->curve->setBrush(QBrush(color, Qt::SolidPattern));
	    plot->curve->setStyle(isStepped(plot) ?
				  QwtPlotCurve::Steps : QwtPlotCurve::Lines);
	    if (my.style == UtilisationStyle)
		my.engine->setAutoScale(true);
	    break;

	case UtilisationStyle:
	    plot->curve->setPen(QColor(Qt::black));
	    plot->curve->setStyle(QwtPlotCurve::Steps);
	    plot->curve->setBrush(QBrush(color, Qt::SolidPattern));
	    my.engine->setScale(false, 0.0, 100.0);
	    break;

	case LineStyle:
	    plot->curve->setPen(color);
	    plot->curve->setBrush(QBrush(Qt::NoBrush));
	    plot->curve->setStyle(isStepped(plot) ?
				  QwtPlotCurve::Steps : QwtPlotCurve::Lines);
	    if (my.style == UtilisationStyle)
		my.engine->setAutoScale(true);
	    break;

	case StackStyle:
	    plot->curve->setPen(QColor(Qt::black));
	    plot->curve->setBrush(QBrush(color, Qt::SolidPattern));
	    plot->curve->setStyle(QwtPlotCurve::Steps);
	    if (my.style == UtilisationStyle)
		my.engine->setAutoScale(true);
	    break;

	case NoStyle:
	default:
	    abort();
    }

    // This is really quite difficult ... a Utilisation plot by definition
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
    if (style == UtilisationStyle) {
	setYAxisTitle("% utilization");
    }
    else {
	setYAxisTitle(pmUnitsStr(&my.units));
    }

    if (style != my.style) {
	my.style = style;
	redoPlotData();
	replot();
    }
}

void Chart::redoPlotData(void)
{
    int		m;
    int		i;
    int		maxCount;
    double	sum;

    switch (my.style) {
	case BarStyle:
	case AreaStyle:
	case LineStyle:
	    for (m = 0; m < my.plots.size(); m++) {
		for (i = 0; i < my.plots[m]->dataCount; i++) {
		    my.plots[m]->plotData[i] = my.plots[m]->data[i];
		}
	    }
	    break;

	case UtilisationStyle:
	    maxCount = 0;
	    for (m = 0; m < my.plots.size(); m++)
		maxCount = qMax(maxCount, my.plots[m]->dataCount);
	    for (i = 0; i < maxCount; i++) {
		sum = 0.0;
		for (m = 0; m < my.plots.size(); m++) {
		    if (i < my.plots[m]->dataCount &&
			!SamplingCurve::isNaN(my.plots[m]->data[i]))
			sum += my.plots[m]->data[i];
		}
		for (m = 0; m < my.plots.size(); m++) {
		    if (sum == 0.0 || i >= my.plots[m]->dataCount || my.plots[m]->hidden ||
			SamplingCurve::isNaN(my.plots[0]->data[i]))
			my.plots[m]->plotData[i] = SamplingCurve::NaN();
		    else
			my.plots[m]->plotData[i] = 100 * my.plots[m]->data[i] / sum;
		}
		sum = 0.0;
		for (m = 0; m < my.plots.size(); m++) {
		    if (!SamplingCurve::isNaN(my.plots[m]->plotData[i])) {
			sum += my.plots[m]->plotData[i];
			my.plots[m]->plotData[i] = sum;
		    }
		}
	    }
	    break;

	case StackStyle:
	    maxCount = 0;
	    for (m = 0; m < my.plots.size(); m++)
		maxCount = qMax(maxCount, my.plots[m]->dataCount);
	    for (i = 0; i < maxCount; i++) {
		for (m = 0; m < my.plots.size(); m++) {
		    if (i >= my.plots[m]->dataCount || my.plots[m]->hidden)
			my.plots[m]->plotData[i] = SamplingCurve::NaN();
		    else
			my.plots[m]->plotData[i] = my.plots[m]->data[i];
		}
		sum = 0.0;
		for (m = 0; m < my.plots.size(); m++) {
		    if (!SamplingCurve::isNaN(my.plots[m]->plotData[i])) {
			sum += my.plots[m]->plotData[i];
			my.plots[m]->plotData[i] = sum;
		    }
		}
	    }
	    break;

	case NoStyle:
	    break;
    }
}

QColor Chart::color(int m)
{
    if (m >= 0 && m < my.plots.size())
	return my.plots[m]->color;
    return QColor("white");
}

void Chart::setColor(Plot *p, QColor c)
{
    p->color = c;
}

void Chart::setLabel(Plot *plot, QString s)
{
    plot->label = s;
}

void Chart::setLabel(int m, QString s)
{
    if (m >= 0 && m < my.plots.size())
	setLabel(my.plots[m], s);
}

void Chart::scale(bool *autoScale, double *yMin, double *yMax)
{
    *autoScale = my.engine->autoScale();
    *yMin = my.engine->minimum();
    *yMax = my.engine->maximum();
}

void Chart::setScale(bool autoScale, double yMin, double yMax)
{
    my.engine->setScale(autoScale, yMin, yMin);
    if (autoScale)
	setAxisAutoScale(QwtPlot::yLeft);
    else
	setAxisScale(QwtPlot::yLeft, yMin, yMax);
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

void Chart::selected(const QwtDoublePoint &p)
{
    console->post("Chart::selected chart=%p x=%f y=%f", this, p.x(), p.y());
    my.tab->setCurrent(this);
    QString string;
    string.sprintf("[%.2f %s at %s]",
		   (float)p.y(), pmUnitsStr(&my.units), timeHiResString(p.x()));
    pmchart->setValueText(string);
}

void Chart::moved(const QwtDoublePoint &p)
{
    console->post("Chart::moved chart=%p x=%f y=%f ", this, p.x(), p.y());
    QString string;
    string.sprintf("[%.2f %s at %s]",
		   (float)p.y(), pmUnitsStr(&my.units), timeHiResString(p.x()));
    pmchart->setValueText(string);
}

bool Chart::legendVisible()
{
    // Legend is on or off for all plots, only need to test the first plot
    if (my.plots.size() > 0)
	return legend() != NULL;
    return false;
}

// Clickable legend styled after the Qwt cpuplot demo.
// Use Edit->Chart Title and Legend to enable/disable the legend.
// Clicking on individual legend buttons will hide/show the
// corresponding plot (which is a nice feature over pmchart).
//
void Chart::setLegendVisible(bool on)
{
    console->post("Chart::setLegendVisible(%d) legend()=%p", on, legend());

    if (on) {
	if (legend() == NULL) {
	    // currently disabled, enable it
	    QwtLegend *l = new QwtLegend;
	    l->setItemMode(QwtLegend::CheckableItem);
	    l->setFrameStyle(QFrame::NoFrame);
	    l->setFrameShadow((Shadow)0);
	    l->setMidLineWidth(0);
	    l->setLineWidth(0);
	    insertLegend(l, QwtPlot::BottomLegend);
	    // force each Legend item to "checked" state matching
	    // the initial plotting state
	    for (int m = 0; m < my.plots.size(); m++) {
		showCurve(my.plots[m]->curve, !my.plots[m]->removed);
	    }
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
    QwtPlotPrintFilter filter;

    if (transparent)
	filter.setOptions(QwtPlotPrintFilter::PrintAll &
	    ~QwtPlotPrintFilter::PrintBackground &
	    ~QwtPlotPrintFilter::PrintGrid);
    else
	filter.setOptions(QwtPlotPrintFilter::PrintAll &
	    ~QwtPlotPrintFilter::PrintGrid);

    console->post("Chart::print: options=%d", filter.options());
    QwtPlot::print(qp, rect, filter);
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

QString Chart::name(int m) const
{
    return my.plots[m]->name;
}

char *Chart::legendSpec(int m) const
{
    return my.plots[m]->legend;
}

QmcDesc *Chart::metricDesc(int m) const
{
    return (QmcDesc *)&my.plots[m]->metric->desc();
}

QString Chart::metricName(int m) const
{
    return my.plots[m]->metric->name();
}

QString Chart::metricInstance(int m) const
{
    if (my.plots[m]->metric->numInst() > 0)
	return my.plots[m]->metric->instName(0);
    return QString::null;
}

QmcContext *Chart::metricContext(int m) const
{
    return my.plots[m]->metric->context();
}

QmcMetric *Chart::metric(int m) const
{
    return my.plots[m]->metric;
}

QSize Chart::minimumSizeHint() const
{
    return QSize(10,10);
}

QSize Chart::sizeHint() const
{
    return QSize(150,100);
}

void Chart::setupTree(QTreeWidget *tree)
{
    for (int i = 0; i < my.plots.size(); i++) {
	Plot *plot = my.plots[i];
	if (!plot->removed)
	    addToTree(tree, plot->name,
		      plot->metric->context(), plot->metric->hasInstances(), 
		      plot->color, plot->label);
    }
}

void Chart::addToTree(QTreeWidget *treeview, QString metric,
	const QmcContext *context, bool isInst, QColor &color, QString &label)
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

bool Chart::activePlot(int m)
{
    if (m >= 0 && m < my.plots.size())
	return (my.plots[m]->removed == false);
    return false;
}
