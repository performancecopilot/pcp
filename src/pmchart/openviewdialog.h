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
#ifndef OPENVIEWDIALOG_H
#define OPENVIEWDIALOG_H

#include "ui_openviewdialog.h"
#include <QtGui/QDirModel>

class OpenViewDialog : public QDialog, public Ui::OpenViewDialog
{
    Q_OBJECT

public:
    OpenViewDialog(QWidget* parent);
    ~OpenViewDialog();

    void reset();
    void sourceAdd();

    static bool openView(const char *);
    static void globals(int *w, int *h, int *pts, int *x, int *y);

public slots:
    virtual void parentToolButton_clicked();
    virtual void userToolButton_clicked(bool);
    virtual void systemToolButton_clicked(bool);
    virtual void pathComboBox_currentIndexChanged(QString);
    virtual void dirListView_selectionChanged();
    virtual void dirListView_activated(const QModelIndex &);

    virtual void sourceComboBox_currentIndexChanged(int);
    virtual void sourcePushButton_clicked();
    virtual void openPushButton_clicked();

private:
    struct {
	QString	userDir;
	QString	systemDir;
	bool archiveSource;
	QDirModel *dirModel;
	QModelIndex dirIndex;
	QCompleter *completer;
    } my;

    void setPath(const QString &);
    void setPath(const QModelIndex &);
    void setPathUi(const QString &);

    void setupComboBoxes(bool);
    int setupLiveComboBoxes();
    int setupArchiveComboBoxes();

    void hostAdd();
    void archiveAdd();

    bool useLiveContext(int);
    bool useArchiveContext(int);
    bool useComboBoxContext(bool);
    bool openViewFiles(const QStringList &);
};

#endif	// OPENVIEWDIALOG_H
