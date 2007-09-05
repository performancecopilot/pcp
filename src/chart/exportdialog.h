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
#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include "ui_exportdialog.h"
#include <QtGui/QFileDialog>

class ExportDialog : public QDialog, public Ui::ExportDialog
{
    Q_OBJECT

public:
    ExportDialog(QWidget* parent);

    virtual void init();
    virtual void flush();
    virtual void displayQualityCounter();
    virtual void displayQualitySlider();

public slots:
    virtual void selectedRadioButtonClicked();
    virtual void allChartsRadioButtonClicked();
    virtual void filePushButtonClicked();
    virtual void qualityValueChanged( double value );

protected slots:
    virtual void languageChange();

private:
    struct {
	double quality;
    } my;
};

class ExportFileDialog : public QFileDialog
{
    Q_OBJECT

public:
    ExportFileDialog(QWidget *);
};

#endif	// EXPORTDIALOG_H
