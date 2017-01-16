# Copyright (c) 2017 Red Hat.
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

default: default_pcp

include $(BUILDRULES)
#ifeq "$(ENABLE_SELINUX)" "true"
default_pcp:
	make -f /usr/share/selinux/devel/Makefile
#else
#endif


install_pcp install: default
	$(INSTALL) -m 755 -d $(PCP_SELINUX_DIR)
	$(INSTALL) -m 600 pcp.pp $(PCP_SELINUX_DIR)/pcp.pp
#       $(INSTALL) -m 644 pcp.if /usr/share/selinux/devel/include/contrib
