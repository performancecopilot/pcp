#
# Copyright (c) 2020 Red Hat, Inc.  All Rights Reserved.
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
-include ./GNUlocaldefs

TARGETS = pmafm mkaf
CONFIGETC = $(PCP_SYSCONF_DIR)/pmafm
CONFIGVAR = $(PCP_VAR_DIR)/config/pmafm

default :: default_pcp

default_pcp : $(TARGETS)

default_pro : $(TARGETS)

install :: install_pcp

install_pcp : default_pcp
	$(INSTALL) -m 755 pmafm $(PCP_BIN_DIR)/pmafm$(SHELLSUFFIX)
	$(INSTALL) -m 755 mkaf $(PCP_BINADM_DIR)/mkaf$(SHELLSUFFIX)
	$(INSTALL) -m 755 -d $(CONFIGETC)
	$(INSTALL) -m 755 -d $(CONFIGVAR)
	$(INSTALL) -m 644 -t $(CONFIGVAR)/pcp pmafm.pcp $(CONFIGETC)/pcp
ifeq "$(ENABLE_QT)" "true"
	$(INSTALL) -m 644 -t $(CONFIGVAR)/pcp-gui pmafm.pcp-gui $(CONFIGETC)/pcp-gui
endif

include $(BUILDRULES)
