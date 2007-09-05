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
#include <pcp/pmc/Group.h>
#include <pcp/pmc/Context.h>

class Source
{
public:
    Source(PMC_Group *);
    int type();
    QString host();
    const char *source();
    void add(PMC_Context *);
//    NameSpace *root(void);
//    void setRoot(NameSpace *);
    void setupTree(QTreeWidget *);
    void setupCombo(QComboBox *);
    void setCurrentFromCombo(const QString);
    void setCurrentInCombo(QComboBox *);

    static QString makeSourceBaseName(const PMC_Context *);
    static QString makeSourceAnnotatedName(const PMC_Context *);
    static int useSourceName(QWidget *, QString &);

    static QString makeComboText(const PMC_Context *);
    static int useComboContext(QWidget *parent, QComboBox *combo);

private:
    static void dump(FILE *);

    struct {
	PMC_Group *fetchGroup;
	PMC_Context *context;
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
	}
};

#endif	// SOURCE_H
