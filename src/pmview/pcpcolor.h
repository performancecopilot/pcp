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
#ifndef _PCPCOLOR_H_
#define _PCPCOLOR_H_

#include <Inventor/SbColor.h>
#include <Inventor/fields/SoSFString.h>
#include <Inventor/fields/SoSFColor.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/nodes/SoSubNode.h>

#include "main.h"
#include "modlist.h"

class PCPColor : public SoNode {
   SO_NODE_HEADER(PCPColor);
 public:
   // Fields:
   SoSFString	  metric;      // PCP metric spec
   SoSFFloat      maxValue;	
   SoSFColor      color;       // Color of glow

   // Initializes this class for use in scene graphs. This
   // should be called after database initialization and before
   // any instance of this node is constructed.
   static void    initClass();
   // Constructor
   PCPColor();
 protected:
   QmcMetric *theMetric;

   // These implement supported actions. The only actions that
   // deal with materials are the callback and GL render
   // actions. We will inherit all other action methods from
   // SoNode.
   virtual void   GLRender(SoGLRenderAction *action);
   virtual void   callback(SoCallbackAction *action);
   // This implements generic traversal of PCPColor node, used in
   // both of the above methods.
   virtual void   doAction(SoAction *action);

 private:
   // Destructor. Private to keep people from trying to delete
   // nodes, rather than using the reference count mechanism.
   virtual ~PCPColor();
   SbColor         emissiveColor;
};

#endif /* _PCPCOLOR_H_ */
