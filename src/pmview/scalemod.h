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
#ifndef _SCALEMOD_H_
#define _SCALEMOD_H_

#include "modulate.h"

class SoBaseColor;
class SoScale;
class SoNode;
class Launch;

class ScaleMod : public Modulate
{
protected:

    State	_state;
    SoBaseColor *_color;
    SoScale     *_scale;
    float	_xScale;
    float	_yScale;
    float	_zScale;

public:

    virtual ~ScaleMod();

    ScaleMod(const char *metric, double scale, const SbColor &color,
		  SoNode *obj, float xScale, float yScale, float zScale);

    virtual void refresh(bool fetchFlag);

    virtual int select(SoPath *);
    virtual int remove(SoPath *);

    virtual void infoText(QString &str, bool) const;

    virtual void launch(Launch &launch, bool) const;

    virtual void dump(QTextStream &) const;

private:

    ScaleMod();
    ScaleMod(const ScaleMod &);
    const ScaleMod &operator=(const ScaleMod &);
    // Never defined
};

#endif /* _SCALEMOD_H_ */
