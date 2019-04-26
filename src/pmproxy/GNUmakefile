#
# Copyright (c) 2014-2019 Red Hat.
# Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

SUBDIRS = src
LDIRT = *.log pmproxy.service

default: $(SUBDIRS) pmproxy.service
	$(SUBDIRS_MAKERULE)

install: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmproxy
	$(INSTALL) -m 644 pmproxy.conf pmproxy.options $(PCP_SYSCONF_DIR)/pmproxy
	$(INSTALL) -m 644 pmproxy.defaults $(PCP_SYSCONFIG_DIR)/pmproxy
	$(INSTALL) -m 755 rc_pmproxy $(PCP_RC_DIR)/pmproxy
ifeq ($(ENABLE_SYSTEMD),true)
	$(INSTALL) -m 644 pmproxy.service $(PCP_SYSTEMDUNIT_DIR)/pmproxy.service
endif
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_USER) -d $(PCP_LOG_DIR)/pmproxy

include $(BUILDRULES)

default_pcp : default

install_pcp : install

pmproxy.service: pmproxy.service.in
	$(SED) <$< >$@ \
	    -e 's;@PCP_RC_DIR@;'$(PCP_RC_DIR)';' \
	    -e 's;@PCP_RUN_DIR@;'$(PCP_RUN_DIR)';' \
	# END
