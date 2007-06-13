/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2007, Nathan Scott.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Ken McDonell, kenj At internode DoT on DoT net
 *                      Nathan Scott, nathans At debian DoT org
 */

#include <pcp/pmc/Desc.h>
#include "main.h"
#include <assert.h>

#include <qapplication.h>
#include <qvbox.h>
#include <qlabel.h>
#include <qpoint.h>
#include <qpainter.h>
#include <qregexp.h>
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

static void
nomem(void)
{
    // no point trying to report anything ... dump core is the best bet
    abort();
}

// default colors for #-cycle in views and metric selection
QColor Chart::defaultColor(int seq)
{
    static int count;

    if (seq < 0)
	seq = count++;
    seq %= settings.defaultColors.count();
    return settings.defaultColors[seq];
}

Chart::Chart(Tab *tab, QWidget *parent):
    QwtPlot(parent)
{
    _tab = tab;

    setFocusPolicy(QWidget::NoFocus);
    setSizePolicy(QSizePolicy::MinimumExpanding,
		  QSizePolicy::MinimumExpanding, false);
    setMinimumSize(256, 128);
    setMaximumSize(32767, 32767);
    plotLayout()->setAlignCanvasToScales(true);
    setAutoReplot(false);
    setMargin(1);
    setCanvasBackground(settings.chartBackground);
    canvas()->setPaintAttribute(QwtPlotCanvas::PaintPacked, true);
    enableXBottomAxis(false);
    setLegendVisible(true);
    legend()->contentsWidget()->setFont(QFont("Sans", 8));
    connect(this, SIGNAL(legendChecked(QwtPlotItem *, bool)),
	    SLOT(showCurve(QwtPlotItem *, bool)));

    // start with autoscale y axis
    setAxisAutoScale(QwtPlot::yLeft);
    setAxisFont(QwtPlot::yLeft, QFont("Sans", 8));

    replot();

    _plots = NULL;
    _title = NULL;
    _autoscale = TRUE;
    _style = None;
    _ymin = -1;
    _ymax = -1;
    _picker = new QwtPlotPicker(
	    QwtPlot::xBottom, QwtPlot::yLeft,
            QwtPicker::PointSelection,
	    QwtPlotPicker::CrossRubberBand,
	    QwtPicker::AlwaysOff,
	    canvas());
    _picker->setRubberBandPen(Qt::green);

    connect(_picker, SIGNAL(selected(const QwtDoublePoint &)),
	    SLOT(selected(const QwtDoublePoint &)));

    // TODO -- pmchart has right mouse events to expose the Metric
    // Value Information dialog ... need some alternative UI mechanism
    // to launch this

    // TODO -- not sure if the moved() signal is going to be useful
    // ... bode uses the window info area to provide a limited version
    // of pmchart's Metric Value Information
    connect(_picker, SIGNAL(moved(const QwtDoublePoint &)),
	    SLOT(moved(const QwtDoublePoint &)));

}

Chart::~Chart(void)
{
#if DESPERATE
    fprintf(stderr, "~Chart() called for this=%p\n", this);
#endif

    if (_plots != NULL) {
	// free() alloc'd memory
	unsigned int	m;
	
	for (m = 0; m < _metrics.length(); m++) {
	    if (_plots[m].data != NULL)
		free(_plots[m].data);
	    if (_plots[m].plot_data != NULL)
		free(_plots[m].plot_data);
	}
	free(_plots);
    }
}

#if DESPERATE
#define verbose_updates (_tab->isArchiveMode())
#else
#define verbose_updates	FALSE
#endif

