/*
 * Copyright (c) 2012, Red Hat.
 * Copyright (c) 2012, Nathan Scott.  All Rights Reserved.
 * Copyright (c) 2006-2010, Aconex.  All Rights Reserved.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
#ifndef CHART_H
#define CHART_H

#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtGui/QColor>
#include <QtGui/QTreeWidget>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_picker.h>
#include <qmc_metric.h>
#include "gadget.h"

class Tab;
class ChartItem;
class TracingItem;
class SamplingItem;
class TracingScaleEngine;
class SamplingScaleEngine;

//
// Centre of the pmchart universe
//
class Chart : public QwtPlot, public Gadget
{
    Q_OBJECT

public:
    Chart(Tab *, QWidget *);
    ~Chart(void);

    typedef enum {
	NoStyle,
	LineStyle,
	BarStyle,
	StackStyle,
	AreaStyle,
	UtilisationStyle,
	EventStyle
    } Style;

    virtual void setCurrent(bool);
    virtual QString scheme() const;	// return chart color scheme
    virtual void setScheme(QString);	// set the chart color scheme

    int addItem(pmMetricSpec *, const char *);
    bool activeItem(int);
    void removeItem(int);
    void reviveItem(int);

    char *title(void);			// return chart title
    void changeTitle(char *, int);	// NULL to clear
    void changeTitle(QString, int);
    QColor color(int);			// return color for ith plot
    Style style(void);			// return chart style
    void setStyle(Style);		// set default chart plot style
    void setStroke(int, Style, QColor);	// set chart style and color
    int sequence();			// return chart color scheme position
    void setSequence(int);		// set the chart color scheme position
    void setScheme(QString, int);	// set the chart scheme and position
    QString label(int);			// return legend label for ith plot
    void setLabel(int, QString);	// set plot legend label
    void scale(bool *, double *, double *);
			// return autoscale state and fixed scale parameters
    void setScale(bool, double, double);
			// set autoscale state and fixed scale parameters
    bool rateConvert();
    void setRateConvert(bool);
    void setYAxisTitle(const char *);
    bool legendVisible();
    void setLegendVisible(bool);
    bool antiAliasing();
    void setAntiAliasing(bool);

    virtual void save(FILE *, bool);
    virtual void print(QPainter *, QRect &, bool);

    virtual void updateValues(bool, bool, int, double, double, double);
    virtual void resetValues(int m, int v);
    virtual void adjustValues();

    virtual void preserveLiveData(int, int);
    virtual void punchoutLiveData(int);

    virtual int metricCount() const;
    virtual QString name(int) const;
    virtual char *legendSpec(int) const;
    virtual QmcMetric *metric(int) const;
    virtual QString metricName(int) const;
    virtual QmcDesc *metricDesc(int) const;
    virtual QString metricInstance(int) const;
    virtual QmcContext *metricContext(int) const;

    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const;

    void setupTree(QTreeWidget *);
    void addToTree(QTreeWidget *, QString, const QmcContext *,
			  bool, QColor, QString);

    static QColor schemeColor(QString, int *);

public slots:
    void replot(void);

private slots:
    void selected(const QPointF &);
    void moved(const QPointF &);
    void selected(const QPolygon &);
    void showItem(QwtPlotItem *, bool);

private:
    bool checkCompatibleUnits(pmUnits *);
    bool checkCompatibleTypes(int);

    void redoScale(void);
    bool autoScale(void);
    void setScaleEngine(void);
    void setPickerMachine(void);
    void setStroke(ChartItem *, Style, QColor);
    void showPoint(const QPointF &);
    void showPoints(const QPolygon &);

    void redoChartItems(void);
    TracingItem *tracingItem(int);
    SamplingItem *samplingItem(int);

    struct {
	Tab *tab;
	QList<ChartItem *> items;
	pmUnits units;

	char *title;
	Style style;
	QString scheme;
	int sequence;

	bool eventType;
	bool rateConvert;
	bool antiAliasing;

	int selectedPoint;
	ChartItem *selectedItem;
	QwtPlotPicker *picker;
	QwtPickerMachine *tracingPickerMachine;
	QwtPickerMachine *samplingPickerMachine;

	TracingScaleEngine *tracingScaleEngine;
	SamplingScaleEngine *samplingScaleEngine;
    } my;
};

//
// Container for an individual plot item within a chart,
// which is always backed by a single metric.
//
class ChartItem
{
public:
    ChartItem(QmcMetric *, pmMetricSpec *, pmDesc *, const char *);
    virtual ~ChartItem(void) { }

    virtual QwtPlotItem *item(void) = 0;
    virtual QwtPlotCurve *curve(void) = 0;

    virtual void preserveLiveData(int, int) = 0;
    virtual void punchoutLiveData(int) = 0;
    virtual void resetValues(int) = 0;
    virtual void updateValues(bool, bool, pmUnits *, int, double, double, double) = 0;
    virtual void rescaleValues(pmUnits *) = 0;
    virtual void showCursor(bool, const QPointF &, int) = 0;
    virtual void setStroke(Chart::Style, QColor, bool) = 0;
    virtual void replot(int, double *) = 0;
    virtual void revive(Chart *parent) = 0;
    virtual void remove(void) = 0;

    QString name(void) const { return my.name; }
    QString label(void) const { return my.label; }
    char *legendSpec(void) const { return my.legend; }
    QString metricName(void) const { return my.metric->name(); }
    QString metricInstance(void) const
        { return my.metric->numInst() > 0 ? my.metric->instName(0) : QString::null; }
    bool metricHasInstances(void) const { return my.metric->hasInstances(); }
    QmcDesc *metricDesc(void) const { return (QmcDesc *)&my.metric->desc(); }
    QmcContext *metricContext(void) const { return my.metric->context(); }
    QmcMetric *metric(void) const { return my.metric; }
    QColor color(void) const { return my.color; }

    void setColor(QColor color) { my.color = color; }
    void setLabel(QString label) { my.label = label; }

    bool hidden(void) { return my.hidden; }
    void setHidden(bool hidden) { my.hidden = hidden; }
    bool removed(void) { return my.removed; }
    void setRemoved(bool removed) { my.removed = removed; }

protected:
    struct {
	QmcMetric *metric;
	pmUnits units;

	QString name;
	char *legend;	// from config
	QString label;	// as appears in plot legend
	QColor color;

	bool removed;
	bool hidden;	// true if hidden through legend push button
    } my;
};

#endif	// CHART_H
