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
#include "settingsdialog.h"
#include "qcolordialog.h"
#include "main.h"

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent)
{
    setupUi(this);
}

void SettingsDialog::languageChange()
{
    retranslateUi(this);
}

int SettingsDialog::defaultColorArray(QPushButton ***array)
{
    static QPushButton *buttons[] = {
	defaultColorsPushButton1,  defaultColorsPushButton2,
	defaultColorsPushButton3,  defaultColorsPushButton4,
	defaultColorsPushButton5,  defaultColorsPushButton6,
	defaultColorsPushButton7,  defaultColorsPushButton8,
	defaultColorsPushButton9,  defaultColorsPushButton10,
	defaultColorsPushButton11, defaultColorsPushButton12,
	defaultColorsPushButton13, defaultColorsPushButton14,
	defaultColorsPushButton15, defaultColorsPushButton16,
	defaultColorsPushButton17, defaultColorsPushButton18,
    };
    *array = &buttons[0];
    return sizeof(buttons) / sizeof(buttons[0]);
}

void SettingsDialog::reset()
{
    QPushButton **buttons;
    QPalette palette;
    int i, colorCount;

    my.visible = my.total = 0;
    globalSettings.sampleHistoryModified = false;
    globalSettings.visibleHistoryModified = false;
    globalSettings.defaultColorsModified = false;
    globalSettings.chartBackgroundModified = false;
    globalSettings.chartHighlightModified = false;
    globalSettings.styleModified = false;

    visibleCounter->setValue(globalSettings.visibleHistory);
    visibleSlider->setValue(globalSettings.visibleHistory);
    visibleSlider->setRange(KmChart::minimumPoints, KmChart::maximumPoints);
    totalCounter->setValue(globalSettings.sampleHistory);
    totalSlider->setValue(globalSettings.sampleHistory);
    totalSlider->setRange(KmChart::minimumPoints, KmChart::maximumPoints);

    palette.setBrush(chartBackgroundPushButton->backgroundRole(),
					globalSettings.chartBackground);
    chartBackgroundPushButton->setPalette(palette);
    palette.setBrush(chartHighlightPushButton->backgroundRole(),
					globalSettings.chartHighlight);
    chartHighlightPushButton->setPalette(palette);

    colorCount = defaultColorArray(&buttons);
    for (i = 0; i < globalSettings.defaultColorNames.count(); i++) {
	palette.setBrush(buttons[i]->backgroundRole(),
					 globalSettings.defaultColors[i]);
	buttons[i]->setPalette(palette);
    }
    colorCount -= i;
    for (; i < colorCount; i++) {
	palette.setBrush(buttons[i]->backgroundRole(), QColor(Qt::white));
	buttons[i]->setPalette(palette);
    }

    my.savedStyle = globalSettings.style;
    my.savedStyleName = globalSettings.styleName;
    for (i = 0; i < systemStyleComboBox->count(); i++)
	if (systemStyleComboBox->itemText(i) == my.savedStyleName)
	    break;
    systemStyleComboBox->setCurrentIndex(i);
}

void SettingsDialog::revert()
{
    // Style is special - we make immediate changes for instant feedback
    // TODO: this doesn't work.  Problem is the default case where we do
    // not have a known style name to fallback to...
    // TODO: make visible/sample points "special" too.

    if (globalSettings.styleModified) {
	if (my.savedStyleName == QString::null)
	    my.savedStyleName = tr("");
	globalSettings.styleName = my.savedStyleName;
	globalSettings.style = QApplication::setStyle(my.savedStyleName);
	kmtime->styleTimeControl((char *)(const char *)
					my.savedStyleName.toAscii());
    }
}

void SettingsDialog::flush()
{
    if (globalSettings.visibleHistoryModified)
	globalSettings.visibleHistory = (int)my.visible;
    if (globalSettings.sampleHistoryModified)
	globalSettings.sampleHistory = (int)my.total;

    if (globalSettings.defaultColorsModified) {
	QPushButton **buttons;
	QStringList colorNames;
	QList<QColor> colors;

	int colorCount = defaultColorArray(&buttons);
	for (int i = 0; i < colorCount; i++) {
	    QPalette palette = buttons[i]->palette();
	    QColor c = palette.color(buttons[i]->backgroundRole());
	    if (c == Qt::white)
		continue;
	    colors.append(c);
	    colorNames.append(c.name());
	}

	globalSettings.defaultColors = colors;
	globalSettings.defaultColorNames = colorNames;
    }

    if (globalSettings.chartBackgroundModified) {
	QPalette palette = chartBackgroundPushButton->palette();
	globalSettings.chartBackground = palette.color(
			chartBackgroundPushButton->backgroundRole());
	globalSettings.chartBackgroundName =
			globalSettings.chartBackground.name();
    }
    if (globalSettings.chartHighlightModified) {
	QPalette palette = chartHighlightPushButton->palette();
	globalSettings.chartHighlight = palette.color(
			chartHighlightPushButton->backgroundRole());
	globalSettings.chartHighlightName =
			globalSettings.chartHighlight.name();
    }
}

