/* -*- C++ -*- */

#ifndef _PMVIEW_H_
#define _PMVIEW_H_

/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 */


#include <stdio.h>
#include "String.h"
#include "Bool.h"
#include "Args.h"
#include "Inv.h"
#include "ColorList.h"

//
// Classes
//

class ViewObj;

//
// Globals
//

// Configuration file name
extern OMC_String	theConfigName;

// Configuration file
extern FILE		*theConfigFile;

// ColorLists generated while parsing config
extern ColorList	theColorLists;

// Scale applied to entire scene
extern float		theGlobalScale;

// True when config is saved to temporary file
extern OMC_Bool		theAltConfigFlag;

// Save the config file here
extern FILE		*theAltConfig;

// Name of the saved configuration file
extern OMC_String	theAltConfigName;

//
// Prototypes
//

int genInventor();
char lastinput();
int input();
int markpos();
int locateError();

#endif /* _PMVIEW_H_ */
