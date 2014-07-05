/* -*- C++ -*- */

#ifndef _INV_APP_H
#define _INV_APP_H

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


#include <Vk/VkApp.h>
#include "Inv.h"

class INV_App: public VkApp
{
private:

    INV_TermCB	_termCB;
    
public:

    INV_App(char *appClassName,
	    int *arg_c,
	    char **arg_v,
	    XrmOptionDescRec *optionList = NULL,
	    int sizeofOptionList = 0,
	    INV_TermCB termCB = NULL);

    virtual void terminate(int status);
};

#endif /* _INV_APP_H_ */
