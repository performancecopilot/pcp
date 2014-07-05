/* -*- C++ -*- */

#ifndef _INV_SCALEMOD_H_
#define _INV_SCALEMOD_H_

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


#include "Modulate.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

class SoBaseColor;
class SoScale;
class SoNode;
class INV_Launch;

class INV_ScaleMod : public INV_Modulate
{
protected:

    State	_state;
    SoBaseColor *_color;
    SoScale     *_scale;
    float	_xScale;
    float	_yScale;
    float	_zScale;

public:

    virtual ~INV_ScaleMod();

    INV_ScaleMod(const char *metric, double scale, const SbColor &color,
		  SoNode *obj, float xScale, float yScale, float zScale);

    virtual void refresh(OMC_Bool fetchFlag);

    virtual uint_t select(SoPath *);
    virtual uint_t remove(SoPath *);

    virtual void infoText(OMC_String &str, OMC_Bool) const;

    virtual void launch(INV_Launch &launch, OMC_Bool) const;

    virtual void dump(ostream &) const;

private:

    INV_ScaleMod();
    INV_ScaleMod(const INV_ScaleMod &);
    const INV_ScaleMod &operator=(const INV_ScaleMod &);
    // Never defined
};

#endif /* _INV_SCALEMOD_H_ */
