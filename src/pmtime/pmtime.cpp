/*
 * Copyright (c) 2014, Red Hat.
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
#include "pmtime.h"
#include <QUrl>
#include <QLibraryInfo>
#include <QDesktopServices>
#include <QWhatsThis>
#include <QMessageBox>
#include <QCloseEvent>
#include "console.h"
#include "aboutdialog.h"
#include "seealsodialog.h"
#include <pcp/pmapi.h>

PmTime::PmTime() : QMainWindow(NULL)
{
}

void PmTime::helpAbout()
{
    AboutDialog about(this);
    about.exec();
}

void PmTime::helpAboutQt()
{
    QApplication::aboutQt();
}

void PmTime::helpSeeAlso()
{
    SeeAlsoDialog about(this);
    about.exec();
}

void PmTime::whatsThis()
{
    QWhatsThis::enterWhatsThisMode();
}

void PmTime::helpManual()
{
    bool ok;
    QString documents("file://");
    QString separator = QString(pmPathSeparator());
    documents.append(pmGetConfig("PCP_HTML_DIR"));
    documents.append(separator).append("timecontrol.html");
    ok = QDesktopServices::openUrl(QUrl(documents, QUrl::TolerantMode));
    if (!ok) {
        documents.prepend("Failed to open:\n");
        QMessageBox::warning(this, pmGetProgname(), documents);
    }
}

void PmTime::hideWindow()
{
    if (isVisible())
	hide();
    else {
	show();
	raise();
    }
}

void PmTime::popup(bool hello_popetts)
{
    if (!hello_popetts)
	hide();
    else {
	show();
	raise();
    }
}

void PmTime::closeEvent(QCloseEvent *ce)
{
    hide();
    ce->ignore();
}

void PmTime::showConsole()
{
    console->show();
}
