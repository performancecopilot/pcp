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
#include <QtGui/QMessageBox>
#include "qcolordialog.h"
#include "main.h"

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent)
{
    setupUi(this);
    chartDeltaLineEdit->setValidator(
		new QDoubleValidator(0.001, INT_MAX, 3, chartDeltaLineEdit));
    loggerDeltaLineEdit->setValidator(
		new QDoubleValidator(0.001, INT_MAX, 3, loggerDeltaLineEdit));
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
	defaultColorsPushButton19, defaultColorsPushButton20,
	defaultColorsPushButton21, defaultColorsPushButton22,
	defaultColorsPushButton23, defaultColorsPushButton24,
    };
    *array = &buttons[0];
    return sizeof(buttons) / sizeof(buttons[0]);
}

void SettingsDialog::reset()
{
    QPushButton **buttons;
    QPalette palette;
    int i, colorCount;

    my.chartUnits = KmTime::Seconds;
    chartDeltaLineEdit->setText(
	KmTime::deltaString(globalSettings.chartDelta, my.chartUnits));
    my.loggerUnits = KmTime::Seconds;
    loggerDeltaLineEdit->setText(
	KmTime::deltaString(globalSettings.loggerDelta, my.loggerUnits));

    my.visibleHistory = my.sampleHistory = 0;
    visibleCounter->setValue(globalSettings.visibleHistory);
    visibleCounter->setRange(KmChart::minimumPoints, KmChart::maximumPoints);
    visibleSlider->setValue(globalSettings.visibleHistory);
    visibleSlider->setRange(KmChart::minimumPoints, KmChart::maximumPoints);
    sampleCounter->setValue(globalSettings.sampleHistory);
    sampleCounter->setRange(KmChart::minimumPoints, KmChart::maximumPoints);
    sampleSlider->setValue(globalSettings.sampleHistory);
    sampleSlider->setRange(KmChart::minimumPoints, KmChart::maximumPoints);

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

    globalSettings.chartDeltaModified = false;
    globalSettings.loggerDeltaModified = false;
    globalSettings.sampleHistoryModified = false;
    globalSettings.visibleHistoryModified = false;
    globalSettings.defaultColorsModified = false;
    globalSettings.chartBackgroundModified = false;
    globalSettings.chartHighlightModified = false;
}

