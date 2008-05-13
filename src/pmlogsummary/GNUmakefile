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
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

CFILES	= pmlogcheck.c pmlogsummary.c

TARGETS	= pmlogcheck$(EXECSUFFIX) pmlogsummary$(EXECSUFFIX)

LLDLIBS	= -lpcp -lm

LDIRT += $(TARGETS)

default:	$(TARGETS)

pmlogcheck$(EXECSUFFIX):	pmlogcheck.o
	$(CCF) -o $@ $(LDFLAGS) pmlogcheck.o $(LDLIBS)

pmlogsummary$(EXECSUFFIX):	pmlogsummary.o
	$(CCF) -o $@ $(LDFLAGS) pmlogsummary.o $(LDLIBS)

include $(BUILDRULES)

install:	$(TARGETS)
	$(INSTALL) -m 755 $(TARGETS) $(PCP_BIN_DIR)

default_pcp:	default

install_pcp:	install
