#!gmake
#
# Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

SCRIPTS	= mover2pcp
DATAFILES = mover.log
TARGETS = $(SCRIPTS) $(DATAFILES)
LDIRT	= mover_v?.*
LSRCFILES	= $(TARGETS) moverv? mk.mover.log README \
	weather.dat 20624-copyright_permission_letter.pdf
EX_DIR	= $(PCP_SHARE_DIR)/examples/logimport

default: $(TARGETS)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(EX_DIR)
	$(INSTALL) -m 644 $(TARGETS) $(EX_DIR)

default_pcp : default

install_pcp : install
