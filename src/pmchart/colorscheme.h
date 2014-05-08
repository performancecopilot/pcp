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
#ifndef COLORSCHEME_H
#define COLORSCHEME_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QColor>

class ColorScheme
{
public:
    ColorScheme();

    QString name() { return my.name; }
    int size() { return my.colors.size(); }
    QColor color(int i) { return my.colors.at(i); }
    QString colorName(int i) { return my.colorNames.at(i); }

    QList<QColor> colors() { return my.colors; }
    QStringList colorNames() { return my.colorNames; }

    void setName(QString name) { my.name = name; }
    void setModified(bool modified) { my.isModified = modified; }

    void clear();
    void addColor(QString name);
    void addColor(QColor color);
    void setColorNames(QStringList);

    static ColorScheme *findScheme(QString);
    static bool lookupScheme(QString);	// search in global list
    static bool removeScheme(QString);	// remove from global list

    static QColor colorSpec(QString);	// QT color / convert rgbi:

private:
    struct {
	QString name;
	bool isModified;
	QList<QColor> colors;
	QStringList colorNames;
    } my;
};

#endif
