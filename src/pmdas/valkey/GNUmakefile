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

CFILES		= pmdavalkey.c

LLDLIBS		= $(PCP_PMDALIB) $(TOPDIR)/src/libvalkey/src/libvalkey.a
LCFLAGS		= -I. -I$(TOPDIR)/vendor/github.com/valkey-io/libvalkey/include \
		  -I$(TOPDIR)/vendor/github.com/valkey-io/libvalkey/src

PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDACONFIG	= $(PCP_SYSCONF_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)

LDIRT		= domain.h *.o $(IAM).log pmda$(IAM) pmda_$(IAM).$(DSOSUFFIX)

default_pcp default:	$(CMDTARGET)

include $(BUILDRULES)

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

pmdavalkey.o: pmdavalkey.c domain.h

default_pcp : default

install_pcp : install
