/*
 * Size conversion for different sized types extracted from the
 * kernel on different platforms, particularly where the sizeof
 * "long" differs.
 *
 * Copyright (c) 2007-2008 Aconex.  All Rights Reserved.
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

/*
 * Some metrics are exported by the kernel as "unsigned long".
 * On most 64bit platforms this is not the same size as an
 * "unsigned int". 
 */
#if defined(HAVE_64BIT_LONG)
#define KERNEL_ULONG PM_TYPE_U64
#define _pm_assign_ulong(atomp, val) do { (atomp)->ull = (val); } while (0)
#else
#define KERNEL_ULONG PM_TYPE_U32
#define _pm_assign_ulong(atomp, val) do { (atomp)->ul = (val); } while (0)
#endif
