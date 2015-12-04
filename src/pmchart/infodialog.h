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
#ifndef INFODIALOG_H
#define INFODIALOG_H

#include "ui_infodialog.h"
#include <QProcess>

class InfoDialog : public QDialog, public Ui::InfoDialog
{
    Q_OBJECT

public:
    InfoDialog(QWidget* parent);
    void reset(QString, QString, QString, int);
    void pminfo();
    void pmval();

public slots:
    virtual void pminfoStdout();
    virtual void pminfoStderr();
    virtual void pmvalStdout();
    virtual void pmvalStderr();
    virtual void infoTabCurrentChanged(int);
    virtual void quit();

protected slots:
    virtual void languageChange();

private:
    struct {
	bool pminfoStarted;
	bool pmvalStarted;
	int sourceType;
	QString source;
	QString metric;
	QString instance;
	QProcess *pminfoProc;
	QProcess *pmvalProc;
    } my;
};

#endif // INFODIALOG_H
