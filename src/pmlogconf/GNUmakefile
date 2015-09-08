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

SUBDIRS = v1.0 platform disk sgi memory cpu kernel filesystem networking tools

LSRCFILES = pmlogconf.sh pmlogconf-setup.sh GNUmakefile.groups

default: makefiles pmlogconf.sh pmlogconf-setup.sh

include $(BUILDRULES)

install: default $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 pmlogconf.sh $(PCP_BINADM_DIR)/pmlogconf
	$(INSTALL) -m 755 pmlogconf-setup.sh $(PCP_BINADM_DIR)/pmlogconf-setup

default_pcp : default

install_pcp : install

# for src-link-pcp target from buildrules
$(SUBDIRS):	makefiles

makefiles:
	@for d in $(SUBDIRS); do \
	    rm -f $$d/GNUmakefile; \
	    cp GNUmakefile.groups $$d/GNUmakefile; \
	done

groups: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
