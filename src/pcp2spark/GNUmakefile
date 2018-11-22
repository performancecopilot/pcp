#
# Copyright (c) 2018 Red Hat.
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

TARGET = pcp2spark
MAN_SECTION = 1
MAN_PAGES = $(TARGET).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)
DFILES = README.md PCP2SparkStreamCollector.python
EX_DIR = $(PCP_SHARE_DIR)/examples/pcp2spark

default: $(TARGET).py $(MAN_PAGES)

default:

include $(BUILDRULES)

install: default
ifeq "$(HAVE_PYTHON)" "true"
	$(INSTALL) -m 755 $(TARGET).py $(PCP_BIN_DIR)/$(TARGET)
	$(INSTALL) -m 755 -d $(EX_DIR)
	$(INSTALL) -m 644 $(DFILES) $(EX_DIR)
	@$(INSTALL_MAN)
endif

default_pcp: default

install_pcp: install

check:: $(TARGET).py
	$(PYLINT) $^
