/*
 * Copyright (c) 2007-2009, Aconex.  All Rights Reserved.
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
#include "view.h"
#include <QtGui/QMessageBox>
#include "recorddialog.h"

void View::init(SceneGroup *group, QMenu *menu, QString title)
{
    my.group = group;
    ViewControl::init(group, menu, title, globalSettings.loggerDelta);
}

QStringList View::hostList(bool selectedOnly)
{
    // TODO
    return QStringList();
}

QString View::pmloggerSyntax(bool selectedOnly)
{
    View *view = pmchart->activeView();
    QString configdata;

    if (selectedOnly)
	configdata.append(pmchart->activeGadget()->pmloggerSyntax());
    else
	for (int c = 0; c < view->gadgetCount(); c++)
	    configdata.append(gadget(c)->pmloggerSyntax());
    return configdata;
}


bool View::saveConfig(QString filename, bool hostDynamic,
			bool sizeDynamic, bool allViews, bool allCharts)
{
    return SaveViewDialog::saveView(filename,
				hostDynamic, sizeDynamic, allViews, allCharts);
}

bool View::stopRecording()
{
    QString errmsg;
    bool error = ViewControl::stopRecording(errmsg);
    QStringList archiveList = ViewControl::archiveList();

    for (int i = 0; i < archiveList.size(); i++) {
	QString archive = archiveList.at(i);
	int sts;

	console->post("View::stopRecording opening archive %s",
			(const char *)archive.toAscii());
	if ((sts = archiveGroup->use(PM_CONTEXT_ARCHIVE, archive)) < 0) {
	    errmsg.append(QApplication::tr("Cannot open PCP archive: "));
	    errmsg.append(archive);
	    errmsg.append("\n");
	    errmsg.append(pmErrStr(sts));
	    errmsg.append("\n");
	    error = true;
	}
	else {
	    archiveGroup->updateBounds();
	    QmcSource source = archiveGroup->context()->source();
	    pmtime->addArchive(source.start(), source.end(),
				source.timezone(), source.host(), true);
	}
    }

    // If all is well, we can now create the new "Record" View.
    // Order of cleanup and changing Record mode state is different
    // in the error case to non-error case, this is important for
    // getting the window state correct (i.e. pmchart->enableUi()).

    if (error) {
	cleanupRecording();
	pmchart->setRecordState(false);
	QMessageBox::warning(NULL, pmProgname, errmsg,
		QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else {
	// Make the current View stop recording before changing Views
	pmchart->setRecordState(false);

	View *view = new View;
	console->post("View::stopRecording creating view: delta=%.2f pos=%.2f",
			App::timevalToSeconds(*pmtime->archiveInterval()),
			App::timevalToSeconds(*pmtime->archivePosition()));
	// TODO: may need to update archive samples/visible?
	view->init(archiveGroup, pmchart->viewMenu(), "Record");
	pmchart->addActiveView(view);
	OpenViewDialog::openView((const char *)ViewControl::view().toAscii());
	cleanupRecording();
    }

    return error;
}

bool View::queryRecording(void)
{
    QString errmsg;
    bool error = ViewControl::queryRecording(errmsg);

    if (error) {
	pmchart->setRecordState(false);
	QMessageBox::warning(NULL, pmProgname, errmsg,
		QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    return error;
}

bool View::detachLoggers(void)
{
    QString errmsg;
    bool error = ViewControl::detachLoggers(errmsg);

    if (error) {
	pmchart->setRecordState(false);
	QMessageBox::warning(NULL, pmProgname, errmsg,
		QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		QMessageBox::NoButton, QMessageBox::NoButton);
    }
    else {
	pmchart->setRecordState(false);
	cleanupRecording();
    }
    return error;
}
