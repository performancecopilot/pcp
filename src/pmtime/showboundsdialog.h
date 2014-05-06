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
#ifndef SHOWBOUNDS_H
#define SHOWBOUNDS_H

#include "ui_showboundsdialog.h"
#include "console.h"

class ShowBounds : public QDialog, public Ui::ShowBounds
{
    Q_OBJECT

public:
    ShowBounds(QWidget* parent);

    virtual void init(struct timeval *, struct timeval *,
		      struct timeval *, struct timeval *);
    virtual void reset();
    virtual void flush();
    virtual void displayStartText();
    virtual void displayEndText();
    virtual void displayStartSlider();
    virtual void displayEndSlider();

public slots:
    virtual void changedStart(double value);
    virtual void changedEnd(double value);
    virtual void accept();
    virtual void reject();

signals:
    void boundsChanged();

private:
    struct {
	double localCurrentStart;
	double localCurrentEnd;
	double localAbsoluteStart;
	double localAbsoluteEnd;
	struct timeval *currentStart;
	struct timeval *currentEnd;
	struct timeval *absoluteStart;
	struct timeval *absoluteEnd;
    } my;
};

#endif // SHOWBOUNDS_H
