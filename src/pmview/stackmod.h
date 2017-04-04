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
#ifndef _STACKMOD_H_
#define _STACKMOD_H_

#include <QVector>
#include "modulate.h"

class SoBaseColor;
class SoTranslation;
class SoScale;
class SoNode;
class SoSwitch;
class Launch;

struct StackBlock {
    SoSeparator		*_sep;
    SoBaseColor		*_color;
    SoScale		*_scale;
    SoTranslation	*_tran;
    Modulate::State	_state;
    bool		_selected;
};

typedef QVector<StackBlock> StackBlockList;

class StackMod : public Modulate
{
public:

    enum Height { unfixed, fixed, util };

private:

    static const float	theDefFillColor[];
    static const char	theStackId;

    StackBlockList	_blocks;
    SoSwitch		*_switch;
    Height		_height;
    QString		_text;
    int			_selectCount;
    int			_infoValue;
    int			_infoMetric;
    int			_infoInst;

public:

    virtual ~StackMod();

    StackMod(MetricList *metrics,
		 SoNode *obj, 
		 Height height = unfixed);

    void setFillColor(const SbColor &col);
    void setFillColor(int packedcol);
    void setFillText(const char *str)
	{ _text = str; }

    virtual void refresh(bool fetchFlag);

    virtual void selectAll();
    virtual int select(SoPath *);
    virtual int remove(SoPath *);

    virtual void selectInfo(SoPath *);
    virtual void removeInfo(SoPath *);

    virtual void infoText(QString &str, bool) const;

    virtual void launch(Launch &launch, bool all) const;

    virtual void dump(QTextStream &) const;

private:

    StackMod();
    StackMod(const StackMod &);
    const StackMod &operator=(const StackMod &);
    // Never defined

    void findBlock(SoPath *path, int &metric, int &inst, 
		   int &value, bool idMetric = true);
};

#endif /* _STACKMOD_H_ */
