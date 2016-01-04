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
#ifndef SEARCHDIALOG_H
#define SEARCHDIALOG_H

#include "ui_searchdialog.h"
#include <QProcess>

class NameSpace;
class QTreeWidget;

class SearchDialog : public QDialog, public Ui::SearchDialog
{
    Q_OBJECT

public:
    SearchDialog(QWidget* parent);
    void reset(QTreeWidget *pmns);

public slots:
    virtual void clear();
    virtual void search();
    virtual void ok();
    virtual void changed();
    virtual void selectall();
    virtual void listchanged();

protected slots:
    virtual void languageChange();

private:
    struct {
	QTreeWidget *pmns;
	bool isArchive;
	QString source;
	QString metric;
	int count;
	QList<NameSpace *> pmnsList;
    } my;
};

#endif // SEARCHDIALOG_H
