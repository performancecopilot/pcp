/*
 * Copyright (c) 2007, Nathan Scott.  All Rights Reserved.
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
 * Contact information: Nathan Scott, nathans At debian DoT org
 */

#include <qevent.h>
#include <qdatetime.h>
#include <qwt/qwt_scale_draw.h>
#include <qwt/qwt_scale_widget.h>
#include <qwt/qwt_text.h>
#include <qwt/qwt_text_label.h>
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

TimeAxis::TimeAxis(QWidget *parent, const char *name): QwtPlot(parent, name)
{
    setFixedHeight(30);
    setFocusPolicy(QWidget::NoFocus);
}

void TimeAxis::init()
{
    enableXBottomAxis(true);
    enableXTopAxis(false);
    enableYRightAxis(false);
    enableYLeftAxis(false);
    setAutoReplot(false);
    plotLayout()->setAlignCanvasToScales(true);
    canvas()->hide();
    setMargin(1);
    setAxisScaleDraw(QwtPlot::xBottom, new TimeScaleDraw());
    setAxisLabelAlignment(QwtPlot::xBottom, Qt::AlignHCenter | Qt::AlignBottom);
    setAxisFont(QwtPlot::xBottom, QFont("Sans", 6));
}

double TimeAxis::scaleValue(double delta, int count)
{
    double scale;

    scale = (1.0 / (width() / (count * 8.0))) * 8.0;
#if DESPERATE
    fprintf(stderr, "%s: scale=%.2f x delta=%.2f\n", __func__, scale, delta);
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
	activeTab->updateTimeAxis();
}
