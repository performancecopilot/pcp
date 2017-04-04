/*
 * Copyright (c) 2006-2008, Aconex.  All Rights Reserved.
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
#ifndef TAB_H
#define TAB_H

#include <QList>
#include <QSplitter>
#include <QTabWidget>
#include "groupcontrol.h"
#include "gadget.h"

class PmLogger;

class Tab : public QWidget
{
    Q_OBJECT

public:
    Tab();
    void init(QTabWidget *, GroupControl *, QString);
    QWidget *splitter() { return my.splitter; }
    GroupControl *group() { return my.group; }
    bool isArchiveSource();	// query if tab is for archives

    void addGadget(Gadget *);	// append gadget to the Tab, make it current
    int deleteCurrent();	// remove current gadget, return new current
    int deleteGadget(Gadget *);	// remove given gadget, return current index
    int deleteGadget(int);	// remove 'N'th gadget, return current index

    int gadgetCount();		// count of entries in the list of gadgets
    Gadget *gadget(int);	// gadget at specified list position
    Gadget *currentGadget();	// current gadget (can be NULL)
    void setCurrent(Gadget *);
    int currentGadgetIndex();	// current gadget index (can be -1)
    void setCurrentGadget(int);

    void showGadgets();

    void addFolio(QString, QString);
    void addLogger(PmLogger *, QString);

    bool isRecording();
    bool startRecording();
    void queryRecording();
    void stopRecording();
    void detachLoggers();

private:
    void cleanupRecording();

    struct {
	QSplitter *splitter;		// dynamically divides charts

	GroupControl *group;
	QList<Gadget*> gadgetsList;
	int currentGadget;

	bool recording;			// running any pmlogger's?
	QString view;
	QString folio;
	QList<QString> archiveList;	// list of archive names
	QList<PmLogger*> loggerList;	// list of pmloggers for our Tab
    } my;
};

#endif	// TAB_H
