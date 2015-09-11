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

#include "qed_legend.h"

QedLegend::QedLegend()
{
    // default is white when no _default appears in the legend ...
    my.defaultColor = Qt::white;
}

QedLegend::QedLegend(const char *name) : QString(name)
{
    QedLegend();
}

const char *
QedLegend::identity(void) const
{
    return this->toAscii();
}

void
QedLegend::setDefaultColor(const char *color)
{
    my.defaultColor = QColor(color);
}

void
QedLegend::setThresholds(QStringList &tl)
{
    // TODO: iterate over tl and build my.thresholds
    (void)tl;
}

void
QedLegend::addThreshold(double value, const char *color)
{
    // TODO: append to my.thresholds
    (void)value;
    (void)color;
}
