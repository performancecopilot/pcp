/* -*- C++ -*- */

#ifndef _GRIDOBJ_H_
#define _GRIDOBJ_H_

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


#include "BaseObj.h"
#include "DefaultObj.h"
#include "Vector.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

struct GridItem
{
    ViewObj	*_item;
    uint_t	_row;
    uint_t	_col;
};

typedef OMC_List<GridItem> GridList;

class GridObj : public BaseObj
{
protected:

    uint_t		_minDepth;
    uint_t		_minWidth;
    uint_t		_width;
    uint_t		_depth;
    GridList		_list;
    OMC_IntVector	_rowDepth;
    OMC_IntVector	_colWidth;
    OMC_Bool		_finished;
    DefaultObj		_defs;

    static uint_t	theDefRowDepth;
    static uint_t	theDefColWidth;

public:

    virtual ~GridObj();

    GridObj(OMC_Bool onFlag,
	    const DefaultObj &defaults,
	    uint_t x, uint_t y,
	    uint_t cols = 1, uint_t rows = 1, 
	    GridObj::Alignment align = GridObj::center);

    DefaultObj * defs() { return & _defs; }

    uint_t numObj() const
	{ return _list.length(); }
    uint_t minDepth() const
	{ return _minDepth; }
    uint_t minWidth() const
	{ return _minWidth; }
    uint_t rows() const
	{ return _rowDepth.length(); }
    uint_t cols() const
	{ return _colWidth.length(); }

    // Local changes
    uint_t &minDepth()
	{ return _minDepth; }
    uint_t &minWidth()
	{ return _minWidth; }

    virtual uint_t width() const
	{ return _width; }
    virtual uint_t depth() const
	{ return _depth; }

    virtual void setTran(float xTran, float zTran, uint_t width, uint_t depth);

    void addObj(ViewObj *obj, uint_t col, uint_t row);

    virtual void finishedAdd();

    // Output
    virtual void display(ostream& os) const;

    virtual const char* name() const
	{ return "Grid"; }

    friend ostream& operator<<(ostream& os, GridObj const& rhs);

protected:

    void add(INV_Modulate *mod);

private:

    GridObj();
    GridObj(GridObj const &);
    GridObj const& operator=(GridObj const &);
};

#endif /* _GRIDOBJ_H_ */
