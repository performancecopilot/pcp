/*
 * Old (deprecated) PMAPI routines ... someday these might all go away.
 *
 * Copyright (c) 2017 Ken McDonell  All Rights Reserved.
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
 */

#include "pmapi.h"
#include "pmapi_old.h"
#include "impl.h"
#include "internal.h"

int
__pmSetProgname(const char *program)
{
    pmSetProgname(program);
    return 0;
}
