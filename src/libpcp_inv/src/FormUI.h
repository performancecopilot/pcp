/* -*- C++ -*- */

#ifndef _INV_FORMUI_H_
#define _INV_FORMUI_H_

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


#include <Vk/VkComponent.h>

class SoXtExaminerViewer;

class INV_FormUI : public VkComponent
{ 

public:

    INV_FormUI(const char *, Widget);
    INV_FormUI(const char *);
    ~INV_FormUI();
    void create ( Widget );
    const char *  className();

    class SoXtExaminerViewer *_viewer;

    Widget  _form;
    Widget  _label;
    Widget  _scaleLabel;
    Widget  _scaleText;
    Widget  _scaleWheel;
    Widget  _timeLabel;
    Widget  _vcr;

    static float theMultiplier;

protected:

    virtual void vcrActivateCB ( Widget, XtPointer ) = 0;
    virtual void scaleTextActivateCB ( Widget, XtPointer );
    virtual void wheelChangedCB ( Widget, XtPointer );
    virtual void wheelDragCB ( Widget, XtPointer );

private: 

    static String      _defaultFormResources[];

    static void vcrActivateCBCallback ( Widget, XtPointer, XtPointer );
    static void scaleTextActivateCBCallback ( Widget, XtPointer, XtPointer );
    static void wheelChangedCBCallback ( Widget, XtPointer, XtPointer );
    static void wheelDragCBCallback ( Widget, XtPointer, XtPointer );

    static float calcScale(int value);
    
    void setScale(float value);
    float setScale(char *value);
    void showScale();
};

#endif /* _INV_FORMUI_H_ */
