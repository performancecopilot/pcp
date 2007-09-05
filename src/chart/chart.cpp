/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2007, Aconex.  All Rights Reserved.
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
#include <pcp/pmc/Desc.h>
#include "main.h"

#include <QtCore/QPoint>
#include <QtCore/QRegExp>
#include <QtGui/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QLabel>
#include <qwt/qwt_plot_layout.h>
#include <qwt/qwt_plot_canvas.h>
#include <qwt/qwt_plot_curve.h>
#include <qwt/qwt_plot_picker.h>
#include <qwt/qwt_double_rect.h>
#include <qwt/qwt_legend.h>
#include <qwt/qwt_legend_item.h>
#include <qwt/qwt_scale_draw.h>
#include <qwt/qwt_scale_widget.h>
#include <qwt/qwt_text.h>
#include <qwt/qwt_text_label.h>

#define DESPERATE 0

// default colors for #-cycle in views and metric selection
QColor Chart::defaultColor(int seq)
{
    static int count;

    if (seq < 0)
	seq = count++;
    seq %= globalSettings.defaultColors.count();
    return globalSettings.defaultColors[seq];
}

Chart::Chart(Tab *chartTab, QWidget *parent) : QwtPlot(parent)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setMinimumSize(256, 128);
    setMaximumSize(32767, 32767);
    plotLayout()->setAlignCanvasToScales(true);
    setAutoReplot(false);
    setMargin(1);
    setCanvasBackground(globalSettings.chartBackground);
    canvas()->setPaintAttribute(QwtPlotCanvas::PaintPacked, true);
    enableAxis(xBottom, false);
    setLegendVisible(true);
    legend()->contentsWidget()->setFont(QFont("Sans", 8));
    connect(this, SIGNAL(legendChecked(QwtPlotItem *, bool)),
	    SLOT(showCurve(QwtPlotItem *, bool)));

    // start with autoscale y axis
    setAxisAutoScale(QwtPlot::yLeft);
    setAxisFont(QwtPlot::yLeft, QFont("Sans", 8));

    my.tab = chartTab;
    my.plots = NULL;
    my.title = NULL;
    my.autoScale = true;
    my.style = NoStyle;
    my.yMin = -1;
    my.yMax = -1;
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
}

Chart::~Chart()
{
    console->post("Chart::~Chart() for chart %p\n", this);

    if (my.plots != NULL) {
	for (int m = 0; m < (int)my.metrics.length(); m++) {
	    if (my.plots[m].data != NULL)
		free(my.plots[m].data);
	    if (my.plots[m].plotData != NULL)
		free(my.plots[m].plotData);
	}
	free(my.plots);
	my.plots = NULL;
    }
}

