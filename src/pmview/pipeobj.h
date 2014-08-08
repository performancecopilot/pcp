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
#ifndef _PIPEOBJ_H_
#define _PIPEOBJ_H_

#include "modobj.h"

class SoCylinder;
class SoTranslation;

class PipeObj : public ModObj {
public:
    virtual ~PipeObj () {}
    PipeObj (const DefaultObj &, int, int, int, int, Alignment);

    int width () const { return _width; }
    int depth () const { return _depth; }

    const char * name() const { return "Pipe"; }

    void finishedAdd ();
    void setTran (float, float, int, int);
    void setTag (const char *);

private:
    PipeObj ();
    PipeObj (PipeObj const &);
    PipeObj const& operator=(PipeObj const &);

    int _width, _depth;
    int cellWidth, cellDepth, cellHeight;
    Alignment _align;
    float _stackHeight;
    QString _tag;

    SoCylinder * _cyl;
    SoTranslation * _cylTrans, * _origt;

    float _color[3];
};

#endif /* _PIPEOBJ_H_ */
