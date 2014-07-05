/* -*- C++ -*- */

#ifndef _INV_H_
#define _INV_H_

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
#include "Bool.h"
#include "X11/Xlib.h"
#include "X11/Xresource.h"

//
// Typedefs
//

class INV_App;
class INV_View;
class INV_ModList;
class SoXtExaminerViewer;

typedef void (*INV_TermCB)(int);

//
// Globals
//

extern "C" {

extern char			*pmProgname;
// Executable name

extern int			pmDebug;
// Debugging flags;

extern int			errno;
// Error number

}	// extern "C"

extern OMC_String		theAppName;
// The application name, uses for resources

extern float			theScale;
// The scale controls multiplier

extern OMC_Bool			theStderrFlag;
// Use stderr if true

extern INV_ModList		*theModList;
// List of modulated objects

extern INV_View			*theView;
// Viewer coordinator

extern INV_App			*theApp;
// Viewkit application object

extern SoXtExaminerViewer	*theViewer;
// The examiner window

extern const uint_t		theBufferLen;
// Length of theBuffer

extern char			theBuffer[];
// String buffer that can be used for anything

extern const OMC_String		theDefaultFlags;
// Default flags parsed by INV_View::parseArgs

//
// Setup routines
//

int INV_setup(const char *appname, int *argc, char **argv, 
	      XrmOptionDescRec *cmdopts, int numOpts, INV_TermCB termCB);

//
// Error message routines
//
// DBG_TRACE_1 is used in setup, launch, selection classes
// DBG_TRACE_2 is used in modulation classes
// 
// If pmDebug is set to anything and PCP_USE_STDERR is not set, these 
// routines below will also dump a message to stderr in addition to a dialog
//

#define _POS_	__FILE__, __LINE__
 
int INV_warningMsg(const char *fileName, int line, const char *msg, ...);
int INV_errorMsg(const char *fileName, int line, const char *msg, ...);
int INV_fatalMsg(const char *fileName, int line, const char *msg, ...);

#endif /* _INV_H_ */
