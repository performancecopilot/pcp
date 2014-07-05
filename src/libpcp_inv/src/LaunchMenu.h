/* -*- C++ -*- */

#ifndef _INV_LAUNCHMENU_H_
#define _INV_LAUNCHMENU_H_

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

#include "Menu.h"

class INV_LaunchMenu
{
private:

    INV_Menu	_menuItems;
    VkSubMenu	*_menu;

public:

    ~INV_LaunchMenu();

    INV_LaunchMenu(VkSubMenu *menu);

    uint_t numItems() const
	{ return _menuItems.length(); }

    const INV_MenuItem &operator[](int i) const
	{ return _menuItems[i]; }
    INV_MenuItem &operator[](int i)
	{ return _menuItems[i]; }

    static void callback(Widget, XtPointer, XtPointer);
    
private:

    void parseConfig(char const* path);

    INV_LaunchMenu(INV_LaunchMenu const&);
    INV_LaunchMenu const& operator=(INV_LaunchMenu const&);
    // Never defined
};

#endif /* _INV_LAUNCHMENU_H_ */
