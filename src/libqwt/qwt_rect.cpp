/* -*- mode: C++ ; c-file-style: "stroustrup" -*- *****************************
 * Qwt Widget Library
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "qwt_math.h"
#include "qwt_rect.h"

//! Constructor

QwtRect::QwtRect(): 
    QRect() 
{
}

//! Copy constructor
QwtRect::QwtRect(const QRect &r): 
    QRect(r) 
{
}

inline static void addPoint(QwtPolygon &pa, uint pos, const QPoint &point)
{
    if ( uint(pa.size()) <= pos ) 
        pa.resize(pos + 5);

    pa.setPoint(pos, point);
}

//! Sutherland-Hodgman polygon clipping

QwtPolygon QwtRect::clip(const QwtPolygon &pa) const
{
    if ( contains( pa.boundingRect() ) )
        return pa;

    QwtPolygon cpa(pa.size());

    clipEdge((Edge)0, pa, cpa);

    for ( uint edge = 1; edge < NEdges; edge++ ) 
    {
        const QwtPolygon rpa = cpa;
#if QT_VERSION < 0x040000
        cpa.detach();
#endif
        clipEdge((Edge)edge, rpa, cpa);
    }

    return cpa;
}

bool QwtRect::insideEdge(const QPoint &p, Edge edge) const
{
    switch(edge) 
    {
        case Left:
            return p.x() > left();
        case Top:
            return p.y() > top();
        case Right:
            return p.x() < right();
        case Bottom:
            return p.y() < bottom();
        default:
            break;
    }

    return false;
}

QPoint QwtRect::intersectEdge(const QPoint &p1, 
    const QPoint &p2, Edge edge ) const
{
    int x=0, y=0;
    double m = 0;

    const double dy = p2.y() - p1.y();
    const double dx = p2.x() - p1.x();

    switch ( edge ) 
    {
        case Left:
            x = left();
            m = double(qwtAbs(p1.x() - x)) / qwtAbs(dx);
            y = p1.y() + int(dy * m);
            break;
        case Top:
            y = top();
            m = double(qwtAbs(p1.y() - y)) / qwtAbs(dy);
            x = p1.x() + int(dx * m);
            break;
        case Right:
            x = right();
            m = double(qwtAbs(p1.x() - x)) / qwtAbs(dx);
            y = p1.y() + int(dy * m);
            break;
        case Bottom:
            y = bottom();
            m = double(qwtAbs(p1.y() - y)) / qwtAbs(dy);
            x = p1.x() + int(dx * m);
            break;
        default:
            break;
    }

    return QPoint(x,y);
}

void QwtRect::clipEdge(Edge edge, 
    const QwtPolygon &pa, QwtPolygon &cpa) const
{
    if ( pa.count() == 0 )
    {
        cpa.resize(0);
        return;
    }

    unsigned int count = 0;

    QPoint p1 = pa.point(0);
    if ( insideEdge(p1, edge) )
        addPoint(cpa, count++, p1);

    const uint nPoints = pa.size();
    for ( uint i = 1; i < nPoints; i++ )
    {
        const QPoint p2 = pa.point(i);
        if ( insideEdge(p2, edge) )
        {
            if ( insideEdge(p1, edge) )
                addPoint(cpa, count++, p2);
            else
            {
                addPoint(cpa, count++, intersectEdge(p1, p2, edge));
                addPoint(cpa, count++, p2);
            }
        }
        else
        {
            if ( insideEdge(p1, edge) )
                addPoint(cpa, count++, intersectEdge(p1, p2, edge));
        }
        p1 = p2;
    }
    cpa.resize(count);
}
