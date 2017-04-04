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

#include <QWidget>
#include "qed_line.h"

QedLine::QedLine(QWidget *parent, qreal length) : QedGadget(parent)
{
    setup();
    my.box = QRect(0, 0, length, 1);
    my.length = length * 1.0;
}

QedLine::QedLine(QWidget *parent, int x, int y, int w, int h) : QedGadget(parent)
{
    setup();
    my.box = QRect(x, y, w, h);
    my.length = QLineF(QLine(x, y, w, h)).length();
}

QedLine::QedLine(QWidget *parent, int length, Qt::Orientation oriented) : QedGadget(parent)
{
    setup();
    my.oriented = oriented;
    my.box = QRect(0, 0, length, 1);
    my.length = 1.0 * length;
}

void
QedLine::setup(void)
{
    const qreal baseBound = 32.0;
    const int iBaseBound = (int)(baseBound+0.5);
    my.oriented = Qt::Horizontal;
    my.bound = baseBound;
    setMinimumSize(iBaseBound, iBaseBound);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

void
QedLine::resizeEvent(QResizeEvent *event)
{
    QSize size = event->size();
    my.bound = qMin(size.height(), size.width());
}

void
QedLine::paintEvent(QPaintEvent *event)
{
    qreal lowerInset, upperInset, radius = my.bound / 16.0;
    (void)event;

    QPainterPath mainSquarePath;
    lowerInset = 0.3 * my.bound;
    upperInset = (my.bound * my.length) - (lowerInset * 2.0);
    QRectF mainBox(lowerInset, lowerInset,
		   upperInset, my.bound - (lowerInset * 2.0));
    mainSquarePath.addRoundedRect(mainBox, radius, radius);
    mainSquarePath.closeSubpath();
    QLinearGradient mainGradient(0, 0, 0, 1.0 * my.bound);
    mainGradient.setColorAt(0.0, Qt::lightGray);
    mainGradient.setColorAt(1.0, Qt::darkGray);

    QPainterPath reflectSquarePath;
    QRectF reflectBox(0.13538 * my.bound, 0.12969 * my.bound,
		      0.73084 * my.bound, 0.48615 * my.bound);
    reflectSquarePath.addRoundedRect(reflectBox, radius, radius);
    reflectSquarePath.closeSubpath();
    QLinearGradient reflectGradient(0, 0, 0, 1.22953 * my.bound);
    reflectGradient.setColorAt(0.0, Qt::white);
    reflectGradient.setColorAt(1.0, Qt::gray);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    painter.setBrush(mainGradient);
    painter.drawPath(mainSquarePath);
    painter.setBrush(reflectGradient);
    painter.drawPath(reflectSquarePath);
}
