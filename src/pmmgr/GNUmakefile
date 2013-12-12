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

LSRCFILES=TODO rc_pmmgr pmmgr.options

SUBDIRS = config

# ... but only built on posix platforms with a c++ compiler
ifneq ($(TARGET_OS),mingw)
ifneq ($(CXX),"")
CXXMDTARGET = pmmgr$(EXECSUFFIX)
HFILES = pmmgr.h
CXXFILES = pmmgr.cxx
LLDLIBS	= $(PCPLIB)
PCPLIB_LDFLAGS +=
LLDFLAGS += $(RDYNAMIC_FLAG) $(PIELDFLAGS)
LCFLAGS += $(PIECFLAGS)
endif
endif

default : $(SUBDIRS) $(CXXMDTARGET)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

install :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 644 pmmgr.options $(PCP_PMMGROPTIONS_PATH)
	$(INSTALL) -m 755 rc_pmmgr $(PCP_RC_DIR)/pmmgr
ifneq ($(TARGET_OS),mingw)
ifneq ($(CXX),"")
	$(INSTALL) -m 755 $(CXXMDTARGET) $(PCP_BINADM_DIR)/$(CXXMDTARGET)
endif
endif

default_pcp : default

install_pcp : install

pmmgr.o: $(HFILES)

# Hey, does anyone have a spare vowel?
