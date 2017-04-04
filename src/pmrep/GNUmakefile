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
CONFIG = pmrep.conf
MAN_PAGES = pmrep.1 pmrep.conf.5

default: $(TARGET).py $(MAN_PAGES)

default:

include $(BUILDRULES)

install: default
ifeq "$(HAVE_PYTHON)" "true"
	$(INSTALL) -m 755 $(TARGET).py $(PCP_BIN_DIR)/$(TARGET)
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/$(TARGET)
	$(INSTALL) -m 644 $(CONFIG) $(PCP_SYSCONF_DIR)/$(TARGET)/$(CONFIG)
	@$(INSTALL_MAN)
endif

default_pcp: default

install_pcp: install