void Chart::update(bool forward, bool visible)
{
    int	sh = my.tab->sampleHistory();
    int	vh = my.tab->visibleHistory();
    int	idx, m;

#if DESPERATE
    console->post("Chart::update(forward=%d,vis=%d) sh=%d vh=%d (%d plots)\n",
			forward, visible, sh, vh, (int)my.metrics.length());
#endif

    if ((int)my.metrics.length() < 1)
	return;

    for (m = 0; m < (int)my.metrics.length(); m++) {
	double value = my.metrics[m]->value(0) * my.plots[m].scale;
	int sz;

	if (my.plots[m].dataCount < sh)
	    sz = qMax(0, (int)(my.plots[m].dataCount * sizeof(double)));
	else
	    sz = qMax(0, (int)((my.plots[m].dataCount - 1) * sizeof(double)));

#if DESPERATE
	console->post("BEFORE Chart::update (%s) 0-%d (sz=%d,v=%.2f):\n",
		my.metrics[m]->name().ptr(), my.plots[m].dataCount, sz, value);
	for (i = 0; i < my.plots[m].dataCount; i++)
	    console->post("\t[%d] data=%.2f\n", i, my.plots[m].data[i]);
#endif

	if (forward) {
	    memmove(&my.plots[m].data[1], &my.plots[m].data[0], sz);
	    memmove(&my.plots[m].plotData[1], &my.plots[m].plotData[0], sz);
	    my.plots[m].data[0] = value;
	}
	else {
	    memmove(&my.plots[m].data[0], &my.plots[m].data[1], sz);
	    memmove(&my.plots[m].plotData[0], &my.plots[m].plotData[1], sz);
	    my.plots[m].data[my.plots[m].dataCount - 1] = value;
	}
	if (my.plots[m].dataCount < sh)
	    my.plots[m].dataCount++;

#if DESPERATE
	console->post(KmChart::DebugApp, "AFTER Chart::update (%s) 0-%d:\n",
			my.metrics[m]->name().ptr(), my.plots[m].dataCount);
	for (int i = 0; i < plots[m].dataCount; i++)
	    console->post(KmChart::DebugApp, "\t[%d] data=%.2f time=%s\n",
				i, my.plots[m].data[i],
				timeString(my.tab->timeData()[i]));
#endif
    }

    if (my.style == BarStyle || my.style == AreaStyle) {
	if (forward)
	    for (m = 0; m < (int)my.metrics.length(); m++)
		my.plots[m].plotData[0] = my.plots[m].data[0];
	else
	    for (m = 0; m < (int)my.metrics.length(); m++) {
		idx = my.plots[m].dataCount - 1;
		my.plots[m].plotData[idx] = my.plots[m].data[idx];
	    }
    }
    else if (my.style == UtilisationStyle) {
	// like Stack, but normalize value to a percentage (0,100)
	double sum = 0;
	if (forward) {
	    for (m = 0; m < (int)my.metrics.length(); m++)
		sum += my.plots[m].data[0];
	    if (sum)
		for (m = 0; m < (int)my.metrics.length(); m++)
		    my.plots[m].plotData[0] = 100 * my.plots[m].data[0] / sum;
	    else	// avoid divide-by-zero
		for (m = 0; m < (int)my.metrics.length(); m++)
		    my.plots[m].plotData[0] = 0;
	    for (m = 1; m < (int)my.metrics.length(); m++)
		my.plots[m].plotData[0] += my.plots[m-1].plotData[0];
	}
	else {
	    for (m = 0; m < (int)my.metrics.length(); m++)
		sum += my.plots[m].data[my.plots[m].dataCount - 1];
	    if (sum)
		for (m = 0; m < (int)my.metrics.length(); m++) {
		    idx = my.plots[m].dataCount - 1;
		    my.plots[m].plotData[idx] =
					(100 * my.plots[m].data[idx] / sum);
		}
	    else	// avoid divide-by-zero
		for (m = 0; m < (int)my.metrics.length(); m++)
		    my.plots[m].plotData[my.plots[m].dataCount - 1] = 0;
	    for (m = 1; m < (int)my.metrics.length(); m++) {
		idx = my.plots[m].dataCount - 1;
		my.plots[m].plotData[idx] += my.plots[m-1].plotData[idx];
	    }
	}
    }
    else if (my.style == LineStyle) {
	if (forward)
	    for (m = 0; m < (int)my.metrics.length(); m++)
		my.plots[m].plotData[0] = my.plots[m].data[0];
	else
	    for (m = 0; m < (int)my.metrics.length(); m++) {
		idx = my.plots[m].dataCount - 1;
		my.plots[m].plotData[idx] = my.plots[m].data[idx];
	    }
    }
    else if (my.style == StackStyle) {
	// Stack, by adding values cummulatively
	// TODO -- here and everywhere else we stack (but not Util)
	// need to _skip_ any plots that are currently being hidden
	// due to legend pushbutton activity

	if (forward) {
	    my.plots[0].plotData[0] = my.plots[0].data[0];
	    for (m = 1; m < (int)my.metrics.length(); m++)
		my.plots[m].plotData[0] =
			my.plots[m].data[0] + my.plots[m-1].plotData[0];
	}
	else {
	    idx = my.plots[0].dataCount - 1;
	    my.plots[0].plotData[idx] = my.plots[0].data[idx];
	    for (m = 1; m < (int)my.metrics.length(); m++) {
		idx = my.plots[m].dataCount - 1;
		my.plots[m].plotData[idx] =
			my.plots[m].data[idx] + my.plots[m-1].plotData[idx];
	    }
	}
    }

    for (m = 0; m < (int)my.metrics.length(); m++) {
	my.plots[m].curve->setRawData(my.tab->timeAxisData(),
			my.plots[m].plotData, qMin(vh, my.plots[m].dataCount));
    }

#if DESPERATE
    for (m = 0; m < (int)my.metrics.length(); m++)
	console->post(KmChart::DebugApp, "metric[%d] value %f plot %f\n", m,
			my.metrics[m]->value(0), my.plots[m].plotData[0]);
#endif

    if (visible)
	replot();
}

