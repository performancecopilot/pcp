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
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCube.h>
#include "gridobj.h"
#include "defaultobj.h"

#include <iostream>
using namespace std;

GridObj::~GridObj()
{
}

GridObj::GridObj(bool onFlag, 
		 const DefaultObj &defaults,
		 int x, int y, 
		 int cols, int rows, 
		 GridObj::Alignment align)
    : BaseObj(onFlag, defaults, x, y, cols, rows, align),
      _minDepth(defaults.gridMinDepth()),
      _minWidth(defaults.gridMinWidth()),
      _width(_minWidth + baseWidth()),
      _depth(_minDepth + baseDepth()),
      _list(),
      _rowDepth(1, _minDepth),
      _colWidth(1, _minWidth),
      _finished(false)
{
    _objtype |= GRIDOBJ;
    _defs = defaults;
}

void
GridObj::setTran(float xTran, float zTran, int setWidth, int setDepth)
{
    int			i, j;
    int			totalWidth;
    int			totalDepth;
    float		xShift = width() / 2.0;
    float		zShift = depth() / 2.0;
    QVector<int>	rowPos(rows());
    QVector<int>	colPos(cols());

    assert(_finished == true);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "\nGridObj(" << width() << "x" << depth() << ")@" 
	     << col() << "," << row() 
	     << "::setTran (" 
	     << xTran << ", " << zTran << ", "
	     << setWidth << ", " << setDepth << ")" << endl;
#endif

    BaseObj::setBaseSize(width(), depth());
    BaseObj::setTran(xTran + xShift, zTran + zShift, setWidth, setDepth);

    colPos[0] = 0;
    for (i = 1; i < cols(); i++) {
	colPos[i] = colPos[i-1] + _colWidth[i-1];
    }
    rowPos[0] = 0;
    for (i = 1; i < rows(); i++) {
	rowPos[i] = rowPos[i-1] + _rowDepth[i-1];
    }

    for (i = 0; i < _list.size(); i++) {
	GridItem &item = _list[i];
	totalWidth = totalDepth = 0;
	for (j = item._col; j < item._col + item._item->cols(); j++)
	    totalWidth += _colWidth[j];
	for (j = item._row; j < item._row + item._item->rows(); j++)
	    totalDepth += _rowDepth[j];
	
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "GridObj::setTran: [" << i << "] at " << item._col
		 << ',' << item._row << ": ";
#endif

	item._item->setTran(colPos[item._col] - xShift + borderX(), 
			    rowPos[item._row] - zShift + borderZ(), 
			    totalWidth, totalDepth);
    }
}

int
addRowCols(QVector<int> & ivec, int nsize, int min)
{
    int extra = 0;

    if (nsize > ivec.size()) {
	extra = min * (nsize - ivec.size());

	ivec.resize(nsize);
    }

    return extra;
}

void
GridObj::addObj(ViewObj *obj, int col, int row)
{
    int		i;
    GridItem	newItem;

    for (i = 0; i < _list.size(); i++) {
	const GridItem &item = _list[i];
	if ((row >= item._row && row < (item._row + item._item->rows())) &&
	    (col >= item._col && col < (item._col + item._item->cols()))) {
	    pmprintf("%s: %s at %d,%d (%dx%d) collides with %s at %d,%d (%dx%d)\nThe later object will be ignored\n",
		     pmProgname, item._item->name(), item._col, 
		     item._row, item._item->cols(), item._item->rows(),
		     obj->name(), col, row, obj->cols(), obj->rows());
	    return;
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "GridObj::addObj: Adding item " << _list.length() << ": "
	     << obj->name() << ", size = " << obj->width() << 'x'
	     << obj->depth() << endl;
#endif

    newItem._item = obj;
    newItem._row = row;
    newItem._col = col;
    _list.append(newItem);

    // Add extra cols & rows as necessary
    _width += addRowCols (_colWidth, col + obj->cols(), _minWidth);
    _depth += addRowCols (_rowDepth, row + obj->rows(), _minDepth);


    // Fasttrack size adjustments for simple objects
    if (obj->cols() == 1 && _colWidth[col] < obj->width()) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    cerr << "GridObj::addObj: increasing col[" << col << "] from " 
		 << _colWidth[col] << " to " << obj->width() << endl;
#endif
	_width += obj->width() - _colWidth[col];
	_colWidth[col] = obj->width();
    }

    if (obj->rows() == 1 && _rowDepth[row] < obj->depth()) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    cerr << "GridObj::addObj: increasing row[" << row << "] from " 
		 << _rowDepth[row] << " to " << obj->depth() << endl;
#endif
	_depth += obj->depth() - _rowDepth[row];
	_rowDepth[row] = obj->depth();
    }
}

