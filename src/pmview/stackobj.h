/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef _STACKOBJ_H_
#define _STACKOBJ_H_

#include "modobj.h"
#include "stackmod.h"

class SoSeparator;

class StackObj : public ModObj
{
protected:

    int				_width;
    int				_depth;
    StackMod::Height		_height;
    ViewObj::Shape		_shape;
    StackMod			*_stack;
    QString			_text;

public:

    virtual ~StackObj();

    StackObj(StackMod::Height height,
	     ViewObj::Shape shape,
	     bool baseFlag,
	     const DefaultObj &defaults,
	     int x, int z,
	     int cols = 1, int rows = 1, 
	     BaseObj::Alignment align = BaseObj::center);

    virtual int width() const
	{ return _width; }
    virtual int depth() const
	{ return _depth; }
    StackMod::Height height() const
	{ return _height; }

    void setFillText(const char *str)
	{ _text = str; }

    virtual void finishedAdd();

    virtual void setTran(float xTran, float zTran, int width, int depth);

    virtual const char* name() const
    	{ return "Stack"; }

    virtual void display(QTextStream& os) const;

    friend QTextStream& operator<<(QTextStream& os, StackObj const& rhs);

    virtual void setBarHeight (int h) { _maxHeight = h; }

private:

    StackObj();
    StackObj(StackObj const&);
    StackObj const& operator=(StackObj const &);
};

#endif /* _STACKOBJ_H_ */
