/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
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
#ifndef SOURCE_H
#define SOURCE_H

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtGui/QComboBox>
#include <QtGui/QFileDialog>
#include <QtGui/QToolButton>
#include "namespace.h"
#include "fileiconprovider.h"
#include <qmc_group.h>
#include <qmc_context.h>

class Source
{
public:
    Source(QmcGroup *);
    int type();
    QString host();
    const char *sourceAscii();
    void add(QmcContext *);
    void setupTree(QTreeWidget *, bool);
    void setupCombo(QComboBox *, bool);

    static QString makeSourceBaseName(const QmcContext *);
    static QString makeSourceAnnotatedName(const QmcContext *);
    static int useSourceName(QWidget *, QString &);

    static void setCurrentFromCombo(const QString, bool);
    static QString makeComboText(const QmcContext *);
    static int useComboContext(QWidget *parent, QComboBox *combo, bool);

private:
    void dump();

    struct {
	QmcGroup *fetchGroup;
	QmcContext *context;
    } my;
};

class ArchiveDialog : public QFileDialog
{
    Q_OBJECT

public:
    ArchiveDialog(QWidget *parent) : QFileDialog(parent)
	{
	    setFileMode(QFileDialog::ExistingFiles);
	    setAcceptMode(QFileDialog::AcceptOpen);
	    setIconProvider(fileIconProvider);
	    setDirectory(QDir::homePath());
	    setWindowTitle(tr("Add Archive"));
	}
};

#endif	// SOURCE_H
