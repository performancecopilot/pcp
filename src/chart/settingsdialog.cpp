/*
 * Copyright (c) 2007, 2009, Aconex.  All Rights Reserved.
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

#ifndef IS_DARWIN	// only relevent as an option on Mac OS X
    nativeToolbarCheckBox->setEnabled(false);
#endif

    QList<QAction*> actionsList = pmchart->toolbarActionsList();
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

int SettingsDialog::colorArray(ColorButton ***array)
{
    static ColorButton *buttons[] = {
	colorButton1,	colorButton2,	colorButton3,	colorButton4,
	colorButton5,	colorButton6,	colorButton7,	colorButton8,
	colorButton9,	colorButton10,	colorButton11,	colorButton12,
	colorButton13,	colorButton14,	colorButton15,	colorButton16,
	colorButton17,	colorButton18,	colorButton19,	colorButton20,
	colorButton21,	colorButton22,
    };
    *array = &buttons[0];
    return sizeof(buttons) / sizeof(buttons[0]);
}

void SettingsDialog::enableUi()
{
    bool colors = (settingsTab->currentIndex() == 1);
    bool userScheme = (schemeComboBox->currentIndex() > 1);

    if (colors) {
	removeSchemeButton->show();
	updateSchemeButton->show();
    }
    else {
	removeSchemeButton->hide();
	updateSchemeButton->hide();
    }
    removeSchemeButton->setEnabled(userScheme);
}

void SettingsDialog::reset()
{
    my.chartUnits = PmTime::Seconds;
    chartDeltaLineEdit->setText(
	PmTime::deltaString(globalSettings.chartDelta, my.chartUnits));
    my.loggerUnits = PmTime::Seconds;
    loggerDeltaLineEdit->setText(
	PmTime::deltaString(globalSettings.loggerDelta, my.loggerUnits));

    my.visibleHistory = my.sampleHistory = 0;
    visibleCounter->setValue(globalSettings.visibleHistory);
    visibleCounter->setRange(PmChart::minimumPoints(), PmChart::maximumPoints());
    visibleSlider->setValue(globalSettings.visibleHistory);
    visibleSlider->setRange(PmChart::minimumPoints(), PmChart::maximumPoints());
    sampleCounter->setValue(globalSettings.sampleHistory);
    sampleCounter->setRange(PmChart::minimumPoints(), PmChart::maximumPoints());
    sampleSlider->setValue(globalSettings.sampleHistory);
    sampleSlider->setRange(PmChart::minimumPoints(), PmChart::maximumPoints());

    defaultBackgroundButton->setColor(QColor(globalSettings.chartBackground));
    selectedHighlightButton->setColor(QColor(globalSettings.chartHighlight));

    setupSchemeComboBox();
    setupSchemePalette();

    QList<QAction*> actionsList = pmchart->toolbarActionsList();
    QList<QAction*> enabledList = pmchart->enabledActionsList();
    for (int i = 0; i < actionsList.size(); i++) {
	QAction *action = actionsList.at(i);
	QListWidgetItem *item = actionListWidget->item(i);
	if (enabledList.contains(action) == false)
	    item->setBackground(disabled);
    }
    startupToolbarCheckBox->setCheckState(
		globalSettings.initialToolbar ? Qt::Checked : Qt::Unchecked);
    nativeToolbarCheckBox->setCheckState(
		globalSettings.nativeToolbar ? Qt::Checked : Qt::Unchecked);
    toolbarAreasComboBox->setCurrentIndex(
		globalSettings.toolbarLocation ? 1: 0);

    enableUi();
}

void SettingsDialog::settingsTab_currentChanged(int)
{
    enableUi();
}

void SettingsDialog::chartDeltaLineEdit_editingFinished()
{
    double input = globalSettings.chartDelta;

    // convert to seconds, make sure its still in range 0.001-INT_MAX
    if (chartDeltaLineEdit->isModified())
	input = PmTime::deltaValue(chartDeltaLineEdit->text(), my.chartUnits);
    if (input < 0.001 || input > INT_MAX) {
	QString msg = tr("Default Chart Sampling Interval is invalid.\n");
	msg.append(chartDeltaLineEdit->text());
	msg.append(" is out of range (0.001 to 0x7fffffff seconds)\n");
	QMessageBox::warning(this, pmProgname, msg);
    }
    else if (input != globalSettings.chartDelta) {
	globalSettings.chartDeltaModified = true;
	globalSettings.chartDelta = input;
	writeSettings();
    }
}

void SettingsDialog::loggerDeltaLineEdit_editingFinished()
{
    double input = globalSettings.loggerDelta;

    // convert to seconds, make sure its still in range 0.001-INT_MAX
    if (loggerDeltaLineEdit->isModified())
	input = PmTime::deltaValue(loggerDeltaLineEdit->text(), my.loggerUnits);
    if (input < 0.001 || input > INT_MAX) {
	QString msg = tr("Default Record Sampling Interval is invalid.\n");
	msg.append(loggerDeltaLineEdit->text());
	msg.append(" is out of range (0.001 to 0x7fffffff seconds)\n");
	QMessageBox::warning(this, pmProgname, msg);
    }
    else if (input != globalSettings.loggerDelta) {
	globalSettings.loggerDeltaModified = true;
	globalSettings.loggerDelta = input;
	writeSettings();
    }
}

void SettingsDialog::chartDeltaUnitsComboBox_activated(int value)
{
    double v = PmTime::deltaValue(chartDeltaLineEdit->text(), my.chartUnits);
    my.chartUnits = (PmTime::DeltaUnits)value;
    chartDeltaLineEdit->setText(PmTime::deltaString(v, my.chartUnits));
}

void SettingsDialog::loggerDeltaUnitsComboBox_activated(int value)
{
    double v = PmTime::deltaValue(loggerDeltaLineEdit->text(), my.loggerUnits);
    my.loggerUnits = (PmTime::DeltaUnits)value;
    loggerDeltaLineEdit->setText(PmTime::deltaString(v, my.loggerUnits));
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
	globalSettings.visibleHistory = my.visibleHistory;
	writeSettings();
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
	globalSettings.sampleHistory = my.sampleHistory;
	writeSettings();
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

void SettingsDialog::selectedHighlightButton_clicked()
{
    selectedHighlightButton->clicked();
    if (selectedHighlightButton->isSet()) {
	globalSettings.chartHighlightModified = true;
	globalSettings.chartHighlight = selectedHighlightButton->color();
	globalSettings.chartHighlightName =
			globalSettings.chartHighlight.name();
	writeSettings();
    }
}

void SettingsDialog::defaultBackgroundButton_clicked()
{
    defaultBackgroundButton->clicked();
    if (defaultBackgroundButton->isSet()) {
	globalSettings.chartBackgroundModified = true;
	globalSettings.chartBackground = defaultBackgroundButton->color();
	globalSettings.chartBackgroundName =
			globalSettings.chartBackground.name();
	pmchart->updateBackground();
	writeSettings();
    }
}

void SettingsDialog::colorButtonClicked(int n)
{
    ColorButton **buttons;

    colorArray(&buttons);
    buttons[n-1]->clicked();
    if (buttons[n-1]->isSet()) {
	ColorButton **buttons;

	int colorCount = colorArray(&buttons);
	for (int i = 0; i < colorCount; i++) {
	    QColor c = buttons[i]->color();
	    if (c == Qt::white)
		continue;
	    globalSettings.defaultScheme.addColor(c);
	}

	globalSettings.defaultSchemeModified = true;
	writeSettings();
    }
}

void SettingsDialog::startupToolbarCheckBox_clicked()
{
    globalSettings.initialToolbar =
			startupToolbarCheckBox->checkState() == Qt::Checked;
    globalSettings.initialToolbarModified = true;
    writeSettings();
}

void SettingsDialog::nativeToolbarCheckBox_clicked()
{
    globalSettings.nativeToolbar =
			nativeToolbarCheckBox->checkState() == Qt::Checked;
    globalSettings.nativeToolbarModified = true;
    writeSettings();
    pmchart->updateToolbarLocation();
}

void SettingsDialog::toolbarAreasComboBox_currentIndexChanged(int)
{
    globalSettings.toolbarLocation = toolbarAreasComboBox->currentIndex();
    globalSettings.toolbarLocationModified = true;
    writeSettings();
    pmchart->updateToolbarLocation();
}

void SettingsDialog::actionListWidget_itemClicked(QListWidgetItem *item)
{
    QStringList actions;

    item->setBackground(item->background() == disabled ? enabled : disabled);
    for (int i = 0; i < actionListWidget->count(); i++) {
	QListWidgetItem *listitem = actionListWidget->item(i);
	if (listitem->background() != disabled)
	    actions.append(listitem->text());
    }
    globalSettings.toolbarActions = actions;
    globalSettings.toolbarActionsModified = true;
    writeSettings();
    pmchart->setEnabledActionsList(actions, true);
    pmchart->updateToolbarContents();
}

void SettingsDialog::newScheme()
{
    reset();
    my.newScheme = QString::null;
    settingsTab->setCurrentIndex(1);	// Colors Tab

    // Disable signals here and explicitly call the index changed
    // routine so that we don't race with setFocus and selectAll.
    schemeComboBox->blockSignals(true);
    schemeComboBox->setCurrentIndex(1);	// New Scheme
    schemeComboBox->blockSignals(false);
    schemeComboBox_currentIndexChanged(1);
    schemeLineEdit->selectAll();
    schemeLineEdit->setFocus();
    show();
}

void SettingsDialog::removeSchemeButton_clicked()
{
    ColorScheme::removeScheme(schemeComboBox->currentText());
    setupSchemeComboBox();
    schemeLineEdit->clear();
    globalSettings.colorSchemesModified = true;
    writeSettings();
}

void SettingsDialog::updateSchemeColors(ColorScheme *scheme)
{
    ColorButton **buttons;
    int colorCount = colorArray(&buttons);

    scheme->clear();
    for (int i = 0; i < colorCount; i++) {
	QColor c = buttons[i]->color();
	if (c == Qt::white)
	    continue;
	scheme->addColor(c);
    }
    scheme->setModified(true);
}

void SettingsDialog::updateSchemeButton_clicked()
{
    int index;
    QString oldName = schemeComboBox->currentText();
    QString newName = schemeLineEdit->text();

    if (schemeComboBox->currentIndex() > 1) {		// Edit scheme
	if (newName != oldName) {
	    if (ColorScheme::lookupScheme(newName) == true)
		goto conflict;
	    index = schemeComboBox->currentIndex();
	    schemeComboBox->setItemText(index, newName);
	}
	for (int i = 0; i < globalSettings.colorSchemes.size(); i++) {
	    if (oldName == globalSettings.colorSchemes[i].name()) {
		globalSettings.colorSchemes[i].setName(newName);
		updateSchemeColors(&globalSettings.colorSchemes[i]);
		break;
	    }
	}
    }
    else if (schemeComboBox->currentIndex() == 1) {	// New Scheme
	if (ColorScheme::lookupScheme(newName) == true)
	    goto conflict;
	ColorScheme scheme;
	my.newScheme = newName;
	scheme.setName(newName);
	updateSchemeColors(&scheme);
	index = globalSettings.colorSchemes.size();
	globalSettings.colorSchemes.append(scheme);
	pmchart->newScheme(my.newScheme);
	schemeComboBox->blockSignals(true);
	schemeComboBox->addItem(newName);
	schemeComboBox->setCurrentIndex(index + 2);
	schemeComboBox->blockSignals(false);
    }
    else if (schemeComboBox->currentIndex() == 0) {	// Default
	updateSchemeColors(&globalSettings.defaultScheme);
    }
    globalSettings.colorSchemesModified = true;
    writeSettings();
    return;

conflict:
    QString msg = newName;
    msg.prepend("New scheme name \"");
    msg.append("\" conflicts with an existing name");
    QMessageBox::warning(this, pmProgname, msg);
}

void SettingsDialog::setupSchemePalette()
{
    ColorButton **buttons;
    int colorCount = colorArray(&buttons);
    int i = 0, index = schemeComboBox->currentIndex();

    if (index == 1)	// keep whatever is there as the starting point
	i = colorCount;
    else if (index == 0) {
	for (i = 0; i < globalSettings.defaultScheme.size() && i < colorCount; i++)
	    buttons[i]->setColor(globalSettings.defaultScheme.color(i));
    }
    else if (index > 1) {
	int j = index - 2;
	for (i = 0; i < globalSettings.colorSchemes[j].size() && i < colorCount; i++)
	    buttons[i]->setColor(globalSettings.colorSchemes[j].color(i));
    }

    while (i < colorCount)
	buttons[i++]->setColor(QColor(Qt::white));
}

void SettingsDialog::setupSchemeComboBox()
{
    schemeComboBox->blockSignals(true);
    schemeComboBox->clear();
    schemeComboBox->addItem(tr("Default Scheme"));
    schemeComboBox->addItem(tr("New Scheme"));
    for (int i = 0; i < globalSettings.colorSchemes.size(); i++)
	schemeComboBox->addItem(globalSettings.colorSchemes[i].name());
    schemeComboBox->setCurrentIndex(0);
    schemeComboBox->blockSignals(false);
}

void SettingsDialog::schemeComboBox_currentIndexChanged(int index)
{
    if (index == 0) {	// Default Scheme
	schemeLineEdit->setEnabled(false);
	schemeLineEdit->setText("#-cycle");
	removeSchemeButton->setEnabled(false);
	setupSchemePalette();
    }
    else {
	schemeLineEdit->setText(schemeComboBox->currentText());
	schemeLineEdit->setEnabled(true);
	if (index == 1)
	    removeSchemeButton->setEnabled(false);
	else {
	    removeSchemeButton->setEnabled(true);
	    setupSchemePalette();
	}
    }
}
