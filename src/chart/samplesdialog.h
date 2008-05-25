/*
 * Copyright (c) 2006-2008, Aconex.  All Rights Reserved.
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
#ifndef SAMPLESDIALOG_H
#define SAMPLESDIALOG_H

#include "ui_samplesdialog.h"

class SamplesDialog : public QDialog, public Ui::SamplesDialog
{
    Q_OBJECT

public:
    SamplesDialog(QWidget* parent);
    void reset(int, int);
    int samples();
    int visible();

public slots:
    void sampleValueChanged(int);
    void visibleValueChanged(int);

protected slots:
    void languageChange();

private:
    void displaySampleSlider();
    void displayVisibleSlider();
    void displaySampleCounter();
    void displayVisibleCounter();

    struct {
	int samples;
	int visible;
    } my;
};

#endif // SAMPLESDIALOG_H
