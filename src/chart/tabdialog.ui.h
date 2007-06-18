/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

#include "main.h"

void TabDialog::reset(QString label, bool liveMode, int samples, int visible)
{
    archiveMode = !liveMode;
    labelLineEdit->setText(label);

    sampleHistory = samples;
    samplePointsCounter->setRange(1, MAXIMUM_POINTS);
    samplePointsSlider->setRange(1, MAXIMUM_POINTS);
    displaySamplePointsCounter();
    displaySamplePointsSlider();

    visibleHistory = visible;
    visiblePointsCounter->setRange(1, MAXIMUM_POINTS);
    visiblePointsSlider->setRange(1, MAXIMUM_POINTS);
    displayVisiblePointsCounter();
    displayVisiblePointsSlider();
}

bool TabDialog::isArchiveMode()
{
    return archiveMode;
}

void TabDialog::samplePointsValueChanged(double value)
{
    if (sampleHistory != value) {
	sampleHistory = (double)(int)value;
	displaySamplePointsCounter();
	displaySamplePointsSlider();
	if (visibleHistory > sampleHistory)
	    visiblePointsSlider->setValue(value);
    }
}

void TabDialog::visiblePointsValueChanged(double value)
{
    if (visibleHistory != value) {
	visibleHistory = (double)(int)value;
	displayVisiblePointsCounter();
	displayVisiblePointsSlider();
	if (visibleHistory > sampleHistory)
	    samplePointsSlider->setValue(value);
    }
}

void TabDialog::displaySamplePointsSlider()
{
    samplePointsSlider->blockSignals(TRUE);
    samplePointsSlider->setValue(sampleHistory);
    samplePointsSlider->blockSignals(FALSE);
}

void TabDialog::displayVisiblePointsSlider()
{
    visiblePointsSlider->blockSignals(TRUE);
    visiblePointsSlider->setValue(visibleHistory);
    visiblePointsSlider->blockSignals(FALSE);
}

void TabDialog::displaySamplePointsCounter()
{
    samplePointsCounter->blockSignals(TRUE);
    samplePointsCounter->setValue(sampleHistory);
    samplePointsCounter->blockSignals(FALSE);
}

void TabDialog::displayVisiblePointsCounter()
{
    visiblePointsCounter->blockSignals(TRUE);
    visiblePointsCounter->setValue(visibleHistory);
    visiblePointsCounter->blockSignals(FALSE);
}
