/*
 * Copyright (c) 2014, Red Hat.
 * Copyright (c) 2006-2009, Aconex.  All Rights Reserved.
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
#ifndef QED_VIEWCONTROL_H
#define QED_VIEWCONTROL_H

#include <QtCore/QList>
#include <QtGui/QAction>
#include "qed_groupcontrol.h"

class PmLogger;

class QedViewControl
{
public:
    QedViewControl();
    virtual ~QedViewControl();

    void init(QedGroupControl *, QMenu *, QString, double);
    QedGroupControl *group() const { return my.group; }
    bool isArchiveSource();	// query if tab is for archives

    QAction *action() const { return my.action; }

    QString view() const { return my.view; }
    QString title() const { return my.title; }
    void setTitle(QString &text) { my.title = text; my.action->setText(text); }

    void addFolio(QString, QString);
    void addLogger(PmLogger *, QString);
    QStringList &archiveList() { return my.archiveList; }

    virtual QStringList hostList(bool) = 0;
    virtual QString pmloggerSyntax(bool) = 0;
    virtual bool saveConfig(QString, bool, bool, bool, bool) = 0;

    bool isRecording();
    bool startRecording();
    void cleanupRecording();
    bool queryRecording(QString &);
    bool stopRecording(QString &);
    bool detachLoggers(QString &);

private:
    struct {
	double delta;			// default recording interval
	QString title;
	QAction *action;
	QedGroupControl *group;

	bool recording;			// running any pmlogger's?
	QString view;
	QString folio;
	QStringList archiveList;	// list of archive names
	QList<PmLogger*> loggerList;	// list of pmloggers for our View
    } my;
};

#endif	// QED_VIEWCONTROL_H
