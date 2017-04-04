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
#ifndef QED_ACTIONLIST_H
#define QED_ACTIONLIST_H

#include <QtGui>

class QedActionList : public QString
{
public:
    QedActionList(const char *id);
    const char *identity() const;

    void addName(const char *name);
    void addAction(const char *act);
    // QMenu &menu() { /* TODO: construct a real QMenu */ }

    int defaultPos(void);
    void setDefaultPos(unsigned int pos);

private:
    struct {
	QStringList	names;		// menu names
	QStringList	actions;	// commands to enact
	int		defaultPos;	// position of default action in list
    } my;
};

#endif // QED_ACTIONLIST_H
