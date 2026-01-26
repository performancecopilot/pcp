#
# Copyright (c) 2015 Red Hat.
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

TARGET = pmrep
SCRIPT = $(TARGET).py
LNKTARGET = pcp2csv
MAN_PAGES = pmrep.1 pmrep.conf.5
BASHDIR = $(PCP_BASHSHARE_DIR)/completions
PMREPLIB = $(PCP_SHARE_DIR)/lib/pmrep
PMREPMODULES = groups.py
PYSCRIPTS = $(SCRIPT) groups.py metrics.py config.py header.py

SUBDIRS = conf test

include $(BUILDRULES)

default: $(SCRIPT) $(MAN_PAGES) $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install: default $(SUBDIRS)
ifeq "$(HAVE_PYTHON_ORDEREDDICT)" "true"
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 $(SCRIPT) $(PCP_BIN_DIR)/$(TARGET)
	$(INSTALL) -m 755 -d $(PMREPLIB)
	$(INSTALL) -m 644 groups.py $(PMREPLIB)/groups.py
	$(INSTALL) -S $(PCP_BIN_DIR)/$(TARGET) $(PCP_BIN_DIR)/$(LNKTARGET)
	$(INSTALL) -S $(BASHDIR)/pcp $(BASHDIR)/$(TARGET)
	@$(INSTALL_MAN)
endif

default_pcp: default

install_pcp: install

check :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

check :: $(PYSCRIPTS)
	$(PYLINT) $^

check :: pmrep.1 pmrep.conf.5
	$(MANLINT) $^
