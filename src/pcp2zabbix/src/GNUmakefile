#
# Copyright (c) 2015 Red Hat.
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

CFILES	= zbxpcp.c
HFILES	= module.h
VERSION_SCRIPT = exports

LIBTARGET = zbxpcp.$(DSOSUFFIX)
LLDLIBS = $(PCPLIB)
ZABBIX_DIR = /var/lib/zabbix
MODULE_DIR = $(ZABBIX_DIR)/modules

default: $(LIBTARGET)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(ZABBIX_DIR)
	$(INSTALL) -m 755 -d $(MODULE_DIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(MODULE_DIR)/$(LIBTARGET)

default_pcp: default

install_pcp: install

zbxpcp.o:	module.h
$(LIBTARGET):	$(VERSION_SCRIPT)
