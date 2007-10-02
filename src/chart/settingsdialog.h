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
#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "ui_settingsdialog.h"
#include "kmtime.h"

class SettingsDialog : public QDialog, public Ui::SettingsDialog
{
    Q_OBJECT

public:
    SettingsDialog(QWidget* parent);

    virtual void reset();
    virtual void flush();

public slots:
    virtual void buttonOk_clicked();

    virtual void chartDeltaUnitsComboBox_activated(int value);
    virtual void loggerDeltaUnitsComboBox_activated(int value);
    virtual void visible_valueChanged(int value);
    virtual void sample_valueChanged(int value);
    
    virtual void chartHighlightPushButton_clicked();
    virtual void chartBackgroundPushButton_clicked();
    virtual void defaultColorsPushButton1_clicked();
    virtual void defaultColorsPushButton2_clicked();
    virtual void defaultColorsPushButton3_clicked();
    virtual void defaultColorsPushButton4_clicked();
    virtual void defaultColorsPushButton5_clicked();
    virtual void defaultColorsPushButton6_clicked();
    virtual void defaultColorsPushButton7_clicked();
    virtual void defaultColorsPushButton8_clicked();
    virtual void defaultColorsPushButton9_clicked();
    virtual void defaultColorsPushButton10_clicked();
    virtual void defaultColorsPushButton11_clicked();
    virtual void defaultColorsPushButton12_clicked();
    virtual void defaultColorsPushButton13_clicked();
    virtual void defaultColorsPushButton14_clicked();
    virtual void defaultColorsPushButton15_clicked();
    virtual void defaultColorsPushButton16_clicked();
    virtual void defaultColorsPushButton17_clicked();
    virtual void defaultColorsPushButton18_clicked();
    virtual void defaultColorsPushButton19_clicked();
    virtual void defaultColorsPushButton20_clicked();
    virtual void defaultColorsPushButton21_clicked();
    virtual void defaultColorsPushButton22_clicked();
    virtual void defaultColorsPushButton23_clicked();
    virtual void defaultColorsPushButton24_clicked();

    virtual void toolbarCheckBox_clicked();
    virtual void toolbarAreasComboBox_currentIndexChanged(int);
    virtual void actionListWidget_itemClicked(QListWidgetItem *);

protected slots:
    virtual void languageChange();

private:
    virtual int defaultColorArray(QPushButton *** array);
    virtual void displayTotalSlider();
    virtual void displayVisibleSlider();
    virtual void displayTotalCounter();
    virtual void displayVisibleCounter();
    struct {
	KmTime::DeltaUnits chartUnits;
	KmTime::DeltaUnits loggerUnits;
	int visibleHistory;
	int sampleHistory;
    } my;

    void defaultColorsPushButtonClicked(int);
    QBrush enabled, disabled; // brushes for painting action list backgrounds
};

#endif // SETTINGSDIALOG_H
