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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TARGET = pmatop
PYFILES	= $(TARGET).py

default default_pcp: $(PYFILES)

include $(BUILDRULES)

install install_pcp: $(PYFILES)
ifneq "$(PYTHON)" ""
	$(INSTALL) -m 755 $(PYFILES) $(PCP_BIN_DIR)/$(TARGET)
endif
