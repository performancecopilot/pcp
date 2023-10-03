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

CMDSCRIPT = pmlogredact.sh
CMDTARGET = pmlogredact$(SHELLSUFFIX)
SYSCONFIG = $(PCP_SYSCONF_DIR)/pmlogredact
VARCONFIG = $(PCP_VAR_DIR)/config/pmlogredact

default:

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 $(CMDSCRIPT) $(PCP_BIN_DIR)/$(CMDTARGET)
	$(INSTALL) -S $(PCP_BIN_DIR)/$(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)
	$(INSTALL) -m 755 -d $(SYSCONFIG)/pmlogredact
	$(INSTALL) -m 755 -d $(VARCONFIG)
	$(INSTALL) -m 644 -t $(VARCONFIG)/network network $(SYSCONFIG)/network
	$(INSTALL) -m 644 -t $(VARCONFIG)/usernames usernames $(SYSCONFIG)/usernames

default_pcp:	default

install_pcp:	install
