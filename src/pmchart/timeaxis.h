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
#ifndef TIMEAXIS_H
#define TIMEAXIS_H

#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_plot.h>
#include <QResizeEvent>

class Tab;

class TimeAxis : public QwtPlot 
{
    Q_OBJECT
public:
    TimeAxis(QWidget *);

    void init();
    void resetFont();
    void clearScaleCache();
    double scaleValue(double delta, int count);
    double delta(void) { return my.delta; }
    double points(void) { return my.points; }
    void noArchiveSources();
    void print(QPainter *, QRect &, bool);

protected:
    void resizeEvent(QResizeEvent *);

private:
    struct {
	int points;
	double delta;
	double scale;
    } my;
};

#endif	/* TIMEAXIS_H */