void
GridObj::finishedAdd()
{
    int		i, j;
    int		size;
    int		current;
    int		adjust;

    BaseObj::addBase(_root);

    for (i= 0; i < _list.size(); i++) {
	const GridItem &item = _list[i];
	_root->addChild(item._item->root());
	if (item._item->modObj() != NULL) {
	    BaseObj::add(item._item->modObj());
	}

	if (item._item->cols() > 1) {
	    size = item._item->width();
	    for (j = item._col, current = 0; 
		 j < item._col + item._item->cols(); 
		 j++) {
		current += _colWidth[j];
	    }

	    if (current < size) {
		size -= current;
		adjust = (int)((size / (float)item._item->cols()) + 0.5);
		for (j = item._col; 
		     j < item._col + item._item->cols() && adjust > 0; j++) {
		    if (adjust > size) {
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_APPL0)
			    cerr << "GridObj::finishedAdd: increasing col["
				 << j << "] from " << _colWidth[j] << " to "
				 << _colWidth[j] + size << endl;
#endif
			_colWidth[j] += size;
			_width+= size;
			adjust = 0;
		    }
		    else {
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_APPL0)
			    cerr << "GridObj::finishedAdd: increasing col["
				 << j << "] from " << _colWidth[j] << " to "
				 << _colWidth[j] + adjust << endl;
#endif
			_colWidth[j] += adjust;
			_width += adjust;
			size -= adjust;
		    }
		}
	    }
	}

	if (item._item->rows() > 1) {
	    size = item._item->depth();
	    for (j = item._row, current = 0; 
		 j < item._row + item._item->rows(); 
		 j++) {
		current += _rowDepth[j];
	    }

	    if (current < size) {
		size -= current;
		adjust = (int)((size / (float)item._item->rows()) + 0.5);
		for (j = item._row; 
		     j < item._row + item._item->rows() && adjust > 0; j++) {
		    if (adjust > size) {
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_APPL0)
			    cerr << "GridObj::finishedAdd: increasing row["
				 << j << "] from " << _rowDepth[j] << " to "
				 << _rowDepth[j] + size << endl;
#endif
			_rowDepth[j] += size;
			_depth+= size;
			adjust = 0;
		    }
		    else {
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_APPL0)
			    cerr << "GridObj::finishedAdd: increasing row["
				 << j << "] from " << _rowDepth[j] << " to "
				 << _rowDepth[j] + adjust << endl;
#endif
			_rowDepth[j] += adjust;
			_depth += adjust;
			size -= adjust;
		    }
		}
	    }
	}
    }

    _finished = true;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "GridObj::finishedAdd: " << *this << endl;
#endif
}

QTextStream&
operator<<(QTextStream& os, GridObj const& rhs)
{
    rhs.display(os);
    return os;
}

void
GridObj::display(QTextStream& os) const
{
    int		i;
    int		sum;

    BaseObj::display(os);
    os << ", minRowDepth = " << _minDepth << ", minColWidth = "
       << _minWidth << ", rows = " << rows() << ", cols = " << cols()
       << ", finishedAdd = " << (_finished == true ? "true" : "false")
       << endl;

    os << "Column widths: ";
    for (i = 0, sum = 0; i < _colWidth.size(); i++) {
	os << _colWidth[i] << ' ';
	sum += _colWidth[i];
    }
    os << endl;
    assert(_width == sum + baseWidth());

    os << "Row depths: ";
    for (i = 0, sum = 0; i < _rowDepth.size(); i++) {
	os << _rowDepth[i] << ' ';
	sum += _rowDepth[i];
    }
    os << endl;
    assert(_depth == sum + baseDepth());

    for (i = 0; i < _list.size(); i++)
	os << '[' << i << "] at " << _list[i]._col << ',' << _list[i]._row
	   << ": " << *(_list[i]._item) << endl;
}
