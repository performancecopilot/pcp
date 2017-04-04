/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
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
#ifndef _COLORLIST_H_
#define _COLORLIST_H_

#include "metriclist.h"
#include <QStringList>

struct ColorSpec;

typedef QList<ColorSpec *> ColorsList;

class ColorList
{
private:

    QStringList	_names;
    ColorsList	_colors;

public:

    virtual ~ColorList();

    ColorList();

    int numLists() const
	{ return _colors.size(); }

    const ColorSpec *list(const char *name);

    bool add(const char *name, const char *color = NULL);
    bool add(const char *name, float red, float green, float blue);

    // Add colors
    bool addColor(const char *color);
    bool addColor(float red, float blue, float green);

    // Add scaled colors
    bool addColor(const char *color, double max);
    bool addColor(float red, float green, float blue, double max);

    static bool findColor(const char *color, float &r, float &g, float &b);

    friend QTextStream& operator<<(QTextStream& os, ColorList const& rhs);

private:
    bool findColor(const char *color);

    ColorList(ColorList const &);
    ColorList const& operator=(ColorList const &);
    // Never defined
};

struct ColorSpec
{
    bool		_scale;
    SbColorList		_list;
    QList<double>	_max;

    ColorSpec(bool scale) : _scale(scale) {}
};

extern ColorList	theColorLists;

#endif /* _COLORLIST_H_ */
