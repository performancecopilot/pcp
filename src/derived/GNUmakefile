#
# Copyright (c) 2016-2017,2020 Red Hat.  All Rights Reserved.
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

CONFIGVAR = $(PCP_VAR_DIR)/config/derived
CONFIGETC = $(PCP_SYSCONF_DIR)/derived
CONFIGFILES = $(shell echo *.conf)

default:	$(CONFIGFILES)

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(CONFIGETC)
	$(INSTALL) -m 755 -d $(CONFIGVAR)
	$(INSTALL) -m 644 -t $(CONFIGVAR) $(CONFIGFILES) $(CONFIGETC)

default_pcp:	default

install_pcp:	install
