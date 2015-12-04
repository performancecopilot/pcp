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
#ifndef _VIEWOBJ_H_
#define _VIEWOBJ_H_

#include <QTextStream>

class SoNode;
class SoSeparator;
class SoTranslation;
class Modulate;
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

    int			_objtype;
    int			_col;
    int			_row;
    int			_cols;
    int			_rows;
    int			_maxHeight;
    float		_xAlign;
    float		_zAlign;

    static int		theNumModObjects;

public:

    virtual ~ViewObj();

    ViewObj(int, int, int cols = 1, int rows = 1, 
	    Alignment align = center);

    // The Scene Graph Root for this object
    SoSeparator* root()
	{ return _root; }

    int objbits() const { return _objtype; }
    int row() const { return _row; }
    int col() const { return _col; }

    int cols() const
	{ return _cols; }
    int rows() const
	{ return _rows; }
    int height() const
	{ return _maxHeight; }
    int &height()
	{ return _maxHeight; }
    float xAlign() const
	{ return _xAlign; }
    float zAlign() const
	{ return _zAlign; }

    // Set the coordinates (and the allocated size) from parent
    virtual void setTran(float xTran, float zTran, int width, int depth);

    // Size (in object coordinates). Must be the correct value before
    // the object is added to the parent.
    virtual int width() const = 0;
    virtual int depth() const = 0;

    static int numModObjects()
	{ return theNumModObjects; }

    // Return default object
    static SoNode *object(Shape shape);

    virtual Modulate *modObj()
	{ return (Modulate *)0; }

    // Inform object parsing stuff is done

    virtual void finishedAdd() = 0;

    // Output
    virtual void display(QTextStream& os) const;

    virtual const char* name() const = 0;

    friend QTextStream& operator<<(QTextStream& os, ViewObj const& rhs);

protected:

    void dumpShape(QTextStream& os, ViewObj::Shape shape) const;

private:

    ViewObj();
    ViewObj(ViewObj const &);
    ViewObj const& operator=(ViewObj const &);
    // Never defined
};

#endif /* _VIEWOBJ_H_ */
