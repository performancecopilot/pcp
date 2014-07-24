/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef _BAROBJ_H_
#define _BAROBJ_H_

#include "barmod.h"
#include "modobj.h"
#include "metriclist.h"
#include <QtCore/QStringList>

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
    int			_width;
    int			_depth;
    int			_xSpace;
    int			_zSpace;
    int			_labelSpace;
    BarMod		*_bars;
    LabelDir		_metDir;
    QStringList		*_metLabels;
    LabelDir		_instDir;
    QStringList		*_instLabels;
    float		_margins[numSides];
    float		_labelColor[3];

public:

    virtual ~BarObj();

    BarObj(ViewObj::Shape shape,
	   BarMod::Direction dir,
	   BarMod::Modulation mod,
	   BarMod::Grouping group,
	   bool baseFlag,
	   const DefaultObj &defaults,
	   int x, int y, 
	   int cols = 1, int rows = 1, 
	   BaseObj::Alignment align = BaseObj::center);

    virtual int width() const
	{ return _width; }
    virtual int depth() const
	{ return _depth; }
    Shape shape() const
	{ return _shape; }
    BarMod::Direction dir() const
	{ return _dir; }
    BarMod::Modulation mod() const
	{ return _mod; }
    int numMetricLabels() const
	{ return _metLabels->size(); }
    LabelDir metricLabelDir() const
	{ return _metDir; }
    int numInstLabels() const
	{ return _instLabels->size(); }
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
    int &xSpace()
	{ return _xSpace; }
    int &zSpace()
	{ return _zSpace; }
    LabelDir &metricLabelDir()
	{ return _metDir; }
    LabelDir &instLabelDir()
	{ return _instDir; }

    virtual void setTran(float xTran, float zTran, int width, int depth);

    virtual const char* name() const;

    virtual void display(QTextStream& os) const;

    friend QTextStream& operator<<(QTextStream& os, BarObj const& rhs);

private:

    Text ** calcLabels(const QStringList &labels, LabelSide side, 
		       int numLabels);
    SoNode *doLabels(Text **text, LabelSide side, int numLabels);

    BarObj();
    BarObj(BarObj const&);
    BarObj const& operator=(BarObj const &);
};

#endif /* _BAROBJ_H_ */
