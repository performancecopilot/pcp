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

ifeq ($(ENABLE_SYSTEMD),true)
LDIRT	= pcp-reboot-init.service
else
LDIRT	= 
endif

default:: $(LDIRT)

install:: default
	$(INSTALL) -m 755 pcp-reboot-init.sh $(PCP_BINADM_DIR)/pcp-reboot-init$(SHELLSUFFIX)
ifeq ($(ENABLE_SYSTEMD),true)
	$(INSTALL) -m 644 pcp-reboot-init.service $(PCP_SYSTEMDUNIT_DIR)/pcp-reboot-init.service
endif

include $(BUILDRULES)

default_pcp : default

install_pcp : install

pcp-reboot-init.service : pcp-reboot-init.service.in
	$(SED) <$< >$@ \
	    -e 's;@PCP_BINADM_DIR@;'$(PCP_BINADM_DIR)';' \
	# END
