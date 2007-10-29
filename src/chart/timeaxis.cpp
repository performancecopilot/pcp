/*
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
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
#include <qwt_scale_draw.h>
#include <qwt_scale_widget.h>
#include <qwt_text.h>
#include <qwt_text_label.h>
#include <pcp/pmapi.h>
#include "timeaxis.h"
#include "main.h"

#define DESPERATE 0

class TimeScaleDraw: public QwtScaleDraw
{
public:
    TimeScaleDraw(void) { }
    virtual QwtText label(double v) const
    {
	struct tm tm;
	QString string;
	time_t seconds = (time_t)v;

	pmLocaltime(&seconds, &tm);
	string.sprintf("%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
	return string;
    }
};

TimeAxis::TimeAxis(QWidget *parent) : QwtPlot(parent)
{
    clearScaleCache();
    setFixedHeight(30);
    setFocusPolicy(Qt::NoFocus);
}

void TimeAxis::init()
{
    enableAxis(xBottom, true);
    enableAxis(xTop, false);
    enableAxis(yLeft, false);
    enableAxis(yRight, false);
    setAutoReplot(false);
    plotLayout()->setAlignCanvasToScales(true);
    canvas()->hide();
    setMargin(1);
    setAxisScaleDraw(QwtPlot::xBottom, new TimeScaleDraw());
    setAxisLabelAlignment(QwtPlot::xBottom, Qt::AlignHCenter | Qt::AlignBottom);
    setAxisFont(QwtPlot::xBottom, globalFont);
}

void TimeAxis::clearScaleCache()
{
    my.points = 0;
    my.delta = my.scale = 0.0;
}

double TimeAxis::scaleValue(double delta, int points)
{
    if (my.delta == delta && my.points == points)
	return my.scale;

    // divisor is the amount of space (pixels) set aside for one major label.
    int maxMajor = qMax(1, width() / 54);
    int maxMinor;

    my.scale = (1.0 / ((double)width() / (points * 8.0))) * 8.0; // 8.0 is magic
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
	maxMinor = 6;
    } else if (my.scale < 86400.0) {	// hourly up to a day
	my.scale = floor((my.scale + 1800) / 3600) * 3600.0;	
	maxMinor = 24;
    } else {				// daily then on (60 * 60 * 24)
	my.scale = 86400.0;
	maxMinor = 10;
    }

#if DESPERATE
    console->post("TimeAxis::scaleValue"
		  " width=%d points=%d scale=%.2f delta=%.2f maj=%d min=%d\n",
		    width(), points, my.scale, delta, maxMajor, maxMajor);
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
	kmchart->activeTab()->updateTimeAxis();
    }
}
