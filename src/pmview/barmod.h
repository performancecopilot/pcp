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
#ifndef _BARMOD_H_
#define _BARMOD_H_

#include "colorscale.h"
#include "modulate.h"
#include <QtCore/QVector>

class SoBaseColor;
class SoScale;
class SoNode;
class SoTranslation;
class Launch;

struct BarBlock {
    SoSeparator		*_sep;
    SoBaseColor		*_color;
    SoScale		*_scale;
    SoTranslation	*_tran;
    Modulate::State	_state;
    bool		_selected;
};

typedef QVector<BarBlock> BarBlockList;

class BarMod : public Modulate
{
public:

    enum Direction      { instPerCol, instPerRow };
    enum Modulation     { yScale, color, colYScale };    
    enum Grouping	{ groupByRow, groupByCol, groupByMetric, groupByInst };

private:

    static const char theBarId;

    BarBlockList	_blocks;
    Direction		_dir;
    Modulation		_mod;
    Grouping		_group;
    ColorScale		_colScale;
    int			_selectCount;
    int			_infoValue;
    int			_infoMetric;
    int			_infoInst;
    float		_xScale;
    float		_yScale;
    float		_zScale;
    int			_width;
    int			_depth;
    int			_cols;
    int			_rows;

public:

    virtual ~BarMod();

    BarMod(MetricList *list,
	   SoNode *obj,
	   BarMod::Direction dir,
	   BarMod::Grouping group,
	   float xScale, float yScale, float zScale,
	   float xSpace, float zSpace);

    BarMod(MetricList *list,
	   const ColorScale &colScale,
	   SoNode *obj,
	   BarMod::Direction dir,
	   BarMod::Modulation mod,
	   BarMod::Grouping group,
	   float xScale, float yScale, float zScale,
	   float xSpace, float zSpace);

    Direction	dir() const
	{ return _dir; }
    int		width() const
	{ return _width; }
    int		depth() const
	{ return _depth; }
    int		numBars() const
	{ return _blocks.size(); }
    int		rows() const
	{ return _rows; }
    int		cols() const
	{ return _cols; }

    virtual void refresh(bool fetchFlag);

    virtual void selectAll();
    virtual int select(SoPath *);
    virtual int remove(SoPath *);

    virtual void selectInfo(SoPath *);
    virtual void removeInfo(SoPath *);

    virtual void infoText(QString &str, bool) const;

    virtual void launch(Launch &launch, bool) const;

    virtual void dump(QTextStream &) const;

    void regenerate(float xScale, float zScale, float xSpace, float zSpace);

    const char *dirStr() const;
    const char *modStr() const;

private:

    BarMod();
    BarMod(const BarMod &);
    const BarMod &operator=(const BarMod &);
    // Never defined

    void generate(SoNode *obj, float xSpace, float zSpace);
    void findBlock(SoPath *path, int &metric, int &inst, 
		   int &value, bool idMetric = true);
};

#endif /* _BARMOD_H_ */
