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
#ifndef _COLORMOD_H
#define _COLORMOD_H

#include "colorscale.h"
#include "modulate.h"

class SoBaseColor;
class SoNode;
class Launch;

class ColorMod : public Modulate
{
private:

    State		_state;
    ColorScale		_scale;
    SoBaseColor		*_color;

public:

    virtual ~ColorMod();

    ColorMod(const char *metric, double scale, 
		 const ColorScale &colors, SoNode *obj);

    virtual void refresh(bool fetchFlag);

    virtual int select(SoPath *);
    virtual int remove(SoPath *);

    virtual void infoText(QString &str, bool) const;

    virtual void launch(Launch &launch, bool) const;

    virtual void dump(QTextStream &) const;

private:

    ColorMod();
    ColorMod(const ColorMod &);
    const ColorMod &operator=(const ColorMod &);
    // Never defined
};

#endif /* _COLORMOD_H */
