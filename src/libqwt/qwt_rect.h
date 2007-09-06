/* -*- mode: C++ ; c-file-style: "stroustrup" -*- *****************************
 * Qwt Widget Library
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#ifndef QWT_RECT_H
#define QWT_RECT_H

#include <qrect.h>
#include "qwt_global.h"
#include "qwt_polygon.h"

/*!
  \brief Some extensions for QRect
*/

class QWT_EXPORT QwtRect : public QRect
{
public:
    QwtRect();
    QwtRect(const QRect &r);

    QwtPolygon clip(const QwtPolygon &) const;

private:
    enum Edge { Left, Top, Right, Bottom, NEdges };

    void clipEdge(Edge, const QwtPolygon &, QwtPolygon &) const;
    bool insideEdge(const QPoint &, Edge edge) const;
    QPoint intersectEdge(const QPoint &p1, 
        const QPoint &p2, Edge edge) const;
};

#endif
