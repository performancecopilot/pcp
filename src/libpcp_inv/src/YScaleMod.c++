/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 */


#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSeparator.h>
#include "Inv.h"
#include "YScaleMod.h"
#include "ModList.h"
#include "Metric.h"
#include "Launch.h"

INV_YScaleMod::~INV_YScaleMod()
{
}

INV_YScaleMod::INV_YScaleMod(const char *str,
                     double scale,
                     const SbColor &color,
                     SoNode *obj)
: INV_ScaleMod(str, scale, color, obj, 0.0, 1.0, 0.0)
{
}

void
INV_YScaleMod::dump(ostream &os) const
{
    os << "INV_YScaleMod: ";

    if (status() < 0)
        os << "Invalid metric";
    else {
        os << "state = ";
	dumpState(os, _state);
        os << ", scale = " << _scale->scaleFactor.getValue()[1] << ": ";
        _metrics->metric(0).dump(os, OMC_true);
    }
}
