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
#ifndef _TOGGLEMOD_H_
#define _TOGGLEMOD_H_

#include <Inventor/SbString.h>
#include "modulate.h"
#include "modlist.h"

class SoSeparator;
class SoPath;
class Launch;
class Record;

class ToggleMod : public Modulate
{
private:

    ModulateList	_list;
    QString		_label;

public:

    virtual ~ToggleMod();

    ToggleMod(SoNode *obj, const char *label);

    void addMod(Modulate *mod)
    	{ _list.append(mod); }

    virtual void selectAll();
    virtual int select(SoPath *);
    virtual int remove(SoPath *);

    virtual void selectInfo(SoPath *)
	{}
    virtual void removeInfo(SoPath *)
	{}

    virtual void infoText(QString &str, bool) const
	{ str = _label; }

    virtual void refresh(bool)
    	{}
    virtual void launch(Launch &, bool) const
    	{}
    virtual void record(Record &) const
    	{}

    virtual void dump(QTextStream &) const;
    void dumpState(QTextStream &os, Modulate::State state) const;

    friend QTextStream &operator<<(QTextStream &os, const ToggleMod &rhs);

private:

    ToggleMod();
    ToggleMod(const ToggleMod &);
    const ToggleMod &operator=(const ToggleMod &);
    // Never defined
};

#endif /* _TOGGLEMOD_H_ */
