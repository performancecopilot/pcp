/*
 * Copyright (c) 2013-2015,2022-2023 Red Hat.
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

    connect(buttonBox, SIGNAL(accepted()),
		this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()),
		this, SLOT(reject()));
    connect(proxyCheckBox, SIGNAL(toggled(bool)),
		this, SLOT(proxyCheckBox_toggled(bool)));
    connect(containerCheckBox, SIGNAL(toggled(bool)),
		this, SLOT(containerCheckBox_toggled(bool)));
    connect(advancedPushButton, SIGNAL(clicked()),
		this, SLOT(advancedPushButton_clicked()));
    connect(authenticateCheckBox, SIGNAL(toggled(bool)),
		this, SLOT(authenticateCheckBox_toggled(bool)));

    my.advancedState = false;
    my.advancedString = advancedPushButton->text();
    my.originalHeight = geometry().height();
    my.minimalHeight = minimumHeight();
    changedAdvancedState();
}

void
HostDialog::quit()
{
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
        return QString();
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
        return QString();

    if (proxyLineEdit->isModified()) {
        QString proxy = proxyLineEdit->text().trimmed();
        if (proxy.length() > 0)
            host.prepend("@").prepend(proxy);
    }

    if (containerCheckBox->isChecked() ||
        encryptedCheckBox->isChecked() ||
	authenticateCheckBox->isChecked())
	need_delimiter = true;

    if (need_delimiter)
	host.append("?");

    if (containerCheckBox->isChecked()) {
	QString container = containerLineEdit->text().trimmed();
	host.append("container=").append(container);
	need_separator = true;
    }

    if (encryptedCheckBox->isChecked()) {
	if (need_separator)
	    host.append("&");
	host.append("secure=enforce");
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

    if (encryptedCheckBox->isChecked())
        flags |= PM_CTXFLAG_SECURE;
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

    encryptedCheckBox->setHidden(hidden);
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
