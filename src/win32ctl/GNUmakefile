#
# Copyright (c) 2008-2011 Aconex.  All Rights Reserved.
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

SUBDIRS	= include lib \
	  eventlog services setevent
BATCHES	= pcp.bat pmafm.bat mkaf.bat pmsignal.bat
LSRCFILES = $(BATCHES)

default :: default_pcp

default_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install :: default_pcp install_pcp

install_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
ifeq "$(TARGET_OS)" "mingw"
	$(INSTALL) -m 755 $(BATCHES) $(PCP_BIN_DIR)
endif

include $(BUILDRULES)
