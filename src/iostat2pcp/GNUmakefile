#!gmake
#
# Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
# Copyright (c) 2009 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

SCRIPT	= iostat2pcp
LDIRT	= $(MAN_PAGES) $(MAN_PAGES).tmp
LSRCFILES	= $(SCRIPT) README

ifneq ($(POD2MAN),)
MAN_SECTION = 1
MAN_PAGES = $(SCRIPT).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)
endif

default: $(MAN_PAGES)

$(SCRIPT).$(MAN_SECTION): $(SCRIPT)
	$(POD_MAKERULE)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 $(SCRIPT) $(PCP_BIN_DIR)/$(SCRIPT)
	@$(INSTALL_MAN)

default_pcp : default

install_pcp : install
