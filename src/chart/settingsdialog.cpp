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
#include <QtGui/QListWidgetItem>
#include "main.h"

SettingsDialog::SettingsDialog(QWidget* parent)
	: QDialog(parent), disabled(Qt::Dense4Pattern)
{
    setupUi(this);

    QList<QAction*> actionsList = kmchart->toolbarActionsList();
    for (int i = 0; i < actionsList.size(); i++) {
	QAction *action = actionsList.at(i);
	actionListWidget->insertItem(i,
		new QListWidgetItem(action->icon(), action->iconText()));
    }
    enabled = actionListWidget->item(0)->background();

    chartDeltaLineEdit->setValidator(
		new QDoubleValidator(0.001, INT_MAX, 3, chartDeltaLineEdit));
    loggerDeltaLineEdit->setValidator(
		new QDoubleValidator(0.001, INT_MAX, 3, loggerDeltaLineEdit));
}

void SettingsDialog::languageChange()
{
    retranslateUi(this);
}

int SettingsDialog::defaultColorArray(ColorButton ***array)
{
    static ColorButton *buttons[] = {
	defaultColorButton1,	defaultColorButton2,	defaultColorButton3,
	defaultColorButton4,	defaultColorButton5,	defaultColorButton6,
	defaultColorButton7,	defaultColorButton8,	defaultColorButton9,
	defaultColorButton10,	defaultColorButton11,	defaultColorButton12,
	defaultColorButton13,	defaultColorButton14,	defaultColorButton15,
	defaultColorButton16,	defaultColorButton17,	defaultColorButton18,
	defaultColorButton19,	defaultColorButton20,	defaultColorButton21,
	defaultColorButton22,	defaultColorButton23,	defaultColorButton24,
    };
    *array = &buttons[0];
    return sizeof(buttons) / sizeof(buttons[0]);
}

void SettingsDialog::reset()
{
    ColorButton **buttons;
    int i, colorCount;

    my.chartUnits = KmTime::Seconds;
    chartDeltaLineEdit->setText(
	KmTime::deltaString(globalSettings.chartDelta, my.chartUnits));
    my.loggerUnits = KmTime::Seconds;
    loggerDeltaLineEdit->setText(
	KmTime::deltaString(globalSettings.loggerDelta, my.loggerUnits));

    my.visibleHistory = my.sampleHistory = 0;
    visibleCounter->setValue(globalSettings.visibleHistory);
    visibleCounter->setRange(KmChart::minimumPoints(), KmChart::maximumPoints());
    visibleSlider->setValue(globalSettings.visibleHistory);
    visibleSlider->setRange(KmChart::minimumPoints(), KmChart::maximumPoints());
    sampleCounter->setValue(globalSettings.sampleHistory);
    sampleCounter->setRange(KmChart::minimumPoints(), KmChart::maximumPoints());
    sampleSlider->setValue(globalSettings.sampleHistory);
    sampleSlider->setRange(KmChart::minimumPoints(), KmChart::maximumPoints());

    chartBackgroundButton->setColor(QColor(globalSettings.chartBackground));
    chartHighlightButton->setColor(QColor(globalSettings.chartHighlight));

    colorCount = defaultColorArray(&buttons);
    for (i = 0; i < globalSettings.defaultColorNames.count(); i++)
	buttons[i]->setColor(QColor(globalSettings.defaultColors[i]));
    for (; i < colorCount; i++)
	buttons[i]->setColor(QColor(Qt::white));

    QList<QAction*> actionsList = kmchart->toolbarActionsList();
    QList<QAction*> enabledList = kmchart->enabledActionsList();
    for (int i = 0; i < actionsList.size(); i++) {
	QAction *action = actionsList.at(i);
	QListWidgetItem *item = actionListWidget->item(i);
	if (enabledList.contains(action) == false)
	    item->setBackground(disabled);
    }
    toolbarCheckBox->setCheckState(
		globalSettings.initialToolbar ? Qt::Checked : Qt::Unchecked);
    toolbarAreasComboBox->setCurrentIndex(
		globalSettings.toolbarLocation ? 1: 0);

    globalSettings.chartDeltaModified = false;
    globalSettings.loggerDeltaModified = false;
    globalSettings.sampleHistoryModified = false;
    globalSettings.visibleHistoryModified = false;
    globalSettings.defaultColorsModified = false;
    globalSettings.chartBackgroundModified = false;
    globalSettings.chartHighlightModified = false;
    globalSettings.initialToolbarModified = false;
    globalSettings.toolbarLocationModified = false;
    globalSettings.toolbarActionsModified = false;
}

