#
# Copyright (c) 2013-2015 Red Hat.
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

CONFIGS = pmie pmieconf pmlogconf pmlogger \
	pmlogmerge pmlogmerge-rewrite pmlogmerge-granular pmlogreduce \
	README target-discovery.example-avahi 

PMMGR_SYSCONF_DIR=$(PCP_SYSCONF_DIR)/pmmgr

default:

build-me:

include $(BUILDRULES)

install:
	$(INSTALL) -m 755 -d $(PMMGR_SYSCONF_DIR)
	for file in $(CONFIGS); do \
		$(INSTALL) -m 644 $$file $(PMMGR_SYSCONF_DIR)/$$file; \
	done

default_pcp : default

install_pcp : install
