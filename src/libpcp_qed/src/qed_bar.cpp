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
#include "qed_bar.h"

QedBar::QedBar(QWidget *parent, int x, int y, int w, int h) : QedGadget(parent)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}

void
QedBar::setMinimum(double minimum)
{
    (void)minimum;
}

void
QedBar::setMaximum(double maximum)
{
    (void)maximum;
}

void
QedBar::setColor(const char *color)
{
    (void)color;
}

void
QedBar::setOrientation(Qt::Orientation o)
{
    (void)o;
}

void
QedBar::setScaleRange(int range)
{
    (void)range;
}

void
QedBar::paintEvent(QPaintEvent *event)
{
    (void)event;
}

void
QedBar::resizeEvent(QResizeEvent *event)
{
    (void)event;
}

QedMultiBar::QedMultiBar(QWidget *parent, int x, int y, int w, int h,
		QedColorList *l, int history) : QedBar(parent, x, y, w, h)
{
    (void)history;
    (void)l;
}

void
QedMultiBar::setOutline(bool on)
{
    (void)on;
}

void
QedMultiBar::setMaximum(double maximum, bool on)
{
    (void)maximum;
    (void)on;
}

QedBarGraph::QedBarGraph(QWidget *parent, int x, int y, int w, int h, int history)
	: QedBar(parent, x, y, w, h)
{
    (void)history;
}

void
QedBarGraph::clipRange(void)
{
}
