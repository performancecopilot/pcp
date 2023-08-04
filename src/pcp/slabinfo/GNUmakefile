#
# Copyright (c) 2023 Oracle and/or its affiliates.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
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

TARGET = pcp-slabinfo
SCRIPT = $(TARGET).py
MAN_SECTION = 1
MAN_PAGES = $(TARGET).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default: $(SCRIPT) $(MAN_PAGES)

include $(BUILDRULES)

install: default
ifeq "$(HAVE_PYTHON)" "true"
	$(INSTALL) -m 755 $(SCRIPT) $(PCP_BINADM_DIR)/$(TARGET)
	@$(INSTALL_MAN)
endif

default_pcp : default

install_pcp : install

check:: $(SCRIPT)
	$(PYLINT) $^

check :: $(MAN_PAGES)
	$(MANLINT) $^

