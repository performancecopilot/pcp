#!gmake
#
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
# 
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#

TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

CFILES=mlannetu.c mlansesu.c mlanllu.c linuxlnk.c ds2480ut.c mlantrnu.c
HFILES=mlan.h ds2480.h
STATICLIBTARGET = libmlan.a
LDIRT = libmlan.a
LCFLAGS = -g

default : $(STATICLIBTARGET)

ifeq "$(findstring $(TARGET_OS),solaris linux)" ""
default:
else
default : $(STATICLIBTARGET)
endif

.NOTPARALLEL:

install : 

default_pcp : default

install_pcp : install

include $(BUILDRULES)
