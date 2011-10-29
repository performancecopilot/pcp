#
# Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
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

LCFLAGS += -DPM_FAULT_INJECTION=1

include ../../libpcp/src/GNUmakefile

# over-ride soname and library name, keep DSOVERSION to make libpcp
#
STATICLIBTARGET = libpcp_fault.a
LIBTARGET = libpcp_fault.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libpcp_fault.$(DSOSUFFIX)

ifeq "$(TARGET_OS)" "darwin"
LIBTARGET = libpcp_fault.$(DSOVERSION).$(DSOSUFFIX)
endif
ifeq "$(TARGET_OS)" "mingw"
STATICLIBTARGET = 
LIBTARGET = libpcp_fault.$(DSOSUFFIX)
SYMTARGET =
endif

LDIRT += $(HFILES) $(CFILES) libpcp.$(DSOSUFFIX)

$(HFILES) $(CFILES):
	ln -s ../../libpcp/src/$@ .

qa_install:	install
	$(INSTALL) -m 644 ../../include/fault.h $(PCP_INC_DIR)/fault.h
