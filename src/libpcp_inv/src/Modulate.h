/* -*- C++ -*- */

#ifndef _INV_MODULATE_H_
#define _INV_MODULATE_H_

/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 */


#include <Inventor/SbString.h>
#include "String.h"
#include "MetricList.h"

class SoSeparator;
class SoPath;
class INV_Launch;
class INV_Record;

extern double		theNormError;
extern float		theScale;

class INV_Modulate
{
public:

    enum State	{ start, error, saturated, normal };

protected:

    static const OMC_String	theErrorText;
    static const OMC_String	theStartText;
    static const float		theDefErrorColor[];
    static const float		theDefSaturatedColor[];
    static const double		theMinScale;

    int				_sts;
    INV_MetricList		*_metrics;
    SoSeparator			*_root;
    SbColor			_errorColor;
    SbColor			_saturatedColor;

public:

    virtual ~INV_Modulate();

    INV_Modulate(const char *metric, double scale,
		 INV_MetricList::AlignColor align = INV_MetricList::perMetric);

    INV_Modulate(const char *metric, double scale, const SbColor &color,
		 INV_MetricList::AlignColor align = INV_MetricList::perMetric);

    INV_Modulate(INV_MetricList *list);

    int status() const
	{ return _sts; }
    const SoSeparator *root() const
	{ return _root; }
    SoSeparator *root()
	{ return _root; }

    uint_t numValues() const
	{ return _metrics->numValues(); }

    const char *add();

    void setErrorColor(const SbColor &color)
	{ _errorColor.setValue(color.getValue()); }
    void setSaturatedColor(const SbColor &color)
        { _saturatedColor.setValue(color.getValue()); }

    virtual void refresh(OMC_Bool fetchFlag) = 0;

    // Return the number of objects still selected
    virtual void selectAll();
    virtual uint_t select(SoPath *)
	{ return 0; }
    virtual uint_t remove(SoPath *)
	{ return 0; }

    // Should expect selectInfo calls to different paths without
    // previous removeInfo calls
    virtual void selectInfo(SoPath *)
	{}
    virtual void removeInfo(SoPath *)
	{}

    virtual void infoText(OMC_String &str, OMC_Bool selected) const = 0;

    virtual void launch(INV_Launch &launch, OMC_Bool all) const = 0;
    virtual void record(INV_Record &rec) const;

    virtual void dump(ostream &) const
	{}
    void dumpState(ostream &os, State state) const;

    friend ostream &operator<<(ostream &os, const INV_Modulate &rhs);

protected:

    static void add(INV_Modulate *obj);

private:

    INV_Modulate();
    INV_Modulate(const INV_Modulate &);
    const INV_Modulate &operator=(const INV_Modulate &);
    // Never defined
};

#endif /* _INV_MODULATE_H_ */
