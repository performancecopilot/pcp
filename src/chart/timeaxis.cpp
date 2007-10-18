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

double TimeAxis::scaleValue(double delta, int count)
{
    double scale;

    scale = (1.0 / (width() / (count * 8.0))) * 8.0;
#if DESPERATE
    console->post("TimeAxis::scaleValue scale=%.2f x delta=%.2f",
			scale, delta);
#endif
    scale *= delta;
    return scale;
}

//
// Update the time axis if width changes, idea is to display increased
// precision as more screen real estate becomes available.  scaleValue
// is the critical piece of code in implemting it, as it uses width().
//
void TimeAxis::resizeEvent(QResizeEvent *e)
{
    QwtPlot::resizeEvent(e);
    if (e->size().width() != e->oldSize().width())
	kmchart->activeTab()->updateTimeAxis();
}
