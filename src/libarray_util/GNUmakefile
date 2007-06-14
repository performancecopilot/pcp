#!gmake
#
# Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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
# This is a DO NOTHING makefile for non-IRIX builds ... all we really
# want to do here is
# 	- nothing for default, install, etc targets
# 	- remove crud for clean and install targets
#
# $Id: GNUmakefile,v 1.1 2004/06/15 09:39:44 kenmcd Exp $

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

IRIX_DIRT = 32 N32 64 BUILD libpcp_util*.so* libpcp_util*.a
LDIRT	= $(IRIX_DIRT)

default:

include $(BUILDRULES)

install:
default_pcp: default
install_pcp: install
