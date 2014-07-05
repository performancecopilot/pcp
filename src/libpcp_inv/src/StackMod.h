/* -*- C++ -*- */

#ifndef _INV_STACKMOD_H_
#define _INV_STACKMOD_H_

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


#include "Bool.h"
#include "Vector.h"
#include "Modulate.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

class SoBaseColor;
class SoTranslation;
class SoScale;
class SoNode;
class SoSwitch;
class INV_Launch;

struct INV_StackBlock {
    SoSeparator		*_sep;
    SoBaseColor		*_color;
    SoScale		*_scale;
    SoTranslation	*_tran;
    INV_Modulate::State	_state;
    OMC_Bool		_selected;
};

typedef OMC_Vector<INV_StackBlock> INV_BlockList;

class INV_StackMod : public INV_Modulate
{
public:

    enum Height { unfixed, fixed, util };

private:

    static const float	theDefFillColor[];
    static const char	theStackId;

    INV_BlockList	_blocks;
    SoSwitch		*_switch;
    Height		_height;
    OMC_String		_text;
    uint_t		_selectCount;
    uint_t		_infoValue;
    uint_t		_infoMetric;
    uint_t		_infoInst;

public:

    virtual ~INV_StackMod();

    INV_StackMod(INV_MetricList *metrics,
		 SoNode *obj, 
		 Height height = unfixed);

    void setFillColor(const SbColor &col);
    void setFillColor(uint32_t packedcol);
    void setFillText(const char *str)
	{ _text = str; }

    virtual void refresh(OMC_Bool fetchFlag);

    virtual void selectAll();
    virtual uint_t select(SoPath *);
    virtual uint_t remove(SoPath *);

    virtual void selectInfo(SoPath *);
    virtual void removeInfo(SoPath *);

    virtual void infoText(OMC_String &str, OMC_Bool) const;

    virtual void launch(INV_Launch &launch, OMC_Bool all) const;

    virtual void dump(ostream &) const;

private:

    INV_StackMod();
    INV_StackMod(const INV_StackMod &);
    const INV_StackMod &operator=(const INV_StackMod &);
    // Never defined

    void findBlock(SoPath *path, uint_t &metric, uint_t &inst, 
		   uint_t &value, OMC_Bool idMetric = OMC_true);
};

#endif /* _INV_STACKMOD_H_ */