void Chart::showCurve(QwtPlotItem *item, bool on)
{
    item->setVisible(on);
    if (legend()) {
	QWidget *w = legend()->find(item);
	if (w && w->inherits("QwtLegendItem"))
	    ((QwtLegendItem *)w)->setChecked(on);
    }
    replot();
}

void Chart::fixLegendPen()
{
    // Need to force the pen width in the legend back to 10 pixels
    // for every chart after every legend update ... Qwt provides
    // no way to make this sticky, and calls
    // QwtLegendItem::setCurvePen() from lots of places internally.
    // Ditto for the alignment and other visual effects for legend
    // buttons
    if (legendVisible()) {
	for (int m = 0; m < (int)my.metrics.length(); m++) {
	    QWidget *w = legend()->find(my.plots[m].curve);
	    if (w && w->inherits("QwtLegendItem")) {
		QwtLegendItem	*lip;
		QPen p;
		p = my.plots[m].curve->pen();
		p.setWidth(10);
		lip = (QwtLegendItem *)w;
		lip->setCurvePen(p);
	    }
	}
    }
}

void Chart::resetDataArrays(int m, int v)
{
    // Reset sizes of pcp data array, the plot data array, and the time array
    my.plots[m].data = (double *)realloc(my.plots[m].data,
					(v * sizeof(my.plots[m].data[0])));
    if (my.plots[m].data == NULL)
	nomem();
    my.plots[m].plotData = (double *)realloc(my.plots[m].plotData,
					(v * sizeof(my.plots[m].plotData[0])));
    if (my.plots[m].plotData == NULL)
	nomem();
    if (my.plots[m].dataCount > v)
	my.plots[m].dataCount = v;
}

