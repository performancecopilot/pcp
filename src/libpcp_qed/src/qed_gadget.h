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
#ifndef QED_GADGET_H
#define QED_GADGET_H

#include <QtGui>

class QedGadget : public QWidget
{
    Q_OBJECT

public:
    QedGadget(QWidget *parent);
    void dump(FILE *f);

private:
    struct {
	int	depth;	// z-axis setting for rendering
    } my;
};

#endif // QED_GADGET_H
