/* -*- C++ -*- */

#ifndef _INV_TOGGLEMOD_H_
#define _INV_TOGGLEMOD_H_

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
#include "Modulate.h"
#include "ModList.h"

class SoSeparator;
class SoPath;
class INV_Launch;
class INV_Record;

class INV_ToggleMod : public INV_Modulate
{
private:

    INV_ModulateList	_list;
    OMC_String		_label;

public:

    virtual ~INV_ToggleMod();

    INV_ToggleMod(SoNode *obj, const char *label);

    void addMod(INV_Modulate *mod)
    	{ _list.append(mod); }

    virtual void selectAll();
    virtual uint_t select(SoPath *);
    virtual uint_t remove(SoPath *);

    virtual void selectInfo(SoPath *)
	{}
    virtual void removeInfo(SoPath *)
	{}

    virtual void infoText(OMC_String &str, OMC_Bool) const
	{ str = _label; }

    virtual void refresh(OMC_Bool)
    	{}
    virtual void launch(INV_Launch &, OMC_Bool) const
    	{}
    virtual void record(INV_Record &) const
    	{}

    virtual void dump(ostream &) const;
    void dumpState(ostream &os, INV_Modulate::State state) const;

    friend ostream &operator<<(ostream &os, const INV_ToggleMod &rhs);

private:

    INV_ToggleMod();
    INV_ToggleMod(const INV_ToggleMod &);
    const INV_ToggleMod &operator=(const INV_ToggleMod &);
    // Never defined
};

#endif /* _INV_TOGGLEMOD_H_ */
