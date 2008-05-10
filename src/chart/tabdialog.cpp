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

void TabDialog::reset(QString label, bool live, int samples, int visible, int index)
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
    my.samples = my.visible = 0;

    visibleCounter->setRange(KmChart::minimumPoints(), KmChart::maximumPoints());
    visibleSlider->setRange(KmChart::minimumPoints(), KmChart::maximumPoints());
    sampleCounter->setRange(KmChart::minimumPoints(), KmChart::maximumPoints());
    sampleSlider->setRange(KmChart::minimumPoints(), KmChart::maximumPoints());
    visibleCounter->setValue(visible);
    visibleSlider->setValue(visible);
    sampleCounter->setValue(samples);
    sampleSlider->setValue(samples);

    tabWidget->setCurrentIndex(index);

    console->post(KmChart::DebugUi,
			"TabDialog::reset arch=%s tot=%d/%d vis=%d/%d",
			my.archiveSource ? "true" : "false",
			samples, my.samples, visible, my.visible);
}

bool TabDialog::isArchiveSource()
{
    console->post(KmChart::DebugUi, "TabDialog::isArchiveSource archive=%s",
		  	my.archiveSource ? "true" : "false");
    return my.archiveSource;
}

void TabDialog::liveHostRadioButtonClicked()
{
    if (labelLineEdit->text() == tr("Archive"))
	labelLineEdit->setText(tr("Live"));
    liveHostRadioButton->setChecked(true);
    archivesRadioButton->setChecked(false);
    my.archiveSource = false;
    console->post(KmChart::DebugUi,
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
    console->post(KmChart::DebugUi,
		  "TabDialog::archivesRadioButtonClicked archive=%s",
		  my.archiveSource ? "true" : "false");
}

void TabDialog::sampleValueChanged(int value)
{
    if (my.samples != value) {
	my.samples = value;
	displaySampleCounter();
	displaySampleSlider();
	if (my.visible > my.samples)
	    visibleSlider->setValue(value);
    }
}

void TabDialog::visibleValueChanged(int value)
{
    if (my.visible != value) {
	my.visible = value;
	displayVisibleCounter();
	displayVisibleSlider();
	if (my.visible > my.samples)
	    sampleSlider->setValue(value);
    }
}

void TabDialog::displaySampleSlider()
{
    sampleSlider->blockSignals(true);
    sampleSlider->setValue(my.samples);
    sampleSlider->blockSignals(false);
}

void TabDialog::displayVisibleSlider()
{
    visibleSlider->blockSignals(true);
    visibleSlider->setValue(my.visible);
    visibleSlider->blockSignals(false);
}

void TabDialog::displaySampleCounter()
{
    sampleCounter->blockSignals(true);
    sampleCounter->setValue(my.samples);
    sampleCounter->blockSignals(false);
}

void TabDialog::displayVisibleCounter()
{
    visibleCounter->blockSignals(true);
    visibleCounter->setValue(my.visible);
    visibleCounter->blockSignals(false);
}
