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

#include "qed_actionlist.h"

QedActionList::QedActionList(const char *id) : QString(id)
{
    my.defaultPos = -1;
}

const char *
QedActionList::identity(void) const
{
    return this->toAscii();
}

void
QedActionList::addName(const char *name)
{
    my.names << QString(name);
}

void
QedActionList::addAction(const char *act)
{
    my.actions << QString(act);
}

// QMenu &QedActionList::menu() { }

int
QedActionList::defaultPos(void)
{
    return my.defaultPos;
}

void
QedActionList::setDefaultPos(unsigned int pos)
{
    my.defaultPos = (int)pos;
}
