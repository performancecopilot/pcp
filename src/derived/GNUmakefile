#
# Copyright (c) 2016 Red Hat.  All Rights Reserved.
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

DIR = $(PCP_VAR_DIR)/config/derived

default :: default_pcp

default_pcp :

include $(BUILDRULES)

install :: default_pcp install_pcp

install_pcp : $(SUBDIRS) default_pcp
ifeq ($(TARGET_OS), linux)
	$(INSTALL) -m 755 -d $(DIR)
	$(INSTALL) -m 644 iostat.conf $(DIR)/iostat.conf
	$(INSTALL) -m 644 cpu-util.conf $(DIR)/cpu-util.conf
endif
