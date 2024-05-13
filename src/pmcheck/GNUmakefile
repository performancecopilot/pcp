#
# Copyright (c) 2024 Ken McDonell.  All Rights Reserved.
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

LSRCFILES = pmcheck.sh checkproc.sh

default:

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 pmcheck.sh $(PCP_BINADM_DIR)/pmcheck$(SHELLSUFFIX)
	$(INSTALL) -S $(PCP_BINADM_DIR)/pmcheck$(SHELLSUFFIX) $(PCP_BINADM_DIR)/pcp-check$(SHELLSUFFIX)
	$(INSTALL) -m 644 -t $(PCP_SHARE_DIR)/lib/checkproc.sh checkproc.sh $(PCP_LIBADM_DIR)/checkproc.sh
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)/lib/pmcheck

default_pcp : default

install_pcp : install
