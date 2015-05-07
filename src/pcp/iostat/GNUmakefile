#
# Copyright (c) 2014-2015 Red Hat, Inc.  All Rights Reserved.
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

CMDTARGET = pcp-iostat
LDIRT = $(CMDTARGET)

default: $(CMDTARGET).py

include $(BUILDRULES)

install: default
ifeq "$(HAVE_PYTHON)" "true"
	$(INSTALL) -m 755 $(CMDTARGET).py $(PCP_BINADM_DIR)/$(CMDTARGET)
	$(INSTALL) -S $(PCP_BINADM_DIR)/$(CMDTARGET) $(PCP_BIN_DIR)/pmiostat
endif

default_pcp: default

install_pcp: install
