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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * Some metrics are exported by the kernel as "unsigned long".
 * On most 64bit platforms this is not the same size as an
 * "unsigned int". 
 */
#if defined(HAVE_64BIT_LONG)
#define KERNEL_ULONG PM_TYPE_U64
#define _pm_assign_ulong(atomp, val) do { (atomp)->ull = (val); } while (0)
#define __pm_kernel_ulong_t __uint64_t
#else
#define KERNEL_ULONG PM_TYPE_U32
#define _pm_assign_ulong(atomp, val) do { (atomp)->ul = (val); } while (0)
#define __pm_kernel_ulong_t __uint32_t
#endif

/*
 * Some metrics need to have their type set at runtime, based on the
 * running kernel version (not simply a 64 vs 32 bit machine issue).
 */
#define KERNEL_UTYPE PM_TYPE_NOSUPPORT	/* set to real type at runtime */
#define _pm_metric_type(type, size) \
    do { \
	(type) = ((size)==8 ? PM_TYPE_U64 : PM_TYPE_U32); \
    } while (0)
#define _pm_assign_utype(size, atomp, val) \
    do { \
	if ((size)==8) { (atomp)->ull = (val); } else { (atomp)->ul = (val); } \
    } while (0)

