/* -*- C++ -*- */

#ifndef _INV_FORM_H
#define _INV_FORM_H

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


#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Vk/VkWindow.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkSubMenu.h>
#include "FormUI.h"

class SoXtPrintDialog;

class INV_Form : public INV_FormUI
{
protected:
    
    VkWindow		*_parent;
    SoXtPrintDialog	*_printDialog;
    
public:
    
    INV_Form(const char *, Widget);
    INV_Form(const char *);
    ~INV_Form();

    const char *  className();

    virtual void setParent(VkWindow  *);
    virtual void vcrActivateCB(Widget, XtPointer );
    virtual void exitButtonCB(Widget, XtPointer);
    virtual void printButtonCB(Widget, XtPointer);
    virtual void saveButtonCB(Widget, XtPointer);
    virtual void showVCRButtonCB(Widget, XtPointer);
    virtual void newVCRButtonCB(Widget, XtPointer);
    virtual void recordButtonCB(Widget, XtPointer);
};

#endif /* _INV_FORM_H_ */
