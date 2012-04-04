#
# Copyright (c) 2000,2004,2012 Silicon Graphics, Inc.  All Rights Reserved.
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

CFILES = py-pmapi.c
# LIB_FOR_BASENAME = "-lpcp"
LLDLIBS = -lpcp $(LIB_FOR_BASENAME)

LCFLAGS += '-DDSO_SUFFIX="$(DSOSUFFIX)"'

DSOVERSION = 3

LIBTARGET = pcp.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = pcp.$(DSOSUFFIX) libpcp.$(DSOSUFFIX).2

base default : $(LIBTARGET)

default_pcp : default

install_pcp : install

$(LIBTARGET): $(CFILES)
	$(CC) -shared -o $@ -g py-pmapi.c -fpic -lpcp