// add a new plot
// the pmMetricSpec has been filled in, and ninst is always 0
// (PM_INDOM_NULL) or 1 (one instance at a time)
//
int Chart::addPlot(pmMetricSpec *pmsp, char *legend)
{
    int maxCount;
    PMC_Metric *mp;
    pmDesc desc;
    int m, size;

    console->post("Chart::addPlot src=%s", pmsp->source);
    if (pmsp->ninst == 0)
	console->post("addPlot metric=%s", pmsp->metric);
    else
	console->post("addPlot instance %s[%s]\n", pmsp->metric, pmsp->inst[0]);

    mp = my.tab->group()->addMetric(pmsp, 0.0, PMC_true);
    if (mp->status() < 0)
	return mp->status();
    desc = mp->desc().desc();

    maxCount = 0;
    for (m = 0; m < (int)my.metrics.length(); m++) {
	if (my.plots[m].dataCount > maxCount)
	    maxCount = my.plots[m].dataCount;
    }
    my.metrics.append(mp);
    size = my.metrics.length() * sizeof(my.plots[0]);
    console->post("addPlot metrics.length = %d (new size=%d)\n",
			(int)my.metrics.length(), size);
    my.plots = (Plot *)realloc(my.plots, size);
    if (my.plots == NULL)
	nomem();
    m = my.metrics.length() - 1;
    my.plots[m].name = new PMC_String(pmsp->metric);
    if (pmsp->ninst == 1) {
	my.plots[m].name->append("[");
	my.plots[m].name->append(pmsp->inst[0]);
	my.plots[m].name->append("]");
    }
    //
    // Build the legend label string, even if the chart is declared
    // "legend off" so that subsequent Edit->Chart Title and Legend
    // changes can turn the legend on and off dynamically
    //
    if (legend != NULL) {
	my.plots[m].legend = strdup(legend);
	my.plots[m].legendLabel = new PMC_String(legend);
    }
    else {
	my.plots[m].legend = NULL;
	if ((int)my.plots[m].name->length() > KmChart::maximumLegendLength) {
	    // show name as ...[end of name]
	    char *q = my.plots[m].name->ptr();
	    my.plots[m].legendLabel = new PMC_String("...");
	    size = my.plots[m].name->length() - KmChart::maximumLegendLength -3;
	    q = &q[size];
	    my.plots[m].legendLabel->append(q);
	}
	else
	    my.plots[m].legendLabel = new PMC_String(my.plots[m].name->ptr());
    }

    // initialize the pcp data and plot data arrays
    my.plots[m].dataCount = 0;
    my.plots[m].data = NULL;
    my.plots[m].plotData = NULL;
    resetDataArrays(m, my.tab->sampleHistory());

    // create and attach the plot right here
    my.plots[m].curve = new QwtPlotCurve(my.plots[m].legendLabel->ptr());
    my.plots[m].curve->attach(this);

    // the 1000 is arbitrary ... just want numbers to be monotonic
    // decreasing as plots are added
    my.plots[m].curve->setZ(1000-m);

    // force plot to be visible, legend visibility is controlled by
    // legend() to a state matching the initial state
    showCurve(my.plots[m].curve, true);
    my.plots[m].removed = false;

    // set default color ... may call setColor to change subsequently
    setColor(m, defaultColor(m));

    // set the prevailing chart style
    setStyle(my.style);

    fixLegendPen();

    // Set all the values for all plots from dataCount to maxCount to zero
    // so that the Stack <--> Line transitions work correctly
    for (int m = 0; m < (int)my.metrics.length(); m++) {
	for (int i = my.plots[m].dataCount+1; i < maxCount; i++)
	    my.plots[m].data[i] = 0;
	// don't re-set dataCount ... so we don't plot these values,
	// we just want them to count 0 towards any Stack aggregation
    }

    if (my.metrics.length() == 1) {
	// first plot, set y-axis title
	if (desc.sem == PM_SEM_COUNTER) {
	    if (desc.units.dimTime == 0) {
		if (my.style == UtilisationStyle)
		    setYAxisTitle("% utilization");
		else {
		    desc.units.dimTime = -1;
		    desc.units.scaleTime = PM_TIME_SEC;
		    setYAxisTitle((char *)pmUnitsStr(&desc.units));
		}
	    }
	    else if (desc.units.dimTime == 1) {
		if (my.style == UtilisationStyle)
		    setYAxisTitle("% time utilization");
		else
		    setYAxisTitle("time utilization");
	    }
	    else {
		// TODO -- rate conversion when units.dimTime != 0 ...
		// check what libpcp_pmc does with this, then make the
		// y axis label match
		if (my.style == UtilisationStyle)
		    setYAxisTitle("% utilization");
		else
		    setYAxisTitle((char *)pmUnitsStr(&desc.units));
	    }
	}
	else {
	    if (my.style == UtilisationStyle)
		setYAxisTitle("% utilization");
	    else
		setYAxisTitle((char *)pmUnitsStr(&desc.units));
	}
    }

    my.plots[m].scale = 1;
    if (desc.sem == PM_SEM_COUNTER && desc.units.dimTime == 1 &&
	my.style != UtilisationStyle) {
	// value to plot is time / time ... set scale
	if (desc.units.scaleTime == PM_TIME_USEC)
	    my.plots[m].scale = 0.000001;
	else if (desc.units.scaleTime == PM_TIME_MSEC)
	    my.plots[m].scale = 0.001;
    }

    return my.metrics.length() - 1;
}

void Chart::delPlot(int m)
{
    console->post("Chart::delPlot plot=%d\n", m);

    showCurve(my.plots[m].curve, false);
    legend()->remove(my.plots[m].curve);
    my.plots[m].removed = true;

    // We can't really do this properly (free memory, etc) - working around
    // libpcp_pmc limits (its using an ordinal index for metrics, remove any
    // and we'll get problems.  Which means the plots array must also remain
    // unchanged, as we drive things via the metriclist at times.  D'oh.
    // This blows - it means we have to continue to fetch metrics for those
    // metrics that have been removed from the chart, which may be remote
    // hosts, hosts which are down (introducing retry issues...).  Bother.

    //delete my.plots[m].curve;
    //delete my.plots[m].legendLabel;
    //free(my.plots[m].legend);
    //my.metrics.remove(m);
}

int Chart::numPlot()
{
    return my.metrics.length();
}

