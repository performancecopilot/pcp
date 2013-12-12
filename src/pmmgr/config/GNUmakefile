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

LLDIRT =

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

LSRCFILES = hostid-metrics.default log-directory.default pmie		\
	 pmieconf pmlogconf pmlogger pmlogmerge.default			\
	 pmlogmerge-retain.default README target-discovery.default	\
	 target-host.default target-discovery.avahi

PMMGR_SYSCONF_DIR=$(PCP_SYSCONF_DIR)/pmmgr

default:

include $(BUILDRULES)

install:
	$(INSTALL) -m 755 -o $(PCP_USER) -g $(PCP_GROUP) -d $(PMMGR_SYSCONF_DIR)
	for file in $(LSRCFILES); do \
	 	$(INSTALL) -m 644 -o $(PCP_USER) -g $(PCP_GROUP) $$file $(PMMGR_SYSCONF_DIR)/$$file; \
	done

default_pcp : default

install_pcp : install
