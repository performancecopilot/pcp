#
# Copyright (c) 2018 Red Hat.
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

CFILES = pminfo.c sha1.c
EXT_FILES = sha1.h sha1.c
CMDTARGET = pminfo$(EXECSUFFIX)
LLDLIBS = $(PCPLIB)
LDIRT = $(EXT_FILES)

default : $(CMDTARGET)

include $(BUILDRULES)

install : default
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)

default_pcp : $(EXT_FILES) default

install_pcp : install

$(EXT_FILES):
	$(LN_S) $(TOPDIR)/src/external/$@ .

pminfo.o:	$(TOPDIR)/src/include/pcp/libpcp.h $(EXT_FILES)

check:: $(CFILES)
	$(CLINT) $^
