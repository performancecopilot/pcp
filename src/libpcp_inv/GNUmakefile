#! gmake
#
# Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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


TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs.pro

IRIX_DIRT = 32 N32 64 libpcp_inv*.so* libpcp_inv*.a
LDIRT	= $(IRIX_DIRT)

SUBDIRS = src

default install : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(TOPDIR)/src/include/buildrules.pro

default_pro : default

install_pro : install
