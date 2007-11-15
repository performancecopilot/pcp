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

    defaultBackgroundButton->setColor(QColor(globalSettings.chartBackground));
    selectedHighlightButton->setColor(QColor(globalSettings.chartHighlight));

    setupSchemeComboBox();
    setupSchemePalette();

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
    globalSettings.defaultSchemeModified = false;
    globalSettings.chartBackgroundModified = false;
    globalSettings.chartHighlightModified = false;
    globalSettings.initialToolbarModified = false;
    globalSettings.toolbarLocationModified = false;
    globalSettings.toolbarActionsModified = false;

    enableUi();
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

    if (globalSettings.defaultSchemeModified) {
	ColorButton **buttons;

	int colorCount = colorArray(&buttons);
	for (int i = 0; i < colorCount; i++) {
	    QColor c = buttons[i]->color();
	    if (c == Qt::white)
		continue;
	    globalSettings.defaultScheme.addColor(c);
	}
    }

    if (globalSettings.chartBackgroundModified) {
	globalSettings.chartBackground = defaultBackgroundButton->color();
	globalSettings.chartBackgroundName =
			globalSettings.chartBackground.name();
    }
    if (globalSettings.chartHighlightModified) {
	globalSettings.chartHighlight = selectedHighlightButton->color();
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

void SettingsDialog::settingsTab_currentChanged(int)
{
    enableUi();
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

    if (inputValid) {
	if (my.newScheme != QString::null)
	    kmchart->newScheme(my.newScheme);
	QDialog::accept();
    }
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

void SettingsDialog::selectedHighlightButton_clicked()
{
    selectedHighlightButton->clicked();
    if (selectedHighlightButton->isSet())
	globalSettings.chartHighlightModified = true;
}

void SettingsDialog::defaultBackgroundButton_clicked()
{
    defaultBackgroundButton->clicked();
    if (defaultBackgroundButton->isSet())
	globalSettings.chartBackgroundModified = true;
}

void SettingsDialog::colorButtonClicked(int n)
{
    ColorButton **buttons;

    colorArray(&buttons);
    buttons[n-1]->clicked();
    if (buttons[n-1]->isSet())
	globalSettings.defaultSchemeModified = true;
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
	schemeComboBox->blockSignals(true);
	schemeComboBox->addItem(newName);
	schemeComboBox->setCurrentIndex(index + 2);
	schemeComboBox->blockSignals(false);
    }
    else if (schemeComboBox->currentIndex() == 0) {	// Default
	updateSchemeColors(&globalSettings.defaultScheme);
    }
    globalSettings.colorSchemesModified = true;
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
