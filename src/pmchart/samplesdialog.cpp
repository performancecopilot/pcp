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
#include "samplesdialog.h"
#include "main.h"

SamplesDialog::SamplesDialog(QWidget* parent) : QDialog(parent)
{
    setupUi(this);
}

void SamplesDialog::languageChange()
{
    retranslateUi(this);
}

void SamplesDialog::reset(int samples, int visible)
{
    my.samples = my.visible = 0;

    visibleCounter->setRange(PmChart::minimumPoints(), PmChart::maximumPoints());
    visibleSlider->setRange(PmChart::minimumPoints(), PmChart::maximumPoints());
    sampleCounter->setRange(PmChart::minimumPoints(), PmChart::maximumPoints());
    sampleSlider->setRange(PmChart::minimumPoints(), PmChart::maximumPoints());
    visibleCounter->setValue(visible);
    visibleSlider->setValue(visible);
    sampleCounter->setValue(samples);
    sampleSlider->setValue(samples);

    console->post(PmChart::DebugUi, "SamplesDialog::reset tot=%d/%d vis=%d/%d",
		  samples, my.samples, visible, my.visible);
}

int SamplesDialog::samples()
{
    return sampleCounter->value();
}

int SamplesDialog::visible()
{
    return visibleCounter->value();
}

void SamplesDialog::sampleValueChanged(int value)
{
    if (my.samples != value) {
	my.samples = value;
	displaySampleCounter();
	displaySampleSlider();
	if (my.visible > my.samples)
	    visibleSlider->setValue(value);
    }
}

void SamplesDialog::visibleValueChanged(int value)
{
    if (my.visible != value) {
	my.visible = value;
	displayVisibleCounter();
	displayVisibleSlider();
	if (my.visible > my.samples)
	    sampleSlider->setValue(value);
    }
}

void SamplesDialog::displaySampleSlider()
{
    sampleSlider->blockSignals(true);
    sampleSlider->setValue(my.samples);
    sampleSlider->blockSignals(false);
}

void SamplesDialog::displayVisibleSlider()
{
    visibleSlider->blockSignals(true);
    visibleSlider->setValue(my.visible);
    visibleSlider->blockSignals(false);
}

void SamplesDialog::displaySampleCounter()
{
    sampleCounter->blockSignals(true);
    sampleCounter->setValue(my.samples);
    sampleCounter->blockSignals(false);
}

void SamplesDialog::displayVisibleCounter()
{
    visibleCounter->blockSignals(true);
    visibleCounter->setValue(my.visible);
    visibleCounter->blockSignals(false);
}
