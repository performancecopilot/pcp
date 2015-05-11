/*
 * Copyright (c) 2012-2015, Red Hat.
 * Copyright (c) 2008, Aconex.  All Rights Reserved.
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
#ifndef GADGET_H
#define GADGET_H

#include <QtGui/QColor>
#include <QtGui/QWidget>
#include <QtGui/QPainter>
#include "qmc_metric.h"

class Gadget
{
public:
    Gadget(QWidget *);
    virtual ~Gadget() { }
    virtual void resetFont() { }
    virtual void setCurrent(bool) { }
    virtual void setScheme(const QString &s) { my.scheme = s; }
    virtual QString scheme() const { return my.scheme; }

    virtual void updateBackground(QColor) { }
    virtual void updateValues(bool, bool, int, int, double, double, double) { }

    virtual void resetValues(int, double, double) { }
    virtual void adjustValues() { }
    virtual void preserveSample(int, int) { }
    virtual void punchoutSample(int) { }

    virtual void showWidget() { return my.widget->show(); }
    virtual int width() const { return my.widget->width(); }
    virtual int height() const { return my.widget->height(); }
    virtual QSize size() const { return my.widget->size(); }

    virtual void activateTime(QMouseEvent *) { }
    virtual void reactivateTime(QMouseEvent *) { }
    virtual void deactivateTime(QMouseEvent *) { }

    virtual void save(FILE *, bool) { }
    virtual void print(QPainter *, QRect &, bool) { }

    virtual int metricCount() const { return 0; }
    virtual bool activeMetric(int) const { return true; }
    virtual QmcMetric *metricPtr(int) const { return NULL; }
    virtual QmcDesc *metricDesc(int) const { return NULL; }
    virtual QString metricInstance(int) const { return QString::null; }
    virtual QmcContext *metricContext(int) const { return NULL; }

    virtual QStringList hosts();	// unique hostnames across all metrics
    virtual QString pmloggerSyntax();	// pmlogger config text for all metrics
    virtual QString pmloggerMetricSyntax(int);	// config text for 1 metric

private:
    struct {
	QWidget		*widget;
	QString		scheme;
    } my;
};

#endif	// GADGET_H
