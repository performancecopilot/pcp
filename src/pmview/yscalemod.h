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
#ifndef _YSCALEMOD_H_
#define _YSCALEMOD_H_

#include "scalemod.h"

class SoBaseColor;
class SoScale;
class SoNode;
class Launch;

class YScaleMod : public ScaleMod
{
public:

    virtual ~YScaleMod();

    YScaleMod(const char *metric, double scale, const SbColor &color,
		  SoNode *obj);

    virtual void dump(QTextStream &) const;

private:

    YScaleMod();
    YScaleMod(const YScaleMod &);
    const YScaleMod &operator=(const YScaleMod &);
    // Never defined
};

#endif /* _YSCALEMOD_H_ */
