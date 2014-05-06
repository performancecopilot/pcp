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
    ~ExportDialog();

    virtual void init();
    virtual void reset();
    virtual void flush();
    virtual void displayQualitySpinBox();
    virtual void displayQualitySlider();

    static bool exportFile(QString &file, const char *format,
			   int quality, int width, int height,
			   bool transparent, bool everything);
    static int exportFile(char *outfile, char *geometry, bool transparent);

public slots:
    virtual void selectedRadioButton_clicked();
    virtual void allChartsRadioButton_clicked();
    virtual void quality_valueChanged(int);
    virtual void filePushButton_clicked();
    virtual void formatComboBox_currentIndexChanged(QString);

protected slots:
    virtual void languageChange();

private:
    QSize imageSize();

    struct {
	int quality;
	char *format;
    } my;
};

class ExportFileDialog : public QFileDialog
{
    Q_OBJECT

public:
    ExportFileDialog(QWidget *);
};

#endif	// EXPORTDIALOG_H
