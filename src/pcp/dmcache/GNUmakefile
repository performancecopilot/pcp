#
# Copyright (c) 2014-2015 Red Hat.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TARGET = pcp-dmcache
LVTARGET = pcp-lvmcache
MAN_SECTION = 1
MAN_PAGES = $(TARGET).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default: $(TARGET).py $(MAN_PAGES)

include $(BUILDRULES)

install: default
ifeq "$(HAVE_PYTHON)" "true"
	$(INSTALL) -m 755 $(TARGET).py $(PCP_BINADM_DIR)/$(TARGET)
	$(INSTALL) -S $(PCP_BINADM_DIR)/$(TARGET) $(PCP_BINADM_DIR)/$(LVTARGET)
	@$(INSTALL_MAN)
endif

default_pcp : default

install_pcp : install
