/*
 * Copyright (c) 2012, Red Hat.
 * Copyright (c) 2012, Nathan Scott.  All Rights Reserved.
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
#include "chart.h"

class SamplingCurve : public QwtPlotCurve
{
public:
    SamplingCurve(const QString &title) : QwtPlotCurve(title) { }

    virtual void drawSeries(QPainter *p,
		const QwtScaleMap &xMap, const QwtScaleMap &yMap,
		const QRectF &canvasRect, int from, int to) const;

    static double NaN();
    static bool isNaN(double v);
};

class SamplingItem : public ChartItem
{
public:
    SamplingItem(Chart *,
		QmcMetric *, pmMetricSpec *, pmDesc *,
		const char *, Chart::Style, int, int);
    ~SamplingItem(void);

    QwtPlotItem *item();
    QwtPlotCurve *curve();

    void preserveLiveData(int, int);
    void punchoutLiveData(int);
    void updateValues(bool, bool, pmUnits *, int, double, double, double);
    void rescaleValues(pmUnits *);
    void replot(int, double *);
    void resetValues(int);
    void revive(Chart *);
    void remove();
    void setStroke(Chart::Style, QColor, bool);
    void showCursor(bool, const QPointF &, int);

    void copyRawDataArray(void);
    void copyRawDataPoint(int index);
    void copyDataPoint(int index);
    int maximumDataCount(int maximum);
    void truncateData(int offset);
    double sumData(int index, double sum);
    void setPlotUtil(int index, double sum);
    double setPlotStack(int index, double sum);
    double setDataStack(int index, double sum);

private:
    struct {
	SamplingCurve *curve;
	double scale;
	double *data;
	double *itemData;
	int dataCount;
    } my;
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
    SamplingScaleEngine();

    double minimum() const { return my.minimum; }
    double maximum() const { return my.maximum; }
    bool autoScale() const { return my.autoScale; }
    void setAutoScale(bool autoScale) { my.autoScale = autoScale; }
    void setScale(bool autoScale, double minValue, double maxValue);
    virtual void autoScale(int maxSteps, double &minValue,
			   double &maxValue, double &stepSize) const;

private:
    struct {
	bool autoScale;
	double minimum;
	double maximum;
    } my;
};

#endif	// SAMPLING_H
