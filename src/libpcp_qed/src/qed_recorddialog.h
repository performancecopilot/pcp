/*
 * Copyright (c) 2014, Red Hat.
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
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
#ifndef QED_RECORDDIALOG_H
#define QED_RECORDDIALOG_H

#include "ui_qed_recorddialog.h"
#include <QtCore/QProcess>
#include <QtGui/QFileDialog>
#include "qmc_time.h"

class QedViewControl;

class QedRecordDialog : public QDialog, public Ui::QedRecordDialog
{
    Q_OBJECT

public:
    QedRecordDialog();

    virtual void init(QedViewControl *, double);
    virtual bool saveFolio(QString, QString);
    virtual bool saveConfig(QString, QString);
    virtual void startLoggers();

public slots:
    virtual void deltaUnitsComboBox_activated(int);
    virtual void selectedRadioButton_clicked();
    virtual void allGadgetsRadioButton_clicked();
    virtual void viewPushButton_clicked();
    virtual void folioPushButton_clicked();
    virtual void archivePushButton_clicked();
    virtual void buttonOk_clicked();

protected slots:
    virtual void languageChange();

private:
    struct {
	QedViewControl *view;
	QString delta;
	double userDelta;
	QmcTime::DeltaUnits units;

	QString viewName;
	QString folioName;
	QStringList hosts;
	QStringList archives;
    } my;
};

class PmLogger : public QProcess
{
    Q_OBJECT

public:
    PmLogger(QObject *);
    void init(QedViewControl *view, QString host, QString log);
    QString host() { return my.host; }

public slots:
    void terminate();
    void finished(int, QProcess::ExitStatus);

private:
    struct {
	QedViewControl *view;
	QString host;
	QString logfile;
	bool terminating;
    } my;
};

class QedRecordFileDialog : public QFileDialog, public Ui::QedRecordDialog
{
    Q_OBJECT

public:
    QedRecordFileDialog(QWidget* parent);
    void setFileName(QString);
};

#endif	// QED_RECORDDIALOG_H
