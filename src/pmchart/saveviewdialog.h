/*
 * Copyright (c) 2014 Red Hat.
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
#ifndef SAVEVIEWDIALOG_H
#define SAVEVIEWDIALOG_H

#include "ui_saveviewdialog.h"
#include <QtGui/QDirModel>

class Chart;

class SaveViewDialog : public QDialog, public Ui::SaveViewDialog
{
    Q_OBJECT

public:
    SaveViewDialog(QWidget* parent);
    ~SaveViewDialog();
    void reset(bool);

    static void saveChart(FILE *, Chart *, bool);
    static bool saveView(QString, bool, bool, bool, bool);
    static void setGlobals(int w, int h, int pts, int x, int y);

public slots:
    virtual void parentToolButton_clicked();
    virtual void userToolButton_clicked(bool);
    virtual void pathComboBox_currentIndexChanged(QString);

    virtual void dirListView_selectionChanged();
    virtual void dirListView_activated(const QModelIndex &);

    virtual void preserveHostCheckBox_toggled(bool);
    virtual void preserveSizeCheckBox_toggled(bool);
    virtual void savePushButton_clicked();

private:
    struct {
	QString userDir;
	bool hostDynamic;	// on-the-fly or explicit-hostnames-in-view
	bool sizeDynamic;	// on-the-fly or explicit-geometry-in-view
	QDirModel *dirModel;
	QModelIndex dirIndex;
	QCompleter *completer;
    } my;

    void setPath(const QString &);
    void setPath(const QModelIndex &);
    void setPathUi(const QString &);

    bool saveViewFile(const QString &);
};

#endif	// SAVEVIEWDIALOG_H
