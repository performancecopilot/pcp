/*
 * Copyright (c) 2007, Aconex.  All Rights Reserved.
 * Copyright (c) 2013, Red Hat, Inc.
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

#include "colorscheme.h"
#include "main.h"

ColorScheme::ColorScheme()
{
    my.isModified = false;
    my.name = QString::null;
}

bool ColorScheme::lookupScheme(QString name)
{
    for (int i = 0; i < globalSettings.colorSchemes.size(); i++)
	if (name == globalSettings.colorSchemes[i].name())
	    return true;
    return false;
}

ColorScheme *ColorScheme::findScheme(QString name)
{
    for (int i = 0; i < globalSettings.colorSchemes.size(); i++)
	if (name == globalSettings.colorSchemes[i].name())
	    return &globalSettings.colorSchemes[i];
    return NULL;
}

bool ColorScheme::removeScheme(QString name)
{
    for (int i = 0; i < globalSettings.colorSchemes.size(); i++)
	if (name == globalSettings.colorSchemes[i].name()) {
	    globalSettings.colorSchemes.removeAt(i);
	    return true;
	}
    return false;
}

static inline int hexval(float f)
{
    return ((int)(0.5 + f*256) < 256 ? (int)(0.5 + f*256) : 256);
}

QColor ColorScheme::colorSpec(QString name)
{
    QColor color;
    QString rgbi = name;

    if (rgbi.left(5) != "rgbi:")
	color.setNamedColor(name);
    else {
	float fr, fg, fb;
	if (sscanf((const char *)rgbi.toAscii(), "rgbi:%f/%f/%f", &fr, &fg, &fb) == 3)
	    color.setRgb(hexval(fr), hexval(fg), hexval(fb));
	// else return color as-is, i.e. invalid.
    }
    return color;
}

void ColorScheme::clear()
{
    my.colors.clear();
    my.colorNames.clear();
}

void ColorScheme::setColorNames(QStringList colorNames)
{
    my.colorNames = colorNames;
    for (int i = 0; i < colorNames.size(); i++)
	my.colors << QColor(colorNames.at(i));
}

void ColorScheme::addColor(QColor color)
{
    my.colors.append(color);
    my.colorNames.append(color.name());
}

void ColorScheme::addColor(QString name)
{
    my.colors.append(colorSpec(name));
    my.colorNames.append(name);
}
