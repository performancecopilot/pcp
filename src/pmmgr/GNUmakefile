#!gmake
#
# Copyright (c) 2013 Red Hat.
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

LLDIRT		= *.log *.dir *.pag so_locations

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = src
LSRCFILES = rc_pmmgr pmmgr.options

default : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 -d `dirname $(PCP_PMMGROPTIONS_PATH)`
	$(INSTALL) -m 644 pmmgr.options $(PCP_PMMGROPTIONS_PATH)
	$(INSTALL) -m 755 -d `dirname $(PCP_PMMGRRCLOCAL_PATH)`
	$(INSTALL) -m 755 rc_pmmgr $(PCP_RC_DIR)/pmmgr
	$(INSTALL) -o $(PCP_USER) -g $(PCP_GROUP) -m 775 -d $(PCP_LOG_DIR)/pmmgr
ifneq ($(PACKAGE_DISTRIBUTION),debian)
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmmgr
endif

include $(BUILDRULES)

default_pcp : default

install_pcp : install

# Hey, does anyone have a spare vowel?
