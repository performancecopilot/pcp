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
# Makefile - sample PMDA (both shared object and daemon)
#
#*************************************************************************

#
# Every Makefile must define ISM_DEPTH to be the path to the top of
# the ISM. In this case the top is two directories up. The ismdefs file
# must then be included.
#
TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

#
# The TARGETS macro must be set to the name of the executable we
# are building.
#
CMDTARGET = pmdasample$(EXECSUFFIX)
LIBTARGET = pmda_sample.$(DSOSUFFIX)
TARGETS = $(CMDTARGET) $(LIBTARGET)

PMDADIR = $(PCP_PMDAS_DIR)/sample

LDIRT = $(TARGETS)

CFILES = pmda.c sample.c

LCFLAGS = -DDYNAMIC_SPEC=\"$(PCP_PMDAS_DIR)/sample/dynamic.indom\"
LLDLIBS = -lpcp -lpcp_pmda

default:	../domain.h $(TARGETS)

../domain.h:
	$(MAKE) -C .. domain.h

install:	default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(CMDTARGET) $(LIBTARGET) $(PMDADIR)

include $(BUILDRULES)

default_pcp:	default

install_pcp:	install
