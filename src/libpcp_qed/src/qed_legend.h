/*
 * Copyright (c) 2013-2014, Red Hat.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#ifndef QED_LEGEND_H
#define QED_LEGEND_H

#include <QtGui>

class QedThreshold
{
public:
    QedThreshold(double value, const QColor &color)
	{ my.value = value; my.color = color; }

    double value() const { return my.value; }
    QColor color() const { return my.color; }

private:
    struct {
	double	value;	// maximum value for range
	QColor	color;
    } my;
};

// Simple legend - a named list of value ranges,
// with colors associated with each value range.
//
class QedLegend : public QString
{
public:
    QedLegend();
    QedLegend(const char *legend);
    const char *identity() const;
    void setDefaultColor(const char *color);
    void addThreshold(double value, const char *color);
    void setThresholds(QStringList &tl);

private:
    struct {
	QColor			defaultColor;
	QList<QedThreshold>	thresholds;
    } my;
};

#endif // QED_LEGEND_H
