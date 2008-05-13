#! gmake
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
#*************************************************************************
#
# Makefile - newhelp and chkhelp
#
# This Makefile builds the newhelp and chkhelp program executables.
#
#*************************************************************************
#

TOPDIR = ../..
include	$(TOPDIR)/src/include/builddefs

TARGETS = newhelp$(EXECSUFFIX) chkhelp$(EXECSUFFIX)

LLDFLAGS = $(WARN_OFF)

CFILES = newhelp.c chkhelp.c

LLDLIBS = -lpcp_pmda -lpcp

LDIRT = $(TARGETS)

default:	$(TARGETS)

install:	default
	$(INSTALL) -m 755 $(TARGETS) $(PCP_BINADM_DIR)

include $(BUILDRULES)

newhelp$(EXECSUFFIX):	newhelp.o
	$(CCF) -o $@ $(LDFLAGS) newhelp.o $(LDLIBS)

chkhelp$(EXECSUFFIX):	chkhelp.o
	$(CCF) -o $@ $(LDFLAGS) chkhelp.o $(LDLIBS)

default_pcp:	default

install_pcp:	install
