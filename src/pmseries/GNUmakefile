#
# Copyright (c) 2017-2019 Red Hat.
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

CFILES = pmseries.c
CMDTARGET = pmseries$(EXECSUFFIX)
LLDLIBS	= $(PCP_WEBLIB)
LCFLAGS = $(LIBUVCFLAGS)
ifeq "$(TARGET_OS)" "mingw"
LLDLIBS += -lws2_32
endif

default:	build-me

ifeq "$(HAVE_LIBUV)" "true"
build-me:	$(CMDTARGET)

install:	default
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/pmseries
	# compatibility symlink (share pmproxy config Redis server list)
	$(INSTALL) -S $(PCP_SYSCONF_DIR)/pmproxy/pmproxy.conf $(PCP_SYSCONF_DIR)/pmseries/pmseries.conf
else
build-me:
install:
endif

include $(BUILDRULES)

default_pcp : default

install_pcp : install

check:: $(CFILES)
	$(CLINT) $^
