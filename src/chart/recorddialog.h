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

class Tab;

class RecordDialog : public QDialog, public Ui::RecordDialog
{
    Q_OBJECT

public:
    RecordDialog(QWidget* parent);

    virtual void init(Tab *tab);
    virtual void displayDeltaText();
    virtual bool saveFolio(QString, QString);
    virtual bool saveConfig(QString, QString);
    virtual void startLoggers();

public slots:
    virtual void deltaUnitsComboBox_activated(int);
    virtual void selectedRadioButton_clicked();
    virtual void allChartsRadioButton_clicked();
    virtual void viewPushButton_clicked();
    virtual void folioPushButton_clicked();
    virtual void archivePushButton_clicked();
    virtual void buttonOk_clicked();

protected slots:
    virtual void languageChange();

private:
    typedef enum {
	Milliseconds,	Seconds,
	Minutes,	Hours,
	Days,		Weeks,
    } DeltaUnits;

    double secondsToUnits(double value);
    QString extractDeltaString();

    struct {
	Tab *tab;
	DeltaUnits units;
	QString delta;
	QString folio;
	QStringList hosts;
	QStringList archives;
    } my;
};

class Chart;

class PmLogger : public QProcess
{
    Q_OBJECT

public:
    PmLogger(QObject *parent);
    void init(Tab *tab, QString host, QString log);
    QString configure(Chart *cp);

public slots:
    void terminate();
    void finished(int, QProcess::ExitStatus);

private:
    struct {
	Tab *tab;
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
