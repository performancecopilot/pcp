#ifndef _INV_MENU_H_
#define _INV_MENU_H_

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

#include "pmapi.h"
#include "String.h"
#include "Args.h"
#include "Window.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

class VkMenuItem;

class INV_MenuItem
{
private:

    OMC_String	_name;
    OMC_Args	_script;
    VkMenuItem	*_button;

 public:

    ~INV_MenuItem();

    INV_MenuItem(char const* name, char const* script, OMC_Bool createButton);
    INV_MenuItem(char const* name, char const* script);
    INV_MenuItem(char const* name, const OMC_Args &args);
    INV_MenuItem(const INV_MenuItem &rhs);

    const OMC_String &name() const
    	{ return _name; }

    const OMC_Args &script() const
    	{ return _script; }

    const VkMenuItem &button() const
    	{ return *(_button); }

    friend ostream& operator<<(ostream& os, INV_MenuItem const& rhs);

    void buildButton();

 private:

    INV_MenuItem();
    const INV_MenuItem &operator=(const INV_MenuItem &rhs);
    // Never defined
};

typedef OMC_List<INV_MenuItem *> INV_MenuItemList;

class INV_Menu
{
 private:

    INV_MenuItemList	_items;

 public:

    ~INV_Menu();

    INV_Menu(uint_t size = 4)
	: _items(size) {}

    INV_Menu(INV_Menu const& rhs);

    uint_t length() const
	{ return _items.length(); }

    uint_t size() const
	{ return _items.size(); }

    int append(INV_MenuItem *item);
    int append(const INV_Menu &menu);
    void addVkMenuItems(VkSubMenu *menu);

    void remove(int i);

    void removeAll();

    INV_MenuItem const& operator[](int i) const
	{ return *(_items[i]); }
    INV_MenuItem& operator[](int i)
	{ return *(_items[i]); }

    INV_MenuItem const& last() const
	{ return *(_items.tail()); }
    INV_MenuItem& last()
	{ return *(_items.tail()); }

    friend ostream& operator<<(ostream& os, INV_Menu const& rhs);

 private:

    INV_Menu const& operator=(INV_Menu const &);
};

#endif /* _INV_MENU_H_ */
