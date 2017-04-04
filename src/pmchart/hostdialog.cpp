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
#include <QDir>
#include <QMessageBox>
#include "hostdialog.h"
#include "main.h"

HostDialog::HostDialog(QWidget *parent) : QDialog(parent)
{
    setupUi(this);
    my.nssGuiStarted = false;
    my.advancedState = false;
    my.advancedString = advancedPushButton->text();
    my.originalHeight = geometry().height();
    my.minimalHeight = minimumHeight();
    changedAdvancedState();
}

void
HostDialog::quit()
{
    if (my.nssGuiStarted) {
	my.nssGuiProc->terminate();
	my.nssGuiStarted = false;
    }
    my.advancedState = false;
}

void
HostDialog::languageChange()
{
    retranslateUi(this);
}

void
HostDialog::containerCheckBox_toggled(bool enableContainers)
{
    containerLineEdit->setEnabled(enableContainers);
}

void
HostDialog::secureCheckBox_toggled(bool enableSecure)
{
    certificatesPushButton->setEnabled(enableSecure);
}

void
HostDialog::proxyCheckBox_toggled(bool enableProxy)
{
    proxyLineEdit->setEnabled(enableProxy);
}

void
HostDialog::authenticateCheckBox_toggled(bool enableAuthenticate)
{
    usernameLabel->setEnabled(enableAuthenticate);
    usernameLineEdit->setEnabled(enableAuthenticate);
    passwordLabel->setEnabled(enableAuthenticate);
    passwordLineEdit->setEnabled(enableAuthenticate);
    realmLabel->setEnabled(enableAuthenticate);
    realmLineEdit->setEnabled(enableAuthenticate);
}

QString
HostDialog::getHostName(void) const
{
    QString host;

    if (hostLineEdit->isModified())
        host = hostLineEdit->text().trimmed();
    if (host.length() == 0)
        return QString::null;
    return host;
}

QString
HostDialog::getHostSpecification(void) const
{
    QString host = getHostName();
    bool need_separator = false;
    bool need_delimiter = false;

    if (hostLineEdit->isModified())
        host = hostLineEdit->text().trimmed();
    if (host.length() == 0)
        return QString::null;

    if (proxyLineEdit->isModified()) {
        QString proxy = proxyLineEdit->text().trimmed();
        if (proxy.length() > 0)
            host.prepend("@").prepend(proxy);
    }

    if (containerCheckBox->isChecked() ||
	authenticateCheckBox->isChecked())
	need_delimiter = true;

    if (need_delimiter)
	host.append("?");

    if (containerCheckBox->isChecked()) {
	QString container = containerLineEdit->text().trimmed();
	host.append("container=").append(container);
	need_separator = true;
    }

    if (authenticateCheckBox->isChecked()) {
	QString username = usernameLineEdit->text().trimmed();
	QString password = passwordLineEdit->text().trimmed();
	QString realm = realmLineEdit->text().trimmed();

	if (need_separator)
	    host.append("&");
	host.append("username=").append(username);
	host.append("&password=").append(password);
	host.append("&realm=").append(realm);
	need_separator = true;
    }
    return host;
}

int
HostDialog::getContextFlags(void) const
{
    int flags = 0;

    if (secureCheckBox->isChecked())
        flags |= PM_CTXFLAG_SECURE;
    if (compressCheckBox->isChecked())
        flags |= PM_CTXFLAG_COMPRESS;
    if (authenticateCheckBox->isChecked())
        flags |= PM_CTXFLAG_AUTH;
    return flags;
}

void
HostDialog::changedAdvancedState(void)
{
    int	height;
    bool hidden = (my.advancedState == false);

    proxyCheckBox->setHidden(hidden);
    proxyLineEdit->setHidden(hidden);
    containerCheckBox->setHidden(hidden);
    containerLineEdit->setHidden(hidden);

    secureCheckBox->setHidden(hidden);
    compressCheckBox->setHidden(hidden);
    certificatesPushButton->setHidden(hidden);

    authenticateCheckBox->setHidden(hidden);
    usernameLabel->setHidden(hidden);
    usernameLineEdit->setHidden(hidden);
    passwordLabel->setHidden(hidden);
    passwordLineEdit->setHidden(hidden);
    realmLabel->setHidden(hidden);
    realmLineEdit->setHidden(hidden);

    if (my.advancedState) {
	advancedPushButton->setText(tr("Hide"));
    } else {
	advancedPushButton->setText(my.advancedString);
	proxyCheckBox->setChecked(false);
	proxyLineEdit->clear();
	containerCheckBox->setChecked(false);
	containerLineEdit->clear();
	secureCheckBox->setChecked(false);
	compressCheckBox->setChecked(false);
	authenticateCheckBox->setChecked(false);
	usernameLineEdit->clear();
	passwordLineEdit->clear();
	realmLineEdit->clear();
    }

    height = my.advancedState ? my.originalHeight : my.minimalHeight;
    resize(geometry().width(), height);
}

void
HostDialog::advancedPushButton_clicked()
{
    my.advancedState = !my.advancedState;
    changedAdvancedState();
}

void
HostDialog::certificatesPushButton_clicked()
{
    if (!my.nssGuiStarted) {
	my.nssGuiStarted = true;
	nssGuiStart();
    }
}

void
HostDialog::nssGuiStart()
{
    QString	dbpath = QDir::toNativeSeparators(QDir::homePath());
    int		sep = __pmPathSeparator();

    dbpath.append(sep).append(".pki").append(sep).append("nssdb");
    dbpath.prepend("sql:");	// only use sqlite NSS databases

    QStringList	arguments;
    arguments << "--dbdir";
    arguments << dbpath;

    my.nssGuiProc = new QProcess(this);
    connect(my.nssGuiProc, SIGNAL(error(QProcess::ProcessError)),
                 this, SLOT(nssGuiError(QProcess::ProcessError)));
    connect(my.nssGuiProc, SIGNAL(finished(int, QProcess::ExitStatus)),
                 this, SLOT(nssGuiFinished(int, QProcess::ExitStatus)));
    my.nssGuiProc->start("nss-gui", arguments);
}

void
HostDialog::nssGuiError(QProcess::ProcessError error)
{
    QString message;

    if (error == QProcess::FailedToStart) {
	message = tr("Unable to start the nss-gui helper program.\n");
    } else {
	message.append(tr("Generic nss-gui program failure, state="));
	message.append(error).append("\n");
    }
    QMessageBox::warning(this, pmProgname, message,
		QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		Qt::NoButton, Qt::NoButton);
    my.nssGuiStarted = false;
}

void
HostDialog::nssGuiFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    (void)exitStatus;

    if (exitCode) {
	QString message(tr("nss-gui helper process failed\nExit status was:"));
	message.append(exitCode).append("\n");
	QMessageBox::warning(this, pmProgname, message,
		QMessageBox::Ok|QMessageBox::Default|QMessageBox::Escape,
		Qt::NoButton, Qt::NoButton);
    }
    my.nssGuiStarted = false;
}
