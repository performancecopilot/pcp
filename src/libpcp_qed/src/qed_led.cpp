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
#include "qed_led.h"

QedLED::QedLED(QWidget *parent, QColor color) : QedGadget(parent)
{
    const qreal baseBound = 32.0;
    const int iBaseBound = (int)(baseBound+0.5);

    my.color = color;
    my.bound = baseBound;
    setMinimumSize(iBaseBound, iBaseBound);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

QedLED::QedLED(QWidget *parent, int x, int y, int w, int h, QedLegend *color) : QedGadget(parent)
{
    // TODO
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)color;
}

void QedLED::resizeEvent(QResizeEvent *event)
{
    QSize size = event->size();
    my.bound = qMin(size.height(), size.width());
}

void QedRoundLED::paintEvent(QPaintEvent *event)
{
    qreal lowerInset, upperInset;
    (void)event;

    QPainterPath baseCirclePath;
    QRectF baseBox(0.0, 0.0, my.bound, my.bound);
    baseCirclePath.arcTo(baseBox, 0.0, 360.0);
    baseCirclePath.closeSubpath();
    QLinearGradient baseGradient(0, 0, 0, 1.33846 * my.bound);
    baseGradient.setColorAt(0.0, Qt::lightGray);
    baseGradient.setColorAt(1.0, Qt::white);

    QPainterPath seatCirclePath;
    lowerInset = 0.06666 * my.bound;
    upperInset = my.bound - (lowerInset * 2.0);
    QRectF seatBox(lowerInset, lowerInset, upperInset, upperInset);
    seatCirclePath.arcTo(seatBox, 0.0, 360.0);
    seatCirclePath.closeSubpath();
    QLinearGradient seatGradient(0, 0, 0, 0.7692 * my.bound);
    seatGradient.setColorAt(0.0, Qt::lightGray);
    seatGradient.setColorAt(1.0, Qt::darkGray);

    QPainterPath mainCirclePath;
    lowerInset = 0.1 * my.bound;
    upperInset = my.bound - (lowerInset * 2.0);
    QRectF mainBox(lowerInset, lowerInset, upperInset, upperInset);
    mainCirclePath.arcTo(mainBox, 0.0, 360.0);
    mainCirclePath.closeSubpath();
    QLinearGradient mainGradient(0, 0, 0, 1.23076 * my.bound);
    mainGradient.setColorAt(0.0, my.color);
    mainGradient.setColorAt(1.0, Qt::white);

    QPainterPath reflectCirclePath;
    QRectF reflectBox(0.18538 * my.bound, 0.12969 * my.bound,
		      0.61384 * my.bound, 0.48615 * my.bound);
    reflectCirclePath.arcTo(reflectBox, 0.0, 360.0);
    reflectCirclePath.closeSubpath();
    QLinearGradient reflectGradient(0, 0, 0, 1.22953 * my.bound);
    reflectGradient.setColorAt(0.0, Qt::white);
    reflectGradient.setColorAt(1.0, my.color);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    painter.setBrush(baseGradient);
    painter.drawPath(baseCirclePath);
    painter.setBrush(seatGradient);
    painter.drawPath(seatCirclePath);
    painter.setBrush(mainGradient);
    painter.drawPath(mainCirclePath);
    painter.setBrush(reflectGradient);
    painter.drawPath(reflectCirclePath);
}

void QedSquareLED::paintEvent(QPaintEvent *event)
{
    qreal lowerInset, upperInset, radius = my.bound / 16.0;
    (void)event;

    QPainterPath baseSquarePath;
    QRectF baseBox(0.0, 0.0, my.bound, my.bound);
    baseSquarePath.addRoundedRect(baseBox, radius, radius);
    baseSquarePath.closeSubpath();
    QLinearGradient baseGradient(0, 0, 0, 1.33846 * my.bound);
    baseGradient.setColorAt(0.0, Qt::lightGray);
    baseGradient.setColorAt(1.0, Qt::white);

    QPainterPath seatSquarePath;
    lowerInset = 0.06666 * my.bound;
    upperInset = my.bound - (lowerInset * 2.0);
    QRectF seatBox(lowerInset, lowerInset, upperInset, upperInset);
    seatSquarePath.addRoundedRect(seatBox, radius, radius);
    seatSquarePath.closeSubpath();
    QLinearGradient seatGradient(0, 0, 0, 0.7692 * my.bound);
    seatGradient.setColorAt(0.0, Qt::lightGray);
    seatGradient.setColorAt(1.0, Qt::darkGray);

    QPainterPath mainSquarePath;
    lowerInset = 0.1 * my.bound;
    upperInset = my.bound - (lowerInset * 2.0);
    QRectF mainBox(lowerInset, lowerInset, upperInset, upperInset);
    mainSquarePath.addRoundedRect(mainBox, radius, radius);
    mainSquarePath.closeSubpath();
    QLinearGradient mainGradient(0, 0, 0, 1.23076 * my.bound);
    mainGradient.setColorAt(0.0, my.color);
    mainGradient.setColorAt(1.0, Qt::white);

    QPainterPath reflectSquarePath;
    QRectF reflectBox(0.13538 * my.bound, 0.12969 * my.bound,
		      0.73084 * my.bound, 0.48615 * my.bound);
    reflectSquarePath.addRoundedRect(reflectBox, radius, radius);
    reflectSquarePath.closeSubpath();
    QLinearGradient reflectGradient(0, 0, 0, 1.22953 * my.bound);
    reflectGradient.setColorAt(0.0, Qt::white);
    reflectGradient.setColorAt(1.0, my.color);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    painter.setBrush(baseGradient);
    painter.drawPath(baseSquarePath);
    painter.setBrush(seatGradient);
    painter.drawPath(seatSquarePath);
    painter.setBrush(mainGradient);
    painter.drawPath(mainSquarePath);
    painter.setBrush(reflectGradient);
    painter.drawPath(reflectSquarePath);
}
