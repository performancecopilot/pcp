#
# Copyright (c) 2026 Red Hat.
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

IAM		= valkey
DOMAIN		= VALKEY
CMDTARGET	= pmdavalkey$(EXECSUFFIX)
DFILES		= README help
CONFIG		= valkey.conf

LIBVALKEY_HFILES = $(addprefix deps/libvalkey/, \
	include/valkey/alloc.h include/valkey/async.h include/valkey/cluster.h \
	include/valkey/valkey.h include/valkey/net.h include/valkey/read.h \
	include/valkey/visibility.h include/valkey/sockcompat.h src/adlist.h \
	src/async_private.h src/cmddef.h src/command.h src/dict.h src/fmacros.h \
	src/sds.h src/sdsalloc.h src/valkey_private.h src/vkutil.h src/win32.h)
LIBVALKEY_CFILES = $(addprefix deps/libvalkey/, \
	src/adlist.c src/alloc.c src/async.c src/cluster.c src/command.c \
	src/conn.c src/crc16.c src/dict.c src/net.c src/read.c src/sds.c \
	src/sockcompat.c src/valkey.c src/vkutil.c)
LIBVALKEY_XFILES = $(LIBVALKEY_HFILES) $(LIBVALKEY_CFILES)

CFILES		= pmdavalkey.c $(LIBVALKEY_CFILES)

LLDLIBS		= $(PCP_PMDALIB)
LCFLAGS		= -I. -Ideps/libvalkey/include/valkey -Ideps/libvalkey/include -Ideps/libvalkey/src

PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDACONFIG	= $(PCP_SYSCONF_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)

LDIRT		= domain.h *.o $(IAM).log pmda$(IAM) pmda_$(IAM).$(DSOSUFFIX) $(LIBVALKEY_XFILES)

default_pcp default:	$(LIBVALKEY_XFILES) $(CMDTARGET)

include $(BUILDRULES)

deps/libvalkey/include/valkey deps/libvalkey/src:
	mkdir -p $@

$(LIBVALKEY_XFILES): | deps/libvalkey/include/valkey deps/libvalkey/src
	$(LN_S) -f $(realpath $(TOPDIR))/vendor/github.com/valkey-io/$(@:deps/%=%) $@

install_pcp install:	default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) root pmns domain.h $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR)/$(CMDTARGET) $(CMDTARGET) $(PMDAADMDIR)/$(CMDTARGET)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) $(DFILES) $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/$(CONFIG) $(CONFIG) $(PMDACONFIG)/$(CONFIG)

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

%.o: $(LIBVALKEY_XFILES)

pmdavalkey.o: pmdavalkey.c domain.h

default_pcp : default

install_pcp : install
