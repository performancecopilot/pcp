#
# Copyright (c) 2018,2020 Red Hat.
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
COLLECTOR = PCP2SparkStreamCollector
MAN_SECTION = 1
MAN_PAGES = $(TARGET).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)
BASHDIR = $(PCP_BASHSHARE_DIR)/completions
EX_DIR = $(PCP_SHARE_DIR)/examples/pcp2spark

default: $(TARGET).py $(COLLECTOR).py $(MAN_PAGES)

include $(BUILDRULES)

install: default
ifeq "$(HAVE_PYTHON)" "true"
	$(INSTALL) -m 755 $(TARGET).py $(PCP_BIN_DIR)/$(TARGET)
	$(INSTALL) -S $(BASHDIR)/pcp $(BASHDIR)/$(TARGET)
	$(INSTALL) -m 755 -d $(EX_DIR)
	$(INSTALL) -m 644 README.md $(EX_DIR)/README.md
	$(INSTALL) -m 644 $(COLLECTOR).py $(EX_DIR)/$(COLLECTOR).python
	@$(INSTALL_MAN)
endif

default_pcp: default

install_pcp: install

check:: $(TARGET).py $(COLLECTOR).py
	$(PYLINT) $^

check :: $(MAN_PAGES)
	$(MANLINT) $^
