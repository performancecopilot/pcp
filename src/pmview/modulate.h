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
#ifndef _MODULATE_H_
#define _MODULATE_H_

#include <Inventor/SbString.h>
#include "metriclist.h"

class SoSeparator;
class SoPath;
class Launch;
class Record;

extern double		theNormError;
extern float		theScale;

class Modulate
{
public:

    enum State	{ start, error, saturated, normal };

protected:

    static const QString	theErrorText;
    static const QString	theStartText;
    static const float		theDefErrorColor[];
    static const float		theDefSaturatedColor[];
    static const double		theMinScale;

    int				_sts;
    MetricList			*_metrics;
    SoSeparator			*_root;
    SbColor			_errorColor;
    SbColor			_saturatedColor;

public:

    virtual ~Modulate();

    Modulate(const char *metric, double scale,
		 MetricList::AlignColor align = MetricList::perMetric);

    Modulate(const char *metric, double scale, const SbColor &color,
		 MetricList::AlignColor align = MetricList::perMetric);

    Modulate(MetricList *list);

    int status() const
	{ return _sts; }
    const SoSeparator *root() const
	{ return _root; }
    SoSeparator *root()
	{ return _root; }

    int numValues() const
	{ return _metrics->numValues(); }

    const char *add();

    void setErrorColor(const SbColor &color)
	{ _errorColor.setValue(color.getValue()); }
    void setSaturatedColor(const SbColor &color)
        { _saturatedColor.setValue(color.getValue()); }

    virtual void refresh(bool fetchFlag) = 0;

    // Return the number of objects still selected
    virtual void selectAll();
    virtual int select(SoPath *)
	{ return 0; }
    virtual int remove(SoPath *)
	{ return 0; }

    // Should expect selectInfo calls to different paths without
    // previous removeInfo calls
    virtual void selectInfo(SoPath *)
	{}
    virtual void removeInfo(SoPath *)
	{}

    virtual void infoText(QString &str, bool selected) const = 0;

    virtual void launch(Launch &launch, bool all) const = 0;
    virtual void record(Record &rec) const;

    virtual void dump(QTextStream &) const
	{}
    void dumpState(QTextStream &os, State state) const;

    friend QTextStream &operator<<(QTextStream &os, const Modulate &rhs);

protected:

    static void add(Modulate *obj);

private:

    Modulate();
    Modulate(const Modulate &);
    const Modulate &operator=(const Modulate &);
    // Never defined
};

#endif /* _MODULATE_H_ */
