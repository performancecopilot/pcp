/* -*- C++ -*- */

#ifndef _BASEOBJ_H_
#define _BASEOBJ_H_

/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 */


#include "ViewObj.h"
#include "ToggleMod.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

class SoCube;

class BaseObj : public ViewObj
{
protected:

    OMC_Bool		_on;
    uint_t		_borderX;
    uint_t		_borderZ;
    uint_t		_baseHeight;
    uint_t		_length;
    uint_t		_maxHeight;
    float		_baseColor[3];
    INV_ToggleMod	*_mod;
    SoCube		*_cube;
    OMC_String		_label;
	
public:

    virtual ~BaseObj();

    BaseObj(OMC_Bool onFlag,
	    const DefaultObj &defaults,
	    uint_t , uint_t , 
	    uint_t cols = 1, uint_t rows = 1, 
	    BaseObj::Alignment align = BaseObj::center);

    uint_t borderX() const
	{ return _borderX; }
    uint_t borderZ() const
	{ return _borderZ; }
    uint_t baseWidth() const
	{ return _borderX * 2; }
    uint_t baseDepth() const
	{ return _borderZ * 2; }
    uint_t baseHeight() const
	{ return _baseHeight; }
    float baseColor(uint_t i) const
	{ return _baseColor[i]; }
    const OMC_String &label() const
	{ return _label; }
    int length() const
	{ return _length; }
    int maxHeight() const
	{ return _maxHeight; }    

    // Local changes
    uint_t &borderX()
	{ return _borderX; }
    uint_t &borderZ()
	{ return _borderZ; }
    uint_t &baseHeight()
	{ return _baseHeight; }
    void baseColor(float r, float g, float b)
	{ _baseColor[0] = r; _baseColor[1] = g; _baseColor[2] = b; }
    OMC_String &label()
	{ return _label; }
    uint_t &length()
	{ return _length; }
    uint_t& maxHeight()
	{ return _maxHeight; }

    OMC_Bool state() const
	{ return _on; }
    void state(OMC_Bool flag);
    
    virtual uint_t width() const = 0;
    virtual uint_t depth() const = 0;

    void addBase(SoSeparator *sep);

    void setBaseSize (uint_t width, uint_t depth);

//    virtual void setTran(float xTran, float zTran, uint_t width, uint_t depth);

    virtual INV_Modulate *modObj()
	{ return _mod; }

    virtual void finishedAdd();

    // Output
    virtual void display(ostream& os) const;

    virtual const char* name() const = 0;

    friend ostream& operator<<(ostream& os, BaseObj const& rhs);

protected:

    void add(INV_Modulate *mod);

private:

    BaseObj();
    BaseObj(BaseObj const &);
    BaseObj const& operator=(BaseObj const &);
};

#endif /* _BASEOBJ_H_ */
