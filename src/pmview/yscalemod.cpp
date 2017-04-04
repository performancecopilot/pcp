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
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSeparator.h>
#include "main.h"
#include "yscalemod.h"
#include "modlist.h"
#include "launch.h"

YScaleMod::~YScaleMod()
{
}

YScaleMod::YScaleMod(const char *str, double scale,
                     const SbColor &color, SoNode *obj)
: ScaleMod(str, scale, color, obj, 0.0, 1.0, 0.0)
{
}

void
YScaleMod::dump(QTextStream &os) const
{
    os << "YScaleMod: ";

    if (status() < 0)
        os << "Invalid metric";
    else {
        os << "state = ";
	dumpState(os, _state);
        os << ", scale = " << _scale->scaleFactor.getValue()[1] << ": ";
        _metrics->metric(0).dump(os, true);
    }
}
