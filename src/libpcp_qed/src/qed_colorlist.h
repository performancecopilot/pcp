/*
 * Copyright (c) 2013-2014, Red Hat.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#ifndef QED_COLORLIST_H
#define QED_COLORLIST_H

#include <QtGui>

class QedColorList : public QString
{
public:
    QedColorList(const char *id);
    void addColor(const char *name);
    const char *identity() const;
    unsigned int length();

private:
    struct {
	QStringList	names;		// color names
    } my;
};

#endif // QED_COLORLIST_H