char *Chart::title()
{
    return my.title;
}

// expand is true to expand %h to host name in title
//
void Chart::changeTitle(char *title, int expand)
{
    if (my.title) {
	free(my.title);
	my.title = NULL;
    }
    if (title != NULL) {
	QwtText t = titleLabel()->text();
	t.setFont(QFont("Sans", 8));
	setTitle(t);
	my.title = strdup(title);

	// TODO: rewrite this using QString API, waay simpler
	char *w;
	if (expand && (w = strstr(title, "%h")) != NULL) {
	    // expand %h -> (short) hostname in title
	    char	*tmp;
	    char	*p;
	    char	*host;

	    tmp = (char *)malloc(MAXHOSTNAMELEN+strlen(title));
	    if (tmp == NULL)
		nomem();
	    *w = '\0';	// copy up to (but not including) the %
	    strcpy(tmp, title);
	    host = strdup((char *)activeSources->source());
	    if ((p = strchr(host, '.')) != NULL)
		*p = '\0';
	    strcat(tmp, host);
	    free(host);
	    w += 2;	// skip %h
	    strcat(tmp, w);
	    setTitle(tmp);
	    free(tmp);
	}
	else 
	    setTitle(my.title);
    }
    else
	setTitle(NULL);
}

void Chart::changeTitle(QString title, int expand)
{
    changeTitle((char *)(const char *)title.toAscii(), expand);
}

Chart::Style Chart::style()
{
    return my.style;
}

int Chart::setStyle(Style style)
{
    console->post("Chart::setStyle(%d) [was %d]\n", style, my.style);

    int maxCount = 0;
    for (int m = 0; m < (int)my.metrics.length(); m++)
	maxCount = qMax(maxCount, my.plots[m].dataCount);

    switch (style) {
	case BarStyle:
	    for (int m = 0; m < (int)my.metrics.length(); m++) {
		my.plots[m].curve->setStyle(QwtPlotCurve::Sticks);
		my.plots[m].curve->setBrush(Qt::NoBrush);
	    }
	    break;

	case AreaStyle:
	    for (int m = 0; m < (int)my.metrics.length(); m++) {
		my.plots[m].curve->setStyle(QwtPlotCurve::Lines);
		my.plots[m].curve->setBrush(QBrush(QColor(), Qt::SolidPattern));
	    }
	    break;

	case UtilisationStyle:
	    for (int m = 0; m < (int)my.metrics.length(); m++) {
		my.plots[m].curve->setStyle(QwtPlotCurve::Steps);
		my.plots[m].curve->setBrush(QBrush(QColor(), Qt::SolidPattern));
	    }
	    setScale(false, 0.0, 100.0);
	    if (my.style != UtilisationStyle) {
		// Need to redo the munging of plotData[]
		for (int i = maxCount-1; i >= 0; i--) {
		    double sum = 0;
		    for (int m = 0; m < (int)my.metrics.length(); m++) {
			if (my.plots[m].dataCount > i)
			    sum += my.plots[m].data[i];
		    }
		    for (int m = 0; m < (int)my.metrics.length(); m++) {
			if (sum != 0 && my.plots[m].dataCount > i)
			    my.plots[m].plotData[i] = 
						100 * my.plots[m].data[i] / sum;
			else
			    my.plots[m].plotData[i] = 0;
		    }
#ifdef STACK_BOTTOM_2_TOP
		    for (int m = 1; m < (int)my.metrics.length(); m++) {
			if (sum != 0 && my.plots[m].dataCount > i)
			    my.plots[m].plotData[i] +=
						my.plots[m-1].plotData[i];
		    }
#else
		    for (int m = (int)my.metrics.length()-2; m >= 0; m--) {
			if (sum != 0 && my.plots[m].dataCount > i)
			    my.plots[m].plotData[i] +=
						my.plots[m+1].plotData[i];
		    }
#endif
		}
	    }
	    break;

	case LineStyle:
	    for (int m = 0; m < (int)my.metrics.length(); m++) {
		my.plots[m].curve->setStyle(QwtPlotCurve::Lines);
		my.plots[m].curve->setBrush(Qt::NoBrush);
	    }
	    if (my.style != LineStyle) {
		// Need to undo any munging of plotData[]
		for (int m = 0; m < (int)my.metrics.length(); m++) {
		    for (int i = my.plots[m].dataCount-1; i >= 0; i--) {
			my.plots[m].plotData[i] = my.plots[m].data[i];
		    }
		}
	    }
	    break;

	case StackStyle:
	    for (int m = 0; m < (int)my.metrics.length(); m++) {
		my.plots[m].curve->setStyle(QwtPlotCurve::Steps);
		my.plots[m].curve->setBrush(QBrush(QColor(), Qt::SolidPattern));
	    }
	    if (my.style != StackStyle) {
		// Need to redo the munging of plotData[]
		for (int i = maxCount-1; i >= 0; i--) {
#ifdef STACK_BOTTOM_2_TOP
		    if (my.plots[0].dataCount > i)
			my.plots[0].plotData[i] = my.plots[0].data[i];
		    else
			my.plots[0].plotData[i] = 0;
		    for (int m = 1; m < (int)my.metrics.length(); m++) {
			if (my.plots[m].dataCount > i)
			    my.plots[m].plotData[i] = 
				my.plots[m].data[i] + my.plots[m-1].plotData[i];
			else
			    my.plots[m].plotData[i] = my.plots[m-1].plotData[i];
		    }
#else
		    if (my.plots[(int)my.metrics.length()-1].dataCount > i)
			my.plots[(int)my.metrics.length()-1].plotData[i] =
			    my.plots[(int)my.metrics.length()-1].data[i];
		    else
			my.plots[(int)my.metrics.length()-1].plotData[i] = 0;
		    for (int m = (int)my.metrics.length()-2; m >= 0; m--) {
			if (my.plots[m].dataCount > i)
			    my.plots[m].plotData[i] = 
				my.plots[m].data[i] + my.plots[m+1].plotData[i];
			else
			    my.plots[m].plotData[i] = my.plots[m+1].plotData[i];
		    }
#endif
		}
	    }
	    break;

	case NoStyle:
	default:
	    abort();
    }
    my.style = style;
    return 0;
}

