#
# Copyright (c) 2012 Red Hat.
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

TARGET = pcp-collectl
PYFILES	= $(TARGET).py
MAN_PAGES = $(TARGET).1

default default_pcp: $(PYFILES)

include $(BUILDRULES)

install install_pcp: $(PYFILES)
ifeq "$(HAVE_PYTHON)" "true"
	$(INSTALL) -m 755 $(PYFILES) $(PCP_BINADM_DIR)/$(TARGET)
	$(INSTALL) -S $(PCP_BINADM_DIR)/$(TARGET) $(PCP_BIN_DIR)/pmcollectl
	@$(INSTALL_MAN)
endif