void SettingsDialog::flush()
{
    globalSettings.chartDeltaModified = chartDeltaLineEdit->isModified();
    globalSettings.loggerDeltaModified = loggerDeltaLineEdit->isModified();

    if (globalSettings.chartDeltaModified)
	globalSettings.chartDelta = (int)
		KmTime::deltaValue(chartDeltaLineEdit->text(), my.chartUnits);
    if (globalSettings.loggerDeltaModified)
	globalSettings.loggerDelta = (int)
		KmTime::deltaValue(loggerDeltaLineEdit->text(), my.loggerUnits);

    if (globalSettings.visibleHistoryModified)
	globalSettings.visibleHistory = my.visibleHistory;
    if (globalSettings.sampleHistoryModified)
	globalSettings.sampleHistory = my.sampleHistory;

    if (globalSettings.defaultColorsModified) {
	ColorButton **buttons;
	QStringList colorNames;
	QList<QColor> colors;

	int colorCount = defaultColorArray(&buttons);
	for (int i = 0; i < colorCount; i++) {
	    QColor c = buttons[i]->color();
	    if (c == Qt::white)
		continue;
	    colors.append(c);
	    colorNames.append(c.name());
	}

	globalSettings.defaultColors = colors;
	globalSettings.defaultColorNames = colorNames;
    }

    if (globalSettings.chartBackgroundModified) {
	globalSettings.chartBackground = chartBackgroundButton->color();
	globalSettings.chartBackgroundName =
			globalSettings.chartBackground.name();
    }
    if (globalSettings.chartHighlightModified) {
	globalSettings.chartHighlight = chartHighlightButton->color();
	globalSettings.chartHighlightName =
			globalSettings.chartHighlight.name();
    }

    if (globalSettings.initialToolbarModified)
	globalSettings.initialToolbar =
			toolbarCheckBox->checkState() == Qt::Checked;
    if (globalSettings.toolbarLocationModified) {
	globalSettings.toolbarLocation = toolbarAreasComboBox->currentIndex();
	kmchart->updateToolbarLocation();
    }

    if (globalSettings.toolbarActionsModified) {
	QStringList actions;
	QListWidgetItem *item;
	for (int i = 0; i < actionListWidget->count(); i++) {
	    item = actionListWidget->item(i);
	    if (item->background() != disabled)
		actions.append(item->text());
	}
	globalSettings.toolbarActions = actions;
	kmchart->setEnabledActionsList(actions, true);
	kmchart->updateToolbarContents();
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

void SettingsDialog::chartHighlightButton_clicked()
{
    chartHighlightButton->clicked();
    if (chartHighlightButton->isSet())
	globalSettings.chartHighlightModified = true;
}

void SettingsDialog::chartBackgroundButton_clicked()
{
    chartBackgroundButton->clicked();
    if (chartBackgroundButton->isSet())
	globalSettings.chartBackgroundModified = true;
}

void SettingsDialog::defaultColorButtonClicked(int n)
{
    ColorButton **buttons;

    defaultColorArray(&buttons);
    buttons[n-1]->clicked();
    if (buttons[n-1]->isSet())
	globalSettings.defaultColorsModified = true;
}

void SettingsDialog::toolbarCheckBox_clicked()
{
    globalSettings.initialToolbarModified = true;
}

void SettingsDialog::toolbarAreasComboBox_currentIndexChanged(int)
{
    globalSettings.toolbarLocationModified = true;
}

void SettingsDialog::actionListWidget_itemClicked(QListWidgetItem *item)
{
    globalSettings.toolbarActionsModified = true;
    item->setBackground(item->background() == disabled ? enabled : disabled);
}
