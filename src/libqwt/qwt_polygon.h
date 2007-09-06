/* -*- mode: C++ ; c-file-style: "stroustrup" -*- *****************************
 * Qwt Widget Library
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

// vim: expandtab

#ifndef QWT_POLYGON_H
#define QWT_POLYGON_H

#include "qwt_global.h"

/*!
  \def QwtPolygon
 */

#if QT_VERSION < 0x040000
#include <qpointarray.h>
typedef QPointArray QwtPolygon;
#else
#include <qpolygon.h>
typedef QPolygon QwtPolygon;
#endif

#endif
