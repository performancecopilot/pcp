/* -*- C++ -*- */

#ifndef _INV_WINDOW_H
#define _INV_WINDOW_H

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


#include <Vk/VkWindow.h>
#include "Bool.h"
#include "LaunchMenu.h"

class VkMenuItem;
class VkMenuToggle;
class VkMenuConfirmFirstAction;
class VkSubMenu;
class VkRadioSubMenu;

class INV_Window;
class INV_Form;

class INV_Window: public VkWindow
{     
private:

    static String	_defaultWindowResources[];

protected:

    INV_Form		*_form;

    VkSubMenu		*_fileMenu;
    VkMenuItem		*_recordButton;
    VkMenuItem		*_saveButton;
    VkMenuItem		*_printButton;
    VkMenuItem		*_exitButton;
    VkSubMenu		*_optionsMenu;
    VkMenuItem		*_showVCRButton;
    VkMenuItem		*_newVCRButton;
    VkMenuItem		*_separator;
    VkSubMenu		*_launchMenu;    
    INV_LaunchMenu	*_launchItems;

public:
    
    virtual ~INV_Window();

    INV_Window(const char * name, 
	       ArgList args = NULL,
	       Cardinal argCount = 0 );

    const char *className()
	{ return "INV_Window"; }
    virtual Boolean okToQuit();
    
    VkSubMenu *optionsMenu()
	{ return _optionsMenu; }

protected:
    
    virtual void exitButtonCB ( Widget, XtPointer );
    virtual void printButtonCB ( Widget, XtPointer );
    virtual void saveButtonCB ( Widget, XtPointer );
    virtual void recordButtonCB ( Widget, XtPointer );
    virtual void showVCRButtonCB ( Widget, XtPointer );
    virtual void newVCRButtonCB ( Widget, XtPointer );
    
    void switchLaunchMenu(uint_t i);

    void hideRecordButton();
    void recordState(OMC_Bool state);

private:
    
    static void exitButtonCBCallback ( Widget, XtPointer, XtPointer );
    static void printButtonCBCallback ( Widget, XtPointer, XtPointer );
    static void saveButtonCBCallback ( Widget, XtPointer, XtPointer );
    static void recordButtonCBCallback ( Widget, XtPointer, XtPointer );
    static void showVCRButtonCBCallback ( Widget, XtPointer, XtPointer );
    static void newVCRButtonCBCallback ( Widget, XtPointer, XtPointer );
};

#endif /* _INV_WINDOW_H_ */
