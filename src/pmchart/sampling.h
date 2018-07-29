/*
 * Copyright (c) 2012-2018, Red Hat.
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

#include <QVariant>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_scale_engine.h>
#include "chart.h"

class SamplingCurve : public ChartCurve
{
public:
    SamplingCurve(const QString &title) : ChartCurve(title) { }

    virtual void drawSeries(QPainter *painter,
		const QwtScaleMap &xMap, const QwtScaleMap &yMap,
		const QRectF &canvasRect, int from, int to) const;
};

class SamplingItem : public ChartItem
{
public:
    SamplingItem(Chart *,
		QmcMetric *, pmMetricSpec *, pmDesc *,
		const QString &, Chart::Style, int, int);
    ~SamplingItem(void);

    QwtPlotItem *item();
    QwtPlotCurve *curve();

    void preserveSample(int, int);
    void punchoutSample(int);
    void updateValues(bool, bool, pmUnits *, int, int, double, double, double);
    void rescaleValues(const pmUnits *, const pmUnits *);
    void resetValues(int, double, double);
    void revive();
    void remove();
    void setStroke(Chart::Style, QColor, bool);

    void clearCursor();
    bool containsPoint(const QRectF &, int);
    void updateCursor(const QPointF &, int);
    const QString &cursorInfo();

    void replot(int, const QVector<double> &);
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
	Chart *chart;
	SamplingCurve *curve;
	QString info;
	double scale;
	QVector<double> data;
	QVector<double> itemData;
	QVector<QPointF> samples;
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
    friend class Chart;

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
//
// 
// Implement sampling-specific behaviour within a Chart
//
class SamplingEngine : public ChartEngine
{
public:
    SamplingEngine(Chart *chart, pmDesc &);

    bool isCompatible(pmDesc &);
    ChartItem *addItem(QmcMetric *, pmMetricSpec *, pmDesc *, const QString &);

    void updateValues(bool, int, int, double, double, double);
    void replot(void);

    bool autoScale() { return my.scaleEngine->autoScale(); }
    void redoScale(void);
    void setScale(bool, double, double);
    void scale(bool *, double *, double *);
    void setStyle(Chart::Style);

    bool rateConvert() const { return my.rateConvert; }
    void setRateConvert(bool enabled) { my.rateConvert = enabled; }
    bool antiAliasing() const { return my.antiAliasing; }
    void setAntiAliasing(bool enabled) { my.antiAliasing = enabled; }

    void selected(const QPolygon &);
    void moved(const QPointF &);

private:
    SamplingItem *samplingItem(int index);
    void normaliseUnits(pmDesc &desc);

    struct {
	pmUnits units;
	bool rateConvert;
	bool antiAliasing;
	SamplingScaleEngine *scaleEngine;
	Chart *chart;
    } my;
};

#endif	// SAMPLING_H
