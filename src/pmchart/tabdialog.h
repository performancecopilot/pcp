/*
 * Copyright (c) 2006-2008, Aconex.  All Rights Reserved.
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

    void reset(QString, bool);
    bool isArchiveSource();
    QString label() const;

public slots:
    void liveHostRadioButtonClicked();
    void archivesRadioButtonClicked();

protected slots:
    void languageChange();

private:
    struct {
	bool archiveSource;
    } my;
};

#endif // TABDIALOG_H
