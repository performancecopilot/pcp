/*
 * Copyright (c) 2006, Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2007, Nathan Scott.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Ken McDonell, kenj At internode DoT on DoT net
 *                      Nathan Scott, nathans At debian DoT org
 */
#ifndef SOURCE_H
#define SOURCE_H

#include <qstring.h>
#include <qptrlist.h>
#include <qcombobox.h>
#include <qfiledialog.h>
#include <qtoolbutton.h>
#include "namespace.h"
#include <pcp/pmc/Group.h>
#include <pcp/pmc/Context.h>

class Source
{
public:
    Source(PMC_Group *);
    int 	type();
    QString	host();
    const char	*source();
    void	add(PMC_Context *);
    NameSpace	*root(void);
    void	setRoot(NameSpace *);
    void	setupListView(QListView *);
    void	setupCombo(QComboBox *);
    void	setCurrentFromCombo(const QString);
    void	setCurrentInCombo(QComboBox *);

    static QString makeSourceBaseName(const PMC_Context *);
    static QString makeSourceAnnotatedName(const PMC_Context *);
    static int useSourceName(QWidget *, QString &);

    static QString makeComboText(const PMC_Context *);
    static int useComboContext(QWidget *parent, QComboBox *combo);

private:
    void	dump(FILE *);

    PMC_Group		*fetchGroup;
    struct source	*firstSource;
    struct source	*currentSource;
};

class ArchiveDialog : public QFileDialog
{
    Q_OBJECT
public:
    ArchiveDialog(QWidget *);
    ~ArchiveDialog();
private slots:
    void logDirClicked();
private:
    QToolButton *logButton;
};


#endif
