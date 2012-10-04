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
#include <qwt_scale_draw.h>
#include <qwt_scale_engine.h>
#include <qwt_interval_symbol.h>
#include <qwt_plot_intervalcurve.h>

class TracingEvent
{
public:
    TracingEvent() { }
    TracingEvent(QmcEventRecord const &);
    bool operator<(TracingEvent const& rhs)	// for sorting
	{ return my.timestamp < rhs.timestamp(); }
    virtual ~TracingEvent();

    double timestamp(void) const { return my.timestamp; }
    int missed(void) const { return my.missed; }
    bool hasIdentifier(void) const { return my.flags & PM_EVENT_FLAG_ID; }
    bool hasParent(void) const { return my.flags & PM_EVENT_FLAG_PARENT; }
    bool hasStartFlag(void) const { return my.flags & PM_EVENT_FLAG_START; }
    bool hasEndFlag(void) const { return my.flags & PM_EVENT_FLAG_END; }

    const QString &spanID(void) const { return my.spanID; }
    const QString &rootID(void) const { return my.rootID; }
    const QString &description(void) const { return my.description; }

private:
    struct {
	double		timestamp;	// from PMDA
	int		missed;		// from PMDA
	int		flags;		// from PMDA
	QString		spanID;		// identifier
	QString		rootID;		// parent ID
	QString 	description;	// parameters, etc
    } my;
};

class TracingItem : public ChartItem
{
public:
    TracingItem() : ChartItem() { }
    TracingItem(Chart *, QmcMetric *, pmMetricSpec *, pmDesc *, const char *);
    virtual ~TracingItem();

    QwtPlotItem *item();
    QwtPlotCurve *curve();

    void preserveLiveData(int, int) { /*TODO*/ }
    void punchoutLiveData(int) { /*TODO*/ }
    void resetValues(int);
    void updateValues(bool, bool, pmUnits*, int, int, double, double, double);
    void rescaleValues(double *, double *);

    void clearCursor(void);
    bool containsPoint(const QRectF &, int);
    void updateCursor(const QPointF &, int);
    const QString &cursorInfo();

    void setStroke(Chart::Style, QColor, bool);
    void revive(void);
    void remove(void);

private:
    void cullOutlyingDrops(double, double);
    void cullOutlyingSpans(double, double);
    void cullOutlyingPoints(double, double);
    void cullOutlyingEvents(double, double);

    void updateEvents(QmcMetric *);
    void updateEventRecords(QmcMetric *, int);
    void showEventInfo(bool, int);

    struct {
	QVector<TracingEvent> events;		// all events, raw data
	QVector<QPointF> selections;		// time series of selected points
	QString selectionInfo;

	QVector<QPointF> points;		// displayed trace data (point form)
	ChartCurve *pointCurve;
	QwtSymbol *pointSymbol;

	QVector<QPointF> selectionPoints;	// displayed user-selected trace points
	QwtPlotCurve *selectionCurve;
	QwtSymbol *selectionSymbol;

	QVector<QwtIntervalSample> spans;	// displayed trace data (horizontal span)
	QwtPlotIntervalCurve *spanCurve;
	QwtIntervalSymbol *spanSymbol;

	QVector<QwtIntervalSample> drops;	// displayed trace data (vertical drop)
	QwtPlotIntervalCurve *dropCurve;
	QwtIntervalSymbol *dropSymbol;

	double minSpanID;
	double maxSpanID;
	double previousTimestamp;
	Chart *chart;
    } my;
};

class TracingScaleEngine : public QwtLinearScaleEngine
{
public:
    TracingScaleEngine(Chart *chart);

    virtual void autoScale(int maxSteps, double &minValue,
                           double &maxValue, double &stepSize) const;
    virtual QwtScaleDiv divideScale(double x1, double x2,
        int numMajorSteps, int numMinorSteps, double stepSize = 0.0) const;

    void setScale(double minSpanID, double maxSpanID);

private:
    struct {
	double	maxSpanID;
	double	minSpanID;
	Chart	*chart;
    } my;
};

class TracingScaleDraw : public QwtScaleDraw
{
public:
    TracingScaleDraw(Chart *chart) : QwtScaleDraw() { my.chart = chart; }
    virtual QwtText label(double v) const;
    virtual void getBorderDistHint(const QFont &f, int &start, int &end) const;
    void invalidate() { invalidateCache(); }

private:
    struct {
	Chart	*chart;
    } my;
};

#endif	// TRACING_H
