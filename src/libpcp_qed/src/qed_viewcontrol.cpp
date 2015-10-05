/*
 * Copyright (c) 2014, Red Hat.
 * Copyright (c) 2007-2009, Aconex.  All Rights Reserved.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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

#include <QtGui/QMenu>
#include <QtGui/QMessageBox>
#include "qed_console.h"
#include "qed_viewcontrol.h"
#include "qed_recorddialog.h"

QedViewControl::QedViewControl()
{
    my.recording = false;
    my.action = NULL;
    my.group = NULL;
}

QedViewControl::~QedViewControl()
{
    if (my.action)
	delete my.action;
}

void QedViewControl::init(QedGroupControl *group, QMenu *menu, QString title, double delta)
{
    my.delta = delta;
    my.title = title;
    my.group = group;
    my.action = new QAction(title, menu);
}

bool QedViewControl::isArchiveSource(void)
{
    return my.group->isArchiveSource();
}

bool QedViewControl::isRecording(void)
{
    return my.recording;
}

void QedViewControl::addFolio(QString folio, QString view)
{
    my.view = view;
    my.folio = folio;
}

void QedViewControl::addLogger(PmLogger *pmlogger, QString archive)
{
    my.loggerList.append(pmlogger);
    my.archiveList.append(archive);
}

bool QedViewControl::startRecording(void)
{
    QedRecordDialog record;

    console->post("QedView::startRecording");
    record.init(this, my.delta);
    if (record.exec() != QDialog::Accepted)
	my.recording = false;
    else {	// write pmlogger/pmchart/pmafm configs and start up loggers.
	console->post("QedView::startRecording starting loggers");
	record.startLoggers();
	my.recording = true;
    }
    return my.recording;
}

bool QedViewControl::stopRecording(QString &errmsg)
{
    QString msg = "Q\n";
    int count = my.loggerList.size();
    bool error = false;

    console->post("QedView::stopRecording stopping %d logger(s)", count);
    for (int i = 0; i < count; i++) {
	if (my.loggerList.at(i)->state() == QProcess::NotRunning) {
	    errmsg.append(QApplication::tr(
			"Record process (pmlogger) failed for host: "));
	    errmsg.append(my.loggerList.at(i)->host());
	    errmsg.append("\n");
	    error = true;
	}
	else {
	    my.loggerList.at(i)->write(msg.toAscii());
	    my.loggerList.at(i)->terminate();
	}
    }
    return error;
}

void QedViewControl::cleanupRecording(void)
{
    my.recording = false;
    my.loggerList.clear();
    my.archiveList.clear();
    my.view = QString::null;
    my.folio = QString::null;
}

bool QedViewControl::queryRecording(QString &errmsg)
{
    QString msg = "?\n";
    bool error = false;
    int i, count = my.loggerList.size();

    console->post("QedView::stopRecording querying %d logger(s)", count);
    for (i = 0; i < count; i++) {
	if (my.loggerList.at(i)->state() == QProcess::NotRunning) {
	    errmsg.append(QApplication::tr(
			"Record process (pmlogger) failed for host: "));
	    errmsg.append(my.loggerList.at(i)->host());
	    errmsg.append("\n");
	    error = true;
	}
	else {
	    my.loggerList.at(i)->write(msg.toAscii());
	}
    }

    if (error) {
	msg = "Q\n";    // if one fails, we shut down all loggers
	for (i = 0; i < count; i++)
	    my.loggerList.at(i)->write(msg.toAscii());
	cleanupRecording();
    }

    return error;
}

bool QedViewControl::detachLoggers(QString &errmsg)
{
    QString msg = "D\n";
    bool error = false;
    int count = my.loggerList.size();

    console->post("QedView::detachLoggers detaching %d logger(s)", count);
    for (int i = 0; i < count; i++) {
	if (my.loggerList.at(i)->state() == QProcess::NotRunning) {
	    errmsg.append(QApplication::tr(
			"Record process (pmlogger) failed for host: "));
	    errmsg.append(my.loggerList.at(i)->host());
	    errmsg.append("\n");
	    error = true;
	}
	else {
	    my.loggerList.at(i)->write(msg.toAscii());
	}
    }
    if (error)
	cleanupRecording();
    return error;
}
