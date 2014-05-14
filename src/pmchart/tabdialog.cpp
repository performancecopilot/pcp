/*
 * Copyright (c) 2007-2008, Aconex.  All Rights Reserved.
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
#include "tabdialog.h"
#include "main.h"

TabDialog::TabDialog(QWidget* parent) : QDialog(parent)
{
    setupUi(this);
}

void TabDialog::languageChange()
{
    retranslateUi(this);
}

void TabDialog::reset(QString label, bool live)
{
    if (label == QString::null) {
	setWindowTitle(tr("Add Tab"));
	labelLineEdit->setText(live ? tr("Live") : tr("Archive"));
    }
    else {
	setWindowTitle(tr("Edit Tab"));
	liveHostRadioButton->setEnabled(false);
	archivesRadioButton->setEnabled(false);
	labelLineEdit->setText(label);
    }

    liveHostRadioButton->setChecked(live);
    archivesRadioButton->setChecked(!live);

    my.archiveSource = !live;

    console->post(PmChart::DebugUi, "TabDialog::reset arch=%s",
			my.archiveSource ? "true" : "false");
}

bool TabDialog::isArchiveSource()
{
    return my.archiveSource;
}

QString TabDialog::label() const
{
    return labelLineEdit->text();
}

void TabDialog::liveHostRadioButtonClicked()
{
    if (labelLineEdit->text() == tr("Archive"))
	labelLineEdit->setText(tr("Live"));
    liveHostRadioButton->setChecked(true);
    archivesRadioButton->setChecked(false);
    my.archiveSource = false;
    console->post(PmChart::DebugUi,
		  "TabDialog::liveHostRadioButtonClicked archive=%s",
		  my.archiveSource ? "true" : "false");
}

void TabDialog::archivesRadioButtonClicked()
{
    if (labelLineEdit->text() == tr("Live"))
	labelLineEdit->setText(tr("Archive"));
    liveHostRadioButton->setChecked(false);
    archivesRadioButton->setChecked(true);
    my.archiveSource = true;
    console->post(PmChart::DebugUi,
		  "TabDialog::archivesRadioButtonClicked archive=%s",
		  my.archiveSource ? "true" : "false");
}
