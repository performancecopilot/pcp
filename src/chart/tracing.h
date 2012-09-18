/*
 * Copyright (c) 2012, Red Hat.
 * Copyright (c) 2012, Nathan Scott.  All Rights Reserved.
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
#ifndef TRACING_H
#define TRACING_H

#include "chart.h"
#include <qvector.h>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_scale_engine.h>
#include <qwt_interval_symbol.h>
#include <qwt_plot_intervalcurve.h>

class TraceEvent
{
public:
    TraceEvent(QmcEventRecord const &);
    bool operator<(TraceEvent const& rhs)	// for sorting
	{ return my.timestamp < rhs.timestamp(); }

    int flags(void) const { return my.flags; }
    int missed(void) const { return my.missed; }
    double timestamp(void) const { return my.timestamp; }

    const QString &spanID(void) const { return my.spanID; }
    const QString &rootID(void) const { return my.rootID; }
    const QString &description(void) const { return my.description; }

private:
    struct {
	double		timestamp;
	int		missed;
	int		flags;
	QString		spanID;		// identifier
	QString		rootID;		// parent ID
	QString 	description;	// parameters, etc
    } my;
};

class TracingItem : public ChartItem
{
public:
    TracingItem(Chart *, QmcMetric *, pmMetricSpec *, pmDesc *, const char *);
    ~TracingItem(void);

    QwtPlotItem* item();
    QwtPlotCurve* curve();

    void preserveLiveData(int, int) { }
    void punchoutLiveData(int) { }
    void resetValues(int);
    void updateValues(bool, bool, pmUnits*, int, double, double, double);
    void rescaleValues(pmUnits*);
    void showCursor(bool, const QPointF &, int);
    void showPoints(const QRectF &);
    void setStroke(Chart::Style, QColor, bool);
    void replot(int, double*);
    void revive(Chart *parent);
    void remove(void);

    void setPlotEnd(int index);

private:
    void cullOutlyingRanges(QVector<QwtIntervalSample> &, double, double);
    void cullOutlyingPoints(QVector<QPointF> &, double, double);
    void cullOutlyingEvents(QList<TraceEvent*> &, double, double);

    void updateEvents(QmcMetric *);
    void updateEventRecords(QmcMetric *, int);

    struct {
	QHash<QString, int> yMap;		// reverse map, event ID to y-axis point
	QList<TraceEvent*> events;		// all events, raw data

	QVector<QPointF> points;		// displayed trace data (point form)
	QwtPlotCurve *pointCurve;
	QwtSymbol *pointSymbol;

	QVector<QwtIntervalSample> spans;	// displayed trace data (horizontal span)
	QwtPlotIntervalCurve *spanCurve;
	QwtIntervalSymbol *spanSymbol;

	QVector<QwtIntervalSample> drops;	// displayed trace data (vertical drop)
	QwtPlotIntervalCurve *dropCurve;
	QwtIntervalSymbol *dropSymbol;
    } my;
};

class TracingScaleEngine : public QwtLinearScaleEngine
{
public:
    TracingScaleEngine();

    void setScale(bool autoScale, double minValue, double maxValue);
    virtual void autoScale(int maxSteps, double &minValue,
                           double &maxValue, double &stepSize) const;
};

#endif	// TRACING_H
