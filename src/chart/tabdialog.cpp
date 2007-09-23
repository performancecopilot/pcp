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

void TabDialog::reset(QString label, bool live, int samples, int visible)
{
    labelLineEdit->setText(label);
    if (label == QString::null)
	setWindowTitle(tr("Add Tab"));
    else {
	setWindowTitle(tr("Edit Tab"));
	liveHostRadioButton->setEnabled(false);
	archivesRadioButton->setEnabled(false);
    }

    liveHostRadioButton->setChecked(live);
    archivesRadioButton->setChecked(!live);

    my.archiveSource = !live;
    my.samples = my.visible = 0;

    samplePointsCounter->setValue(samples);
    samplePointsCounter->setRange(KmChart::minimumPoints, KmChart::maximumPoints);
    samplePointsSlider->setValue(samples);
    samplePointsSlider->setRange(KmChart::minimumPoints, KmChart::maximumPoints);
    visiblePointsCounter->setValue(visible);
    visiblePointsCounter->setRange(KmChart::minimumPoints, KmChart::maximumPoints);
    visiblePointsSlider->setValue(visible);
    visiblePointsSlider->setRange(KmChart::minimumPoints, KmChart::maximumPoints);

    console->post(KmChart::DebugGUI, "TabDialog::reset archive=%s",
					my.archiveSource?"true":"false");
}

bool TabDialog::isArchiveSource()
{
    console->post(KmChart::DebugGUI, "TabDialog::isArchiveSource archive=%s",
		  my.archiveSource?"true":"false");
    return my.archiveSource;
}

void TabDialog::liveHostRadioButtonClicked()
{
    liveHostRadioButton->setChecked(true);
    archivesRadioButton->setChecked(false);
    my.archiveSource = false;
    console->post(KmChart::DebugGUI,
		  "TabDialog::liveHostRadioButtonClicked archive=%s",
		  my.archiveSource?"true":"false");
}

void TabDialog::archivesRadioButtonClicked()
{
    liveHostRadioButton->setChecked(false);
    archivesRadioButton->setChecked(true);
    my.archiveSource = true;
    console->post(KmChart::DebugGUI,
		  "TabDialog::archivesRadioButtonClicked archive=%s",
		  my.archiveSource?"true":"false");
}

void TabDialog::samplePointsValueChanged(int value)
{
    if (my.samples != value) {
	my.samples = value;
	displaySamplePointsCounter();
	displaySamplePointsSlider();
	if (my.visible > my.samples)
	    visiblePointsSlider->setValue(value);
    }
}

void TabDialog::visiblePointsValueChanged(int value)
{
    if (my.visible != value) {
	my.visible = value;
	displayVisiblePointsCounter();
	displayVisiblePointsSlider();
	if (my.visible > my.samples)
	    samplePointsSlider->setValue(value);
    }
}

void TabDialog::displaySamplePointsSlider()
{
    samplePointsSlider->blockSignals(true);
    samplePointsSlider->setValue(my.samples);
    samplePointsSlider->blockSignals(false);
}

void TabDialog::displayVisiblePointsSlider()
{
    visiblePointsSlider->blockSignals(true);
    visiblePointsSlider->setValue(my.visible);
    visiblePointsSlider->blockSignals(false);
}

void TabDialog::displaySamplePointsCounter()
{
    samplePointsCounter->blockSignals(true);
    samplePointsCounter->setValue(my.samples);
    samplePointsCounter->blockSignals(false);
}

void TabDialog::displayVisiblePointsCounter()
{
    visiblePointsCounter->blockSignals(true);
    visiblePointsCounter->setValue(my.visible);
    visiblePointsCounter->blockSignals(false);
}
