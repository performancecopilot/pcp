/*
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
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
#include "colorbutton.h"
#include <QtGui/QPainter>
#include <QtGui/QColorDialog>
#include <QtGui/QPaintEvent>

ColorButton::ColorButton(QWidget* parent) : QToolButton(parent)
{
    my.modified = false;
    my.color = Qt::white;
}

bool ColorButton::isSet()
{
    return my.color != Qt::white;
}

void ColorButton::paintEvent(QPaintEvent *e)
{
    QToolButton::paintEvent(e);
    QPainter p(this);
    QRect rect = contentsRect()&e->rect();
    p.fillRect(rect.adjusted(+4,+5,-4,-5), my.color);
}

void ColorButton::clicked()
{
    QColor color = QColorDialog::getColor(my.color);
    if (color.isValid()) {
	my.color = color;
	my.modified = true;
	update();
    }
}
