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

#include <QtCore/QString>
#include <qmc_context.h>
#include <qmc_group.h>

class Source
{
public:
    Source(QmcGroup *);
    int type();
    QString host();
    const char *sourceAscii();

    void add(QmcContext *, bool);
    void setupTree(QTreeWidget *, bool);
    void setupCombos(QComboBox *, QComboBox *, bool);
    static void setCurrentFromCombo(const QString, bool);
    static int useComboContext(QComboBox *, bool);

protected:
    void addLive(QmcContext *);
    void addArchive(QmcContext *);
    static int useLiveContext(QString);
    static int useArchiveContext(QString);
    static void setLiveFromCombo(QString);
    static void setArchiveFromCombo(QString);

private:
    struct {
	QmcGroup *fetchGroup;
	QmcContext *context;
    } my;
};

#endif	// SOURCE_H
