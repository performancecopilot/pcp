/*
 * Copyright (c) 2007-2008, Aconex.  All Rights Reserved.
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
#include "main.h"
#include <QtGui/QMessageBox>
#include "openviewdialog.h"
#include "recorddialog.h"

Tab::Tab(): QWidget(NULL)
{
    my.currentGadget = -1;
    my.recording = false;
    my.splitter = NULL;
    my.group = NULL;
}

void Tab::init(QTabWidget *tab, GroupControl *group, QString label)
{
    my.splitter = new QSplitter(tab);
    my.splitter->setOrientation(Qt::Vertical);
    my.splitter->setSizePolicy(QSizePolicy::MinimumExpanding,
				QSizePolicy::MinimumExpanding);
    tab->blockSignals(true);
    tab->addTab(my.splitter, label);
    tab->blockSignals(false);
    my.group = group;
}

void Tab::addGadget(Gadget *gadget)
{
    if (gadgetCount())
	pmchart->updateHeight(PmChart::minimumChartHeight());
    my.gadgetsList.append(gadget);
    if (my.currentGadget == -1)
	setCurrentGadget(gadgetCount() - 1);
    gadget->showWidget();
    console->post("Tab::addChart: [%d]->Chart %p", my.currentGadget, gadget);
}

int Tab::deleteCurrent(void)
{
    return deleteGadget(my.currentGadget);
}

int Tab::deleteGadget(Gadget *gadget)
{
    for (int i = 0; i < gadgetCount(); i++)
	if (my.gadgetsList.at(i) == gadget)
	    return deleteGadget(i);
    return 0;
}

int Tab::deleteGadget(int index)
{
    Gadget *gadget = my.gadgetsList.at(index);
    int newCurrent = my.currentGadget;
    int oldCurrent = my.currentGadget;
    int oldCount = gadgetCount();
    int height = gadget->height();

    if (index < oldCurrent || index == oldCount - 1)
	newCurrent--;
    if (newCurrent < 0)
	my.currentGadget = -1;
    else
	setCurrent(my.gadgetsList.at(newCurrent));

    my.group->deleteGadget(gadget);
    my.gadgetsList.removeAt(index);
    delete gadget;

    if (gadgetCount() > 0)
	pmchart->updateHeight(-height);

    return my.currentGadget;
}

int Tab::gadgetCount(void)
{
    return my.gadgetsList.size();
}

int Tab::currentGadgetIndex(void)
{
    return my.currentGadget;
}

Gadget *Tab::gadget(int index)
{
    if (index >= gadgetCount())
	return NULL;
    return my.gadgetsList.at(index);
}

Gadget *Tab::currentGadget(void)
{
    if (my.currentGadget == -1)
	return NULL;
    return my.gadgetsList.at(my.currentGadget);
}

void Tab::setCurrent(Gadget *gadget)
{
    int index;

    for (index = 0; index < gadgetCount(); index++)
	if (my.gadgetsList.at(index) == gadget)
	    break;
    if (index == gadgetCount())
	abort();    // bad, bad, bad - attempted setCurrent on invalid gadget
    setCurrentGadget(index);
}

void Tab::setCurrentGadget(int index)
{
    if (index >= gadgetCount() || index == my.currentGadget)
        return;
    if (my.currentGadget >= 0)
        my.gadgetsList.at(my.currentGadget)->setCurrent(false);
    my.currentGadget = index;
    if (my.currentGadget >= 0)
        my.gadgetsList.at(my.currentGadget)->setCurrent(true);
}

void Tab::showGadgets()
{
    for (int index = 0; index < gadgetCount() - 1; index++)
	my.gadgetsList.at(index)->showWidget();
}

bool Tab::isArchiveSource(void)
{
    return my.group->isArchiveSource();
}

bool Tab::isRecording(void)
{
    return my.recording;
}

void Tab::addFolio(QString folio, QString view)
{
    my.view = view;
    my.folio = folio;
}

void Tab::addLogger(PmLogger *pmlogger, QString archive)
{
    my.loggerList.append(pmlogger);
    my.archiveList.append(archive);
}

bool Tab::startRecording(void)
{
    RecordDialog record(this);

    console->post("Tab::startRecording");
    record.init(this);
    if (record.exec() != QDialog::Accepted)
	my.recording = false;
    else {	// write pmlogger/pmchart/pmafm configs and start up loggers.
	console->post("Tab::startRecording starting loggers");
	record.startLoggers();
	my.recording = true;
    }
    return my.recording;
}

void Tab::stopRecording(void)
{
    QString msg = "Q\n", errmsg;
    int count = my.loggerList.size();
    int i, sts, error = 0;

    console->post("Tab::stopRecording stopping %d logger(s)", count);
    for (int i = 0; i < count; i++) {
	if (my.loggerList.at(i)->state() == QProcess::NotRunning) {
	    errmsg.append(tr("Record process (pmlogger) failed for host: "));
	    errmsg.append(my.loggerList.at(i)->host());
	    errmsg.append("\n");
	    error++;
	}
	else {
	    my.loggerList.at(i)->write(msg.toAscii());
	    my.loggerList.at(i)->terminate();
	}
    }

    for (i = 0; i < my.archiveList.size(); i++) {
	QString archive = my.archiveList.at(i);

	console->post("Tab::stopRecording opening archive %s",
			(const char *)archive.toAscii());
	if ((sts = archiveGroup->use(PM_CONTEXT_ARCHIVE, archive)) < 0) {
	    errmsg.append(tr("Cannot open PCP archive: "));
	    errmsg.append(archive);
	    errmsg.append("\n");
	    errmsg.append(tr(pmErrStr(sts)));
	    errmsg.append("\n");
	    error++;
	}
	else {
	    archiveGroup->updateBounds();
	    QmcSource source = archiveGroup->context()->source();
	    pmtime->addArchive(source.start(), source.end(),
				source.timezone(), source.host(), true);
	}
    }

    // If all is well, we can now create the new "Record" Tab.
    // Order of cleanup and changing Record mode state is different
    // in the error case to non-error case, this is important for
    // getting the window state correct (i.e. pmchart->enableUi()).

    if (error) {
	cleanupRecording();
	pmchart->setRecordState(false);
	QMessageBox::warning(this, pmProgname, errmsg,
		QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else {
	// Make the current Tab stop recording before changing Tabs
	pmchart->setRecordState(false);

	Tab *tab = new Tab;
	console->post("Tab::stopRecording creating tab: delta=%.2f pos=%.2f",
			tosec(*pmtime->archiveInterval()),
			tosec(*pmtime->archivePosition()));
	// TODO: may need to update archive samples/visible?
	tab->init(pmchart->tabWidget(), archiveGroup, "Record");
	pmchart->addActiveTab(tab);
	OpenViewDialog::openView((const char *)my.view.toAscii());
	cleanupRecording();
    }
}

void Tab::cleanupRecording(void)
{
    my.recording = false;
    my.loggerList.clear();
    my.archiveList.clear();
    my.view = QString::null;
    my.folio = QString::null;
}

void Tab::queryRecording(void)
{
    QString msg = "?\n", errmsg;
    int i, error = 0, count = my.loggerList.size();

    console->post("Tab::stopRecording querying %d logger(s)", count);
    for (i = 0; i < count; i++) {
	if (my.loggerList.at(i)->state() == QProcess::NotRunning) {
	    errmsg.append(tr("Record process (pmlogger) failed for host: "));
	    errmsg.append(my.loggerList.at(i)->host());
	    errmsg.append("\n");
	    error++;
	}
	else {
	    my.loggerList.at(i)->write(msg.toAscii());
	}
    }

    if (error) {
	msg = "Q\n";	// if one fails, we shut down all loggers
	for (i = 0; i < count; i++)
	    my.loggerList.at(i)->write(msg.toAscii());
	cleanupRecording();
	pmchart->setRecordState(false);
	QMessageBox::warning(this, pmProgname, errmsg,
		QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
}

void Tab::detachLoggers(void)
{
    QString msg = "D\n", errmsg;
    int error = 0, count = my.loggerList.size();

    console->post("Tab::detachLoggers detaching %d logger(s)", count);
    for (int i = 0; i < count; i++) {
	if (my.loggerList.at(i)->state() == QProcess::NotRunning) {
	    errmsg.append(tr("Record process (pmlogger) failed for host: "));
	    errmsg.append(my.loggerList.at(i)->host());
	    errmsg.append("\n");
	    error++;
	}
	else {
	    my.loggerList.at(i)->write(msg.toAscii());
	}
    }

    if (error) {
	cleanupRecording();
	pmchart->setRecordState(false);
	QMessageBox::warning(this, pmProgname, errmsg,
		QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else {
	pmchart->setRecordState(false);
	cleanupRecording();
    }
}
