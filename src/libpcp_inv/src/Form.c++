/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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


#include <sys/stat.h>
#include <unistd.h>
#include <Xm/Form.h> 
#include <Xm/Label.h>
 
#include <Vk/VkApp.h>
#include <Vk/VkResource.h>
#include <Vk/VkWindow.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkErrorDialog.h>
#include <Vk/VkWarningDialog.h>
#include <Vk/VkFileSelectionDialog.h>

#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/SoOutput.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/Xt/SoXtPrintDialog.h>
#include <Inventor/nodes/SoSeparator.h>

#include "Inv.h"
#include "App.h"
#include "Form.h"
#include "View.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

INV_Form::INV_Form(const char *name, Widget parent)
: INV_FormUI(name, parent),
  _parent(0),
  _printDialog(0)
{ 
}

INV_Form::INV_Form(const char *name) 
: INV_FormUI(name),
  _parent(0),
  _printDialog(0)
{ 
}

INV_Form::~INV_Form()
{
}

const char * 
INV_Form::className()
{
    return ("INV_Form");
}

void 
INV_Form::setParent( VkWindow  * parent )
{
    _parent = parent;
}

void 
INV_Form::exitButtonCB( Widget, XtPointer)
{
    theApp->terminate(0);
}

void 
INV_Form::saveButtonCB( Widget, XtPointer)
{
    theFileSelectionDialog->setTitle("Save Scene as Inventor");
    theFileSelectionDialog->setFilterPattern("*.iv");

    if (theFileSelectionDialog->postAndWait() == VkDialogManager::OK) {
	struct stat buf;
	if (stat(theFileSelectionDialog->fileName(), &buf) == 0) {
	    sprintf(theBuffer, "%s exists, overwrite?", 
		    theFileSelectionDialog->fileName());
	    int sts = theWarningDialog->postAndWait(theBuffer, TRUE, TRUE);
	    if (sts == VkDialogManager::CANCEL)
		return;
	}
	SoOutput outFile;
	SbBool success = outFile.openFile(theFileSelectionDialog->fileName());
	if (success == FALSE) {
	    sprintf(theBuffer, "Save as %s: %s", 
		    theFileSelectionDialog->fileName(),
		    strerror(errno));
	    theErrorDialog->postAndWait(theBuffer);
	}
	else {
	    SoWriteAction write(&outFile);
	    write.apply(theView->root());
	    outFile.closeFile();
	}
    }
}

void 
INV_Form::printButtonCB( Widget, XtPointer)
{
     if (_printDialog == NULL) {
	_printDialog = new SoXtPrintDialog(_form, NULL, FALSE);

	if (_printDialog == NULL) {
	    cerr << pmProgname << ": Warning: Out of memory" << endl;
	    theErrorDialog->postAndWait("Unable to print: Out of Memory");
	    return;
	}

	_printDialog->setSceneGraph(theView->root());
	_printDialog->setGLRenderAction(_viewer->getGLRenderAction());
	_printDialog->setTitle("Print Scene");
    }
    
    if (!_printDialog->isVisible())
	_printDialog->show();   
}

void 
INV_Form::showVCRButtonCB( Widget, XtPointer)
{
    int sts;

    if ((sts = pmTimeShowDialog(1)) < 0) {
	sprintf(theBuffer, "Could not show VCR: %s", pmErrStr(sts));
	theErrorDialog->postAndWait(theBuffer);
    }
}

void 
INV_Form::newVCRButtonCB( Widget, XtPointer)
{
    int		sts = 0;

    sts = theView->timeConnect(OMC_true);

    if (sts < 0) {
	sprintf(theBuffer, "Unable to connect new time controls: %s",
		pmErrStr(sts));
	theErrorDialog->postAndWait(theBuffer);
	theApp->terminate(1);
    }
}

void 
INV_Form::vcrActivateCB ( Widget, XtPointer)
{
    int sts;

    if ((sts = pmTimeShowDialog(1)) < 0) {
	sprintf(theBuffer, "Could not show VCR: %s", pmErrStr(sts));
	theErrorDialog->postAndWait(theBuffer);
    }    
}

extern "C" VkComponent * CreateForm( const char *name, Widget parent ) 
{  
    VkComponent *obj =  new INV_Form ( name, parent );
    obj->show();

    return ( obj );
}

void 
INV_Form::recordButtonCB( Widget, XtPointer)
{
    theView->record();
}
