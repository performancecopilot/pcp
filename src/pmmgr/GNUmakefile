#!gmake
#
# Copyright (c) 2013 Red Hat.
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

LLDIRT =

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

LSRCFILES=TODO rc_pmmgr

SUBDIRS = config
# ... but only built on posix platforms with a c++ compiler
ifneq ($(TARGET_OS),mingw)
ifneq ($(CXX),"")
SUBDIRS += src
endif
endif

default : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

install :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 rc_pmmgr $(PCP_RC_DIR)/pmmgr

clean :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

default_pcp : default

install_pcp : install

# Hey, does anyone have a spare vowel?
