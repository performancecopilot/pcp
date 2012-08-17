/*
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 2012 Nathan Scott.  All Rights Reserved.
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
#include <qwt_double_rect.h>
#include <qmc_metric.h>
#include "gadget.h"

class Tab;
class SamplingCurve;
class TracingScaleEngine;
class SamplingScaleEngine;

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
	UtilisationStyle
    } Style;

    virtual void setCurrent(bool);
    virtual QString scheme() const;	// return chart color scheme
    virtual void setScheme(QString);	// set the chart color scheme

    int addPlot(pmMetricSpec *, const char *);
    void delPlot(int);
    bool activePlot(int);
    void revivePlot(int m);
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

    virtual void updateTimeAxis(double, double, double);
    virtual void updateValues(bool, bool);
    virtual void resetDataArrays(int m, int v);
    virtual void preserveLiveData(int, int);
    virtual void punchoutLiveData(int);
    virtual void adjustedLiveData();

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
			  bool, QColor&, QString&);

    static QColor schemeColor(QString, int *);

public slots:
    void replot();

private slots:
    void selected(const QwtDoublePoint &);
    void moved(const QwtDoublePoint &);
    void showCurve(QwtPlotItem *, bool);

private:
    typedef struct {
	QmcMetric *metric;
	SamplingCurve *curve;
	QString name;
	char *legend;	// from config
	QString label;	// as appears in plot legend
	QColor color;
	double scale;
	double *data;
	double *plotData;
	int dataCount;
	pmUnits units;
	bool eventType;
	bool removed;
	bool hidden;	// true if hidden through legend push button
    } Plot;

    bool isStepped(Plot *plot);
    void setStroke(Plot *plot, Style style, QColor color);
    void redoPlotData(void);
    void setScaleEngine(void);
    bool autoScale(void);
    void redoScale(void);
    void setColor(Plot *plot, QColor c);
    void setLabel(Plot *plot, QString s);
    void resetDataArrays(Plot *plot, int v);
    bool checkCompatibleUnits(pmUnits *);
    bool checkCompatibleTypes(int);

    struct {
	Tab *tab;
	QList<Plot*> plots;
	pmUnits units;

	Style style;
	char *title;
	QString scheme;
	int sequence;

	bool eventType;
	bool rateConvert;
	bool antiAliasing;

	QwtPlotPicker *picker;
	TracingScaleEngine *tracingScaleEngine;
	SamplingScaleEngine *samplingScaleEngine;
    } my;
};

#endif	// CHART_H
