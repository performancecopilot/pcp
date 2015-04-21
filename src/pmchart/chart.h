/*
 * Copyright (c) 2012-2015, Red Hat.
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
class ChartPicker;
class ChartEngine;
class TracingEngine;
class SamplingEngine;

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

    virtual void resetFont();
    virtual void setCurrent(bool);
    virtual QString scheme() const;	// return chart color scheme
    virtual void setScheme(const QString &);	// set chart color scheme

    int addItem(pmMetricSpec *, const QString &);
    bool activeItem(int) const;
    void removeItem(int);
    void reviveItem(int);

    QString title(void);		// return copy of chart title
    void changeTitle(QString, bool);	// QString::null to clear; expand?
    QString hostNameString(bool);	// short/long host names as qstring

    Style style(void);			// return chart style
    void setStyle(Style);		// set default chart plot style

    QColor color(int);			// return color for ith plot
    static QColor schemeColor(QString, int *);
    void setStroke(int, Style, QColor);	// set chart style and color
    void setScheme(const QString &, int);	// set chart scheme and position

    int sequence()			// return chart color scheme position
	{ return my.sequence; }
    void setSequence(int sequence)	// set the chart color scheme position
	{ my.sequence = sequence; }

    QString label(int);			// return legend label for ith plot
    void setLabel(int, const QString &);	// set plot legend label

    bool autoScale(void);
    void scale(bool *, double *, double *);
			// return autoscale state and fixed scale parameters
    void setScale(bool, double, double);
			// set autoscale state and fixed scale parameters
    QString YAxisTitle() const;
    void setYAxisTitle(const char *);
    bool legendVisible();
    void setLegendVisible(bool);
    bool rateConvert();
    void setRateConvert(bool);
    bool antiAliasing();
    void setAntiAliasing(bool);

    virtual void save(FILE *, bool);
    virtual void print(QPainter *, QRect &, bool);

    virtual void updateValues(bool, bool, int, int, double, double, double);
    virtual void resetValues(int, double, double);
    virtual void adjustValues();

    virtual void preserveSample(int, int);
    virtual void punchoutSample(int);

    virtual int metricCount() const;
    virtual bool activeMetric(int) const;
    virtual QString name(int) const;
    virtual QString legend(int) const;
    virtual QmcMetric *metricPtr(int) const;
    virtual QString metricName(int) const;
    virtual QmcDesc *metricDesc(int) const;
    virtual QString metricInstance(int) const;
    virtual QmcContext *metricContext(int) const;

    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const;

    void setupTree(QTreeWidget *);
    void addToTree(QTreeWidget *, const QString &, const QmcContext *,
			  bool, QColor, const QString &);

    void activateTime(QMouseEvent *);
    void reactivateTime(QMouseEvent *);
    void deactivateTime(QMouseEvent *);

Q_SIGNALS:
    void timeSelectionActive(Gadget *, int);
    void timeSelectionReactive(Gadget *, int);
    void timeSelectionInactive(Gadget *);

public Q_SLOTS:
    void replot(void);

private Q_SLOTS:
    void activated(bool);
    void selected(const QPolygon &);
    void selected(const QPointF &);
    void moved(const QPointF &);
    void legendChecked(QwtPlotItem *, bool);

private:
    // changing properties
    void setStroke(ChartItem *, Style, QColor);
    void resetTitleFont(void);

    // handling selection
    void showInfo(void);
    void showPoint(const QPointF &);
    void showPoints(const QPolygon &);

    struct {
	Tab *tab;
	QList<ChartItem *> items;

	QString title;
	QString scheme;
	int sequence;
	Style style;

	ChartEngine *engine;
	ChartPicker *picker;
    } my;

    friend class TracingEngine;
    friend class SamplingEngine;
};

//
// Wrapper class that simply allows us to call the mouse event handlers
// for the picker class.  Helps implement the time axis picker extender.
//
class ChartPicker : public QwtPlotPicker
{
public:
    ChartPicker(QwtPlotCanvas *canvas) :
	QwtPlotPicker(QwtPlot::xBottom, QwtPlot::yLeft,
	QwtPicker::CrossRubberBand, QwtPicker::AlwaysOff, canvas) { }

    void widgetMousePressEvent(QMouseEvent *event)
	{ QwtPlotPicker::widgetMousePressEvent(event); }
    void widgetMouseReleaseEvent(QMouseEvent *event)
	{ QwtPlotPicker::widgetMouseReleaseEvent(event); }
    void widgetMouseMoveEvent(QMouseEvent *event)
	{ QwtPlotPicker::widgetMouseMoveEvent(event); }
};

//
// Abstraction for differences between event tracing and sampling models
// Note that this base class is used for an initially empty chart
//
class ChartEngine
{
public:
    ChartEngine() { }
    ChartEngine(Chart *chart);
    virtual ~ChartEngine() {}

    // test whether a new metric (type and units) would be compatible
    // with this engine and any metrics already plotted in the chart.
    virtual bool isCompatible(pmDesc &) { return true; }

    // insert a new item (plot curve) into a chart
    virtual ChartItem *addItem(QmcMetric *, pmMetricSpec *, pmDesc *, const QString &)
	{ return NULL; }	// cannot add to an empty engine

    // indicates movement forward/backward occurred
    virtual void updateValues(bool, int, int, double, double, double) { }

    // indicates the Y-axis scale needs updating
    virtual bool autoScale(void) { return false; }
    virtual void redoScale(void) { }
    virtual void setScale(bool, double, double) { }
    virtual void scale(bool *autoScale, double *yMin, double *yMax)
	{ *autoScale = false; *yMin = 0.0; *yMax = 1.0; }

    // get/set other attributes of the chart
    virtual bool rateConvert(void) const { return my.rateConvert; }
    virtual void setRateConvert(bool enabled) { my.rateConvert = enabled; }
    virtual bool antiAliasing(void) const { return my.antiAliasing; }
    virtual void setAntiAliasing(bool enabled) { my.antiAliasing = enabled; }
    virtual void setStyle(Chart::Style) { }

    // prepare for chart replot() being called
    virtual void replot(void) { }

    // a selection has been made/changed, handle it
    virtual void selected(const QPolygon &) { }
    virtual void moved(const QPointF &) { }

private:
    struct {
	bool	rateConvert;
	bool	antiAliasing;
    } my;
};

//
// Helper dealing with overriding of legend behaviour
//
class ChartCurve : public QwtPlotCurve
{
public:
    ChartCurve(const QString &title)
	: QwtPlotCurve(title), legendColor(Qt::white) { }

    virtual void drawLegendIdentifier(QPainter *painter,
		const QRectF &rect ) const;
    void setLegendColor(QColor color) { legendColor = color; }
    QColor legendColor;
};

//
// Container for an individual plot item within a chart,
// which is always backed by a single metric.
//
class ChartItem
{
public:
    ChartItem() { }
    ChartItem(QmcMetric *, pmMetricSpec *, pmDesc *, const QString &);
    virtual ~ChartItem(void) { }

    virtual QwtPlotItem *item(void) = 0;
    virtual QwtPlotCurve *curve(void) = 0;

    virtual void preserveSample(int, int) = 0;
    virtual void punchoutSample(int) = 0;
    virtual void resetValues(int, double, double) = 0;

    virtual void clearCursor() = 0;
    virtual bool containsPoint(const QRectF &, int) = 0;
    virtual void updateCursor(const QPointF &, int) = 0;
    virtual const QString &cursorInfo() = 0;

    virtual void setVisible(bool on) { item()->setVisible(on); }
    virtual void setStroke(Chart::Style, QColor, bool) = 0;
    virtual void revive(void) = 0;
    virtual void remove(void) = 0;

    QString name(void) const { return my.name; }
    QString label(void) const { return my.label; } // as displayed, expanded
    QString legend(void) const { return my.legend; } // no %i/%h/.. expansion
    QString metricName(void) const { return my.metric->name(); }
    QString metricInstance(void) const
        { return my.metric->numInst() > 0 ? my.metric->instName(0) : QString::null; }
    bool metricHasInstances(void) const { return my.metric->hasInstances(); }
    QmcDesc *metricDesc(void) const { return (QmcDesc *)&my.metric->desc(); }
    QmcContext *metricContext(void) const { return my.metric->context(); }
    QmcMetric *metricPtr(void) const { return my.metric; }
    QColor color(void) const { return my.color; }

    void setColor(QColor color) { my.color = color; }
    void setLegend(const QString &legend);	// may include wildcards
    void updateLegend();

    bool hidden(void) { return my.hidden; }
    void setHidden(bool hidden) { my.hidden = hidden; }

    bool removed(void) { return my.removed; }
    void setRemoved(bool removed) { my.removed = removed; }

protected:
    struct {
	QmcMetric *metric;
	pmUnits units;	// base units, *not* scaled

	QString name;
	QString inst;
	QString legend;	// may contain wildcards (not expanded)
	QString label;	// as appears visibly, in plot legend
	QColor color;

	bool removed;
	bool hidden;	// true if hidden through legend push button
    } my;

private:
    void expandLegendLabel(const QString &legend);
    void clearLegendLabel(void);

    QString hostname(void) const;
    QString shortHostName(void) const;
    QString shortMetricName(void) const;
    QString shortInstName(void) const;
};

#endif	// CHART_H
