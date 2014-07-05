/* -*- C++ -*- */

#ifndef _VIEWOBJ_H_
#define _VIEWOBJ_H_

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


#include "Bool.h"
#include "Inv.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

class SoNode;
class SoSeparator;
class SoTranslation;
class INV_Modulate;
class DefaultObj;

class ViewObj
{
public:

    enum Alignment { north, south, east, west, 
		     northEast, northWest, southEast, southWest,
		     center };

    enum Shape { cube, cylinder };

    // Poor man substitution for RTTI until O32 compiler will either
    // die or start supporting that stuff
    enum ObjType {VIEWOBJ = 1, 
		  BASEOBJ = 2, 
		  LABELOBJ = 4,
		  MODOBJ = 8,
		  GRIDOBJ = 16,
		  PIPEOBJ = 32,
		  BAROBJ = 64,
		  STACKOBJ = 128,
		  LINK = 256, 
		  XING = 512,
		  SCENEFILEOBJ = 1024
    };

protected:

    SoSeparator		*_root;
    SoTranslation	*_tran;

    uint_t		_objtype;
    uint_t		_col;
    uint_t		_row;
    uint_t		_cols;
    uint_t		_rows;
    uint_t		_maxHeight;
    float		_xAlign;
    float		_zAlign;

    static uint_t	theNumModObjects;

public:

    virtual ~ViewObj();

    ViewObj(uint_t, uint_t, uint_t cols = 1, uint_t rows = 1, 
	    Alignment align = center);

    // The Scene Graph Root for this object
    SoSeparator* root()
	{ return _root; }

    uint_t objbits() const { return _objtype; }
    uint_t row() const { return _row; }
    uint_t col() const { return _col; }

    uint_t cols() const
	{ return _cols; }
    uint_t rows() const
	{ return _rows; }
    uint_t height() const
	{ return _maxHeight; }
    uint_t &height()
	{ return _maxHeight; }
    float xAlign() const
	{ return _xAlign; }
    float zAlign() const
	{ return _zAlign; }

    // Set the coordinates (and the allocated size) from parent
    virtual void setTran(float xTran, float zTran, uint_t width, uint_t depth);

    // Size (in object coordinates). Must be the correct value before
    // the object is added to the parent.
    virtual uint_t width() const = 0;
    virtual uint_t depth() const = 0;

    static uint_t numModObjects()
	{ return theNumModObjects; }

    // Return default object
    static SoNode *object(Shape shape);

    virtual INV_Modulate *modObj()
	{ return (INV_Modulate *)0; }

    // Inform object parsing stuff is done

    virtual void finishedAdd() = 0;

    // Output
    virtual void display(ostream& os) const;

    virtual const char* name() const = 0;

    friend ostream& operator<<(ostream& os, ViewObj const& rhs);

protected:

    void dumpShape(ostream& os, ViewObj::Shape shape) const;

private:

    ViewObj();
    ViewObj(ViewObj const &);
    ViewObj const& operator=(ViewObj const &);
    // Never defined
};

#endif /* _VIEWOBJ_H_ */
