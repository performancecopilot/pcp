/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef _MODOBJ_H_
#define _MODOBJ_H_

#include "baseobj.h"

class ModObj : public BaseObj
{
public:
    ModObj(bool onFlag,
	   const DefaultObj &defaults,
	   int x, int y,
	   int cols = 1, int rows = 1, 
	   BaseObj::Alignment align = BaseObj::center) 
	: BaseObj (onFlag, defaults, x, y, cols, rows, align)
	, _history(0)
	, _colors()
	, _metrics ()
	{ _objtype |= MODOBJ; }

    void setColorList(const char *list) { _colors = list; }
    void addMetric(const char * m, double s) { _metrics.add(m, s, _history); }
    void setHistory(int history) { _history = history; }

protected:
    int			_history;
    QString		_colors;
    MetricList		_metrics;
};
#endif
