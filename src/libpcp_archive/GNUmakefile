#
# Copyright (C) 2001,2009 Silicon Graphics, Inc.  All Rights Reserved.
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
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


# libpcp_arch.so - library of helper routines for PCP archive writers
#

TOPDIR = ../..

include $(TOPDIR)/src/include/builddefs

BASE	= libpcp_arch.a

SUBDIRS = src

default install: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

default_pcp: default

install_pcp: install

