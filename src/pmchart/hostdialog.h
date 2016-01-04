/*
 * Copyright (c) 2013-2015, Red Hat.
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
#ifndef HOSTDIALOG_H
#define HOSTDIALOG_H

#include "ui_hostdialog.h"
#include <QProcess>

class HostDialog : public QDialog, public Ui::HostDialog
{
    Q_OBJECT

public:
    HostDialog(QWidget* parent);
    int getContextFlags() const;
    QString getHostName(void) const;
    QString getHostSpecification() const;
    void reset();

protected slots:
    virtual void languageChange();

private slots:
    virtual void quit();
    virtual void proxyCheckBox_toggled(bool);
    virtual void secureCheckBox_toggled(bool);
    virtual void containerCheckBox_toggled(bool);
    virtual void advancedPushButton_clicked();
    virtual void certificatesPushButton_clicked();
    virtual void authenticateCheckBox_toggled(bool);
    virtual void nssGuiError(QProcess::ProcessError);
    virtual void nssGuiFinished(int, QProcess::ExitStatus);

private:
    void nssGuiStart();
    void changedAdvancedState();

    struct {
	QProcess	*nssGuiProc;
	bool		nssGuiStarted;
	bool		advancedState;
	QString		advancedString;
	int		originalHeight;
	int		minimalHeight;
    } my;
};

#endif // HOSTDIALOG_H
