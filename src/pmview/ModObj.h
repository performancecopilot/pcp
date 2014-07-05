/* -*- C++ -*- */
/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 *
 */

#ifndef _MODOBJ_H_
#define _MODOBJ_H_

#include "BaseObj.h"

class ModObj : public BaseObj {
public:
    ModObj(OMC_Bool onFlag,
	   const DefaultObj &defaults,
	   uint_t x, uint_t y,
	   uint_t cols = 1, uint_t rows = 1, 
	   BaseObj::Alignment align = BaseObj::center) 
	: BaseObj (onFlag, defaults, x, y, cols, rows, align)
	, _history(0)
	, _colors()
	, _metrics ()
	{ _objtype |= MODOBJ; }

    void setColorList(const char *list) { _colors = list; }
    void addMetric(const char * m, double s) { _metrics.add(m, s, _history); }
    void setHistory(uint_t history) { _history = history; }

protected:
    uint_t		_history;
    OMC_String		_colors;
    INV_MetricList	_metrics;
};
#endif