void SettingsDialog::visibleValueChanged(double value)
{
    if (value != my.visible) {
	my.visible = (double)(int)value;
	displayVisibleCounter();
	displayVisibleSlider();
	if (my.visible > my.total)
	    totalSlider->setValue(value);
	globalSettings.visibleHistoryModified = true;
    }
}

void SettingsDialog::totalValueChanged(double value)
{
    if (value != my.total) {
	my.total = (double)(int)value;
	displayTotalCounter();
	displayTotalSlider();
	if (my.visible > my.total)
	    visibleSlider->setValue(value);
	globalSettings.sampleHistoryModified = true;
    }
}

void SettingsDialog::displayTotalSlider()
{
    totalSlider->blockSignals(true);
    totalSlider->setValue(my.total);
    totalSlider->blockSignals(false);
}

void SettingsDialog::displayVisibleSlider()
{
    visibleSlider->blockSignals(true);
    visibleSlider->setValue(my.visible);
    visibleSlider->blockSignals(false);
}

void SettingsDialog::displayTotalCounter()
{
    totalCounter->blockSignals(true);
    totalCounter->setValue(my.total);
    totalCounter->blockSignals(false);
}

void SettingsDialog::displayVisibleCounter()
{
    visibleCounter->blockSignals(true);
    visibleCounter->setValue(my.visible);
    visibleCounter->blockSignals(false);
}

void SettingsDialog::chartHighlightPushButtonClicked()
{
    QPalette palette = chartHighlightPushButton->palette();
    QColor newColor = QColorDialog::getColor(
		palette.color(chartHighlightPushButton->backgroundRole()));
    if (newColor.isValid()) {
	palette.setBrush(chartHighlightPushButton->backgroundRole(), newColor);
	chartHighlightPushButton->setPalette(palette);
	globalSettings.chartHighlightModified = true;
    }
}

void SettingsDialog::chartBackgroundPushButtonClicked()
{
    QPalette palette = chartBackgroundPushButton->palette();
    QColor newColor = QColorDialog::getColor(
		palette.color(chartBackgroundPushButton->backgroundRole()));
    if (newColor.isValid()) {
	palette.setBrush(chartBackgroundPushButton->backgroundRole(), newColor);
	chartBackgroundPushButton->setPalette(palette);
	globalSettings.chartBackgroundModified = true;
    }
}

void SettingsDialog::defaultColorsPushButtonNClicked(int n)
{
    QPushButton **buttons;

    if (n > defaultColorArray(&buttons))
	return;

    int i = n - 1;
    QPalette palette = buttons[i]->palette();
    QColor newColor = QColorDialog::getColor(
				palette.color(buttons[i]->backgroundRole()));
    if (newColor.isValid()) {
	palette.setBrush(buttons[i]->backgroundRole(), newColor);
	buttons[i]->setPalette(palette);
	globalSettings.defaultColorsModified = true;
    }
}

void SettingsDialog::defaultColorsPushButton1Clicked()
{
    defaultColorsPushButtonNClicked(1);
}

void SettingsDialog::defaultColorsPushButton2Clicked()
{
    defaultColorsPushButtonNClicked(2);
}

void SettingsDialog::defaultColorsPushButton3Clicked()
{
    defaultColorsPushButtonNClicked(3);
}

void SettingsDialog::defaultColorsPushButton4Clicked()
{
    defaultColorsPushButtonNClicked(4);
}

void SettingsDialog::defaultColorsPushButton5Clicked()
{
    defaultColorsPushButtonNClicked(5);
}

void SettingsDialog::defaultColorsPushButton6Clicked()
{
    defaultColorsPushButtonNClicked(6);
}

void SettingsDialog::defaultColorsPushButton7Clicked()
{
    defaultColorsPushButtonNClicked(7);
}

void SettingsDialog::defaultColorsPushButton8Clicked()
{
    defaultColorsPushButtonNClicked(8);
}

void SettingsDialog::defaultColorsPushButton9Clicked()
{
    defaultColorsPushButtonNClicked(9);
}

void SettingsDialog::defaultColorsPushButton10Clicked()
{
    defaultColorsPushButtonNClicked(10);
}

void SettingsDialog::defaultColorsPushButton11Clicked()
{
    defaultColorsPushButtonNClicked(11);
}

void SettingsDialog::defaultColorsPushButton12Clicked()
{
    defaultColorsPushButtonNClicked(12);
}

void SettingsDialog::defaultColorsPushButton13Clicked()
{
    defaultColorsPushButtonNClicked(13);
}

void SettingsDialog::defaultColorsPushButton14Clicked()
{
    defaultColorsPushButtonNClicked(14);
}

void SettingsDialog::defaultColorsPushButton15Clicked()
{
    defaultColorsPushButtonNClicked(15);
}

void SettingsDialog::defaultColorsPushButton16Clicked()
{
    defaultColorsPushButtonNClicked(16);
}

void SettingsDialog::defaultColorsPushButton17Clicked()
{
    defaultColorsPushButtonNClicked(17);
}

void SettingsDialog::defaultColorsPushButton18Clicked()
{
    defaultColorsPushButtonNClicked(18);
}
