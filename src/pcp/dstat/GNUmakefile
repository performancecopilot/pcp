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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TARGET = pcp-dstat
SCRIPT = $(TARGET).py
MAN_PAGES = $(TARGET).1 $(TARGET).5

SUBDIRS = plugins

default: build-me

include $(BUILDRULES)

ifeq "$(HAVE_PYTHON_ORDEREDDICT)" "true"
build-me:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

build-me:: $(SCRIPT) $(MAN_PAGES)

install: default $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 $(SCRIPT) $(PCP_BINADM_DIR)/$(TARGET)
ifeq "$(ENABLE_DSTAT)" "true"
	$(INSTALL) -S $(PCP_BINADM_DIR)/pcp-dstat $(PCP_BIN_DIR)/dstat
endif
	@$(INSTALL_MAN)
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

check:: $(SCRIPT)
	$(PYLINT) $^