QColor Chart::color(int m)
{
    if (m >= 0 && m < (int)my.metrics.length())
	return my.plots[m].color;
    return QColor("white");
}

int Chart::setColor(int m, QColor c)
{
    console->post("Chart::setColor(%d, r=%02x g=%02x b=%02x)\n",
			m, Qt::red, Qt::green, Qt::blue);
    if (m >= 0 && m < (int)my.metrics.length()) {
	my.plots[m].color = c;
	my.plots[m].curve->setPen(c);
	my.plots[m].curve->setBrush(c);
	return 0;
    }
    console->post("Chart::setColor - BAD metric index specified (%d)?\n", m);
    return -1;
}

void Chart::scale(bool *autoScale, double *yMin, double *yMax)
{
    *autoScale = my.autoScale;
    *yMin = my.yMin;
    *yMax = my.yMax;
}

void Chart::setScale(bool autoScale, double yMin, double yMax)
{
    my.yMin = yMin;
    my.yMax = yMax;
    my.autoScale = autoScale;
    if (my.autoScale)
	setAxisAutoScale(QwtPlot::yLeft);
    else
	setAxisScale(QwtPlot::yLeft, yMin, yMax);
}

void Chart::setYAxisTitle(char *p)
{
    QwtText *t;

    if (!p || *p == '\0')
	t = new QwtText(" ");	// for y-axis alignment (space is invisible)
    else
	t = new QwtText(p);
    t->setFont(QFont("Sans", 8));
    t->setColor("blue");
    setAxisTitle(QwtPlot::yLeft, *t);
}

void Chart::selected(const QwtDoublePoint &p)
{
    console->post("Chart::selected chart=%p x=%f y=%f\n",
			this, (float)p.x(), (float)p.y());
    my.tab->setCurrent(this);
}

void Chart::moved(const QwtDoublePoint &p)
{
    console->post("Chart::moved chart=%p x=%f y=%f\n",
			this, (float)p.x(), (float)p.y());
}

