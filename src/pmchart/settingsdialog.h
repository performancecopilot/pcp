/*
 * Copyright (c) 2014 Red Hat.
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
#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "ui_settingsdialog.h"
#include "colorbutton.h"
#include "colorscheme.h"
#include <qmc_time.h>

class SettingsDialog : public QDialog, public Ui::SettingsDialog
{
    Q_OBJECT

public:
    SettingsDialog(QWidget* parent);
    void enableUi();
    void reset();

    void newScheme();
    int colorArray(ColorButton *** array);

public slots:
    virtual void settingsTab_currentChanged(int index);

    virtual void chartDeltaLineEdit_editingFinished();
    virtual void loggerDeltaLineEdit_editingFinished();
    virtual void chartDeltaUnitsComboBox_activated(int value);
    virtual void loggerDeltaUnitsComboBox_activated(int value);
    virtual void visible_valueChanged(int value);
    virtual void sample_valueChanged(int value);

    virtual void selectedHighlightButton_clicked();
    virtual void defaultBackgroundButton_clicked();
    virtual void colorButtonClicked(int);
    virtual void removeSchemeButton_clicked();
    virtual void updateSchemeButton_clicked();
    virtual void schemeComboBox_currentIndexChanged(int);

    virtual void familyLineEdit_editingFinished();
    virtual void familyListWidget_itemClicked(QListWidgetItem *);
    virtual void styleLineEdit_editingFinished();
    virtual void styleListWidget_itemClicked(QListWidgetItem *);
    virtual void sizeLineEdit_editingFinished();
    virtual void sizeListWidget_itemClicked(QListWidgetItem *);
    virtual void resetFontButton_clicked();
    virtual void applyFontButton_clicked();

    virtual void hostButton_clicked();
    virtual void savedHostsListWidget_itemSelectionChanged();
    virtual void removeHostButton_clicked();
    virtual void addHostButton_clicked();

    virtual void startupToolbarCheckBox_clicked();
    virtual void nativeToolbarCheckBox_clicked();
    virtual void toolbarAreasComboBox_currentIndexChanged(int);
    virtual void actionListWidget_itemClicked(QListWidgetItem *);

protected slots:
    virtual void languageChange();

    virtual void colorButton1_clicked() { colorButtonClicked(1); }
    virtual void colorButton2_clicked() { colorButtonClicked(2); }
    virtual void colorButton3_clicked() { colorButtonClicked(3); }
    virtual void colorButton4_clicked() { colorButtonClicked(4); }
    virtual void colorButton5_clicked() { colorButtonClicked(5); }
    virtual void colorButton6_clicked() { colorButtonClicked(6); }
    virtual void colorButton7_clicked() { colorButtonClicked(7); }
    virtual void colorButton8_clicked() { colorButtonClicked(8); }
    virtual void colorButton9_clicked() { colorButtonClicked(9); }
    virtual void colorButton10_clicked() { colorButtonClicked(10); }
    virtual void colorButton11_clicked() { colorButtonClicked(11); }
    virtual void colorButton12_clicked() { colorButtonClicked(12); }
    virtual void colorButton13_clicked() { colorButtonClicked(13); }
    virtual void colorButton14_clicked() { colorButtonClicked(14); }
    virtual void colorButton15_clicked() { colorButtonClicked(15); }
    virtual void colorButton16_clicked() { colorButtonClicked(16); }
    virtual void colorButton17_clicked() { colorButtonClicked(17); }
    virtual void colorButton18_clicked() { colorButtonClicked(18); }
    virtual void colorButton19_clicked() { colorButtonClicked(19); }
    virtual void colorButton20_clicked() { colorButtonClicked(20); }
    virtual void colorButton21_clicked() { colorButtonClicked(21); }
    virtual void colorButton22_clicked() { colorButtonClicked(22); }

private:
    // font preferences
    void setupFontLists();
    void updateFontList(QListWidget *, const QString &);

    // hosts preferences
    void setupSavedHostsList();
    void setupHostComboBox(const QString &);

    // toolbar preferences
    void setupActionsList();

    // colors preferences
    void setupSchemePalette();
    void setupSchemeComboBox();
    ColorScheme *lookupScheme(QString);
    void updateSchemeColors(ColorScheme *);

    // sampling preferences
    void displayTotalSlider();
    void displayVisibleSlider();
    void displayTotalCounter();
    void displayVisibleCounter();

    struct {
	QmcTime::DeltaUnits chartUnits;
	QmcTime::DeltaUnits loggerUnits;
	int visibleHistory;
	int sampleHistory;
	QString newScheme;
    } my;

    QBrush enabled, disabled; // brushes for painting action list backgrounds
};

#endif // SETTINGSDIALOG_H
