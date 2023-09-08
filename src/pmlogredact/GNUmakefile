#
# Copyright (c) 2023 Ken McDonell.  All Rights Reserved.
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

default:

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 pmlogredact.sh $(PCP_BINADM_DIR)/pmlogredact$(SHELLSUFFIX)
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmlogredact
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmlogredact
	$(INSTALL) -m 644 -t $(PCP_VAR_DIR)/config/pmlogredact/network network $(PCP_SYSCONF_DIR)/pmlogredact/network
	$(INSTALL) -m 644 -t $(PCP_VAR_DIR)/config/pmlogredact/usernames usernames $(PCP_SYSCONF_DIR)/pmlogredact/usernames

default_pcp:	default

install_pcp:	install
