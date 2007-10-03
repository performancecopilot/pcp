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
#include "colorbutton.h"
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
    
    virtual void chartHighlightButton_clicked();
    virtual void chartBackgroundButton_clicked();
    virtual void defaultColorButtonClicked(int);

    virtual void toolbarCheckBox_clicked();
    virtual void toolbarAreasComboBox_currentIndexChanged(int);
    virtual void actionListWidget_itemClicked(QListWidgetItem *);

protected slots:
    virtual void languageChange();

    virtual void defaultColorButton1_clicked()
	{ defaultColorButtonClicked(1); }
    virtual void defaultColorButton2_clicked()
	{ defaultColorButtonClicked(2); }
    virtual void defaultColorButton3_clicked()
	{ defaultColorButtonClicked(3); }
    virtual void defaultColorButton4_clicked()
	{ defaultColorButtonClicked(4); }
    virtual void defaultColorButton5_clicked()
	{ defaultColorButtonClicked(5); }
    virtual void defaultColorButton6_clicked()
	{ defaultColorButtonClicked(6); }
    virtual void defaultColorButton7_clicked()
	{ defaultColorButtonClicked(7); }
    virtual void defaultColorButton8_clicked()
	{ defaultColorButtonClicked(8); }
    virtual void defaultColorButton9_clicked()
	{ defaultColorButtonClicked(9); }
    virtual void defaultColorButton10_clicked()
	{ defaultColorButtonClicked(10); }
    virtual void defaultColorButton11_clicked()
	{ defaultColorButtonClicked(11); }
    virtual void defaultColorButton12_clicked()
	{ defaultColorButtonClicked(12); }
    virtual void defaultColorButton13_clicked()
	{ defaultColorButtonClicked(13); }
    virtual void defaultColorButton14_clicked()
	{ defaultColorButtonClicked(14); }
    virtual void defaultColorButton15_clicked()
	{ defaultColorButtonClicked(15); }
    virtual void defaultColorButton16_clicked()
	{ defaultColorButtonClicked(16); }
    virtual void defaultColorButton17_clicked()
	{ defaultColorButtonClicked(17); }
    virtual void defaultColorButton18_clicked()
	{ defaultColorButtonClicked(18); }
    virtual void defaultColorButton19_clicked()
	{ defaultColorButtonClicked(19); }
    virtual void defaultColorButton20_clicked()
	{ defaultColorButtonClicked(20); }
    virtual void defaultColorButton21_clicked()
	{ defaultColorButtonClicked(21); }
    virtual void defaultColorButton22_clicked()
	{ defaultColorButtonClicked(22); }
    virtual void defaultColorButton23_clicked()
	{ defaultColorButtonClicked(23); }
    virtual void defaultColorButton24_clicked()
	{ defaultColorButtonClicked(24); }

private:
    virtual int defaultColorArray(ColorButton *** array);
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

    QBrush enabled, disabled; // brushes for painting action list backgrounds
};

#endif // SETTINGSDIALOG_H
