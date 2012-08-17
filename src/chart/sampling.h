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
#include <qwt_scale_engine.h>

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

//
// *Always* clamp minimum metric value at zero when positive -
// preventing confusion when values silently change up towards
// the maximum over time (for pmchart, our users are expecting
// a constant zero baseline at all times, or so we're told).
//
class SamplingScaleEngine : public QwtLinearScaleEngine
{
public:
    SamplingScaleEngine() : QwtLinearScaleEngine()
    {
	my.autoScale = true;
	my.minimum = -1;
	my.maximum = -1;
    }

    double minimum() const { return my.minimum; }
    double maximum() const { return my.maximum; }
    bool autoScale() const { return my.autoScale; }
    void setAutoScale(bool autoScale) { my.autoScale = autoScale; }
    void setScale(bool autoScale, double minimum, double maximum)
    {
	my.autoScale = autoScale;
	my.minimum = minimum;
	my.maximum = maximum;
    }

    virtual void autoScale(int maxSteps, double &minValue,
			   double &maxValue, double &stepSize) const
    {
	if (my.autoScale) {
	    if (minValue > 0)
		minValue = 0.0;
	} else {
	    minValue = my.minimum;
	    maxValue = my.maximum;
	}
	QwtLinearScaleEngine::autoScale(maxSteps, minValue, maxValue, stepSize);
    }

private:
    struct {
	bool autoScale;
	double minimum;
	double maximum;
    } my;
};

#endif	// SAMPLING_H
