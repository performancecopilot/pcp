#
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

CONFFILES = builddefs pcp.conf
LSRCFILES = builddefs.in buildrules pcp.conf.in pcp.env pcp.mingw

LDIRT = builddefs.install

SUBDIRS = pcp

default :: default_pcp

default_pcp : $(SUBDIRS) pcp.conf builddefs.install
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

install :: default_pcp install_pcp

install_pcp : $(SUBDIRS) default_pcp
	$(INSTALL) -m 644 pcp.conf $(PCP_ETC_DIR)/pcp.conf
ifeq "$(TARGET_OS)" "mingw"
	$(INSTALL) -m 644 pcp.mingw $(PCP_ETC_DIR)/pcp.env
else
	$(INSTALL) -m 644 pcp.env $(PCP_ETC_DIR)/pcp.env
endif
	$(INSTALL) -m 644 buildrules $(PCP_INC_DIR)/buildrules
	$(INSTALL) -m 644 builddefs.install $(PCP_INC_DIR)/builddefs
	$(SUBDIRS_MAKERULE)

builddefs.install : builddefs
	sed -e 's;^include .*pcp.conf;include $$(PCP_DIR)/etc/pcp.conf;' \
	    -e 's;^BUILDRULES.*=.*buildrules;BUILDRULES = $$\(PCP_INC_DIR\)/buildrules;' \
	    -e 's;^INSTALL_SH.*=.*install-sh;INSTALL_SH = $$\(PCP_BINADM_DIR\)/install-sh;' \
	    -e 's;^GENPMDA.*=.*genpmda;GENPMDA = $$\(PCP_BIN_DIR\)/genpmda;' \
	        < builddefs > builddefs.install
