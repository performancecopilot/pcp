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
#ifndef COLORBUTTON_H
#define COLORBUTTON_H

#include <QToolButton>

class ColorButton : public QToolButton
{
    Q_OBJECT

public:
    ColorButton(QWidget* parent);

    bool isSet();
    bool isModified() { return my.modified; }

    QColor color() { return my.color; }
    void setColor(QColor color) { my.color = color; update(); }

public slots:
    virtual void clicked();
    virtual void paintEvent(QPaintEvent *);

private:
    struct {
	QColor color;
	bool modified;
    } my;
};

#endif // COLORBUTTON_H
