#
# Copyright (c) 2015,2018 Red Hat.
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.
# 
# zbxpcp DSO - performance data export through Zabbix Agent Daemon
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

CFILES = zbxpcp.c
HFILES = module.h
VERSION_SCRIPT = exports

LIBTARGET = zbxpcp.$(DSOSUFFIX)
LIBCONFIG = zbxpcp.conf
LLDLIBS = $(PCPLIB)
LDIRT = $(LIBCONFIG)

CONFIG_DIR = $(PCP_SYSCONF_DIR)/../zabbix/zabbix_agentd.d
ZABBIX_DIR = $(PCP_LIB_DIR)/zabbix
MODULE_DIR = $(ZABBIX_DIR)/modules
AGENT_DIR = $(ZABBIX_DIR)/agent

MAN_SECTION = 3
MAN_PAGES = zbxpcp.$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default: $(LIBTARGET) $(LIBCONFIG) $(MAN_PAGES)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(ZABBIX_DIR)
	$(INSTALL) -m 755 -d $(MODULE_DIR)
	$(INSTALL) -m 755 -d $(AGENT_DIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(AGENT_DIR)/$(LIBTARGET)
	$(INSTALL) -S $(AGENT_DIR)/$(LIBTARGET) $(MODULE_DIR)/$(LIBTARGET)
	$(INSTALL) -m 755 -d $(CONFIG_DIR)
	$(INSTALL) -m 644 $(LIBCONFIG) $(CONFIG_DIR)/$(LIBCONFIG)
	@$(INSTALL_MAN)

default_pcp: default

install_pcp: install

$(LIBCONFIG):	$(LIBCONFIG).in
	$(SED) -e 's;@library@;$(LIBTARGET);' $< > $@

zbxpcp.o:	module.h
$(LIBTARGET):	$(VERSION_SCRIPT)

zbxpcp.o:	$(TOPDIR)/src/include/pcp/libpcp.h
