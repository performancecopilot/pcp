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

#include "qed_colorlist.h"

QedColorList::QedColorList(const char *id) : QString(id)
{
    // TODO
}

const char *
QedColorList::identity(void) const
{
    return this->toAscii();
}

void
QedColorList::addColor(const char *name)
{
    my.names << QString(name);
}

unsigned int
QedColorList::length(void)
{
    return my.names.length();
}
