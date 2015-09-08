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
#ifndef QED_LABEL_H
#define QED_LABEL_H

#include <QtGui>
#include "qed_gadget.h"

class QedLabel : public QedGadget
{
    Q_OBJECT

public:
    QedLabel(QWidget *parent);
    QedLabel(QWidget *parent, int x, int y, const char *t, const char *font);

    void setOrientation(Qt::Orientation o) { my.oriented = o; }

private:
    struct {
	QString		text;
	QString		font;
	Qt::Orientation	oriented;
    } my;
};

#endif // QED_LABEL_H