bool Chart::legendVisible()
{
    // Legend is on or off for all plots, only need to test the first plot
    if (my.metrics.length() > 0)
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
    console->post("Chart::setLegendVisible(%d) legend()=%p\n", on, legend());

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
	    for (int m = 0; m < (int)my.metrics.length(); m++) {
		showCurve(my.plots[m].curve, !my.plots[m].removed);
	    }
	}
    }
    else {
	QwtLegend *l = legend();
	if (l != NULL) {
	    // currently enabled, disable it
	    insertLegend(NULL, QwtPlot::BottomLegend);
	    // TODO: this can cause a core dump - needs investigating [memleak]
	    // delete l;
	}
    }
}

PMC_String *Chart::name(int m)
{
    return my.plots[m].name;
}

char *Chart::legendSpec(int m)
{
    return my.plots[m].legend;
}

PMC_Desc *Chart::metricDesc(int m)
{
    return (PMC_Desc *)&my.metrics[m]->desc();
}

PMC_String *Chart::metricName(int m)
{
    return (PMC_String *)&my.metrics[m]->name();
}

PMC_Context *Chart::metricContext(int m)
{
    return (PMC_Context *)&my.metrics[m]->context();
}

QString Chart::pmloggerMetricSyntax(int m)
{
    PMC_Metric *metric = my.metrics[m];
    QString syntax = metric->name().ptr();

    if (metric->numInst() == 1) {
	syntax.append(" [ \"");
	syntax.append(metric->indom()->name(0).ptr());
	syntax.append("\" ]");
    }
    return syntax;
}

QSize Chart::minimumSizeHint() const
{
    return QSize(10,10);	// TODO: hmm, seems pretty random?
}

QSize Chart::sizeHint() const
{
    return QSize(150,100);	// TODO: hmm, seems pretty random?
}

void Chart::setupTree(QTreeWidget *tree)
{
    tree->clear();
    for (uint i = 0; i < my.metrics.length(); i++) {
	if (!my.plots[i].removed)
	    addToTree(tree, tr(my.plots[i].name->ptr()),
		      &my.metrics[i]->context(), my.metrics[i]->hasInstances(), 
		      my.tab->isArchiveSource(), my.plots[i].color);
    }
}

void Chart::addToTree(QTreeWidget *treeview, QString metric,
	const PMC_Context *context, bool isInst, bool isArch, QColor &color)
{
    QRegExp regex(tr("\\.|\\[|\\]"));
    QString source = Source::makeSourceAnnotatedName(context);
    QStringList	baselist;

    console->post("Chart::addToTree src=%s metric=%s, isInst=%d isArch=%d\n",
		(const char *)source.toAscii(), (const char *)metric.toAscii(),
		isInst, isArch);

    baselist = metric.split(regex);
    baselist.prepend(source);	// add the host/archive root as well.

    // Walk through each component of this name, creating them in the
    // target tree (if not there already), right down to the leaf.

    NameSpace *tree = (NameSpace *)treeview->invisibleRootItem();
    QTreeWidgetItem *item = NULL;
    QStringList::Iterator blit;
    for (blit = baselist.begin(); blit != baselist.end(); ++blit) {
	QString text = *blit;
	for (int i = 0; i < tree->childCount(); i++) {
	    item = tree->child(i);
	    if (text == item->text(1)) {
		// no insert at this level necessary, move down a level
		tree = (NameSpace *)item;
		break;
	    }
	}

	/* when no more children and no match so far, we create & insert */
	if (!item) {
	    NameSpace *n;
	    if (blit == baselist.begin()) {
		n = new NameSpace(treeview, context, isArch);
		n->setExpandable(true);
		n->setSelectable(false);
	        n->setExpanded(true);
		n->setOpen(true);
	    }
	    else {
		bool isLeaf = (blit == baselist.end());
		n = new NameSpace(tree, text, isLeaf && isInst, isArch);
		if (isLeaf) {
		    n->setOriginalColor(color);
		    n->setCurrentColor(color, NULL);
		}
		n->setExpandable(!isLeaf);
		n->setSelectable(isLeaf);
	        n->setExpanded(true);
		n->setOpen(!isLeaf);
		if (!isLeaf)
		    n->setType(NameSpace::NonLeafName);
		else if (isInst)	// constructor sets Instance type
		    tree->setType(NameSpace::LeafWithIndom);
		else
		    n->setType(NameSpace::LeafNullIndom);
	    }
	    tree = n;
	}
    }
}
