/*
 * Copyright (c) 2007-2008, Aconex.  All Rights Reserved.
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
#include <QtCore/QEvent>
#include <QtCore/QDateTime>
#include <QtGui/QResizeEvent>
#include <qwt_plot_renderer.h>
#include <qwt_scale_draw.h>
#include <qwt_scale_widget.h>
#include <qwt_text.h>
#include <qwt_text_label.h>
#include "timeaxis.h"
#include "main.h"

#define DESPERATE 0

static const char *month[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

class TimeScaleDraw: public QwtScaleDraw
{
protected:
    TimeAxis *axis;

public:
    TimeScaleDraw(TimeAxis *a) :QwtScaleDraw() { axis = a; }
    virtual QwtText label(double v) const
    {
	struct tm tm;
	QString string;
	double delta = fabs(axis->delta());
	double points = axis->points();
	time_t seconds = (time_t)(unsigned long)v;

	pmLocaltime(&seconds, &tm);

	if (delta * points > 24 * 60 * 60 / 6 || (tm.tm_hour == 0 && tm.tm_min == 0))
	    // visible interval is more than 6 hours - omit seconds but include date
	    string.sprintf("%2d:%02d\n%3s %02d",
		tm.tm_hour, tm.tm_min, month[tm.tm_mon], tm.tm_mday);
	else
	    // just show the time, including seconds but omit date
	    string.sprintf("%2d:%02d:%02d",
		tm.tm_hour, tm.tm_min, tm.tm_sec);

	return string;
    }

    virtual void getBorderDistHint(const QFont &f, int &start, int &end) const
    {
	if (orientation() == Qt::Horizontal) {
	    start = 0;
	    end = 0;
	}
	else {
	    QwtScaleDraw::getBorderDistHint(f, start, end);
	}
    }
};

TimeAxis::TimeAxis(QWidget *parent) : QwtPlot(parent)
{
    clearScaleCache();
    setFocusPolicy(Qt::NoFocus);
}

void TimeAxis::init()
{
    enableAxis(xTop, false);
    enableAxis(yLeft, false);
    enableAxis(yRight, false);
    enableAxis(xBottom, true);

    setAutoReplot(false);
    setAxisFont(QwtPlot::xBottom, *globalFont);
    setAxisScaleDraw(QwtPlot::xBottom, new TimeScaleDraw(this));
    setAxisLabelAlignment(QwtPlot::xBottom, Qt::AlignHCenter | Qt::AlignBottom);
}

void TimeAxis::resetFont()
{
    setAxisFont(QwtPlot::xBottom, *globalFont);
}

void TimeAxis::print(QPainter *qp, QRect &rect, bool transparent)
{
    QwtPlotRenderer renderer;

    if (transparent)
	renderer.setDiscardFlag(QwtPlotRenderer::DiscardBackground);
    renderer.render(this, qp, rect);
}

void TimeAxis::noArchiveSources()
{
    setAxisScale(QwtPlot::xBottom, 0, 1, 0);
    replot();
}

void TimeAxis::clearScaleCache()
{
    my.points = 0;
    my.delta = my.scale = 0.0;
}

double TimeAxis::scaleValue(double delta, int points)
{
    int w = width();

    if (my.delta == delta && my.points == points)
	return my.scale;

    // divisor is the amount of space (pixels) set aside for one major label.
    int maxMajor = qMax(1, w / 54);
    int maxMinor;

    my.scale = (1.0 / ((double)w / (points * 8.0))) * 8.0; // 8.0 is magic
    my.scale *= delta;

    // This is a sliding scale which converts arbitrary steps into more
    // human-digestable increments - seconds, ten seconds, minutes, etc.
    if (my.scale <= 10.0) {
	maxMinor = 10;
    } else if (my.scale <= 20.0) {	// two-seconds up to 20 seconds
	my.scale = floor((my.scale + 1) / 2) * 2.0;
	maxMinor = 10;
    } else if (my.scale <= 60.0) {	// ten-secondly up to a minute
	my.scale = floor((my.scale + 5) / 10) * 10.0;
	maxMinor = 10;
    } else if (my.scale <= 600.0) {	// minutely up to ten minutes
	my.scale = floor((my.scale + 30) / 60) * 60.0;
	maxMinor = 10;
    } else if (my.scale < 3600.0) {	// 10 minutely up to an hour
	my.scale = floor((my.scale + 300) / 600) * 600.0;
	maxMinor = 10;
    } else if (my.scale < 86400.0) {	// hourly up to a day
	my.scale = floor((my.scale + 1800) / 3600) * 3600.0;	
	maxMinor = 10;
    } else if (my.scale < 604800.0) {	// daily up to a week
	my.scale = floor((my.scale + 64800) / 86400) * 86400.0;	
	maxMinor = 10;
    } else {				// weekly then on (7 * 60 * 60 * 24)
	my.scale = 604800.0;
	maxMinor = 10;
    }

#if DESPERATE
    console->post(PmChart::DebugForce, "TimeAxis::scaleValue"
		  " width=%d points=%d scale=%.2f delta=%.2f maj=%d min=%d\n",
		    w, points, my.scale, delta, maxMajor, maxMinor);
#endif

    my.delta = delta;
    my.points = points;
    setAxisMaxMajor(xBottom, maxMajor);
    setAxisMaxMinor(xBottom, maxMinor);
    return my.scale;
}

//
// Update the time axis if width changes, idea is to display increased
// precision as more screen real estate becomes available.  scaleValue
// is the critical piece of code in implementing it, as it uses width().
//
void TimeAxis::resizeEvent(QResizeEvent *e)
{
    QwtPlot::resizeEvent(e);
    if (e->size().width() != e->oldSize().width()) {
	clearScaleCache();
	activeGroup->updateTimeAxis();
    }
}