void Chart::update(bool forward, bool visible)
{
    int	vh = _tab->visibleHist();

    if (verbose_updates)
	fprintf(stderr, "Chart::update(forward=%d,visible=%d) (%d plots)\n",
		    forward, visible, (int)_metrics.length());

    if ((int)_metrics.length() < 1) {
	fprintf(stderr, "Error in setting up this chart, nothing plotted\n");
	return;
    }

    for (int m = 0; m < (int)_metrics.length(); m++) {
	double	value = _metrics[m]->value(0) * _plots[m].scale;
	int	sz;

	if (_plots[m].dataCount < vh)
	    sz = max(0, (int)(_plots[m].dataCount * sizeof(double)));
	else
	    sz = max(0, (int)((_plots[m].dataCount - 1) * sizeof(double)));

	if (verbose_updates) {
	    extern char *timestring(double);
	    fprintf(stderr, "BEFORE Chart::update (%s) 0-%d (sz=%d,v=%.2f):\n",
		    _metrics[m]->name().ptr(), _plots[m].dataCount, sz, value);
	    for (int i = 0; i < _plots[m].dataCount; i++)
		fprintf(stderr, "\t[%d] data=%.2f\n", i, _plots[m].data[i]);
	}

	if (forward) {
	    memmove(&_plots[m].data[1], &_plots[m].data[0], sz);
	    memmove(&_plots[m].plot_data[1], &_plots[m].plot_data[0], sz);
	    _plots[m].data[0] = value;
	}
	else {
	    memmove(&_plots[m].data[0], &_plots[m].data[1], sz);
	    memmove(&_plots[m].plot_data[0], &_plots[m].plot_data[1], sz);
	    _plots[m].data[_plots[m].dataCount - 1] = value;
	}
	if (_plots[m].dataCount < vh)
	    _plots[m].dataCount++;

	if (verbose_updates) {
	    fprintf(stderr, "AFTER  Chart::update (%s) 0-%d:\n",
		    _metrics[m]->name().ptr(), _plots[m].dataCount);
	    for (int i = 0; i < _plots[m].dataCount; i++)
		fprintf(stderr, "\t[%d] data=%.2f time=%s\n",
			i, _plots[m].data[i], timestring(_tab->timeData()[i]));
	}
    }

    if (_style == Bar || _style == Area) {
	if (forward)
	    for (int m = 0; m < (int)_metrics.length(); m++)
		_plots[m].plot_data[0] = _plots[m].data[0];
	else
	    for (int m = 0; m < (int)_metrics.length(); m++) {
		int idx = _plots[m].dataCount - 1;
		_plots[m].plot_data[idx] = _plots[m].data[idx];
	    }
    }
    else if (_style == Util) {
	// like Stack, but normalize value to a percentage (0,100)
	double	sum = 0;
	if (forward) {
	    for (int m = 0; m < (int)_metrics.length(); m++)
		sum += _plots[m].data[0];
	    if (sum)
		for (int m = 0; m < (int)_metrics.length(); m++)
		    _plots[m].plot_data[0] = 100 * _plots[m].data[0] / sum;
	    else	// avoid divide-by-zero
		for (int m = 0; m < (int)_metrics.length(); m++)
		    _plots[m].plot_data[0] = 0;
	    for (int m = 1; m < (int)_metrics.length(); m++)
		_plots[m].plot_data[0] += _plots[m-1].plot_data[0];
	}
	else {
	    for (int m = 0; m < (int)_metrics.length(); m++)
		sum += _plots[m].data[_plots[m].dataCount - 1];
	    if (sum)
		for (int m = 0; m < (int)_metrics.length(); m++) {
		    int idx = _plots[m].dataCount - 1;
		    _plots[m].plot_data[idx] = 100 * _plots[m].data[idx] / sum;
		}
	    else	// avoid divide-by-zero
		for (int m = 0; m < (int)_metrics.length(); m++)
		    _plots[m].plot_data[_plots[m].dataCount - 1] = 0;
	    for (int m = 1; m < (int)_metrics.length(); m++) {
		int idx = _plots[m].dataCount - 1;
		_plots[m].plot_data[idx] += _plots[m-1].plot_data[idx];
	    }
	}
    }
    else if (_style == Line) {
	if (forward)
	    for (int m = 0; m < (int)_metrics.length(); m++)
		_plots[m].plot_data[0] = _plots[m].data[0];
	else
	    for (int m = 0; m < (int)_metrics.length(); m++) {
		int idx = _plots[m].dataCount - 1;
		_plots[m].plot_data[idx] = _plots[m].data[idx];
	    }
    }
    else if (_style == Stack) {
	// Stack, by adding values cummulatively
	// TODO -- here and everywhere else we stack (but not Util)
	// need to _skip_ any plots that are currently being hidden
	// due to legend pushbutton activity

	if (forward) {
	    _plots[0].plot_data[0] = _plots[0].data[0];
	    for (int m = 1; m < (int)_metrics.length(); m++)
		_plots[m].plot_data[0] =
			_plots[m].data[0] + _plots[m-1].plot_data[0];
	}
	else {
	    int idx = _plots[0].dataCount - 1;
	    _plots[0].plot_data[idx] = _plots[0].data[idx];
	    for (int m = 1; m < (int)_metrics.length(); m++) {
		idx = _plots[m].dataCount - 1;
		_plots[m].plot_data[idx] =
			_plots[m].data[idx] + _plots[m-1].plot_data[idx];
	    }
	}
    }

    for (int m = 0; m < (int)_metrics.length(); m++) {
	_plots[m].curve->setRawData(
		_tab->timeData(), _plots[m].plot_data, _plots[m].dataCount);
    }

    if (verbose_updates) {
	for (int m = 0; m < (int)_metrics.length(); m++)
	    fprintf(stderr, "metric[%d] value %f plot %f\n", m,
		    _metrics[m]->value(0), _plots[m].plot_data[0]);
    }

    if (visible)
	replot();
}

