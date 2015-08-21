#
# Copyright (c) 2014-2015 Red Hat.
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

CMDTARGET = pmproxy$(EXECSUFFIX)
HFILES = pmproxy.h
CFILES = pmproxy.c client.c util.c

LLDLIBS	= $(PCPLIB)
LDIRT = pmproxy.log pmproxy.service

LCFLAGS += $(PIECFLAGS)
LLDFLAGS += $(PIELDFLAGS)

default: $(CMDTARGET) pmproxy.service

pmproxy.service: pmproxy.service.in
	$(SED) -e 's;@path@;'$(PCP_RC_DIR)';' $< > $@

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d `dirname $(PCP_PMPROXYOPTIONS_PATH)`
	$(INSTALL) -m 644 pmproxy.options $(PCP_PMPROXYOPTIONS_PATH)
	$(INSTALL) -m 755 rc_pmproxy $(PCP_RC_DIR)/pmproxy
ifeq ($(ENABLE_SYSTEMD),true)
	$(INSTALL) -m 644 pmproxy.service $(PCP_SYSTEMDUNIT_DIR)/pmproxy.service
endif
	$(INSTALL) -m 644 pmproxy.defaults $(PCP_SYSCONFIG_DIR)/pmproxy
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)
	$(INSTALL) -m 775 -o $(PCP_USER) -g $(PCP_USER) -d $(PCP_LOG_DIR)/pmproxy

default_pcp : default

install_pcp : install

pmproxy.o client.o:	pmproxy.h
