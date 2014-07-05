/* -*- C++ -*- */

#ifndef _INV_YSCALEMOD_H_
#define _INV_YSCALEMOD_H_

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


#include "ScaleMod.h"

class SoBaseColor;
class SoScale;
class SoNode;
class INV_Launch;

class INV_YScaleMod : public INV_ScaleMod
{
public:

    virtual ~INV_YScaleMod();

    INV_YScaleMod(const char *metric, double scale, const SbColor &color,
		  SoNode *obj);

    virtual void dump(ostream &) const;

private:

    INV_YScaleMod();
    INV_YScaleMod(const INV_YScaleMod &);
    const INV_YScaleMod &operator=(const INV_YScaleMod &);
    // Never defined
};

#endif /* _INV_YSCALEMOD_H_ */

