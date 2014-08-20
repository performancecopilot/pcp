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
#ifndef _LINK_H
#define _LINK_H

#include "baseobj.h"

class SoCylinder;

class Link : public BaseObj
{
public:
    Link (const DefaultObj &, int, int, int, int, Alignment);

    void finishedAdd ();
    void setTran (float, float, int, int);
    int width () const { return _width; }
    int depth () const { return _depth; }
    const char * name () const { return "Link"; }
    void setTag (const char *);

    virtual Modulate *modObj() { return 0; }

private:
    Link();
    Link (Link const &);
    Link const& operator=(Link const &);

    int cellWidth, cellHeight, cellDepth;
    int _width, _depth;
    Alignment align;

    SoCylinder * c1, * c2;
    SoTranslation * t2, * t3;
    QString _tag;

    float _color[3];
};

#endif
