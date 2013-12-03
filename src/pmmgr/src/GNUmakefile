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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

CXXMDTARGET = pmmgr$(EXECSUFFIX)
HFILES = pmmgr.h
CXXFILES = pmmgr.cxx 

LLDLIBS	= $(PCPLIB)
PCPLIB_LDFLAGS +=

LLDFLAGS += $(RDYNAMIC_FLAG) $(PIELDFLAGS)
LCFLAGS += $(PIECFLAGS)

default:	$(CXXMDTARGET) 

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 $(CXXMDTARGET) $(PCP_BINADM_DIR)/$(CXXMDTARGET)

default_pcp:	default

install_pcp:	install

pmmgr.o: $(HFILES)
