/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2006-2007, Aconex.  All Rights Reserved.
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

//
// Chart class ... multiple plots per chart, multiple charts per tab
//

#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtGui/QColor>
#include <QtGui/QTreeWidget>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_picker.h>
#include <qwt_double_rect.h>
#include <qmc_metric.h>

class Tab;

class Chart : public QwtPlot 
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

    void resetDataArrays(int m, int v);
    int addPlot(pmMetricSpec *, char *);
    int numPlot(void);
    void delPlot(int);
    char *title(void);			// return chart title
    void changeTitle(char *, int);	// NULL to clear
    void changeTitle(QString, int);
    Style style(void);			// return chart style
    void setStyle(Style);		// set chart style
    QString label(int);			// return legend label for ith plot
    void setLabel(int, QString);	// set plot legend label
    QColor color(int);			// return color for ith plot
    void setColor(int, QColor);		// set plot color
    void scale(bool *, double *, double *);
			// return autoscale state and fixed scale parameters
    void setScale(bool, double, double);
			// set autoscale state and fixed scale parameters
    void setYAxisTitle(char *);
    bool legendVisible();
    void setLegendVisible(bool);

    void update(bool, bool);

    QString name(int);
    char *legendSpec(int);
    QString metricName(int);
    QmcDesc *metricDesc(int);
    QmcContext *metricContext(int);

    QString pmloggerMetricSyntax(int);

    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const;
    void fixLegendPen(void);

    void setupTree(QTreeWidget *);
    void addToTree(QTreeWidget *, QString, const QmcContext *,
			  bool, bool, QColor&, QString&);

    static QColor defaultColor(int);

private slots:
    void selected(const QwtDoublePoint &);
    void moved(const QwtDoublePoint &);
    void showCurve(QwtPlotItem *, bool);

private:
    typedef struct {
	QmcMetric *metric;
	QwtPlotCurve *curve;
	QString name;
	char *legend;	// from config
	QString label;	// as appears in plot legend
	QColor color;
	double scale;
	double *data;
	double *plotData;
	int dataCount;
	bool removed;
    } Plot;

    void setColor(Plot *plot, QColor c);
    void setLabel(Plot *plot, QString s);
    void resetDataArrays(Plot *plot, int v);

    struct {
	Tab *tab;
	QList<Plot*> plots;
	char *title;
	Style style;
	bool autoScale;
	double yMin;
	double yMax;
	QwtPlotPicker *picker;
    } my;
};

#endif	// CHART_H
