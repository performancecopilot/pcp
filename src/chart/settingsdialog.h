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

class SettingsDialog : public QDialog, public Ui::SettingsDialog
{
    Q_OBJECT

public:
    SettingsDialog(QWidget* parent);

    virtual int defaultColorArray(QPushButton *** array);
    virtual void reset();
    virtual void revert();
    virtual void flush();
    virtual void displayTotalSlider();
    virtual void displayVisibleSlider();
    virtual void displayTotalCounter();
    virtual void displayVisibleCounter();

public slots:
    virtual void visibleValueChanged( double value );
    virtual void totalValueChanged( double value );
    virtual void chartHighlightPushButtonClicked();
    virtual void chartBackgroundPushButtonClicked();
    virtual void defaultColorsPushButton1Clicked();
    virtual void defaultColorsPushButton2Clicked();
    virtual void defaultColorsPushButton3Clicked();
    virtual void defaultColorsPushButton4Clicked();
    virtual void defaultColorsPushButton5Clicked();
    virtual void defaultColorsPushButton6Clicked();
    virtual void defaultColorsPushButton7Clicked();
    virtual void defaultColorsPushButton8Clicked();
    virtual void defaultColorsPushButton9Clicked();
    virtual void defaultColorsPushButton10Clicked();
    virtual void defaultColorsPushButton11Clicked();
    virtual void defaultColorsPushButton12Clicked();
    virtual void defaultColorsPushButton13Clicked();
    virtual void defaultColorsPushButton14Clicked();
    virtual void defaultColorsPushButton15Clicked();
    virtual void defaultColorsPushButton16Clicked();
    virtual void defaultColorsPushButton17Clicked();
    virtual void defaultColorsPushButton18Clicked();

protected:
    struct {
	double visible;
	double total;
	QStyle *savedStyle;
	QString savedStyleName;
    } my;

    void defaultColorsPushButtonNClicked(int);

protected slots:
    virtual void languageChange();
};

#endif // SETTINGSDIALOG_H
