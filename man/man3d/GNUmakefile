#
# Copyright (c) 2013-2014 Red Hat.
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

MAN_SECTION	= 3
MAN_PAGES	= $(shell echo *.3)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)
LSRCFILES	= $(MAN_PAGES)

ifeq "$(MAN_PAGES)" "*.3"
# no man pages in this directory ...
#
default::
default_pcp:
include $(BUILDRULES)
install::
install_pcp:
check::
else
default :: $(MAN_PAGES)

default_pcp : $(MAN_PAGES)

include $(BUILDRULES)

install :: install_pcp

install_pcp : default
	$(INSTALL_MAN)

check :: $(MAN_PAGES)
	$(MANLINT) $^
endif

