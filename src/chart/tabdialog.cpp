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
    if (label == QString::null)
	setWindowTitle(tr("Add Tab"));
    labelLineEdit->setText(label);

    my.archiveSource = !live;
    my.samples = my.visible = 0;

    samplePointsCounter->setValue(samples);
    samplePointsSlider->setValue(samples);
    samplePointsSlider->setRange(KmChart::minimumPoints, KmChart::maximumPoints);
    visiblePointsCounter->setValue(visible);
    visiblePointsSlider->setValue(visible);
    visiblePointsSlider->setRange(KmChart::minimumPoints, KmChart::maximumPoints);
}

bool TabDialog::isArchiveSource()
{
    return my.archiveSource;
}

void TabDialog::samplePointsValueChanged(double value)
{
    if (my.samples != value) {
	my.samples = (double)(int)value;
	displaySamplePointsCounter();
	displaySamplePointsSlider();
	if (my.visible > my.samples)
	    visiblePointsSlider->setValue(value);
    }
}

void TabDialog::visiblePointsValueChanged(double value)
{
    if (my.visible != value) {
	my.visible = (double)(int)value;
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
