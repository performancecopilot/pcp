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
#include "infodialog.h"
#include <QMessageBox>
#include "main.h"

InfoDialog::InfoDialog(QWidget* parent) : QDialog(parent)
{
    setupUi(this);
    my.pminfoStarted = false;
    my.pmvalStarted = false;
}

void InfoDialog::languageChange()
{
    retranslateUi(this);
}

void InfoDialog::reset(QString source, QString metric, QString instance,
			int sourceType)
{
    pminfoTextEdit->setText(tr(""));
    pmvalTextEdit->setText(tr(""));
    my.pminfoStarted = false;
    my.pmvalStarted = false;
    my.metric = metric;
    my.source = source;
    my.sourceType = sourceType;
    my.instance = instance;

    infoTab->setCurrentWidget(pminfoTab);
    infoTabCurrentChanged(0);
}

void InfoDialog::pminfo(void)
{
    my.pminfoProc = new QProcess(this);
    QStringList arguments;

    arguments << "-df";
    switch (my.sourceType) {
    case PM_CONTEXT_ARCHIVE:
	arguments << "-a";
	arguments << my.source;
	// no help text in archive mode
	break;
    case PM_CONTEXT_LOCAL:
	arguments << "-L";
	// no host name in local mode
	arguments << "-tT";
	break;
    default:
	arguments << "-h";
	arguments << my.source;
	arguments << "-tT";
	break;
    }
    arguments << my.metric;

    connect(my.pminfoProc, SIGNAL(readyReadStandardOutput()),
	    this, SLOT(pminfoStdout()));
    connect(my.pminfoProc, SIGNAL(readyReadStandardError()),
	    this, SLOT(pminfoStderr()));

    my.pminfoProc->start("pminfo", arguments);
}

void InfoDialog::pminfoStdout()
{
    QString s(my.pminfoProc->readAllStandardOutput());
    pminfoTextEdit->append(s);
}

void InfoDialog::pminfoStderr()
{
    QString s(my.pminfoProc->readAllStandardError());
    pminfoTextEdit->append(s);
}

void InfoDialog::pmval(void)
{
    QStringList arguments;
    QString port;
    port.setNum(pmtime->port());

    my.pmvalProc = new QProcess(this);
    arguments << "-f4" << "-p" << port;
    if (my.sourceType == PM_CONTEXT_ARCHIVE)
	arguments << "-a" << my.source;
    else if (my.sourceType == PM_CONTEXT_LOCAL)
	arguments << "-L";
    else
	arguments << "-h" << my.source;
    arguments << my.metric;

    connect(my.pmvalProc, SIGNAL(readyReadStandardOutput()),
		    this, SLOT(pmvalStdout()));
    connect(my.pmvalProc, SIGNAL(readyReadStandardError()),
		    this, SLOT(pmvalStderr()));
    connect(this, SIGNAL(finished(int)), this, SLOT(quit()));
    my.pmvalProc->start("pmval", arguments);
}

void InfoDialog::quit()
{
    if (my.pmvalStarted) {
	my.pmvalProc->terminate();
	my.pmvalStarted = false;
    }
    if (my.pminfoStarted) {
	my.pminfoProc->terminate();
	my.pminfoStarted = false;
    }
}

void InfoDialog::pmvalStdout()
{
    QString s(my.pmvalProc->readAllStandardOutput());
    s = s.trimmed();
    pmvalTextEdit->append(s);
}

void InfoDialog::pmvalStderr()
{
    QString s(my.pmvalProc->readAllStandardError());
    s = s.trimmed();
    s.prepend("<b>");
    s.append("</b>");
    pmvalTextEdit->append(s);
}

void InfoDialog::infoTabCurrentChanged(int)
{
    if (infoTab->currentWidget() == pminfoTab) {
	if (!my.pminfoStarted) {
	    pminfo();
	    my.pminfoStarted = true;
	}
    }
    else if (infoTab->currentWidget() == pmvalTab) {
	if (!my.pmvalStarted) {
	    pmval();
	    my.pmvalStarted = true;
	}
    }
}