// same as cpuplot Qwt example
//
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

void Chart::fixLegendPen(void)
{
    // Need to force the pen width in the legend back to 10 pixels
    // for every chart after every legend update ... Qwt provides
    // no way to make this sticky, and calls
    // QwtLegendItem::setCurvePen() from lots of places internally.
    // Ditto for the alignment and other visual effects for legend
    // buttons
    if (legendVisible()) {
	for (int m = 0; m < (int)_metrics.length(); m++) {
	    QWidget *w = legend()->find(_plots[m].curve);
	    if (w && w->inherits("QwtLegendItem")) {
		QwtLegendItem	*lip;
		QPen p;
		p = _plots[m].curve->pen();
		p.setWidth(10);
		lip = (QwtLegendItem *)w;
		lip->setCurvePen(p);
#ifdef HACKED_QWT
		lip->setSunken(false);
#endif
	    }
	}
    }
}

// add a new plot
// the pmMetricSpec has been filled in, and ninst is always 0
// (PM_INDOM_NULL) or 1 (one instance at a time)
//
int Chart::addPlot(pmMetricSpec *pmsp, char *legend)
{
    int			maxCount;
    PMC_Metric		*mp;
    pmDesc		desc;
    int			m;

#if DESPERATE
    fprintf(stderr, "Chart::addPlot(s)");
    if (pmsp->isarch)
	fprintf(stderr, " archive %s/", pmsp->source);
    else
	fprintf(stderr, " host %s:", pmsp->source);
    if (pmsp->ninst == 0)
	fprintf(stderr, "%s\n", pmsp->metric);
    else
	fprintf(stderr, "%s[%s]\n", pmsp->metric, pmsp->inst[0]);
#endif

    mp = _tab->group()->addMetric(pmsp, 0.0, PMC_true);
    if (mp->status() < 0)
	return mp->status();
    desc = mp->desc().desc();

    maxCount = 0;
    for (m = 0; m < (int)_metrics.length(); m++) {
	if (_plots[m].dataCount > maxCount)
	    maxCount = _plots[m].dataCount;
    }
    _metrics.append(mp);
#if DESPERATE
    fprintf(stderr, "_metrics.length = %d\n", (int)_metrics.length());
#endif
    _plots = (plot_t *)realloc(_plots, _metrics.length()*sizeof(_plots[0]));
    if (_plots == NULL) nomem();
    m = _metrics.length() - 1;
    _plots[m].dataCount = 0;
    _plots[m].name = new PMC_String(pmsp->metric);
fprintf(stderr, "%s: NINST=%d\n", __func__, pmsp->ninst);
    if (pmsp->ninst == 1) {
	_plots[m].name->append("[");
	_plots[m].name->append(pmsp->inst[0]);
	_plots[m].name->append("]");
    }
fprintf(stderr, "%s: NAME=%s\n", __func__, _plots[m].name->ptr());
    // build the legend label string, even if the chart is declared
    // "legend off" so that subsequent Edit->Chart Title and Legend
    // changes can turn the legend on and off dynamically
    if (legend != NULL) {
	_plots[m].legend = strdup(legend);
	_plots[m].legend_label = new PMC_String(legend);
    }
    else {
	_plots[m].legend = NULL;
#define MAX_LEGEND_LEN 20
	if (_plots[m].name->length() > MAX_LEGEND_LEN) {
	    // show name as ...[end of name]
	    char	*q = _plots[m].name->ptr();
	    _plots[m].legend_label = new PMC_String("...");
	    q = &q[_plots[m].name->length() - MAX_LEGEND_LEN - 3];
	    _plots[m].legend_label->append(q);
	}
	else
	    _plots[m].legend_label = new PMC_String(_plots[m].name->ptr());
    }

    // initialize the pcp data and plot data arrays
    _plots[m].data = (double *)malloc(
			_tab->visibleHist() * sizeof(_plots[m].data[0]));
    if (_plots[m].data == NULL)
	nomem();
    _plots[m].plot_data = (double *)malloc(
			_tab->visibleHist() * sizeof(_plots[m].plot_data[0]));
    if (_plots[m].plot_data == NULL)
	nomem();

    // create and attach the plot right here
    _plots[m].curve = new QwtPlotCurve(_plots[m].legend_label->ptr());
    _plots[m].curve->attach(this);

    // the 1000 is arbitrary ... just want numbers to be monotonic
    // decreasing as plots are added
    _plots[m].curve->setZ(1000-m);

    // force plot to be visible, legend visibility is controlled by
    // legend() to a state matching the initial state
    showCurve(_plots[m].curve, TRUE);
    _plots[m].removed = FALSE;

    // set default color ... may call setColor to change subsequently
    setColor(m, defaultColor(m));

    // set the prevailing chart style
    setStyle(_style);

    fixLegendPen();

    // Set all the values for all plots from dataCount to maxCount to zero
    // so that the Stack <--> Line transitions work correctly
    for (int m = 0; m < (int)_metrics.length(); m++) {
	for (int i = _plots[m].dataCount+1; i < maxCount; i++)
	    _plots[m].data[i] = 0;
	// don't re-set dataCount ... so we don't plot these values,
	// we just want them to count 0 towards any Stack aggregation
    }

    if (_metrics.length() == 1) {
	// first plot, set y-axis title
	if (desc.sem == PM_SEM_COUNTER) {
	    if (desc.units.dimTime == 0) {
		if (_style == Util)
		    setYAxisTitle("% utilization");
		else {
		    desc.units.dimTime = -1;
		    desc.units.scaleTime = PM_TIME_SEC;
		    setYAxisTitle((char *)pmUnitsStr(&desc.units));
		}
	    }
	    else if (desc.units.dimTime == 1) {
		if (_style == Util)
		    setYAxisTitle("% time utilization");
		else
		    setYAxisTitle("time utilization");
	    }
	    else {
		// TODO -- rate conversion when units.dimTime != 0 ...
		// check what libpcp_pmc does with this, then make the
		// y axis label match
		if (_style == Util)
		    setYAxisTitle("% utilization");
		else
		    setYAxisTitle((char *)pmUnitsStr(&desc.units));
	    }
	}
	else {
	    if (_style == Util)
		setYAxisTitle("% utilization");
	    else
		setYAxisTitle((char *)pmUnitsStr(&desc.units));
	}
    }

    _plots[m].scale = 1;
    if (desc.sem == PM_SEM_COUNTER && desc.units.dimTime == 1 &&
	_style != Util) {
	// value to plot is time / time ... set scale
	if (desc.units.scaleTime == PM_TIME_USEC)
	    _plots[m].scale = 0.000001;
	else if (desc.units.scaleTime == PM_TIME_MSEC)
	    _plots[m].scale = 0.001;
    }

    return(_metrics.length()-1);
}

