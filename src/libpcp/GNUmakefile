#! gmake
#
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
# 
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.
# 
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
#

TOPDIR = ../..

# Getting all the right 32bit libs on x86_64 is getting increasingly difficult.
# pmchart links pcp libs statically anyway, so lets scrap them and see who screams
#PCP_ALTLIBS = 32

include $(TOPDIR)/src/include/builddefs

IRIX_DIRT = 32 N32 64 BUILD libpcp*.so* libpcp*.a
LDIRT	= $(IRIX_DIRT)
LSRCFILES = GNUlocaldefs.32

SUBDIRS = src $(PCP_ALTLIBS)

default install : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

default_pcp : default

install_pcp : install

