/* -*- C++ -*- */

#ifndef _STACKOBJ_H_
#define _STACKOBJ_H_

/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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


#include "ModObj.h"
#include "StackMod.h"

class SoSeparator;

class StackObj : public ModObj
{
protected:

    uint_t			_width;
    uint_t			_depth;
    INV_StackMod::Height	_height;
    ViewObj::Shape		_shape;
    INV_StackMod		*_stack;
    OMC_String			_text;

public:

    virtual ~StackObj();

    StackObj(INV_StackMod::Height height,
	     ViewObj::Shape shape,
	     OMC_Bool baseFlag,
	     const DefaultObj &defaults,
	     uint_t x, uint_t z,
	     uint_t cols = 1, uint_t rows = 1, 
	     BaseObj::Alignment align = BaseObj::center);

    virtual uint_t width() const
	{ return _width; }
    virtual uint_t depth() const
	{ return _depth; }
    INV_StackMod::Height height() const
	{ return _height; }

    void setFillText(const char *str)
	{ _text = str; }

    virtual void finishedAdd();

    virtual void setTran(float xTran, float zTran, uint_t width, uint_t depth);

    virtual const char* name() const
    	{ return "Stack"; }

    virtual void display(ostream& os) const;

    friend ostream& operator<<(ostream& os, StackObj const& rhs);

    virtual void setBarHeight (uint_t h) { _maxHeight = h; }
private:

    StackObj();
    StackObj(StackObj const&);
    StackObj const& operator=(StackObj const &);
};

#endif /* _STACKOBJ_H_ */
