/*
 * Copyright (c) 2012, Red Hat.
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
#ifndef SAMPLING_H
#define SAMPLING_H

#include <QtCore/QVariant>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>

class SamplingCurve : public QwtPlotCurve
{
public:
    SamplingCurve(const QString &title) : QwtPlotCurve(title) { }
    virtual void draw(QPainter *p,
		const QwtScaleMap &xMap, const QwtScaleMap &yMap,
		int from, int to) const;

    static double NaN();
    static bool isNaN(double v);
};

#endif	// SAMPLING_H
