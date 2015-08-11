# Copyright (c) 2013 Red Hat Inc. All Rights Reserved.
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

HFILES = metrics.h
CFILES  = collectl2pcp.c cpu.c disk.c net.c load.c timestamp.c util.c \
          metrics.c header.c generic.c proc.c
LSRCFILES = pmdesc.c

CMDTARGET = collectl2pcp
LLDLIBS = -L$(TOPDIR)/src/libpcp_import/src -lpcp_import $(PCPLIB)
# LCFLAGS += -pg

#
# metrics.c is generated (but committed for now)
# LDIRT   = metrics.c
LDIRT = pmdesc pmdesc.o

default: pmdesc $(CMDTARGET)

include $(BUILDRULES)

install: $(CMDTARGET)
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)

# metrics.c: pmdesc
# 	pminfo hinv kernel mem swap network disk filesys \
# 	swapdev rpc nfs nfs3 nfs4 xfs pmda ipc vfs quota tmpfs sysfs proc \
# 	| ./pmdesc > metrics.c

pmdesc: pmdesc.o

default_pcp:    default

install_pcp:    install
