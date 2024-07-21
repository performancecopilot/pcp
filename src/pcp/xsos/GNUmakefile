#
# Copyright (c) 2024 Red Hat.
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

TARGET = pcp-xsos
XFILES = README.md $(TARGET) $(TARGET).1
MAN_PAGES = $(TARGET).1
LDIRT = $(XFILES)

default: $(XFILES) $(TARGET)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 $(TARGET) $(PCP_BINADM_DIR)/$(TARGET)
	$(INSTALL_MAN)

default_pcp : default

install_pcp : install

$(XFILES):
	$(LN_S) -f $(TOPDIR)/vendor/github.com/performancecopilot/xsos-pcp/$@ $@

check:: $(MAN_PAGES)
	$(MANLINT) $^
