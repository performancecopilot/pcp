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
#ifndef RECORDDIALOG_H
#define RECORDDIALOG_H

#include "ui_recorddialog.h"
#include <QtCore/QProcess>
#include <QtGui/QFileDialog>

class RecordDialog : public QDialog, public Ui::RecordDialog
{
    Q_OBJECT

public:
    RecordDialog(QWidget* parent);

    virtual void init();
    virtual void displayDeltaText();
    virtual int saveFolio(QString, QString);
    virtual int saveConfig(QString, QString);
    virtual void extractDeltaString();
    virtual void startLoggers();

public slots:
    virtual void deltaUnitsComboBoxActivated(int);
    virtual void selectedRadioButtonClicked();
    virtual void allChartsRadioButtonClicked();
    virtual void viewPushButtonClicked();
    virtual void folioPushButtonClicked();
    virtual void archivePushButtonClicked();

protected slots:
    virtual void languageChange();

private:
    typedef enum {
	Milliseconds,	Seconds,
	Minutes,	Hours,
	Days,		Weeks,
    } DeltaUnits;

    double secondsToUnits(double value);

    struct {
	DeltaUnits units;
	QStringList hosts;
	QString deltaString;
    } my;
};

class Chart;

class PmLogger : public QProcess
{
    Q_OBJECT

public:
    PmLogger(QObject *parent);
    void init(QString host, QString log);
    QString configure(Chart *cp);
    void setTerminating() { my.terminating = true; }

private slots:
    void finished(int, QProcess::ExitStatus);

private:
    struct {
	QString host;
	QString logfile;
	bool terminating;
    } my;
};

class RecordFileDialog : public QFileDialog, public Ui::RecordDialog
{
    Q_OBJECT

public:
    RecordFileDialog(QWidget* parent);
    void setFileName(QString);
};

#endif	// RECORDDIALOG_H
