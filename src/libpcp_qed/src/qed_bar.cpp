/*
 * Copyright (c) 2013-2014, Red Hat.
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
#include "qed_bar.h"

QedBar::QedBar(QWidget *parent, int x, int y, int w, int h) : QedGadget(parent)
{
}

void
QedBar::setMinimum(double miniumum)
{
}

void
QedBar::setMaximum(double maximum)
{
}

void
QedBar::setColor(const char *color)
{
}

void
QedBar::setOrientation(Qt::Orientation o)
{
}

void
QedBar::setScaleRange(int range)
{
}

void
QedBar::paintEvent(QPaintEvent *event)
{
}

void
QedBar::resizeEvent(QResizeEvent *event)
{
}

QedMultiBar::QedMultiBar(QWidget *parent, int x, int y, int w, int h,
		QedColorList *l, int history) : QedBar(parent, x, y, w, h)
{
}

void
QedMultiBar::setOutline(bool on)
{
}

void
QedMultiBar::setMaximum(double maximum, bool on)
{
}

QedBarGraph::QedBarGraph(QWidget *parent, int x, int y, int w, int h, int history)
	: QedBar(parent, x, y, w, h)
{
}

void
QedBarGraph::clipRange(void)
{
}