void Chart::delPlot(int m)
{
fprintf(stderr, "%s: needs verification (plot %d)\n", __func__, m);

    showCurve(_plots[m].curve, FALSE);
    legend()->remove(_plots[m].curve);
    _plots[m].removed = TRUE;

    // We can't really do this properly (free memory, etc) - working around
    // libpcp_pmc limits (its using an ordinal index for metrics, remove any
    // and we'll get problems.  Which means the _plots array must also remain
    // unchanged, as we drive things via the metriclist at times.  D'oh.
    // This blows - it means we have to continue to fetch metrics for those
    // metrics that have been removed from the chart.

    //delete _plots[m].curve;
    //delete _plots[m].legend_label;
    //free(_plots[m].legend);
    //_metrics.remove(m);
}

int Chart::numPlot(void)
{
    return _metrics.length();
}

char *Chart::title(void)
{
    return _title;
}

// expand is true to expand %h to host name in title
//
void Chart::changeTitle(char *title, int expand)
{
    if (_title) {
	free(_title);
	_title = NULL;
    }
    if (title != NULL) {
	QwtText		t;
	char		*w;

	t = titleLabel()->text();
	t.setFont(QFont("Sans", 8));
	setTitle(t);
	_title = strdup(title);

	if (expand && (w = strstr(title, "%h")) != NULL) {
	    // expand %h -> (short) hostname in title
	    char	*tmp;
	    char	*p;
	    char	*host;

	    tmp = (char *)malloc(MAXHOSTNAMELEN+strlen(title));
	    if (tmp == NULL) nomem();
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
	    setTitle(title);

    }
    else
	setTitle(NULL);

}

chartStyle Chart::style(void)
{
    return _style;
}

int Chart::setStyle(chartStyle style)
{
    int			maxCount;
    double		sum;
#if DESPERATE
    fprintf(stderr, "Chart::setStyle(%d) [was %d]\n", style, _style);
#endif
    maxCount = 0;
    for (int m = 0; m < (int)_metrics.length(); m++) {
	if (_plots[m].dataCount > maxCount)
	    maxCount = _plots[m].dataCount;
    }

    switch (style) {
	case None:
	    fprintf(stderr, "Chart::setStyle: plot style None botch!\n");
	    abort();
	    /*NOTREACHED*/

	case Bar:
	    // TODO -- not supported yet ... error status?
	    for (int m = 0; m < (int)_metrics.length(); m++) {
		_plots[m].curve->setStyle(QwtPlotCurve::Sticks);
		_plots[m].curve->setBrush(QBrush::NoBrush);
	    }
	    break;

	case Area:
	    for (int m = 0; m < (int)_metrics.length(); m++) {
		_plots[m].curve->setStyle(QwtPlotCurve::Lines);
		_plots[m].curve->setBrush(QBrush(QColor(), Qt::SolidPattern));
	    }
	    break;

	case Util:
	    for (int m = 0; m < (int)_metrics.length(); m++) {
		_plots[m].curve->setStyle(QwtPlotCurve::Steps);
		_plots[m].curve->setBrush(QBrush(QColor(), Qt::SolidPattern));
	    }
	    setScale(FALSE, 0.0, 100.0);
	    if (_style != Util) {
		// Need to redo the munging of plot_data[]
		for (int i = maxCount-1; i >= 0; i--) {
		    sum = 0;
		    for (int m = 0; m < (int)_metrics.length(); m++) {
			if (_plots[m].dataCount > i)
			    sum += _plots[m].data[i];
		    }
		    for (int m = 0; m < (int)_metrics.length(); m++) {
			if (sum != 0 && _plots[m].dataCount > i)
			    _plots[m].plot_data[i] = 100 * _plots[m].data[i] / sum;
			else
			    _plots[m].plot_data[i] = 0;
		    }
#ifdef STACK_BOTTOM_2_TOP
		    for (int m = 1; m < (int)_metrics.length(); m++) {
			if (sum != 0 && _plots[m].dataCount > i)
			    _plots[m].plot_data[i] += _plots[m-1].plot_data[i];
		    }
#else
		    for (int m = (int)_metrics.length()-2; m >= 0; m--) {
			if (sum != 0 && _plots[m].dataCount > i)
			    _plots[m].plot_data[i] += _plots[m+1].plot_data[i];
		    }
#endif
		}
	    }
	    break;

	case Line:
	    for (int m = 0; m < (int)_metrics.length(); m++) {
		_plots[m].curve->setStyle(QwtPlotCurve::Lines);
		_plots[m].curve->setBrush(QBrush::NoBrush);
	    }
	    if (_style != Line) {
		// Need to undo any munging of plot_data[]
		for (int m = 0; m < (int)_metrics.length(); m++) {
		    for (int i = _plots[m].dataCount-1; i >= 0; i--) {
			_plots[m].plot_data[i] = _plots[m].data[i];
		    }
		}
	    }
	    break;

	case Stack:
	    for (int m = 0; m < (int)_metrics.length(); m++) {
		_plots[m].curve->setStyle(QwtPlotCurve::Steps);
		_plots[m].curve->setBrush(QBrush(QColor(), Qt::SolidPattern));
	    }
	    if (_style != Stack) {
		// Need to redo the munging of plot_data[]
		for (int i = maxCount-1; i >= 0; i--) {
#ifdef STACK_BOTTOM_2_TOP
		    if (_plots[0].dataCount > i)
			_plots[0].plot_data[i] = _plots[0].data[i];
		    else
			_plots[0].plot_data[i] = 0;
		    for (int m = 1; m < (int)_metrics.length(); m++) {
			if (_plots[m].dataCount > i)
			    _plots[m].plot_data[i] = 
				_plots[m].data[i] + _plots[m-1].plot_data[i];
			else
			    _plots[m].plot_data[i] = _plots[m-1].plot_data[i];
		    }
#else
		    if (_plots[(int)_metrics.length()-1].dataCount > i)
			_plots[(int)_metrics.length()-1].plot_data[i] =
			    _plots[(int)_metrics.length()-1].data[i];
		    else
			_plots[(int)_metrics.length()-1].plot_data[i] = 0;
		    for (int m = (int)_metrics.length()-2; m >= 0; m--) {
			if (_plots[m].dataCount > i)
			    _plots[m].plot_data[i] = 
				_plots[m].data[i] + _plots[m+1].plot_data[i];
			else
			    _plots[m].plot_data[i] = _plots[m+1].plot_data[i];
		    }
#endif
		}
	    }
	    break;

    }
    _style = style;

    return(0);
}

QColor Chart::color(int m)
{
    if (m >= 0 && m < (int)_metrics.length())
	return _plots[m].color;
    else
	return QColor("white");
    /*NOTREACHED*/
}

int Chart::setColor(int m, QColor c)
{
    if (m >= 0 && m < (int)_metrics.length()) {
#if DESPERATE
	fprintf(stderr, "Chart::setColor(%d, r=%02x g=%02x b=%02x)\n", m, c.red(), c.green(), c.blue());
#endif
	_plots[m].color = c;
	_plots[m].curve->setPen(QPen(c));
	return(0);
    }
    else {
#if DESPERATE
	fprintf(stderr, "Chart::setColor(%d, r=%02x g=%02x b=%02x) BAD m=%d length()=%d\n", m, c.red(), c.green(), c.blue(), m, _metrics.length());
#endif
	// TODO - error status?
	return(-1);
    }
}

void Chart::scale(bool *autoscale, double *ymin, double *ymax)
{
    *autoscale = _autoscale;
    *ymin = _ymin;
    *ymax = _ymax;
}

void Chart::setScale(bool autoscale, double ymin, double ymax)
{
    _autoscale = autoscale;
    _ymin = ymin;
    _ymax = ymax;
    if (autoscale)
	setAxisAutoScale(QwtPlot::yLeft);
    else
	setAxisScale(QwtPlot::yLeft, ymin, ymax);
}

void Chart::setYAxisTitle(char *p)
{
    QwtText	*t;

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
    (void)p;
#if DESPERATE
    fprintf(stderr, "Chart::selected(...) this=%p x=%f y=%f\n", this, (float)p.x(), (float)p.y());
#endif
    _tab->setCurrent(this);
}

// TODO -- see comments above about usefuleness of moved() signal
// handling
void Chart::moved(const QwtDoublePoint &p)
{
    (void)p;
#if DESPERATE
    fprintf(stderr, "Chart::moved(...) this=%p x=%f y=%f\n", this, (float)p.x(), (float)p.y());
#endif
}

bool Chart::legendVisible()
{
    // Legend is on or off for all plots, only need to test the first
    // plot
    if (_metrics.length() > 0)
	return legend() != NULL;
    else
	return FALSE;
    /*NOTREACHED*/
}

// Clickable legend styled after the Qwt cpuplot demo.
// Use Edit->Chart Title and Legend to enable/disable the legend.
// Clicking on individual legend buttons will hide/show the
// corresponding plot (which is a nice feature over pmchart).
//
void Chart::setLegendVisible(bool on)
{
#if DESPERATE
    fprintf(stderr, "Chart::setLegendVisible(%d) legend()=%p\n", on, legend());
#endif
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
	    for (int m = 0; m < (int)_metrics.length(); m++) {
		showCurve(_plots[m].curve, !_plots[m].removed);
	    }
	}
    }
    else {
	QwtLegend *l = legend();
	if (l != NULL) {
	    // currently enabled, disable it
	    insertLegend(NULL, QwtPlot::BottomLegend);
	    delete l;
	}
    }
}

