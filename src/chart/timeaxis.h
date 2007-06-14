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
#ifndef TIMEAXIS_H
#define TIMEAXIS_H

#include <qwt/qwt_plot_canvas.h>
#include <qwt/qwt_plot_layout.h>
#include <qwt/qwt_plot.h>

class Tab;

class TimeAxis : public QwtPlot 
{
    Q_OBJECT
public:
    TimeAxis(QWidget * = 0, const char *name = 0);

    void init();
    double scaleValue(double delta, int count);

protected:
    virtual void resizeEvent(QResizeEvent *);
};

#endif	/* TIMEAXIS_H */
