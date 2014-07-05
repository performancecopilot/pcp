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


#include "App.h"
#include "Inv.h"
#include "View.h"

INV_App::INV_App(char *appClassName,
		 int *arg_c,
		 char **arg_v,
		 XrmOptionDescRec *optionList,
		 int sizeofOptionList,
		 INV_TermCB termCB)
: VkApp(appClassName, arg_c, arg_v, optionList, sizeofOptionList),
  _termCB(termCB)
{
}

void
INV_App::terminate(int status)
{
    if (theView && theView->okToQuit()) {
	if (_termCB != NULL)
	    (*_termCB)(status);
	VkApp::terminate(status);
	/*NOTREACHED*/
    }
    exit(1);
    /*NOTREACHED*/
}
