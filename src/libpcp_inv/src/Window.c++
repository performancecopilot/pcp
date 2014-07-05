/*
 * Copyright (c) 1997-2000 Silicon Graphics, Inc.  All Rights Reserved.
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


#include <Vk/VkApp.h>
#include <Vk/VkFileSelectionDialog.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkRadioSubMenu.h>
#include <Vk/VkMenuItem.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkHelpPane.h>
#include "pmapi.h"
#include "impl.h"
#include "Window.h"
#include "Form.h"

String  INV_Window::_defaultWindowResources[] = {
    "*title:  Performance Metrics Viewer",
    "*fileMenu.labelString:  File",
    "*fileMenu.mnemonic:  F",
    "*recordButton.labelString:  Record",
    "*recordButton.mnemonic:  R",
    "*recordButton.accelerator:  Ctrl<Key>R",
    "*recordButton.acceleratorText:  Ctrl+R",
    "*saveButton.labelString:  Save",
    "*saveButton.mnemonic:  S",
    "*saveButton.accelerator:  Ctrl<Key>S",
    "*saveButton.acceleratorText:  Ctrl+S",
    "*printButton.labelString:  Print",
    "*printButton.mnemonic:  P",
    "*printButton.accelerator:  Ctrl<Key>P",
    "*printButton.acceleratorText:  Ctrl+P",
    "*exitButton.labelString:  Quit",
    "*exitButton.mnemonic:  Q",
    "*exitButton.accelerator:  Ctrl<Key>Q",
    "*exitButton.acceleratorText:  Ctrl+Q",
    "*optionsMenu.labelString:  Options",
    "*optionsMenu.mnemonic:  O",
    "*showVCRButton.labelString:  Show Time Control",
    "*showVCRButton.mnemonic:  T",
    "*showVCRButton.accelerator:  Ctrl<Key>T",
    "*showVCRButton.acceleratorText:  Ctrl+T",
    "*newVCRButton.mnemonic:  N",
    "*newVCRButton.labelString:  New Time Control",
    "*newVCRButton.accelerator:  Ctrl<Key>N",
    "*newVCRButton.acceleratorText:  Ctrl+N",
    "*launchMenu.labelString:  Launch",
    "*launchMenu.mnemonic:  L",
    "*helpPane.labelString:  Help",
    "*helpPane.mnemonic:  H",
    "*help_click_for_help.labelString:  Click For Help",
    "*help_click_for_help.mnemonic:  C",
    "*help_click_for_help.accelerator:  Shift<Key>F1",
    "*help_click_for_help.acceleratorText:  Shift+F1",
    "*help_overview.labelString:  Overview",
    "*help_overview.mnemonic:  O",
    "*help_index.labelString:  Index",
    "*help_index.mnemonic:  I",
    "*help_keys_and_short.labelString:  Keys and Shortcuts",
    "*help_keys_and_short.mnemonic:  K",
    "*help_prod_info.labelString:  Product Information",
    "*help_prod_info.mnemonic:  P",
    "*overviewButton.labelString:  Overview",
    "*overviewButton.mnemonic:  O",
    "*indexButton.labelString:  Index",
    "*indexButton.mnemonic:  I",
    (char*)NULL
};

static const OMC_String		recordStr = "Record";
static const OMC_String		stopRecordStr = "Stop Recording";

INV_Window::INV_Window(const char *name,
		       ArgList args,
		       Cardinal argCount)
: VkWindow (name, args, argCount)
{
    // Load any class-default resources for this object
    setDefaultResources(baseWidget(), _defaultWindowResources  );

    // Create the view component contained by this window
    _form= new INV_Form("form",mainWindowWidget());

    XtVaSetValues (_form->baseWidget(),
		   XmNwidth, 512, 
		   XmNheight, 512, 
		   (XtPointer) NULL );

    // Add the component as the main view
    addView(_form);
    _form->setParent(this);

    // Create the panes of this window's menubar. The menubar itself
    //  is created automatically by ViewKit


    // The fileMenu menu pane
    _fileMenu =  addMenuPane("fileMenu");
    _recordButton =  _fileMenu->addAction("recordButton", 
					  &INV_Window::recordButtonCBCallback, 
					  (XtPointer) this );
    _saveButton =  _fileMenu->addAction("saveButton", 
					&INV_Window::saveButtonCBCallback, 
					(XtPointer) this );
#ifdef HAVE_MENU_FILE_PRINT
    _printButton =  _fileMenu->addAction("printButton", 
					 &INV_Window::printButtonCBCallback, 
					 (XtPointer) this );
#else
    _printButton = 0;
#endif
    _exitButton =  _fileMenu->addAction("exitButton", 
					&INV_Window::exitButtonCBCallback, 
					(XtPointer) this );

    // The optionsMenu menu pane
    _optionsMenu =  addMenuPane("optionsMenu");
    _showVCRButton =  _optionsMenu->addAction("showVCRButton", 
					      &INV_Window::showVCRButtonCBCallback, 
					      (XtPointer) this );
    _newVCRButton =  _optionsMenu->addAction("newVCRButton", 
					     &INV_Window::newVCRButtonCBCallback, 
					     (XtPointer) this );

    // The launchMenu menu pane
    _launchMenu = addMenuPane("launchMenu");
    _launchItems = new INV_LaunchMenu(_launchMenu);

} // End Constructor

INV_Window::~INV_Window()
{
    delete _launchItems;
    delete _form;
}

Boolean 
INV_Window::okToQuit()
{
    return (_form->okToQuit() );
}

void 
INV_Window::exitButtonCBCallback (Widget    w,
				  XtPointer clientData,
				  XtPointer callData ) 
{ 
    INV_Window* obj = (INV_Window * ) clientData;
    obj->exitButtonCB (w, callData );
}

void 
INV_Window::saveButtonCBCallback (Widget    w,
				  XtPointer clientData,
				  XtPointer callData ) 
{ 
    INV_Window* obj = (INV_Window * ) clientData;
    obj->saveButtonCB (w, callData );
}

void 
INV_Window::printButtonCBCallback (Widget    w,
				   XtPointer clientData,
				   XtPointer callData ) 
{ 
    INV_Window* obj = (INV_Window *) clientData;
    obj->printButtonCB (w, callData );
}

void 
INV_Window::showVCRButtonCBCallback (Widget    w,
				     XtPointer clientData,
				     XtPointer callData ) 
{ 
    INV_Window* obj = (INV_Window *) clientData;
    obj->showVCRButtonCB (w, callData );
}

void 
INV_Window::newVCRButtonCBCallback (Widget    w,
				    XtPointer clientData,
				    XtPointer callData ) 
{ 
    INV_Window* obj = (INV_Window *) clientData;
    obj->newVCRButtonCB (w, callData );
}

void 
INV_Window::exitButtonCB (Widget w, XtPointer callData ) 
{
    _form->exitButtonCB(w, callData);
}


void 
INV_Window::printButtonCB (Widget w, XtPointer callData ) 
{
    _form->printButtonCB(w, callData);
}

void 
INV_Window::saveButtonCB (Widget w, XtPointer callData ) 
{
    _form->saveButtonCB(w, callData);
}

void 
INV_Window::showVCRButtonCB (Widget w, XtPointer callData ) 
{
    _form->showVCRButtonCB(w, callData);
}

void 
INV_Window::newVCRButtonCB (Widget w, XtPointer callData ) 
{
    _form->newVCRButtonCB(w, callData);
}

void
INV_Window::hideRecordButton()
{
    _recordButton->deactivate();
}

void
INV_Window::recordState(OMC_Bool state)
{
    if (state)
	_recordButton->setLabel(stopRecordStr.ptr());
    else
	_recordButton->setLabel(recordStr.ptr());
}

void 
INV_Window::recordButtonCB (Widget w, XtPointer callData ) 
{
    _form->recordButtonCB(w, callData);
}

void 
INV_Window::recordButtonCBCallback (Widget    w,
				    XtPointer clientData,
				    XtPointer callData ) 
{ 
    INV_Window* obj = (INV_Window *) clientData;
    obj->recordButtonCB (w, callData );
}