PMC_String *Chart::name(int m)
{
    return _plots[m].name;
}

char *Chart::legend_spec(int m)
{
    return _plots[m].legend;
}

PMC_Desc *Chart::metricDesc(int m)
{
    return (PMC_Desc *)&_metrics[m]->desc();
}

PMC_String *Chart::metricName(int m)
{
    return (PMC_String *)&_metrics[m]->name();
}

PMC_Context *Chart::metricContext(int m)
{
    return (PMC_Context *)&_metrics[m]->context();
}

QString Chart::pmloggerMetricSyntax(int m)
{
    PMC_Metric *metric = _metrics[m];
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

void Chart::setupListView(QListView *listview)
{
    uint	i;
    QString	src;

    listview->clear();
    for (i = 0; i < _metrics.length(); i++) {
	if (!_plots[i].removed)
	    addToList(listview, tr(_plots[i].name->ptr()),
		      &_metrics[i]->context(), _metrics[i]->hasInstances(), 
		      _tab->isArchiveMode(), _plots[i].color);
    }
}

void Chart::addToList(QListView *listview, QString metric,
	const PMC_Context *context, bool isInst, bool isArch, QColor &color)
{
    QRegExp regex(tr("\\.|\\[|\\]"));
    QString source = Source::makeSourceAnnotatedName(context);
    QStringList	baselist;

fprintf(stderr, "%s: src='%s' metric=%s, isInst=%d isArch=%d\n",
	__FUNCTION__, source.ascii(), metric.ascii(), isInst, isArch);

    baselist = QStringList::split(regex, metric);
    baselist.prepend(source);	// add the host/archive root as well.

    // Walk through each component of this name, creating them in the
    // target listview (if not there already), right down to the leaf.

    NameSpace *n, *tree = NULL;
    QListViewItem *last = listview->firstChild();
    QStringList::Iterator blit;
    for (blit = baselist.begin(); blit != baselist.end(); ++blit) {
	do {
	    if (last && *blit == last->text(1)) {
		// no insert at this level necessary, move down a level
		if (*blit != baselist.last()) {	// non-leaf
		    tree = (NameSpace *)last;
		    last = last->firstChild();
		}
		break;
	    }
	    // else keep scanning the direct children.
	} while (last && (last = last->nextSibling()) != NULL);

	/* when no more children and no match so far, we create & insert */
	if (!last) {
	    if (*blit == baselist.first()) {
		n = new NameSpace(listview, context, isArch);
		n->setSelectable(FALSE);
		n->setExpandable(TRUE);
	        n->setExpanded(TRUE);
		n->setOpen(TRUE);
	    }
	    else {
		bool isLeaf = (*blit == baselist.last());
		n = new NameSpace(tree, (*blit).ascii(), isLeaf&isInst, isArch);
		if (isLeaf) {
		    n->setOriginalColor(color);
		    n->setCurrentColor(color, NULL);
		}
		n->setExpandable(!isLeaf);
		n->setSelectable(isLeaf);
	        n->setExpanded(TRUE);
		n->setOpen(!isLeaf);
		if (!isLeaf)
		    n->setType(NONLEAF_NAME);
		else if (isInst)	// constructor sets INSTANCE_TYPE
		    tree->setType(LEAF_WITH_INDOM);
		else
		    n->setType(LEAF_NULL_INDOM);
	    }
	    tree = n;
	}
    }
}
