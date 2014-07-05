/* -*- C++ -*- */

#ifndef _BARMOD_H_
#define _BARMOD_H_

/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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
 */


#include "Vector.h"
#include "ColorScale.h"
#include "Modulate.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

class SoBaseColor;
class SoScale;
class SoNode;
class SoTranslation;
class INV_Launch;

struct BarBlock {
    SoSeparator		*_sep;
    SoBaseColor		*_color;
    SoScale		*_scale;
    SoTranslation	*_tran;
    INV_Modulate::State	_state;
    OMC_Bool		_selected;
};

typedef OMC_Vector<BarBlock> BlockList;

class BarMod : public INV_Modulate
{
public:

    enum Direction      { instPerCol, instPerRow };
    enum Modulation     { yScale, color, colYScale };    
    enum Grouping	{ groupByRow, groupByCol, groupByMetric, groupByInst };

private:

    static const char theBarId;

    BlockList		_blocks;
    Direction		_dir;
    Modulation		_mod;
    Grouping		_group;
    INV_ColorScale	_colScale;
    uint_t		_selectCount;
    uint_t		_infoValue;
    uint_t		_infoMetric;
    uint_t		_infoInst;
    float		_xScale;
    float		_yScale;
    float		_zScale;
    uint_t		_width;
    uint_t		_depth;
    uint_t		_cols;
    uint_t		_rows;

public:

    virtual ~BarMod();

    BarMod(INV_MetricList *list,
	   SoNode *obj,
	   BarMod::Direction dir,
	   BarMod::Grouping group,
	   float xScale, float yScale, float zScale,
	   float xSpace, float zSpace);

    BarMod(INV_MetricList *list,
	   const INV_ColorScale &colScale,
	   SoNode *obj,
	   BarMod::Direction dir,
	   BarMod::Modulation mod,
	   BarMod::Grouping group,
	   float xScale, float yScale, float zScale,
	   float xSpace, float zSpace);

    Direction	dir() const
	{ return _dir; }
    uint_t	width() const
	{ return _width; }
    uint_t	depth() const
	{ return _depth; }
    uint_t	numBars() const
	{ return _blocks.length(); }
    uint_t	rows() const
	{ return _rows; }
    uint_t	cols() const
	{ return _cols; }

    virtual void refresh(OMC_Bool fetchFlag);

    virtual void selectAll();
    virtual uint_t select(SoPath *);
    virtual uint_t remove(SoPath *);

    virtual void selectInfo(SoPath *);
    virtual void removeInfo(SoPath *);

    virtual void infoText(OMC_String &str, OMC_Bool) const;

    virtual void launch(INV_Launch &launch, OMC_Bool) const;

    virtual void dump(ostream &) const;

    void regenerate(float xScale, float zScale, float xSpace, float zSpace);

    const char *dirStr() const;
    const char *modStr() const;

private:

    BarMod();
    BarMod(const BarMod &);
    const BarMod &operator=(const BarMod &);
    // Never defined

    void generate(SoNode *obj, float xSpace, float zSpace);
    void findBlock(SoPath *path, uint_t &metric, uint_t &inst, 
		   uint_t &value, OMC_Bool idMetric = OMC_true);

};

#endif /* _BARMOD_H_ */
