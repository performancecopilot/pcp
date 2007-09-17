/*
 * Copyright (c) 2006-2007, Aconex.  All Rights Reserved.
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
#ifndef TABDIALOG_H
#define TABDIALOG_H

#include "ui_tabdialog.h"

class TabDialog : public QDialog, public Ui::TabDialog
{
    Q_OBJECT

public:
    TabDialog(QWidget* parent);

    virtual void reset(QString, bool, int, int);
    virtual bool isArchiveSource();
    virtual void displaySamplePointsSlider();
    virtual void displayVisiblePointsSlider();
    virtual void displaySamplePointsCounter();
    virtual void displayVisiblePointsCounter();

public slots:
    virtual void samplePointsValueChanged(int);
    virtual void visiblePointsValueChanged(int);
    virtual void liveHostRadioButtonClicked();
    virtual void archivesRadioButtonClicked();

protected slots:
    virtual void languageChange();

private:
    struct {
	bool archiveSource;
	int samples;
	int visible;
    } my;
};

#endif // TABDIALOG_H
