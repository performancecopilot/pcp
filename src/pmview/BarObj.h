/* -*- C++ -*- */

#ifndef _BAROBJ_H_
#define _BAROBJ_H_

/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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


#include "BarMod.h"
#include "ModObj.h"
#include "MetricList.h"

class SoNode;
class SoTranslation;
class Text;

class BarObj : public ModObj
{
public:

    enum LabelDir { away, towards };
    enum LabelSide { left, right, above, below, numSides };

protected:

    ViewObj::Shape	_shape;
    BarMod::Direction	_dir;
    BarMod::Modulation	_mod;
    BarMod::Grouping	_group;
    uint_t		_width;
    uint_t		_depth;
    uint_t		_xSpace;
    uint_t		_zSpace;
    uint_t		_labelSpace;
    BarMod		*_bars;
    LabelDir		_metDir;
    OMC_StrList		*_metLabels;
    LabelDir		_instDir;
    OMC_StrList		*_instLabels;
    float		_margins[numSides];
    float		_labelColor[3];

public:

    virtual ~BarObj();

    BarObj(ViewObj::Shape shape,
	   BarMod::Direction dir,
	   BarMod::Modulation mod,
	   BarMod::Grouping group,
	   OMC_Bool baseFlag,
	   const DefaultObj &defaults,
	   uint_t x, uint_t y, 
	   uint_t cols = 1, uint_t rows = 1, 
	   BaseObj::Alignment align = BaseObj::center);

    virtual uint_t width() const
	{ return _width; }
    virtual uint_t depth() const
	{ return _depth; }
    Shape shape() const
	{ return _shape; }
    BarMod::Direction dir() const
	{ return _dir; }
    BarMod::Modulation mod() const
	{ return _mod; }
    uint_t numMetricLabels() const
	{ return _metLabels->length(); }
    LabelDir metricLabelDir() const
	{ return _metDir; }
    uint_t numInstLabels() const
	{ return _instLabels->length(); }
    LabelDir instLabelDir() const
	{ return _instDir; }

    void addMetric(const char *metric, double scale, const char *label)
	{ if (_metrics.add(metric, scale) >= 0) _metLabels->append(label); }

    void addMetricLabel(const char *label)
	{ _metLabels->append(label); }
    void addInstLabel(const char *label)
	{ _instLabels->append(label); }

    virtual void finishedAdd();

    // Local change
    uint_t &xSpace()
	{ return _xSpace; }
    uint_t &zSpace()
	{ return _zSpace; }
    LabelDir &metricLabelDir()
	{ return _metDir; }
    LabelDir &instLabelDir()
	{ return _instDir; }

    virtual void setTran(float xTran, float zTran, uint_t width, uint_t depth);

    virtual const char* name() const;

    virtual void display(ostream& os) const;

    friend ostream& operator<<(ostream& os, BarObj const& rhs);

private:

    Text ** calcLabels(const OMC_StrList &labels, LabelSide side, 
		       uint_t numLabels);
    SoNode *doLabels(Text **text, LabelSide side, uint_t numLabels);

    BarObj();
    BarObj(BarObj const&);
    BarObj const& operator=(BarObj const &);
};

#endif /* _BAROBJ_H_ */
