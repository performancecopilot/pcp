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
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/bundles/SoMaterialBundle.h>
#include <Inventor/elements/SoEmissiveColorElement.h>

#include "pcpcolor.h"
#include "scenegroup.h"
#include "main.h"

SO_NODE_SOURCE(PCPColor);
// Initializes the PCPColor class. This is a one-time thing that is
// done after database initialization and before any instance of
// this class is constructed.
void
PCPColor::initClass()
{
   // Initialize type id variables. The arguments to the macro
   // are: the name of the node class, the class this is derived
   // from, and the name registered with the type of the parent
   // class.
   SO_NODE_INIT_CLASS(PCPColor, SoNode, "Node");
}
// Constructor
PCPColor::PCPColor()
{
   // Do standard constructor tasks
   SO_NODE_CONSTRUCTOR(PCPColor);

   SO_NODE_ADD_FIELD(maxValue, (1.0));
   SO_NODE_ADD_FIELD(color, (1.0, 1.0, 1.0));
   SO_NODE_ADD_FIELD(metric, (""));

   // SbString s = metric.getValue();
   SbString *s = new SbString("kernel.all.cpu.user");

   double scale = (double)maxValue.getValue();
   theMetric = new QmcMetric(activeGroup, s->getString(), scale);
   elementalNodeList.append(this);
}
// Destructor
PCPColor::~PCPColor()
{
}
// Implements GL render action.
void
PCPColor::GLRender(SoGLRenderAction *action)
{
   // Set the elements in the state correctly. Note that we
   // prefix the call to doAction() with the class name. This
   // avoids problems if someone derives a new class from the
   // PCPColor node and inherits the GLRender() method; PCPColor's
   // doAction() will still be called in that case.

   PCPColor::doAction(action);

   // For efficiency, Inventor nodes make sure that the first
   // defined material is always in GL, so shapes do not have to
   // send the first material each time. (This keeps caches from
   // being dependent on material values in many cases.) The
   // SoMaterialBundle class allows us to do this easily.
   SoMaterialBundle  mb(action);
   mb.forceSend(0);
}
// Implements callback action.
void
PCPColor::callback(SoCallbackAction *action)
{
   // Set the elements in the state correctly.
   PCPColor::doAction(action);
}

// Typical action implementation - it sets the correct element
// in the action's traversal state. We assume that the element
// has been enabled.
void
PCPColor::doAction(SoAction *action)
{
    theMetric->update();
    float f = theMetric->realValue(0);
    f /= maxValue.getValue() + 0.001;
    emissiveColor = color.getValue() * f;
    if (action->getState())
	SoEmissiveColorElement::set(action->getState(), this, 1, &emissiveColor);

    touch();
}
