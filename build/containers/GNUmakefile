#
# Copyright (c) 2015 Red Hat.
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
BUILDDEFS = $(TOPDIR)/src/include/builddefs

include $(BUILDDEFS)
-include ./GNUlocaldefs

SUBDIRS = pcp-base pcp-collector pcp-monitor pcp-pmlogger pcp-testsuite

default default_pcp install install_pcp pack_pcp: $(BUILDDEFS) $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

$(BUILDDEFS) : $(BUILDDEFS).in
	@echo; echo Please run configure in $(TOPDIR) with the appropriate options to generate $@.
	@false

include $(BUILDRULES)

