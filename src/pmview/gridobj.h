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
#ifndef _GRIDOBJ_H_
#define _GRIDOBJ_H_

#include "baseobj.h"
#include "defaultobj.h"
#include <QtCore/QVector>

struct GridItem
{
    ViewObj	*_item;
    int		_row;
    int		_col;
};

typedef QList<GridItem> GridList;

class GridObj : public BaseObj
{
protected:

    int			_minDepth;
    int			_minWidth;
    int			_width;
    int			_depth;
    GridList		_list;
    QVector<int>	_rowDepth;
    QVector<int>	_colWidth;
    bool		_finished;
    DefaultObj		_defs;

    static int		theDefRowDepth;
    static int		theDefColWidth;

public:

    virtual ~GridObj();

    GridObj(bool onFlag,
	    const DefaultObj &defaults,
	    int x, int y,
	    int cols = 1, int rows = 1, 
	    GridObj::Alignment align = GridObj::center);

    DefaultObj * defs() { return & _defs; }

    int numObj() const
	{ return _list.size(); }
    int minDepth() const
	{ return _minDepth; }
    int minWidth() const
	{ return _minWidth; }
    int rows() const
	{ return _rowDepth.size(); }
    int cols() const
	{ return _colWidth.size(); }

    // Local changes
    int &minDepth()
	{ return _minDepth; }
    int &minWidth()
	{ return _minWidth; }

    virtual int width() const
	{ return _width; }
    virtual int depth() const
	{ return _depth; }

    virtual void setTran(float xTran, float zTran, int width, int depth);

    void addObj(ViewObj *obj, int col, int row);

    virtual void finishedAdd();

    // Output
    virtual void display(QTextStream& os) const;

    virtual const char* name() const
	{ return "Grid"; }

    friend QTextStream& operator<<(QTextStream& os, GridObj const& rhs);

protected:

    void add(Modulate *mod);

private:

    GridObj();
    GridObj(GridObj const &);
    GridObj const& operator=(GridObj const &);
};

#endif /* _GRIDOBJ_H_ */
