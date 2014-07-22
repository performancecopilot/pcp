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
#ifndef _BASEOBJ_H_
#define _BASEOBJ_H_

#include "viewobj.h"
#include "togglemod.h"

class SoCube;

class BaseObj : public ViewObj
{
protected:

    bool		_on;
    int			_borderX;
    int			_borderZ;
    int			_baseHeight;
    int			_length;
    int			_maxHeight;
    float		_baseColor[3];
    ToggleMod		*_mod;
    SoCube		*_cube;
    QString		_label;
	
public:

    virtual ~BaseObj();

    BaseObj(bool onFlag,
	    const DefaultObj &defaults,
	    int, int, 
	    int cols = 1, int rows = 1, 
	    BaseObj::Alignment align = BaseObj::center);

    int borderX() const
	{ return _borderX; }
    int borderZ() const
	{ return _borderZ; }
    int baseWidth() const
	{ return _borderX * 2; }
    int baseDepth() const
	{ return _borderZ * 2; }
    int baseHeight() const
	{ return _baseHeight; }
    float baseColor(int i) const
	{ return _baseColor[i]; }
    const QString &label() const
	{ return _label; }
    int length() const
	{ return _length; }
    int maxHeight() const
	{ return _maxHeight; }    

    // Local changes
    int &borderX()
	{ return _borderX; }
    int &borderZ()
	{ return _borderZ; }
    int &baseHeight()
	{ return _baseHeight; }
    void baseColor(float r, float g, float b)
	{ _baseColor[0] = r; _baseColor[1] = g; _baseColor[2] = b; }
    QString &label()
	{ return _label; }
    int &length()
	{ return _length; }
    int& maxHeight()
	{ return _maxHeight; }

    bool state() const
	{ return _on; }
    void state(bool flag);
    
    virtual int width() const = 0;
    virtual int depth() const = 0;

    void addBase(SoSeparator *sep);

    void setBaseSize (int width, int depth);

//    virtual void setTran(float xTran, float zTran, int width, int depth);

    virtual Modulate *modObj()
	{ return _mod; }

    virtual void finishedAdd();

    // Output
    virtual void display(QTextStream& os) const;

    virtual const char* name() const = 0;

    friend QTextStream& operator<<(QTextStream& os, BaseObj const& rhs);

protected:

    void add(Modulate *mod);

private:

    BaseObj();
    BaseObj(BaseObj const &);
    BaseObj const& operator=(BaseObj const &);
};

#endif /* _BASEOBJ_H_ */