void SettingsDialog::flush()
{
    globalSettings.chartDeltaModified = chartDeltaLineEdit->isModified();
    globalSettings.loggerDeltaModified = loggerDeltaLineEdit->isModified();

    if (globalSettings.chartDeltaModified)
	globalSettings.chartDelta =
		KmTime::deltaValue(chartDeltaLineEdit->text(), my.chartUnits);
    if (globalSettings.loggerDeltaModified)
	globalSettings.loggerDelta =
		KmTime::deltaValue(loggerDeltaLineEdit->text(), my.loggerUnits);

    if (globalSettings.visibleHistoryModified)
	globalSettings.visibleHistory = my.visibleHistory;
    if (globalSettings.sampleHistoryModified)
	globalSettings.sampleHistory = my.sampleHistory;

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

void SettingsDialog::buttonOk_clicked()
{
    bool inputValid = true;
    double input;

    if (chartDeltaLineEdit->isModified()) {
	// convert to seconds, make sure its still in range 0.001-INT_MAX
	input = KmTime::deltaValue(chartDeltaLineEdit->text(), my.chartUnits);
	if (input < 0.001 || input > INT_MAX) {
	    QString msg = tr("Default Chart Sampling Interval is invalid.\n");
	    msg.append(chartDeltaLineEdit->text());
	    msg.append(" is out of range (0.001 to 0x7fffffff seconds)\n");
	    QMessageBox::warning(this, pmProgname, msg);
	    inputValid = false;
	}
    }
    if (loggerDeltaLineEdit->isModified() && inputValid) {
	// convert to seconds, make sure its still in range 0.001-INT_MAX
	input = KmTime::deltaValue(loggerDeltaLineEdit->text(), my.loggerUnits);
	if (input < 0.001 || input > INT_MAX) {
	    QString msg = tr("Default Record Sampling Interval is invalid.\n");
	    msg.append(loggerDeltaLineEdit->text());
	    msg.append(" is out of range (0.001 to 0x7fffffff seconds)\n");
	    QMessageBox::warning(this, pmProgname, msg);
	    inputValid = false;
	}
    }

    if (inputValid)
	QDialog::accept();
}

void SettingsDialog::chartDeltaUnitsComboBox_activated(int value)
{
    double v = KmTime::deltaValue(chartDeltaLineEdit->text(), my.chartUnits);
    my.chartUnits = (KmTime::DeltaUnits)value;
    chartDeltaLineEdit->setText(KmTime::deltaString(v, my.chartUnits));
}

void SettingsDialog::loggerDeltaUnitsComboBox_activated(int value)
{
    double v = KmTime::deltaValue(loggerDeltaLineEdit->text(), my.loggerUnits);
    my.loggerUnits = (KmTime::DeltaUnits)value;
    loggerDeltaLineEdit->setText(KmTime::deltaString(v, my.loggerUnits));
}

void SettingsDialog::visible_valueChanged(int value)
{
    if (value != my.visibleHistory) {
	my.visibleHistory = value;
	displayVisibleCounter();
	displayVisibleSlider();
	if (my.visibleHistory > my.sampleHistory)
	    sampleSlider->setValue(value);
	globalSettings.visibleHistoryModified = true;
    }
}

void SettingsDialog::sample_valueChanged(int value)
{
    if (value != my.sampleHistory) {
	my.sampleHistory = value;
	displayTotalCounter();
	displayTotalSlider();
	if (my.visibleHistory > my.sampleHistory)
	    visibleSlider->setValue(value);
	globalSettings.sampleHistoryModified = true;
    }
}

void SettingsDialog::displayTotalSlider()
{
    sampleSlider->blockSignals(true);
    sampleSlider->setValue(my.sampleHistory);
    sampleSlider->blockSignals(false);
}

void SettingsDialog::displayVisibleSlider()
{
    visibleSlider->blockSignals(true);
    visibleSlider->setValue(my.visibleHistory);
    visibleSlider->blockSignals(false);
}

void SettingsDialog::displayTotalCounter()
{
    sampleCounter->blockSignals(true);
    sampleCounter->setValue(my.sampleHistory);
    sampleCounter->blockSignals(false);
}

void SettingsDialog::displayVisibleCounter()
{
    visibleCounter->blockSignals(true);
    visibleCounter->setValue(my.visibleHistory);
    visibleCounter->blockSignals(false);
}

void SettingsDialog::chartHighlightPushButton_clicked()
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

void SettingsDialog::chartBackgroundPushButton_clicked()
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

void SettingsDialog::defaultColorsPushButtonClicked(int n)
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

void SettingsDialog::defaultColorsPushButton1_clicked()
{
    defaultColorsPushButtonClicked(1);
}

void SettingsDialog::defaultColorsPushButton2_clicked()
{
    defaultColorsPushButtonClicked(2);
}

void SettingsDialog::defaultColorsPushButton3_clicked()
{
    defaultColorsPushButtonClicked(3);
}

void SettingsDialog::defaultColorsPushButton4_clicked()
{
    defaultColorsPushButtonClicked(4);
}

void SettingsDialog::defaultColorsPushButton5_clicked()
{
    defaultColorsPushButtonClicked(5);
}

void SettingsDialog::defaultColorsPushButton6_clicked()
{
    defaultColorsPushButtonClicked(6);
}

void SettingsDialog::defaultColorsPushButton7_clicked()
{
    defaultColorsPushButtonClicked(7);
}

void SettingsDialog::defaultColorsPushButton8_clicked()
{
    defaultColorsPushButtonClicked(8);
}

void SettingsDialog::defaultColorsPushButton9_clicked()
{
    defaultColorsPushButtonClicked(9);
}

void SettingsDialog::defaultColorsPushButton10_clicked()
{
    defaultColorsPushButtonClicked(10);
}

void SettingsDialog::defaultColorsPushButton11_clicked()
{
    defaultColorsPushButtonClicked(11);
}

void SettingsDialog::defaultColorsPushButton12_clicked()
{
    defaultColorsPushButtonClicked(12);
}

void SettingsDialog::defaultColorsPushButton13_clicked()
{
    defaultColorsPushButtonClicked(13);
}

void SettingsDialog::defaultColorsPushButton14_clicked()
{
    defaultColorsPushButtonClicked(14);
}

void SettingsDialog::defaultColorsPushButton15_clicked()
{
    defaultColorsPushButtonClicked(15);
}

void SettingsDialog::defaultColorsPushButton16_clicked()
{
    defaultColorsPushButtonClicked(16);
}

void SettingsDialog::defaultColorsPushButton17_clicked()
{
    defaultColorsPushButtonClicked(17);
}

void SettingsDialog::defaultColorsPushButton18_clicked()
{
    defaultColorsPushButtonClicked(18);
}

void SettingsDialog::defaultColorsPushButton19_clicked()
{
    defaultColorsPushButtonClicked(19);
}

void SettingsDialog::defaultColorsPushButton20_clicked()
{
    defaultColorsPushButtonClicked(20);
}

void SettingsDialog::defaultColorsPushButton21_clicked()
{
    defaultColorsPushButtonClicked(21);
}

void SettingsDialog::defaultColorsPushButton22_clicked()
{
    defaultColorsPushButtonClicked(22);
}

void SettingsDialog::defaultColorsPushButton23_clicked()
{
    defaultColorsPushButtonClicked(23);
}

void SettingsDialog::defaultColorsPushButton24_clicked()
{
    defaultColorsPushButtonClicked(24);
}
