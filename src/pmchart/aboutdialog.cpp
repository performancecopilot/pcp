/*
 * Copyright (c) 2023 Red Hat.  All Rights Reserved.
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
#include "aboutdialog.h"
#include <pcp/pmapi.h>
#include <QRegularExpression>

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent)
{
    setupUi(this);

    connect(aboutOKButton, SIGNAL(clicked()),
		this, SLOT(aboutOKButton_clicked()));

    QRegularExpression rx("\\b(VERSION)\\b");
    QString version = versionTextLabel->text();
    version.replace(rx, pmGetConfig("PCP_VERSION"));
    versionTextLabel->setText(version);
}

void AboutDialog::aboutOKButton_clicked()
{
    done(0);
}
