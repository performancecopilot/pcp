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

void TabDialog::reset(QString label, bool liveMode, int value)
{
    archiveMode = !liveMode;
    labelLineEdit->setText(label);

    visibleHistory = value;
    visiblePointsCounter->setValue(value);
    visiblePointsCounter->setRange(1, settings.sampleHistory);
    visiblePointsSlider->setValue(value);
    visiblePointsSlider->setRange(1, settings.sampleHistory);
}

bool TabDialog::isArchiveMode()
{
    return archiveMode;
}

void TabDialog::visiblePointsCounterValueChanged(double value)
{
fprintf(stderr, "%s: value=%.2f, vh=%d\n", __func__, value, visibleHistory);
    if (visibleHistory == (int)value)
	return;
    visibleHistory = (int)value;
    visiblePointsSlider->setValue((double)(int)value);
}

void TabDialog::visiblePointsSliderValueChanged(double value)
{
fprintf(stderr, "%s: value=%.2f, vh=%d\n", __func__, value, visibleHistory);
    if (visibleHistory == (int)value)
	return;
    visibleHistory = (int)value;
    visiblePointsCounter->setValue((double)(int)value);
}
